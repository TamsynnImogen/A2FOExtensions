/*
 * Extend Armada's native object-properties dialog, not its editor input loop.
 * Keep drafts in controls until OK. Craft's own fields are still committed by
 * the native dialog destructor; reapply the four-facing snapshot afterwards.
 *
 * Persistence is an optional prefix at Craft's first scalar IO call. This
 * deliberately leaves Craft::Save/Load entry hooks to A2FOEnergySystems.
 */
#include "object_editor.hpp"
#include "fleetops_object_editor.hpp"
#include "../../rust/stage8_b_object_editor_ffi.h"
#include "../A2FODirectionalShields/api.hpp"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../sdk/include/a2fo_module_provider.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <string_view>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(void*, void*);
void a2fo_editor_save_prefix_bridge();
bool __cdecl a2fo_editor_write_first_field(
    void*, float, const char*, void*) noexcept;
}

namespace a2fo::object_editor {
namespace {

constexpr std::uintptr_t kDialogProcRva = 0x00101560;
constexpr std::uintptr_t kDialogDestructorRva = 0x00100fe0;
constexpr std::uintptr_t kSaveFirstFieldCallRva = 0x000c299d;
constexpr std::uintptr_t kLoadFirstFieldCallRva = 0x000c2359;
constexpr std::uintptr_t kOutFloatRva = 0x0012ee70;
constexpr std::uintptr_t kInFloatRva = 0x0012efb0;
constexpr std::uintptr_t kOutBytesRva = 0x0012c680;
constexpr std::uintptr_t kActiveObjectRva = 0x00363fec;
constexpr std::uintptr_t kDialogCancelledRva = 0x00363f18;
constexpr std::uintptr_t kDialogLabelRva = 0x00363fbc;
constexpr std::size_t kObjectFlagsOffset = 0x14;
constexpr std::size_t kObjectHandleOffset = 0x28;
constexpr std::size_t kObjectClassOffset = 0x40;
constexpr std::size_t kFirstCraftFieldOffset = 0x1c0;
constexpr std::size_t kReaderCurrentOffset = 0x54;
constexpr std::size_t kReaderEndOffset = 0x58;
constexpr int kNativeLabelId = 0x3f2;
constexpr int kNativeShieldCurrentId = 0x3fb;
constexpr int kNativeShieldMaximumId = 0x3fd;
constexpr int kFirstControl = 0x6200;
constexpr int kCaptainEdit = kFirstControl + 1;
constexpr int kCaptainAuto = kFirstControl + 2;
constexpr int kRegistryEdit = kFirstControl + 3;
constexpr int kRegistryAuto = kFirstControl + 4;
constexpr int kShieldEdit = kFirstControl + 8;
constexpr int kLastControl = kFirstControl + 64;
constexpr std::array<std::uint8_t, 6> kDialogSignature{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x50};
constexpr std::array<std::uint8_t, 6> kDestructorSignature{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x3c};
constexpr std::array<std::uint8_t, 5> kSaveSignature{
    0xe8, 0xce, 0xc4, 0x06, 0x00};
constexpr std::array<std::uint8_t, 5> kLoadSignature{
    0xe8, 0x52, 0xcc, 0x06, 0x00};
constexpr std::array<const char*, 4> kFacingNames{
    "Forward", "Aft", "Port", "Starboard"};

using OutFloat = bool (__cdecl *)(void*, float, const char*);
using InFloat = bool (__cdecl *)(void*, float*);
using OutBytes = bool (__cdecl *)(void*, const void*, std::uint32_t,
                                   const char*);
using ShieldEnabled = bool (A2FO_CALL *)(void*);

struct Entry {
    SavedState state{};
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    std::uint64_t revision = 0;
    bool loading = false;
    bool pending_shields = false;
};

struct DialogState {
    HWND window = nullptr;
    void* craft = nullptr;
    RECT original_bounds{};
    std::array<HWND, 32> controls{};
    std::size_t control_count = 0;
    std::array<bool, 2> aggregate_read_only{};
    std::array<char, kTextCapacity> default_captain{};
    std::array<char, kTextCapacity> default_registry{};
    bool active = false;
    bool changing_text = false;
    bool shields = false;
};

struct AcceptedDraft {
    void* craft = nullptr;
    A2FO_DirectionalShieldState shields{};
    bool has_shields = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
ReadDefaults g_read_defaults = nullptr;
bool g_ready = false;
bool g_legacy_ready = false;
A2FO_InlineHook g_dialog_hook{};
A2FO_InlineHook g_destructor_hook{};
std::unordered_map<void*, Entry> g_entries;
std::uint64_t g_revision = 0;
DialogState g_dialog{};
AcceptedDraft g_accepted{};
A2FO_DirectionalShieldsGetStateFn g_get_shields = nullptr;
A2FO_DirectionalShieldsSetStateFn g_set_shields = nullptr;
ShieldEnabled g_shields_enabled = nullptr;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log("A2FOCraftIdentity.ObjectEditor", message);
    OutputDebugStringA("[A2FOCraftIdentity object editor] ");
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_armada) + rva);
}

bool accessible(const void* memory, std::size_t size,
                bool writable = false) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(memory);
    if (!cursor || size > std::numeric_limits<std::uintptr_t>::max() - cursor)
        return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                          sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const DWORD protection = info.Protect & 0xff;
        const bool can_write = protection == PAGE_READWRITE ||
            protection == PAGE_WRITECOPY ||
            protection == PAGE_EXECUTE_READWRITE ||
            protection == PAGE_EXECUTE_WRITECOPY;
        const bool can_read = can_write || protection == PAGE_READONLY ||
            protection == PAGE_EXECUTE_READ;
        if (!can_read || (writable && !can_write)) return false;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base)
            return false;
        const auto next = base + info.RegionSize;
        if (next <= cursor) return false;
        cursor = std::min<std::uintptr_t>(end, next);
    }
    return true;
}

template <typename T>
T read_at(const void* memory, std::size_t offset, T fallback = T{}) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    if (!address || offset > std::numeric_limits<std::uintptr_t>::max() - address)
        return fallback;
    const void* source = reinterpret_cast<const void*>(address + offset);
    if (!accessible(source, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, source, sizeof(value));
    return value;
}

bool is_craft(void* craft) noexcept {
    return (read_at<std::uint32_t>(craft, kObjectFlagsOffset) & 8u) != 0 &&
        read_at<void*>(craft, kObjectClassOffset) != nullptr;
}

Entry* find_entry(void* craft) noexcept {
    struct Context {void* craft;decltype(g_entries)::iterator found{};
        std::exception_ptr error{};} c{craft};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 120:c.found=g_entries.find(c.craft);return c.found!=g_entries.end();
        case 121:return c.found->second.object_class==
            read_at<void*>(c.craft,kObjectClassOffset);
        case 122:return c.found->second.loading;
        case 123:return c.found->second.handle==
            read_at<std::uint32_t>(c.craft,kObjectHandleOffset);
        case 124:g_entries.erase(c.found);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool found=a2fo_rs_stage8_b_object_editor_find_entry(&c,op)!=0;
    if(c.error)std::terminate();
    return found?&c.found->second:nullptr;
}

Entry* prepare_entry(void* craft) noexcept {
    struct Context {void* craft;Entry* result{};Entry entry{};
        std::exception_ptr error{};} c{craft};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 130:c.result=find_entry(c.craft);return c.result!=nullptr;
        case 131:return is_craft(c.craft);
        case 132:c.entry.object_class=read_at<void*>(c.craft,kObjectClassOffset);return 0;
        case 133:c.entry.handle=read_at<std::uint32_t>(c.craft,kObjectHandleOffset);return 0;
        case 134:
            try {c.result=&g_entries.emplace(c.craft,c.entry).first->second;
                return c.result!=nullptr;}
            catch(...){return 0;}
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool ready=a2fo_rs_stage8_b_object_editor_prepare_entry(&c,op)!=0;
    if(c.error)std::terminate();
    return ready?c.result:nullptr;
}

void assign_entry(Entry* entry, const SavedState& state, bool loading) noexcept {
    struct Context {Entry* entry;const SavedState& state;bool loading;
        std::exception_ptr error{};} c{entry,state,loading};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 140:c.entry->state=c.state;return 0;
        case 141:c.entry->revision=++g_revision;
            return c.entry->revision!=0;
        case 142:c.entry->revision=++g_revision;return 0;
        case 143:c.entry->loading=c.loading;return 0;
        case 144:c.entry->pending_shields=c.loading&&
            (c.state.flags&kShields)!=0;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_object_editor_assign_entry(&c,op);
    if(c.error)std::terminate();
}

template <typename Function>
Function resolve_export(HMODULE module, const char* name) noexcept {
    FARPROC address = module ? GetProcAddress(module, name) : nullptr;
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

bool resolve_shields() noexcept {
    struct Context {HMODULE module{};std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 150:return g_get_shields!=nullptr;
        case 151:return g_set_shields!=nullptr;
        case 152:return g_shields_enabled!=nullptr;
        case 153:c.module=a2fo::module_provider("A2FODirectionalShields.dll",
            {"A2FODirectionalShields_GetState",
             "A2FODirectionalShields_SetState",
             "A2FODirectionalShields_IsEnabled"});return 0;
        case 154:g_get_shields=resolve_export<A2FO_DirectionalShieldsGetStateFn>(
            c.module,"A2FODirectionalShields_GetState");return 0;
        case 155:g_set_shields=resolve_export<A2FO_DirectionalShieldsSetStateFn>(
            c.module,"A2FODirectionalShields_SetState");return 0;
        case 156:g_shields_enabled=resolve_export<ShieldEnabled>(
            c.module,"A2FODirectionalShields_IsEnabled");return 0;
        case 157:return g_get_shields!=nullptr;
        case 158:return g_set_shields!=nullptr;
        case 159:return g_shields_enabled!=nullptr;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_resolve_shields(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

void copy_shields(SavedState* state,
                  const A2FO_DirectionalShieldState& shields) noexcept {
    std::copy_n(shields.current, 4, state->current.begin());
    std::copy_n(shields.maximum, 4, state->maximum.begin());
    state->flags |= kShields;
}

A2FO_DirectionalShieldState shield_snapshot(const SavedState& state) noexcept {
    A2FO_DirectionalShieldState shields{};
    shields.struct_size = sizeof(shields);
    std::copy_n(state.current.begin(), 4, shields.current);
    std::copy_n(state.maximum.begin(), 4, shields.maximum);
    return shields;
}

bool make_save_state(void* craft, SavedState* state) noexcept {
    struct Context {void* craft;SavedState* state;Entry* entry{};
        A2FO_DirectionalShieldState shields{};std::exception_ptr error{};
    } c{craft,state};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 160:*c.state=SavedState{};return 0;
        case 161:c.entry=find_entry(c.craft);return c.entry!=nullptr;
        case 162:*c.state=c.entry->state;return 0;
        case 163:return resolve_shields();
        case 164:return g_shields_enabled(c.craft);
        case 165:c.shields.struct_size=sizeof(c.shields);return 0;
        case 166:return g_get_shields(c.craft,&c.shields);
        case 167:copy_shields(c.state,c.shields);return 0;
        case 168:return valid_state(*c.state);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_make_save_state(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

bool __cdecl read_first_field(void* reader, float* destination) noexcept {
    struct Context {void* reader;float* destination;InFloat original{};
        std::uintptr_t address{},begin{},end_address{};void* craft{};
        const char* cursor{};const char* end{};std::size_t available{},consumed{};
        bool binary{},tagged{};SavedState state{};Entry* entry{};
        PrefixResult result{PrefixResult::absent};std::exception_ptr error{};
    } c{reader,destination};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 40:c.original=reinterpret_cast<InFloat>(at(kInFloatRva));return 0;
        case 41:return g_ready;
        case 42:return c.original(c.reader,c.destination);
        case 43:c.address=reinterpret_cast<std::uintptr_t>(c.destination);return 0;
        case 44:return c.address>=kFirstCraftFieldOffset;
        case 45:return accessible(c.reader,kReaderEndOffset+sizeof(void*));
        case 46:return accessible(static_cast<char*>(c.reader)+kReaderCurrentOffset,
            sizeof(void*),true);
        case 47:c.craft=reinterpret_cast<void*>(c.address-kFirstCraftFieldOffset);return 0;
        case 48:return is_craft(c.craft);
        case 49:g_entries.erase(c.craft);return 0;
        case 50:c.cursor=read_at<const char*>(c.reader,kReaderCurrentOffset);return 0;
        case 51:c.end=read_at<const char*>(c.reader,kReaderEndOffset);return 0;
        case 52:c.begin=reinterpret_cast<std::uintptr_t>(c.cursor);return 0;
        case 53:c.end_address=reinterpret_cast<std::uintptr_t>(c.end);return 0;
        case 54:return c.begin!=0;
        case 55:return c.end_address>=c.begin;
        case 56:c.available=std::min<std::size_t>(c.end_address-c.begin,4096);return 0;
        case 57:return accessible(c.cursor,c.available);
        case 58:c.binary=read_at<std::uint8_t>(c.reader,6)!=0;return 0;
        case 59:c.tagged=read_at<std::uint8_t>(c.reader,7)!=0;return 0;
        case 60:c.state=SavedState{};c.consumed=0;return 0;
        case 61:c.result=decode_prefix(std::string_view(c.cursor,c.available),
            c.binary,c.tagged,&c.state,&c.consumed);
            return static_cast<std::uint32_t>(c.result);
        case 62:log_line("Invalid object-editor save record; refusing to misalign Craft data");return 0;
        case 63:c.entry=prepare_entry(c.craft);return c.entry!=nullptr;
        case 64:assign_entry(c.entry,c.state,true);return 0;
        case 65:c.cursor+=c.consumed;
            std::memcpy(static_cast<char*>(c.reader)+kReaderCurrentOffset,
                &c.cursor,sizeof(c.cursor));return 0;
        case 66:return c.original(c.reader,c.destination);
        case 67:g_entries.erase(c.craft);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_read_field(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

void A2FO_CALL craft_event_handler(const A2FO_CraftEvent* event, void*) {
    struct Context {const A2FO_CraftEvent* event;Entry* entry{};
        std::exception_ptr error{};} c{event};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 80:return c.event!=nullptr;
        case 81:return c.event->struct_size>=sizeof(*c.event);
        case 82:return c.event->craft!=nullptr;
        case 83:return c.event->kind==A2FO_CRAFT_EVENT_CLEANUP;
        case 84:g_entries.erase(c.event->craft);return 0;
        case 85:return g_accepted.craft==c.event->craft;
        case 86:g_accepted=AcceptedDraft{};return 0;
        case 87:return g_ready;
        case 88:return c.event->kind==A2FO_CRAFT_EVENT_POST_LOAD;
        case 89:c.entry=find_entry(c.event->craft);return c.entry!=nullptr;
        case 90:c.entry->handle=read_at<std::uint32_t>(c.event->craft,
            kObjectHandleOffset);return 0;
        case 91:c.entry->loading=false;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_object_editor_event(&c,op);
    if(c.error)std::rethrow_exception(c.error);
}

void set_number(HWND control, float number, bool aggregate = false) noexcept {
    char text[64]{};
    std::snprintf(text, sizeof(text), aggregate ? "%.0f" : "%.9g",
                  static_cast<double>(number));
    SetWindowTextA(control, text);
}

bool read_shield_controls(SavedState* state) noexcept {
    struct Context {SavedState* state;std::size_t index{};
        char current[64]{},maximum[64]{};std::exception_ptr error{};
    } c{state};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 240:case 241:case 242:case 243:c.index=id-240;
            std::memset(c.current,0,sizeof(c.current));
            std::memset(c.maximum,0,sizeof(c.maximum));return 0;
        case 244:GetDlgItemTextA(g_dialog.window,kShieldEdit+
            static_cast<int>(c.index)*2,c.current,sizeof(c.current));return 0;
        case 245:GetDlgItemTextA(g_dialog.window,kShieldEdit+
            static_cast<int>(c.index)*2+1,c.maximum,sizeof(c.maximum));return 0;
        case 246:return parse_shield_number(c.current,&c.state->current[c.index]);
        case 247:return parse_shield_number(c.maximum,&c.state->maximum[c.index]);
        case 248:c.state->flags|=kShields;return 0;
        case 249:return valid_state(*c.state);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_read_shield_controls(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

void mirror_shield_totals(const SavedState& state) noexcept {
    float current = 0.0f;
    float maximum = 0.0f;
    for (std::size_t i = 0; i < 4; ++i) {
        current += state.current[i];
        maximum += state.maximum[i];
    }
    set_number(GetDlgItem(g_dialog.window, kNativeShieldCurrentId), current, true);
    set_number(GetDlgItem(g_dialog.window, kNativeShieldMaximumId), maximum, true);
}

void undo_controls() noexcept {
    struct Context {std::size_t index{};std::exception_ptr error{};} c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            if(id>=0x10000) {c.index=id-0x10000;return 0;}
            switch(id) {
            case 250:g_dialog.active=false;return 0;
            case 251:return g_dialog.control_count;
            case 256:return g_dialog.controls[c.index]!=nullptr;
            case 257:DestroyWindow(g_dialog.controls[c.index]);return 0;
            case 258:return g_dialog.shields;
            case 259:SendDlgItemMessageA(g_dialog.window,kNativeShieldCurrentId,
                EM_SETREADONLY,g_dialog.aggregate_read_only[0],0);return 0;
            case 260:SendDlgItemMessageA(g_dialog.window,kNativeShieldMaximumId,
                EM_SETREADONLY,g_dialog.aggregate_read_only[1],0);return 0;
            case 261:{const RECT& rect=g_dialog.original_bounds;
                SetWindowPos(g_dialog.window,nullptr,rect.left,rect.top,
                    rect.right-rect.left,rect.bottom-rect.top,
                    SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
            case 262:g_dialog=DialogState{};return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_object_editor_undo_controls(&c,op);
    if(c.error)std::terminate();
}

bool add_controls(HWND window) noexcept {
    struct Context {
        HWND window;void* craft{};RECT client{},units{0,0,4,8};
        int pane_width{},pane_height{},pane_x{},pane_y{};
        int frame_width{},frame_height{},width{},height{},left{},top{};
        MONITORINFO monitor{};bool have_monitor{},created{true};
        HFONT font{};std::size_t facing{};int row_y{};
        HWND current{},maximum{};bool active_facing{},shields_ok{};
        SavedState state{};const Entry* entry{};
        A2FO_DirectionalShieldState shields{};std::exception_ptr error{};
    } c{window};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            const auto x=[&](int value){return MulDiv(value,c.units.right,4);};
            const auto y=[&](int value){return MulDiv(value,c.units.bottom,8);};
            const auto control=[&](const char* klass,const char* label,DWORD style,
                int control_id,int cx,int cy,int cw,int ch,DWORD extended=0)->HWND {
                if(!c.created||g_dialog.control_count==g_dialog.controls.size()){
                    c.created=false;return nullptr;
                }
                HWND child=CreateWindowExA(extended,klass,label,
                    WS_CHILD|WS_VISIBLE|style,c.pane_x+x(cx),c.pane_y+y(cy),
                    x(cw),y(ch),c.window,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(control_id)),
                    g_armada,nullptr);
                if(!child){c.created=false;return nullptr;}
                g_dialog.controls[g_dialog.control_count++]=child;
                if(c.font)SendMessageA(child,WM_SETFONT,
                    reinterpret_cast<WPARAM>(c.font),TRUE);
                return child;
            };
            const auto label=[&](const char* value,int cx,int cy,int cw,int ch){
                return control("STATIC",value,SS_LEFT,-1,cx,cy,cw,ch);
            };
            const auto edit=[&](int control_id,int cx,int cy,int cw,int limit){
                HWND child=control("EDIT","",WS_TABSTOP|ES_AUTOHSCROLL,
                    control_id,cx,cy,cw,14,WS_EX_CLIENTEDGE);
                if(child)SendMessageA(child,EM_SETLIMITTEXT,limit,0);
                return child;
            };
            switch(id) {
            case 500:g_accepted=AcceptedDraft{};return 0;
            case 501:g_dialog=DialogState{};return 0;
            case 502:c.craft=read_at<void*>(at(kActiveObjectRva),0);return 0;
            case 503:return is_craft(c.craft);
            case 504:g_dialog.window=c.window;g_dialog.craft=c.craft;
                g_dialog.changing_text=true;return 0;
            case 505:return GetWindowRect(c.window,&g_dialog.original_bounds);
            case 506:return GetClientRect(c.window,&c.client);
            case 507:return MapDialogRect(c.window,&c.units);
            case 508:{
                c.pane_width=x(220);c.pane_height=y(250);
                c.pane_x=c.client.right+x(6);c.pane_y=y(6);
                const RECT& original=g_dialog.original_bounds;
                c.frame_width=original.right-original.left-c.client.right;
                c.frame_height=original.bottom-original.top-c.client.bottom;
                return 0;
            }
            case 509:c.monitor.cbSize=sizeof(c.monitor);
                c.have_monitor=GetMonitorInfoA(
                    MonitorFromWindow(c.window,MONITOR_DEFAULTTONEAREST),
                    &c.monitor)!=FALSE;return 0;
            case 510:return c.have_monitor&&
                c.pane_x+c.pane_width+x(6)+c.frame_width>
                c.monitor.rcWork.right-c.monitor.rcWork.left;
            case 511:c.pane_x=x(6);c.pane_y=c.client.bottom+y(6);return 0;
            case 512:c.width=std::max<int>(c.client.right,
                    c.pane_x+c.pane_width+x(6))+c.frame_width;
                c.height=std::max<int>(c.client.bottom,
                    c.pane_y+c.pane_height+y(6))+c.frame_height;return 0;
            case 513:return c.have_monitor&&
                (c.width>c.monitor.rcWork.right-c.monitor.rcWork.left||
                 c.height>c.monitor.rcWork.bottom-c.monitor.rcWork.top);
            case 514:g_dialog=DialogState{};return 0;
            case 515:log_line("Object-editor extension does not fit the current work area");
                return 0;
            case 516:{
                const RECT& original=g_dialog.original_bounds;
                c.left=c.have_monitor?std::max<int>(c.monitor.rcWork.left,
                    std::min<int>(original.left,c.monitor.rcWork.right-c.width))
                    :original.left;
                c.top=c.have_monitor?std::max<int>(c.monitor.rcWork.top,
                    std::min<int>(original.top,c.monitor.rcWork.bottom-c.height))
                    :original.top;return 0;
            }
            case 517:return SetWindowPos(c.window,nullptr,c.left,c.top,
                c.width,c.height,SWP_NOZORDER|SWP_NOACTIVATE);
            case 519:c.font=reinterpret_cast<HFONT>(
                SendMessageA(c.window,WM_GETFONT,0,0));return 0;
            case 520:control("BUTTON","Per-ship properties",BS_GROUPBOX,
                -1,0,0,220,250);return 0;
            case 521:label("Captain",8,14,200,10);return 0;
            case 522:edit(kCaptainEdit,8,26,204,static_cast<int>(kTextCapacity-1));
                return 0;
            case 523:control("BUTTON","Use ODF name list",
                WS_TABSTOP|BS_AUTOCHECKBOX,kCaptainAuto,8,42,200,12);return 0;
            case 524:label("Registry",8,58,200,10);return 0;
            case 525:edit(kRegistryEdit,8,70,204,static_cast<int>(kTextCapacity-1));
                return 0;
            case 526:control("BUTTON","Use ODF name list",
                WS_TABSTOP|BS_AUTOCHECKBOX,kRegistryAuto,8,86,200,12);return 0;
            case 527:label("Directional shields",8,106,204,11);return 0;
            case 528:label("Facing",8,122,66,11);return 0;
            case 529:label("Current",78,122,60,11);return 0;
            case 530:label("Maximum",146,122,66,11);return 0;
            case 535:case 536:case 537:case 538:
                c.facing=id-535;c.row_y=136+static_cast<int>(c.facing)*20;
                return 0;
            case 539:label(kFacingNames[c.facing],8,c.row_y+2,66,11);return 0;
            case 540:edit(kShieldEdit+static_cast<int>(c.facing)*2,
                78,c.row_y,60,63);return 0;
            case 541:edit(kShieldEdit+static_cast<int>(c.facing)*2+1,
                146,c.row_y,66,63);return 0;
            case 550:c.state=SavedState{};return 0;
            case 551:c.entry=find_entry(c.craft);return c.entry!=nullptr;
            case 552:c.state=c.entry->state;return 0;
            case 553:return g_read_defaults!=nullptr;
            case 554:g_read_defaults(c.craft,g_dialog.default_captain.data(),
                g_dialog.default_registry.data(),kTextCapacity);return 0;
            case 555:g_dialog.default_captain.back()='\0';
                g_dialog.default_registry.back()='\0';return 0;
            case 556:SetDlgItemTextA(c.window,kCaptainEdit,
                (c.state.flags&kCaptain)?c.state.captain.data()
                :g_dialog.default_captain.data());return 0;
            case 557:SetDlgItemTextA(c.window,kRegistryEdit,
                (c.state.flags&kRegistry)?c.state.registry.data()
                :g_dialog.default_registry.data());return 0;
            case 558:CheckDlgButton(c.window,kCaptainAuto,
                (c.state.flags&kCaptain)?BST_UNCHECKED:BST_CHECKED);return 0;
            case 559:CheckDlgButton(c.window,kRegistryAuto,
                (c.state.flags&kRegistry)?BST_UNCHECKED:BST_CHECKED);return 0;
            case 560:c.shields.struct_size=sizeof(c.shields);return 0;
            case 561:return resolve_shields();
            case 562:return g_shields_enabled(c.craft);
            case 563:c.shields_ok=g_get_shields(c.craft,&c.shields);
                return c.shields_ok;
            case 564:g_dialog.shields=true;return 0;
            case 565:g_dialog.shields=false;return 0;
            case 566:case 567:case 568:case 569:c.facing=id-566;return 0;
            case 570:c.current=GetDlgItem(c.window,
                    kShieldEdit+static_cast<int>(c.facing)*2);
                c.maximum=GetDlgItem(c.window,
                    kShieldEdit+static_cast<int>(c.facing)*2+1);return 0;
            case 571:return g_dialog.shields;
            case 572:set_number(c.current,c.shields.current[c.facing]);return 0;
            case 573:set_number(c.maximum,c.shields.maximum[c.facing]);return 0;
            case 574:c.active_facing=g_dialog.shields&&
                c.shields.maximum[c.facing]>0.0f;return 0;
            case 575:EnableWindow(c.current,c.active_facing);return 0;
            case 576:EnableWindow(c.maximum,c.active_facing);return 0;
            case 577:label(g_dialog.shields
                ?"Current: 0 to maximum. Inactive facings remain 0/0.\r\nNative shield totals follow the active facings."
                :"Requires directionalShields > 0 and the updated\r\nA2FODirectionalShields module.",
                8,218,204,26);return 0;
            case 578:return g_dialog.shields;
            case 579:c.current=GetDlgItem(c.window,kNativeShieldCurrentId);return 0;
            case 580:c.maximum=GetDlgItem(c.window,kNativeShieldMaximumId);return 0;
            case 581:g_dialog.aggregate_read_only[0]=
                (GetWindowLongPtrA(c.current,GWL_STYLE)&ES_READONLY)!=0;return 0;
            case 582:g_dialog.aggregate_read_only[1]=
                (GetWindowLongPtrA(c.maximum,GWL_STYLE)&ES_READONLY)!=0;return 0;
            case 583:return c.current!=nullptr;
            case 584:return c.maximum!=nullptr;
            case 585:return SendMessageA(c.current,EM_SETREADONLY,TRUE,0)!=0;
            case 586:return SendMessageA(c.maximum,EM_SETREADONLY,TRUE,0)!=0;
            case 587:c.created=false;return 0;
            case 588:return c.created;
            case 589:undo_controls();return 0;
            case 590:log_line("Could not create object-editor controls; native dialog retained");
                return 0;
            case 591:return g_dialog.shields;
            case 592:copy_shields(&c.state,c.shields);return 0;
            case 593:mirror_shield_totals(c.state);return 0;
            case 594:g_dialog.changing_text=false;return 0;
            case 595:g_dialog.active=true;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_add_controls(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}
bool accept_controls() noexcept {
    struct Context {char label[46]{};SavedState state{};Entry* entry{};
        A2FO_DirectionalShieldState shields{};std::exception_ptr error{};
    } c;
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 300:GetDlgItemTextA(g_dialog.window,kNativeLabelId,
            c.label,sizeof(c.label));return 0;
        case 301:return c.label[0]!=0;
        case 302:MessageBoxA(g_dialog.window,
            "Enter an object label before applying these properties.\n"
            "The native editor does not commit an object with an empty label.",
            "Object properties",MB_OK|MB_ICONWARNING);return 0;
        case 303:c.state=SavedState{};return 0;
        case 304:c.entry=find_entry(g_dialog.craft);return c.entry!=nullptr;
        case 305:c.state=c.entry->state;return 0;
        case 306:c.state.flags&=~(kCaptain|kRegistry);return 0;
        case 307:c.state.captain.fill('\0');c.state.registry.fill('\0');return 0;
        case 308:return IsDlgButtonChecked(g_dialog.window,kCaptainAuto)!=BST_CHECKED;
        case 309:c.state.flags|=kCaptain;return 0;
        case 310:GetDlgItemTextA(g_dialog.window,kCaptainEdit,c.state.captain.data(),
            static_cast<int>(c.state.captain.size()));return 0;
        case 311:return IsDlgButtonChecked(g_dialog.window,kRegistryAuto)!=BST_CHECKED;
        case 312:c.state.flags|=kRegistry;return 0;
        case 313:GetDlgItemTextA(g_dialog.window,kRegistryEdit,c.state.registry.data(),
            static_cast<int>(c.state.registry.size()));return 0;
        case 314:return g_dialog.shields;
        case 315:return read_shield_controls(&c.state);
        case 316:MessageBoxA(g_dialog.window,
            "Each shield value must be a finite number.\n"
            "Active maxima must be greater than zero; inactive facings use 0/0.\n"
            "Current must be between zero and its maximum, and totals finite.",
            "Directional shields",MB_OK|MB_ICONWARNING);return 0;
        case 317:c.entry=prepare_entry(g_dialog.craft);return c.entry!=nullptr;
        case 318:MessageBoxA(g_dialog.window,
            "Could not allocate the per-ship properties.",
            "Object properties",MB_OK|MB_ICONERROR);return 0;
        case 319:c.shields=shield_snapshot(c.state);return 0;
        case 320:return g_dialog.shields;
        case 321:return g_set_shields!=nullptr;
        case 322:return g_set_shields(g_dialog.craft,&c.shields);
        case 323:MessageBoxA(g_dialog.window,
            "The directional-shield state could not be applied.\n"
            "The dialog has been left open; no identity changes were committed.",
            "Directional shields",MB_OK|MB_ICONERROR);return 0;
        case 324:assign_entry(c.entry,c.state,false);return 0;
        case 325:g_accepted.craft=g_dialog.craft;return 0;
        case 326:g_accepted.shields=c.shields;return 0;
        case 327:g_accepted.has_shields=g_dialog.shields;return 0;
        case 328:return g_dialog.shields;
        case 329:mirror_shield_totals(c.state);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_accept_controls(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

INT_PTR CALLBACK dialog_proc(HWND window, UINT message,
                             WPARAM wparam, LPARAM lparam) noexcept {
    struct Context {HWND window;UINT message;WPARAM wparam;LPARAM lparam;
        DLGPROC original{};INT_PTR result{};int id{},notification{};
        SavedState state{};std::exception_ptr error{};
    } c{window,message,wparam,lparam};
    const auto op=[](void* opaque,std::uint32_t action) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(action) {
        case 340:c.original=reinterpret_cast<DLGPROC>(g_dialog_hook.gateway);return 0;
        case 341:return c.original!=nullptr;
        case 342:return g_ready;
        case 343:return g_legacy_ready;
        case 344:c.result=c.original(c.window,c.message,c.wparam,c.lparam);return 0;
        case 345:return c.message==WM_INITDIALOG;
        case 346:add_controls(c.window);return 0;
        case 347:return g_dialog.window==c.window;
        case 348:return c.message==WM_COMMAND;
        case 349:c.id=LOWORD(c.wparam);return 0;
        case 350:c.notification=HIWORD(c.wparam);return 0;
        case 351:return c.id>=kFirstControl&&c.id<kLastControl;
        case 352:return g_dialog.active;
        case 353:return g_dialog.changing_text;
        case 354:return c.notification==EN_CHANGE;
        case 355:return c.id==kCaptainEdit;
        case 356:return c.id==kRegistryEdit;
        case 357:CheckDlgButton(c.window,c.id==kCaptainEdit
            ?kCaptainAuto:kRegistryAuto,BST_UNCHECKED);return 0;
        case 358:return c.notification==BN_CLICKED;
        case 359:return c.id==kCaptainAuto;
        case 360:return c.id==kRegistryAuto;
        case 361:return IsDlgButtonChecked(c.window,c.id)==BST_CHECKED;
        case 362:g_dialog.changing_text=true;return 0;
        case 363:SetDlgItemTextA(c.window,c.id==kCaptainAuto
            ?kCaptainEdit:kRegistryEdit,c.id==kCaptainAuto
            ?g_dialog.default_captain.data():g_dialog.default_registry.data());return 0;
        case 364:g_dialog.changing_text=false;return 0;
        case 365:return g_dialog.shields;
        case 366:return c.id>=kShieldEdit&&c.id<kShieldEdit+8;
        case 367:c.state=SavedState{};return read_shield_controls(&c.state);
        case 368:mirror_shield_totals(c.state);return 0;
        case 369:return g_dialog.active;
        case 370:return c.id==IDOK;
        case 371:return accept_controls();
        case 372:return c.id==IDCANCEL;
        case 373:g_accepted=AcceptedDraft{};return 0;
        case 374:return g_dialog.window==c.window;
        case 375:return c.message==WM_CLOSE;
        case 376:g_accepted=AcceptedDraft{};return 0;
        case 377:c.result=c.original(c.window,c.message,c.wparam,c.lparam);return 0;
        case 378:return g_dialog.window==c.window;
        case 379:return c.message==WM_NCDESTROY;
        case 380:g_dialog=DialogState{};return 0;
        case 381:c.result=TRUE;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_object_editor_dialog_proc(&c,op);
    if(c.error)std::terminate();
    return c.result;
}

std::uintptr_t __attribute__((fastcall)) dialog_destructor(
    void* self, void*) noexcept {
    struct Context {void* self;std::uintptr_t result{};
        std::exception_ptr error{};} c{self};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 400:return g_destructor_hook.gateway!=nullptr;
        case 401:return g_ready;
        case 402:return g_legacy_ready;
        case 403:return g_accepted.craft!=nullptr;
        case 404:return g_accepted.craft==read_at<void*>(at(kActiveObjectRva),0);
        case 405:return read_at<std::uint8_t>(at(kDialogCancelledRva),0,1)==0;
        case 406:return read_at<char>(at(kDialogLabelRva),0)!='\0';
        case 407:c.result=a2fo_identity_call_thiscall_0(
            g_destructor_hook.gateway,c.self);return 0;
        case 408:return g_accepted.has_shields;
        case 409:return g_set_shields!=nullptr;
        case 410:return g_set_shields(g_accepted.craft,&g_accepted.shields);
        case 411:log_line("Could not reapply facing values after the native object-dialog commit");return 0;
        case 412:g_accepted=AcceptedDraft{};return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    a2fo_rs_stage8_b_object_editor_dialog_destructor(&c,op);
    if(c.error)std::terminate();
    return c.result;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) noexcept {
    return accessible(at(rva), Size) &&
        std::memcmp(at(rva), expected.data(), Size) == 0;
}

bool supported_image() noexcept {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_armada);
    if (!accessible(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000)
        return false;
    const auto* nt = static_cast<const IMAGE_NT_HEADERS32*>(at(dos->e_lfanew));
    return accessible(nt, sizeof(*nt)) &&
        a2fo::supported_armada::identify(g_armada) !=
            a2fo::supported_armada::Identity::unsupported;
}

}  // namespace

const SavedState* overrides(void* craft) noexcept {
    if (!g_ready) return nullptr;
    const Entry* entry = find_entry(craft);
    return entry ? &entry->state : nullptr;
}

std::uint64_t revision(void* craft) noexcept {
    if (!g_ready) return 0;
    const Entry* entry = find_entry(craft);
    return entry ? entry->revision : 0;
}

bool read_values(void* craft, SavedState* state, bool* directional) noexcept {
    struct Context {void* craft;SavedState* state;bool* directional;
        bool shields{};SavedState defaults{};std::exception_ptr error{};
    } c{craft,state,directional};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 180:return g_ready;
        case 181:return c.state!=nullptr;
        case 182:return c.directional!=nullptr;
        case 183:return is_craft(c.craft);
        case 184:return make_save_state(c.craft,c.state);
        case 185:return resolve_shields();
        case 186:c.shields=g_shields_enabled(c.craft);return 0;
        case 187:*c.directional=c.shields;return 0;
        case 188:c.defaults=SavedState{};return 0;
        case 189:return g_read_defaults!=nullptr;
        case 190:g_read_defaults(c.craft,c.defaults.captain.data(),
            c.defaults.registry.data(),kTextCapacity);return 0;
        case 191:c.defaults.captain.back()='\0';
            c.defaults.registry.back()='\0';return 0;
        case 192:return (c.state->flags&kCaptain)!=0;
        case 193:c.state->captain=c.defaults.captain;return 0;
        case 194:return (c.state->flags&kRegistry)!=0;
        case 195:c.state->registry=c.defaults.registry;return 0;
        case 196:return valid_state(*c.state);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_read_values(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

bool apply_values(void* craft, const SavedState& state,
                  bool apply_directional) noexcept {
    struct Context {void* craft;const SavedState& state;bool directional;
        Entry* entry{};A2FO_DirectionalShieldState shields{};
        std::exception_ptr error{};
    } c{craft,state,apply_directional};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 200:return g_ready;
        case 201:return is_craft(c.craft);
        case 202:return valid_state(c.state);
        case 203:c.entry=prepare_entry(c.craft);return c.entry!=nullptr;
        case 204:return c.directional;
        case 205:return (c.state.flags&kShields)!=0;
        case 206:return resolve_shields();
        case 207:c.shields=shield_snapshot(c.state);return 0;
        case 208:return g_set_shields(c.craft,&c.shields);
        case 209:assign_entry(c.entry,c.state,false);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_apply_values(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

bool initialize(const A2FO_ModuleApi* api, HMODULE armada,
                ReadDefaults defaults) noexcept {
    struct Context {const A2FO_ModuleApi* api;HMODULE armada;ReadDefaults defaults;
        bool save_ok{},load_ok{};std::exception_ptr error{};
    } c{api,armada,defaults};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 0:return g_ready;
        case 1:g_api=c.api;return 0;
        case 2:g_armada=c.armada;return 0;
        case 3:g_read_defaults=c.defaults;return 0;
        case 4:return c.api!=nullptr;
        case 5:return c.armada!=nullptr;
        case 6:return c.api->install_inline_hook!=nullptr;
        case 7:return c.api->patch_call!=nullptr;
        case 8:return A2FO_MODULE_API_HAS(c.api,register_craft_event_handler);
        case 9:return c.api->register_craft_event_handler!=nullptr;
        case 10:return (c.api->capabilities&A2FO_CAP_CRAFT_EVENTS)!=0;
        case 11:return supported_image();
        case 12:return signature_matches(kSaveFirstFieldCallRva,kSaveSignature);
        case 13:return signature_matches(kLoadFirstFieldCallRva,kLoadSignature);
        case 14:log_line("Craft persistence signatures or lifecycle API unavailable; extension disabled");return 0;
        case 15:return c.api->register_craft_event_handler(
            "A2FOCraftIdentity.ObjectEditor",&craft_event_handler,nullptr);
        case 16:log_line("Could not register per-ship property cleanup/load handling");return 0;
        case 17:c.save_ok=c.api->patch_call(at(kSaveFirstFieldCallRva),
            reinterpret_cast<void*>(&a2fo_editor_save_prefix_bridge),
            kSaveSignature.data(),kSaveSignature.size());return c.save_ok;
        case 18:c.load_ok=c.api->patch_call(at(kLoadFirstFieldCallRva),
            reinterpret_cast<void*>(&read_first_field),
            kLoadSignature.data(),kLoadSignature.size());return c.load_ok;
        case 19:g_ready=c.save_ok&&c.load_ok;return 0;
        case 20:log_line("Object-editor persistence installation incomplete; hooks remain pass-through");return 0;
        case 21:return signature_matches(kDialogProcRva,kDialogSignature);
        case 22:return signature_matches(kDialogDestructorRva,kDestructorSignature);
        case 23:return c.api->install_inline_hook(at(kDialogDestructorRva),
            reinterpret_cast<void*>(&dialog_destructor),
            kDestructorSignature.size(),kDestructorSignature.data(),&g_destructor_hook);
        case 24:return g_destructor_hook.gateway!=nullptr;
        case 25:return c.api->install_inline_hook(at(kDialogProcRva),
            reinterpret_cast<void*>(&dialog_proc),kDialogSignature.size(),
            kDialogSignature.data(),&g_dialog_hook);
        case 26:return g_dialog_hook.gateway!=nullptr;
        case 27:g_legacy_ready=true;log_line(
            "Legacy Armada object dialog and versioned per-ship persistence installed");return 0;
        case 28:g_legacy_ready=false;log_line(
            "Legacy Armada dialog unavailable; per-ship persistence remains enabled");return 0;
        case 29:fleetops::initialize(c.api,c.api->fleetops_module
            ?static_cast<HMODULE>(c.api->fleetops_module()):nullptr);return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_initialize(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

bool write_first_field(void* writer, float value, const char* label,
                       void* craft) noexcept {
    struct Context {void* writer;float value;const char* label;void* craft;
        OutFloat original{};OutBytes out_bytes{};SavedState state{};
        std::exception_ptr error{};
    } c{writer,value,label,craft};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 100:c.original=reinterpret_cast<OutFloat>(at(kOutFloatRva));return 0;
        case 101:return g_ready;
        case 102:c.state=SavedState{};return 0;
        case 103:return is_craft(c.craft);
        case 104:return make_save_state(c.craft,&c.state);
        case 105:return c.state.flags!=0;
        case 106:c.out_bytes=reinterpret_cast<OutBytes>(at(kOutBytesRva));return 0;
        case 107:return c.out_bytes(c.writer,&c.state,sizeof(c.state),kSaveLabel);
        case 108:return c.original(c.writer,c.value,c.label);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool result=a2fo_rs_stage8_b_object_editor_write_field(&c,op)!=0;
    if(c.error)std::terminate();
    return result;
}

bool take_loaded_shields(void* craft,
                         A2FO_DirectionalShieldState* result) noexcept {
    struct Context {void* craft;A2FO_DirectionalShieldState* result;
        Entry* entry{};std::exception_ptr error{};
    } c{craft,result};
    const auto op=[](void* opaque,std::uint32_t id) noexcept -> std::uint64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(id) {
        case 220:return g_ready;
        case 221:return c.result!=nullptr;
        case 222:return c.result->struct_size>=sizeof(*c.result);
        case 223:c.entry=find_entry(c.craft);return c.entry!=nullptr;
        case 224:return c.entry->pending_shields;
        case 225:return (c.entry->state.flags&kShields)!=0;
        case 226:*c.result=shield_snapshot(c.entry->state);return 0;
        case 227:c.entry->pending_shields=false;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
    };
    const bool taken=a2fo_rs_stage8_b_object_editor_take_loaded(&c,op)!=0;
    if(c.error)std::terminate();
    return taken;
}

}  // namespace a2fo::object_editor

extern "C" bool __cdecl a2fo_editor_write_first_field(
    void* writer, float value, const char* label, void* craft) noexcept {
    return a2fo::object_editor::write_first_field(writer, value, label, craft);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOCraftIdentity_TakeLoadedDirectionalShields(
    void* craft, A2FO_DirectionalShieldState* state) {
    return a2fo::object_editor::take_loaded_shields(craft, state);
}
