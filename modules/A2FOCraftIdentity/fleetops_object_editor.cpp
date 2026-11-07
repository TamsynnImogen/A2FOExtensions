/*
 * Fleet Ops replaces the old Armada object dialog with a Delphi form and
 * TNextInspector. Register real property descriptors under its Craft node.
 * The form's OK handler validates a complete draft before native Apply/Close;
 * Cancel and window close never invoke this handler.
 */
#include "fleetops_object_editor.hpp"
#include "object_editor.hpp"
#include "../../rust/craft_inspector_ffi.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

extern "C" {
std::uintptr_t __cdecl a2fo_identity_call_delphi_2(
    void*, std::uintptr_t, std::uintptr_t);
void* __cdecl a2fo_fo_editor_add_property(
    void* function, void* form, std::uint32_t type, const char* caption,
    void* parent, void* value, std::uint32_t writable);
void a2fo_fo_editor_build_bridge();
void a2fo_fo_editor_ok_bridge();
void a2fo_fo_editor_destroy_bridge();
}

namespace a2fo::object_editor::fleetops {
namespace {

constexpr std::uintptr_t kBuildCraftRva = 0x1b90d8;
constexpr std::uintptr_t kAddPropertyRva = 0x1b8904;
constexpr std::uintptr_t kOkayRva = 0x1baf34;
constexpr std::uintptr_t kDestroyRva = 0x1b88c8;
constexpr std::uintptr_t kApplyRva = 0x1bacc0;
constexpr std::uintptr_t kCloseRva = 0x0b1b08;
constexpr std::uintptr_t kActiveObjectRva = 0x2447f8;
constexpr std::uintptr_t kWideStringClearRva = 0x005f38;

constexpr std::array<std::uint8_t, 5> kBuildSignature{
    0x55, 0x8b, 0xec, 0x33, 0xc9};
constexpr std::array<std::uint8_t, 5> kOkaySignature{
    0x53, 0x8b, 0xd8, 0x8b, 0xc3};
constexpr std::array<std::uint8_t, 5> kDestroySignature{
    0x53, 0x56, 0x57, 0x8b, 0xf8};
constexpr std::array<std::uint8_t, 5> kPropertySignature{
    0x55, 0x8b, 0xec, 0x51, 0x53};
constexpr std::array<std::uint8_t, 6> kApplySignature{
    0x53, 0x56, 0x57, 0x83, 0xc4, 0xd0};
constexpr std::array<std::uint8_t, 5> kOkayApplyCall{
    0xe8, 0x82, 0xfd, 0xff, 0xff};
constexpr std::array<std::uint8_t, 5> kOkayCloseCall{
    0xe8, 0xc3, 0x6b, 0xef, 0xff};

constexpr std::array<const char*, 12> kCaptions{
    "Captain", "Registry", "Reset captain to ODF", "Reset registry to ODF",
    "Forward current", "Forward maximum", "Aft current", "Aft maximum",
    "Port current", "Port maximum", "Starboard current", "Starboard maximum"};

struct Session {
    void* form = nullptr;
    void* craft = nullptr;
    void* object_class = nullptr;
    std::uint32_t handle = 0;
    SavedState initial{};
    std::array<char*, 2> text_pointers{};
    std::array<std::uint8_t, 2> reset{};
    std::array<void*, 12> rows{};
    bool shields = false;
    bool complete = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_module = nullptr;
bool g_ready = false;
A2FO_InlineHook g_build_hook{};
A2FO_InlineHook g_okay_hook{};
A2FO_InlineHook g_destroy_hook{};
Session g_session{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log("A2FOCraftIdentity", message);
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_module) + rva);
}

bool readable(const void* memory, std::size_t size) noexcept {
    auto cursor = reinterpret_cast<std::uintptr_t>(memory);
    if (!cursor || size > std::numeric_limits<std::uintptr_t>::max() - cursor)
        return false;
    const std::uintptr_t end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &info,
                          sizeof(info)) || info.State != MEM_COMMIT ||
            (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
            return false;
        const DWORD protection = info.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE &&
            protection != PAGE_EXECUTE_WRITECOPY)
            return false;
        const auto base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > std::numeric_limits<std::uintptr_t>::max() - base)
            return false;
        const std::uintptr_t next = base + info.RegionSize;
        if (next <= cursor) return false;
        cursor = std::min(end, next);
    }
    return true;
}

template <typename T>
T read_at(const void* memory, std::size_t offset) noexcept {
    T value{};
    if (!memory) return value;
    const auto address = reinterpret_cast<std::uintptr_t>(memory);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address)
        return value;
    const void* source = reinterpret_cast<const void*>(address + offset);
    if (readable(source, sizeof(value))) std::memcpy(&value, source, sizeof(value));
    return value;
}

std::uintptr_t call(void* function, void* self,
                    std::uintptr_t argument = 0) noexcept {
    return a2fo_identity_call_delphi_2(
        function, reinterpret_cast<std::uintptr_t>(self), argument);
}

void* row_method(void* row, std::size_t offset) noexcept {
    return read_at<void*>(read_at<void*>(row, 0), offset);
}

bool read_text(void* row, char* output, std::size_t capacity) noexcept {
    if (!row || !output || !capacity) return false;
    output[0] = '\0';
    void* getter = row_method(row, 0x48);
    if (!readable(getter, 1)) return false;
    // TNxPropertyItem.GetAsString returns a Delphi WideString through EDX.
    // Use Fleet Ops' own clear routine to match its allocator and ownership.
    wchar_t* wide = nullptr;
    call(getter, row, reinterpret_cast<std::uintptr_t>(&wide));
    bool result = true;
    if (wide) {
        const auto address = reinterpret_cast<std::uintptr_t>(wide);
        const std::uint32_t bytes = address >= 4
            ? read_at<std::uint32_t>(reinterpret_cast<void*>(address - 4), 0)
            : 0;
        result = bytes <= 65536 && (bytes % sizeof(wchar_t)) == 0 &&
            readable(wide, static_cast<std::size_t>(bytes) + sizeof(wchar_t));
        if (result) {
            const int characters = static_cast<int>(bytes / sizeof(wchar_t));
            if (characters) {
                const int written = WideCharToMultiByte(CP_ACP, 0, wide,
                    characters, output, static_cast<int>(capacity - 1),
                    nullptr, nullptr);
                result = written > 0;
                if (result) {
                    output[written] = '\0';
                    result = std::memchr(output, '\0', written) == nullptr;
                }
            }
        }
        call(at(kWideStringClearRva), &wide);
    }
    return result;
}

bool checked(void* row, bool* result) noexcept {
    void* getter = row_method(row, 0x38);
    if (!row || !result || !readable(getter, 1)) return false;
    *result = (call(getter, row) & 0xffu) != 0;
    return true;
}

void* add_row(void* form, void* parent, const char* caption,
              std::uint32_t type, void* value) noexcept {
    // Delphi AnsiString literals have {-1, byte_length} before the characters.
    // The native constructor copies this caption into its managed WideString.
    struct Caption {
        std::int32_t references = -1;
        std::int32_t length = 0;
        char text[80]{};
    } label;
    static_assert(offsetof(Caption, text) == 8);
    std::snprintf(label.text, sizeof(label.text), "%s", caption);
    label.length = static_cast<std::int32_t>(std::strlen(label.text));
    void* descriptor = a2fo_fo_editor_add_property(
        at(kAddPropertyRva), form, type, label.text, parent, value, 1);
    return read_at<void*>(descriptor, 8);
}

bool matching_session(void* form) noexcept {
    return g_session.form == form && g_session.craft &&
        g_session.craft == read_at<void*>(at(kActiveObjectRva), 0) &&
        g_session.handle == read_at<std::uint32_t>(g_session.craft, 0x28) &&
        g_session.object_class == read_at<void*>(g_session.craft, 0x40);
}

void warning(const char* text) noexcept {
    // Normal application dialog behavior; no external input automation.
    MessageBoxA(nullptr, text, "Fleet Ops object properties",
                MB_OK | MB_ICONWARNING | MB_TASKMODAL);
}

struct DraftContext {SavedState* state;std::array<std::array<char,64>,4> current{},maximum{};};
std::uintptr_t draft_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<DraftContext*>(value);auto& state=*c.state;
    switch(op) {
    case 0:state=g_session.initial;return 1;
    case 1:return read_text(g_session.rows[i],i?state.registry.data():state.captain.data(),kTextCapacity);
    case 2:{bool reset=false;if(!checked(g_session.rows[2+i],&reset))return 0;return reset?2:1;}
    case 3:return std::strcmp(i?state.registry.data():state.captain.data(),i?g_session.initial.registry.data():g_session.initial.captain.data())!=0;
    case 4:return read_text(g_session.rows[4+i*2],c.current[i].data(),c.current[i].size());
    case 5:return read_text(g_session.rows[5+i*2],c.maximum[i].data(),c.maximum[i].size());
    case 6:return parse_shield_number(c.current[i].data(),&state.current[i]);
    case 7:return parse_shield_number(c.maximum[i].data(),&state.maximum[i]);
    case 8:return valid_state(state);
    case 10:
        if(i==0)warning("Captain and registry must fit within 511 bytes.\nNo properties have been applied.");
        else if(i==1)warning("Each directional shield value must be a finite number.\nNo properties have been applied.");
        else warning("Active shield maxima must be greater than zero; inactive facings use 0/0.\nCurrent must be between zero and that facing's maximum,\nand the combined totals must be finite.\nNo properties have been applied.");
        return 1;
    default:return 0;
    }
}
bool read_draft(SavedState* state) noexcept {
    DraftContext context{state,{},{}};
    return a2fo_rs_inspector_draft(&context,&state->flags,g_session.shields,&draft_step)!=0;
}

template <std::size_t Size>
bool signature(std::uintptr_t rva,
               const std::array<std::uint8_t, Size>& expected) noexcept {
    return readable(at(rva), Size) &&
        std::memcmp(at(rva), expected.data(), Size) == 0;
}

}  // namespace

namespace {
struct EditorContext {void* form;void* argument;void* craft{};SavedState values{};bool shields{};const A2FO_ModuleApi* api{};HMODULE module{};};
std::uintptr_t build_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<EditorContext*>(value);
    switch(op) {
    case 0:return g_build_hook.gateway!=nullptr;
    case 1:call(g_build_hook.gateway,c.form,reinterpret_cast<std::uintptr_t>(c.argument));return 1;
    case 2:return g_ready;
    case 3:g_session=Session{};return 1;
    case 4:c.craft=read_at<void*>(at(kActiveObjectRva),0);return read_values(c.craft,&c.values,&c.shields);
    case 5:
        g_session.form=c.form;g_session.craft=c.craft;g_session.object_class=read_at<void*>(c.craft,0x40);
        g_session.handle=read_at<std::uint32_t>(c.craft,0x28);g_session.initial=c.values;g_session.shields=c.shields;
        g_session.text_pointers={g_session.initial.captain.data(),g_session.initial.registry.data()};return 1;
    case 6: {
        void* field=i<2?static_cast<void*>(&g_session.text_pointers[i]):i<4?static_cast<void*>(&g_session.reset[i-2]):i%2?static_cast<void*>(&g_session.initial.maximum[(i-5)/2]):static_cast<void*>(&g_session.initial.current[(i-4)/2]);
        g_session.rows[i]=add_row(c.form,c.argument,kCaptions[i],i<2?5:i<4?3:1,field);return 1;
    }
    case 7:return c.shields;
    case 8:return g_session.rows[i]!=nullptr;
    case 9:g_session.complete=i!=0;return 1;
    case 10:
        log_line(i==0?"Fleet Ops inspector: could not snapshot this Craft's properties":i==1?"Fleet Ops inspector: added captain, registry and eight facing values under Craft":i==2?"Fleet Ops inspector: added captain and registry; directional shields are not enabled":"Fleet Ops inspector: incomplete rows; OK is blocked to avoid partial edits");return 1;
    default:return 0;
    }
}
std::uintptr_t okay_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<EditorContext*>(value);
    switch(op) {
    case 0:return g_okay_hook.gateway!=nullptr;
    case 1:return g_ready;
    case 2:return matching_session(c.form);
    case 3:call(g_okay_hook.gateway,c.form,reinterpret_cast<std::uintptr_t>(c.argument));return 1;
    case 4:return g_session.complete;
    case 5:return read_draft(&c.values);
    case 6:c.craft=g_session.craft;c.shields=g_session.shields;return apply_values(c.craft,c.values,c.shields);
    case 7:call(at(kApplyRva),c.form);return 1;
    case 8:return c.shields;
    case 9:return apply_values(c.craft,c.values,true);
    case 10:
        warning(i==0?"The extension properties could not be created completely.\nPlease cancel and reopen this object inspector.":i==1?"The per-ship properties could not be applied.\nThe inspector has been left open.":"The native properties were applied, but restoring the directional\nshield totals failed. Please cancel and reopen the inspector.");return 1;
    case 11:call(at(kCloseRva),c.form);return 1;
    case 12:log_line(i?"Fleet Ops inspector: committed per-ship properties":"Fleet Ops inspector: failed to restore facing totals after native Apply");return 1;
    default:return 0;
    }
}
std::uintptr_t destroy_step(void* value,std::uint32_t op,std::uint32_t) noexcept {
    auto& c=*static_cast<EditorContext*>(value);
    switch(op) {
    case 0:return g_destroy_hook.gateway!=nullptr;
    case 1:call(g_destroy_hook.gateway,c.form,reinterpret_cast<std::uintptr_t>(c.argument));return 1;
    case 2:return g_session.form==c.form;
    case 3:g_session=Session{};return 1;
    default:return 0;
    }
}
std::uintptr_t init_step(void* value,std::uint32_t op,std::uint32_t i) noexcept {
    auto& c=*static_cast<EditorContext*>(value);
    switch(op) {
    case 0:return g_ready;
    case 1:g_api=c.api;g_module=c.module;return 1;
    case 2:return c.api && c.module && c.api->install_inline_hook;
    case 3:
        switch(i) {
        case 0:return signature(kBuildCraftRva,kBuildSignature);case 1:return signature(kOkayRva,kOkaySignature);
        case 2:return signature(kDestroyRva,kDestroySignature);case 3:return signature(kAddPropertyRva,kPropertySignature);
        case 4:return signature(kApplyRva,kApplySignature);case 5:return signature(kOkayRva+5,kOkayApplyCall);
        case 6:return signature(kOkayRva+12,kOkayCloseCall);default:return 0;
        }
    case 4:
        if(i==0)return c.api->install_inline_hook(at(kBuildCraftRva),reinterpret_cast<void*>(&a2fo_fo_editor_build_bridge),kBuildSignature.size(),kBuildSignature.data(),&g_build_hook);
        if(i==1)return c.api->install_inline_hook(at(kOkayRva),reinterpret_cast<void*>(&a2fo_fo_editor_ok_bridge),kOkaySignature.size(),kOkaySignature.data(),&g_okay_hook);
        return c.api->install_inline_hook(at(kDestroyRva),reinterpret_cast<void*>(&a2fo_fo_editor_destroy_bridge),kDestroySignature.size(),kDestroySignature.data(),&g_destroy_hook);
    case 5:return (i==0?g_build_hook.gateway:i==1?g_okay_hook.gateway:g_destroy_hook.gateway)!=nullptr;
    case 6:g_ready=i!=0;return 1;
    case 7:log_line(i?"Fleet Ops advanced object inspector hooks installed (Craft rows / OK / destroy)":"Fleet Ops object inspector installation incomplete; hooks remain pass-through");return 1;
    case 8:log_line("Fleet Ops object inspector signatures are unsupported; property-grid extension disabled");return 1;
    default:return 0;
    }
}
}
void build(void* form,void* parent) noexcept {EditorContext c{form,parent};a2fo_rs_inspector_build(&c,&build_step);}
void okay(void* form,void* sender) noexcept {EditorContext c{form,sender};a2fo_rs_inspector_okay(&c,&okay_step);}
void destroy(void* form,void* sender) noexcept {EditorContext c{form,sender};a2fo_rs_inspector_destroy(&c,&destroy_step);}
bool initialize(const A2FO_ModuleApi* api,HMODULE module) noexcept {EditorContext c{nullptr,nullptr,nullptr,{},false,api,module};return a2fo_rs_inspector_init(&c,&init_step)!=0;}

}  // namespace a2fo::object_editor::fleetops

extern "C" void __cdecl a2fo_fo_editor_build_cpp(void* form, void* parent) noexcept {
    a2fo::object_editor::fleetops::build(form, parent);
}
extern "C" void __cdecl a2fo_fo_editor_ok_cpp(void* form, void* sender) noexcept {
    a2fo::object_editor::fleetops::okay(form, sender);
}
extern "C" void __cdecl a2fo_fo_editor_destroy_cpp(void* form, void* sender) noexcept {
    a2fo::object_editor::fleetops::destroy(form, sender);
}
