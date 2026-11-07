#include "../../sdk/include/a2fo_module_provider.hpp"
// Native shell asset skin: original loading, labels, masks and actions survive.
#include "skin_native.hpp"
#include "../../rust/refit_ui_runtime_ffi.h"
#include <exception>
#include "skin_bindings.hpp"
#include "skin.hpp"
#include "menu_scene.hpp"
#include "mission_backdrop.hpp"
#include "scene_composition.hpp"
#include "setup_native.hpp"
#include "../../sdk/include/a2fo_shell_canvas.h"
#include <atomic>
#include <array>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>

extern "C" void a2fo_refit_bitmap_bridge();
extern "C" void a2fo_refit_bitmap32_bridge();
extern "C" void a2fo_refit_image_paint_bridge();
extern "C" void* a2fo_refit_menu_call(void*, void*, void*);
extern "C" void __fastcall a2fo_refit_shell_draw0(void*, void*, HDC);
extern "C" void __fastcall a2fo_refit_shell_draw1(void*, void*, HDC);
extern "C" void __fastcall a2fo_refit_shell_draw2(void*, void*, HDC);

namespace a2fo::refit_menu::skin_native {
namespace {
namespace n = native;
HMODULE image{};
const A2FO_ModuleApi* api{};
std::atomic<bool> enabled{false};
std::atomic<void*> next_bitmap{nullptr}, next_bitmap32{nullptr};
std::atomic<void*> next_image_paint{nullptr};
A2FO_InlineHook bitmap_hook{}, bitmap32_hook{}, image_paint_hook{};
std::array<A2FO_InlineHook, 3> shell_draw_hooks{};
std::atomic<void*> next_shell_draw[3]{};
scene::Backgrounds backgrounds;
std::mutex background_mutex;
std::atomic<unsigned> diagnostics{0};
void* at(std::uint32_t rva) { return reinterpret_cast<BYTE*>(image) + rva; }
bool read(const void* source, void* out, std::size_t size) {
    SIZE_T count{};
    return source && ReadProcessMemory(GetCurrentProcess(), source, out, size, &count) && count == size;
}
template<class T> bool field(const void* object, std::size_t offset, T& value) {
    const auto base = reinterpret_cast<std::uintptr_t>(object);
    return base && offset <= UINTPTR_MAX - base && read(reinterpret_cast<void*>(base + offset), &value, sizeof(value));
}
bool dimensions(int width,int height) {return a2fo_rs_skin_dimensions(width,height)!=0;}

bool writable_pixels(const void* bits, std::size_t size) {
    auto cursor = reinterpret_cast<std::uintptr_t>(bits);
    if (!cursor || size > UINTPTR_MAX - cursor) return false;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const auto protection = info.Protect & 0xff;
        if (protection != PAGE_READWRITE && protection != PAGE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}
bool bitmap_class(void* object) {
    void* vmt{};
    return field(object, 0, vmt) && vmt == at(n::bitmap_vmt_rva);
}
bool bitmap32_class(void* object) {
    std::uintptr_t vmt{}, name{};
    BYTE length{};
    char text[9]{};
    return field(object, 0, vmt) && vmt >= reinterpret_cast<std::uintptr_t>(image) + 44 &&
        vmt - reinterpret_cast<std::uintptr_t>(image) < 0x300000 &&
        read(reinterpret_cast<void*>(vmt - 44), &name, sizeof(name)) &&
        field(reinterpret_cast<void*>(name), 0, length) && length == 9 &&
        read(reinterpret_cast<void*>(name + 1), text, 9) && std::memcmp(text, "TBitmap32", 9) == 0;
}
std::string filename(void* ansi) {
    const auto address = reinterpret_cast<std::uintptr_t>(ansi);
    std::int32_t length{};
    if (address < 4 || !read(reinterpret_cast<void*>(address - 4), &length, 4) || length <= 0 || length > 1024) return {};
    std::string text(static_cast<std::size_t>(length), '\0');
    if (!read(ansi, text.data(), text.size()) || text.find('\0') != std::string::npos) return {};
    return text;
}
struct Dib {
    HDC dc{}; HBITMAP bitmap{}; HGDIOBJ previous{};
    std::uint32_t* bits{};
    ~Dib() {
        if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
    }
    bool create(HDC source, int width, int height) {
        dc = CreateCompatibleDC(source);
        if (!dc) return false;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* pixels{};
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap || !pixels) return false;
        previous = SelectObject(dc, bitmap); bits = static_cast<std::uint32_t*>(pixels);
        return previous && previous != HGDI_ERROR && BitBlt(dc, 0, 0, width, height, source, 0, 0, SRCCOPY) && GdiFlush();
    }
};
bool bitmap_source(void* bitmap, int& width, int& height, HDC& dc) {
    if (!bitmap_class(bitmap)) return false;
    void* data{};
    if (!field(bitmap, 0x28, data) || !data) return false;
    width = static_cast<int>(reinterpret_cast<std::intptr_t>(a2fo_refit_menu_call(at(0x94000), bitmap, nullptr)));
    height = static_cast<int>(reinterpret_cast<std::intptr_t>(a2fo_refit_menu_call(at(0x93e78), bitmap, nullptr)));
    if (!dimensions(width, height)) return false;
    void* canvas = a2fo_refit_menu_call(at(0x93de0), bitmap, nullptr);
    if (!canvas) return false;
    dc = static_cast<HDC>(a2fo_refit_menu_call(at(0x8f538), canvas, nullptr));
    return dc != nullptr;
}
bool style_bitmap(void* bitmap, skin::Style style) {
    int width{}, height{}; HDC source{};
    if (!bitmap_source(bitmap, width, height, source)) return false;
    // Keep native pixel format, palette and key semantics. Indexed assets
    // cannot represent the new palette faithfully, so leave those native.
    BITMAP description{};
    const auto handle = GetCurrentObject(source, OBJ_BITMAP);
    if (!handle || GetObjectA(handle, sizeof(description), &description) != sizeof(description) ||
        (description.bmBitsPixel != 24 && description.bmBitsPixel != 32)) return false;
    Dib copy;
    if (!copy.create(source, width, height)) return false;
    // GDI BI_RGB leaves the fourth byte undefined/zero; it is not PNG alpha.
    // This scratch image is opaque. GR32's real alpha is preserved separately.
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * height; ++i) copy.bits[i] |= 0xff000000u;
    if (!skin::apply(copy.bits, width, width, height, style)) return false;
    return BitBlt(source, 0, 0, width, height, copy.dc, 0, 0, SRCCOPY) != 0;
}
bool style_bitmap32(void* bitmap, skin::Style style) {
    if (!bitmap32_class(bitmap)) return false;
    int width{}, height{}; void* bits{};
    if (!field(bitmap, n::bitmap32_width, width) || !field(bitmap, n::bitmap32_height, height) ||
        !field(bitmap, n::bitmap32_bits, bits) || !dimensions(width, height) || !bits) return false;
    const auto count = static_cast<std::size_t>(width) * height;
    if (!writable_pixels(bits, count * 4)) return false;
    std::vector<std::uint32_t> copy(count);
    if (!read(bits, copy.data(), count * 4) || !skin::apply(copy.data(), width, width, height, style)) return false;
    SIZE_T written{};
    return WriteProcessMemory(GetCurrentProcess(), bits, copy.data(), count * 4, &written) && written == count * 4;
}
Artwork snapshot(void* bitmap, bool gr32) {
    Artwork result;
    if (gr32) {
        void* bits{};
        if (!bitmap32_class(bitmap) || !field(bitmap, n::bitmap32_width, result.width) ||
            !field(bitmap, n::bitmap32_height, result.height) || !field(bitmap, n::bitmap32_bits, bits) ||
            !dimensions(result.width, result.height)) return {};
        result.pixels.resize(static_cast<std::size_t>(result.width) * result.height);
        if (!read(bits, result.pixels.data(), result.pixels.size() * 4)) return {};
    } else {
        HDC source{};
        if (!bitmap_source(bitmap, result.width, result.height, source)) return {};
        Dib copy;
        if (!copy.create(source, result.width, result.height)) return {};
        result.pixels.assign(copy.bits, copy.bits + static_cast<std::size_t>(result.width) * result.height);
    }
    return result;
}
bool shell_window(HWND window) {
    char name[96]{};
    if (!window || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId() ||
        !GetClassNameA(window, name, sizeof(name))) return false;
    // Exact shell classes from the admitted FO image's embedded DFMs. Do not
    // replace arbitrary TImage controls, mission views or preview pictures.
    return a2fo_rs_skin_shell_class(reinterpret_cast<const std::uint8_t*>(name),std::strlen(name))!=0;
}
bool legacy_window(HWND window) {
    char name[32]{};
    if (!window || GetWindowThreadProcessId(window, nullptr) != GetCurrentThreadId() ||
        !GetClassNameA(window, name, sizeof(name)) || std::strcmp(name, "#32770")) return false;
    const auto callback = GetWindowLongPtrA(window, DWLP_DLGPROC);
    HMODULE owner{};
    if (!callback || !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<const char*>(callback), &owner)) return false;
    // The accepted ShellDisplay bridge wraps verified Armada/FO dialog procs.
    // It must remain authoritative for logical coordinates and real input.
    if (owner == image || owner == static_cast<HMODULE>(api->armada_module())) return true;
    const auto shell = a2fo::startup_module_provider("A2FOShellDisplay.dll",
        {"A2FO_ShellCanvasWindowV1", "A2FO_ShellDialogBridgeReadyV1"});
    if (!shell || owner != shell) return false;
    if (shell != GetModuleHandleA("A2FOExtensions.dll")) return true;
    // A consolidated image contains many unrelated callbacks. Admit only the
    // exact active ShellDisplay dialog bridge, not every address in the core.
    const auto address = GetProcAddress(shell, "A2FO_ShellDialogBridgeReadyV1");
    A2FO_ShellDialogBridgeReadyV1Fn bridge = nullptr;
    static_assert(sizeof(bridge) == sizeof(address));
    std::memcpy(&bridge, &address, sizeof(bridge));
    return bridge && bridge(window) != FALSE;
}
HWND control_window(void* control) {
    // ShellDisplay may paint to a logical memory DC, for which WindowFromDC
    // cannot recover a window. Follow the same parent chain as GetParentForm.
    void* parent = control;
    for (unsigned depth = 0; depth < 12; ++depth) {
        if (!field(parent, 0x30, parent) || !parent) return nullptr;
        HWND window{};
        if (field(parent, n::handle_offset, window) && shell_window(window)) return window;
    }
    return nullptr;
}
HWND canvas_window(HDC dc) {
    using Resolve=A2FO_ShellCanvasWindowV1Fn;
    const auto shell=a2fo::startup_module_provider("A2FOShellDisplay.dll", {"A2FO_ShellCanvasWindowV1", "A2FO_ShellDialogBridgeReadyV1"});
    const auto address=shell?GetProcAddress(shell,"A2FO_ShellCanvasWindowV1"):nullptr;
    Resolve resolve{};
    static_assert(sizeof(address)==sizeof(resolve));std::memcpy(&resolve,&address,sizeof(resolve));
    return resolve?resolve(dc):nullptr;
}
template<class Context,class Function>
void ui_result(Context& c,A2foRsUiResult* out,Function action) noexcept {
    out->value=0;out->failed=0;
    try{out->value=action();}catch(...){c.error=std::current_exception();out->failed=1;}
}
struct ComposeContext {
    void* bitmap;HWND window;Artwork* composed;const Artwork** source;
    Artwork current;std::string key;const Artwork* live{};std::exception_ptr error;
};
void compose_step(void* value,std::uint32_t op,std::uint32_t,A2foRsUiResult* out) noexcept {
    auto& c=*static_cast<ComposeContext*>(value);
    ui_result(c,out,[&]()->std::uint64_t{
        switch(op){
        case 0:c.current=snapshot(c.bitmap,false);return !c.current.pixels.empty();
        case 1:{const std::lock_guard<std::mutex> lock(background_mutex);c.key=backgrounds.key(c.current);return !c.key.empty();}
        case 2:return setup_native::asset(c.key);
        case 3:setup_native::observe(c.window,c.current);return 1;
        case 4:return menu_scene::requested();case 5:return mission_backdrop::active();
        case 6:c.live=menu_scene::frame(c.window,true,false,setup_native::asset(c.key));return c.live!=nullptr;
        case 7:{const std::lock_guard<std::mutex> lock(background_mutex);return backgrounds.compose(c.current,*c.live,*c.composed);}
        case 8:*c.source=c.live;return 1;
        default:return 0;
        }
    });
}
bool compose_scene(void* bitmap,HWND window,Artwork& composed,const Artwork*& source) {
    source=nullptr;ComposeContext c{bitmap,window,&composed,&source,{},{},nullptr,{}};
    const bool okay=a2fo_rs_skin_compose(&c,&compose_step)!=0;
    if(c.error){std::rethrow_exception(c.error);}
    return okay;
}

bool draw_pixels(HDC dc, int x, int y, int width, int height, const Artwork& composed) {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = composed.width; info.bmiHeader.biHeight = -composed.height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    const int saved = SaveDC(dc);
    if (!saved) return false;
    SetStretchBltMode(dc, COLORONCOLOR);
    const int lines = StretchDIBits(dc, x, y, width, height, 0, 0, composed.width, composed.height,
        composed.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    RestoreDC(dc, saved);
    return lines != 0 && lines != static_cast<int>(GDI_ERROR);
}
struct PaintContext {
    unsigned kind,index;void* object;HDC dc{};HWND window{};void* bitmap{};
    int x{},y{},width{},height{};RECT dest{};Artwork composed;const Artwork* source{};std::exception_ptr error;
};
void paint_step(void* value,std::uint32_t op,std::uint32_t,A2foRsUiResult* out) noexcept {
    auto& c=*static_cast<PaintContext*>(value);
    ui_result(c,out,[&]()->std::uint64_t{
        switch(op){
        case 0:return enabled.load(std::memory_order_acquire);
        case 1:
            if(c.kind==0){
                void* component_name{};if(!field(c.object,8,component_name))return 0;
                const auto name=filename(component_name);
                if(!a2fo_rs_skin_background_name(reinterpret_cast<const std::uint8_t*>(name.data()),name.size()))return 0;
                void* picture{};void* canvas{};
                if(!field(c.object,0x190,canvas)||!canvas||!field(c.object,n::image_picture,picture)||!field(picture,n::picture_graphic,c.bitmap)||!field(c.object,0x48,c.width)||!field(c.object,0x4c,c.height)||!dimensions(c.width,c.height))return 0;
                c.dc=static_cast<HDC>(a2fo_refit_menu_call(at(0x8f538),canvas,nullptr));return 1;
            }else{
                BYTE loaded{};
                if(!c.dc||!field(c.object,0x2c,loaded)||!loaded||!field(c.object,8,c.bitmap)||!field(c.object,0x14,c.x)||!field(c.object,0x18,c.y))return 0;
                c.x=a2fo_rs_skin_legacy_coordinate(c.index,c.x);c.y=a2fo_rs_skin_legacy_coordinate(c.index,c.y);return 1;
            }
        case 2:c.window=c.dc?canvas_window(c.dc):nullptr;return reinterpret_cast<std::uintptr_t>(c.window);
        case 3:if(c.kind==0)return reinterpret_cast<std::uintptr_t>(c.dc);c.window=WindowFromDC(c.dc);return reinterpret_cast<std::uintptr_t>(c.window);
        case 4:if(c.kind==0){c.window=control_window(c.object);return reinterpret_cast<std::uintptr_t>(c.window);}return legacy_window(c.window);
        case 5:return compose_scene(c.bitmap,c.window,c.composed,c.source);
        case 6:if(c.kind==0)a2fo_refit_menu_call(at(0xa420c),c.object,&c.dest);else c.window=GetActiveWindow();return 1;
        case 7:return static_cast<std::uint64_t>(static_cast<std::int64_t>(c.dest.right)-c.dest.left);
        case 8:return static_cast<std::uint64_t>(static_cast<std::int64_t>(c.dest.bottom)-c.dest.top);
        case 9:return c.kind==0?draw_pixels(c.dc,c.dest.left,c.dest.top,c.dest.right-c.dest.left,c.dest.bottom-c.dest.top,c.composed):draw_pixels(c.dc,c.x,c.y,c.composed.width,c.composed.height,c.composed);
        case 10:menu_scene::frame_drawn(c.source,c.dc);return 1;
        default:return 0;
        }
    });
}
bool paint_scene(void* control) {
    PaintContext c{0,0,control,nullptr,nullptr,nullptr,0,0,0,0,{}, {},nullptr,{}};
    const bool drawn=a2fo_rs_skin_paint(0,&c,&paint_step)!=0;if(c.error){std::rethrow_exception(c.error);}
    return drawn;
}
bool paint_legacy(unsigned index,void* object,HDC dc) {
    PaintContext c{1,index,object,dc,nullptr,nullptr,0,0,0,0,{}, {},nullptr,{}};
    const bool drawn=a2fo_rs_skin_paint(1,&c,&paint_step)!=0;if(c.error){std::rethrow_exception(c.error);}
    return drawn;
}

}
namespace {
struct SkinInitContext {const A2FO_ModuleApi* supplied;HMODULE fo;std::exception_ptr error;};
void init_step(void* value,std::uint32_t op,std::uint32_t i,A2foRsUiResult* out) noexcept {
    auto& c=*static_cast<SkinInitContext*>(value);
    ui_result(c,out,[&]()->std::uint64_t{
        const auto sig_index=i<2?i:i==2?11u:13u+i-3;
        A2FO_InlineHook* hooks[]{&bitmap_hook,&bitmap32_hook,&image_paint_hook,&shell_draw_hooks[0],&shell_draw_hooks[1],&shell_draw_hooks[2]};
        std::atomic<void*>* gateways[]{&next_bitmap,&next_bitmap32,&next_image_paint,&next_shell_draw[0],&next_shell_draw[1],&next_shell_draw[2]};
        const void* callbacks[]{reinterpret_cast<void*>(&a2fo_refit_bitmap_bridge),reinterpret_cast<void*>(&a2fo_refit_bitmap32_bridge),reinterpret_cast<void*>(&a2fo_refit_image_paint_bridge),reinterpret_cast<void*>(&a2fo_refit_shell_draw0),reinterpret_cast<void*>(&a2fo_refit_shell_draw1),reinterpret_cast<void*>(&a2fo_refit_shell_draw2)};
        switch(op){
        case 0:{const auto& sig=n::skin_signatures[i];std::array<BYTE,64> bytes{};return sig.size<=bytes.size()&&read(reinterpret_cast<BYTE*>(c.fo)+sig.rva,bytes.data(),sig.size)&&std::memcmp(bytes.data(),sig.bytes,sig.size)==0;}
        case 1:image=c.fo;api=c.supplied;return 1;
        case 2:{const auto& sig=n::skin_signatures[sig_index];return api->install_inline_hook(at(sig.rva),const_cast<void*>(callbacks[i]),sig.size,reinterpret_cast<const BYTE*>(sig.bytes),hooks[i]);}
        case 3:gateways[i]->store(hooks[i]->gateway,std::memory_order_release);return 1;
        case 4:enabled.store(true,std::memory_order_release);return 1;
        default:return 0;
        }
    });
}
}
bool preflight(HMODULE fo) {SkinInitContext c{nullptr,fo,{}};const bool okay=a2fo_rs_skin_preflight(static_cast<std::uint32_t>(std::size(n::skin_signatures)),&c,&init_step)!=0;if(c.error){std::rethrow_exception(c.error);}
    return okay;}
bool initialize(const A2FO_ModuleApi* supplied,HMODULE fo) {SkinInitContext c{supplied,fo,{}};const bool okay=a2fo_rs_skin_init(static_cast<std::uint32_t>(std::size(n::skin_signatures)),&c,&init_step)!=0;if(c.error){std::rethrow_exception(c.error);}
    return okay;}

void shutdown() { enabled.store(false, std::memory_order_release); }
Artwork main_artwork(void* form) {
    Artwork result;
    if (!image) return result;
    void* control{}; void* picture{}; void* bitmap{};
    if (!field(form, n::background_field, control) || !field(control, n::image_picture, picture) ||
        !field(picture, n::picture_graphic, bitmap)) return result;
    HDC source{};
    if (!bitmap_source(bitmap, result.width, result.height, source)) return {};
    Dib copy;
    if (!copy.create(source, result.width, result.height)) return {};
    result.pixels.assign(copy.bits, copy.bits + static_cast<std::size_t>(result.width) * result.height);
    return result;
}
namespace {
struct SkinLoadContext {void* bitmap;void* name;bool gr32;skin::Style style{};std::string path;void* original{};std::uintptr_t result{};Artwork base;std::exception_ptr error;};
void load_step(void* value,std::uint32_t op,std::uint32_t,A2foRsUiResult* out) noexcept {
    auto& c=*static_cast<SkinLoadContext*>(value);
    ui_result(c,out,[&]()->std::uint64_t{
        switch(op){
        case 0:return enabled.load(std::memory_order_acquire);
        case 1:c.path=filename(c.name);c.style=skin::classify(c.path);return 1;
        case 2:return c.style.role==skin::Role::background;
        case 3:return mission_backdrop::active();
        case 4:mission_backdrop::frame();return 1;
        case 10:{auto& gateway=c.gr32?next_bitmap32:next_bitmap;while(!(c.original=gateway.load(std::memory_order_acquire)))Sleep(0);return 1;}
        case 11:c.result=reinterpret_cast<std::uintptr_t>(a2fo_refit_menu_call(c.original,c.bitmap,c.name));return 1;
        case 12:return c.style.role!=skin::Role::none;
        case 13:return c.gr32?style_bitmap32(c.bitmap,c.style):style_bitmap(c.bitmap,c.style);
        case 14:c.base=snapshot(c.bitmap,c.gr32);return !c.base.pixels.empty();
        case 15:{const std::lock_guard<std::mutex> lock(background_mutex);backgrounds.remember(c.path,c.base);return 1;}
        case 16:return diagnostics.fetch_add(1,std::memory_order_relaxed);
        case 17:{const auto message=std::string("Refit shell artwork/widget styled: ")+c.path;api->log("A2FORefitMenus",message.c_str());return 1;}
        case 18:api->log("A2FORefitMenus","Shell asset styling unavailable; native asset retained");return 1;
        case 19:c.error=nullptr;return 1;
        default:return 0;
        }
    });
}
}
std::uintptr_t load(void* bitmap,void* name,bool gr32) {
    SkinLoadContext c{bitmap,name,gr32,{},{},nullptr,0,{}, {}};
    const auto propagate=a2fo_rs_skin_load(&c,&load_step);if(propagate&&c.error)std::rethrow_exception(c.error);return c.result;
}

void image_paint(void* control) {
    try { if (paint_scene(control)) return; }
    catch (...) {} // A scene error always falls back to original native painting.
    void* original{};
    while (!(original = next_image_paint.load(std::memory_order_acquire))) Sleep(0);
    a2fo_refit_menu_call(original, control, nullptr);
}
void shell_draw(unsigned index, void* object, HDC dc) {
    try { if (paint_legacy(index, object, dc)) return; } catch (...) {}
    void* gateway{};
    while (!(gateway = next_shell_draw[index].load(std::memory_order_acquire))) Sleep(0);
    // Fastcall's explicit unused EDX parameter describes the same ECX+stack
    // contract without GCC's non-member-thiscall warning under -Wpedantic.
    using Original = void (__fastcall*)(void*, void*, HDC);
    Original original{};
    static_assert(sizeof(original) == sizeof(gateway));
    std::memcpy(&original, &gateway, sizeof(original));
    original(object, nullptr, dc);
}
}
extern "C" __attribute__((force_align_arg_pointer)) std::uintptr_t a2fo_refit_bitmap_load(void* bitmap, void* name) {
    return a2fo::refit_menu::skin_native::load(bitmap, name, false);
}
extern "C" __attribute__((force_align_arg_pointer)) std::uintptr_t a2fo_refit_bitmap32_load(void* bitmap, void* name) {
    return a2fo::refit_menu::skin_native::load(bitmap, name, true);
}
extern "C" __attribute__((force_align_arg_pointer)) void a2fo_refit_image_paint(void* control) {
    a2fo::refit_menu::skin_native::image_paint(control);
}
// Borland C++ thiscall: ECX=self, HDC on stack, callee removes four bytes.
// A fastcall wrapper consumes the unused EDX slot without changing that ABI.
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_shell_draw0(void* self, void*, HDC dc) {
    a2fo::refit_menu::skin_native::shell_draw(0, self, dc);
}
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_shell_draw1(void* self, void*, HDC dc) {
    a2fo::refit_menu::skin_native::shell_draw(1, self, dc);
}
extern "C" __attribute__((force_align_arg_pointer)) void __fastcall a2fo_refit_shell_draw2(void* self, void*, HDC dc) {
    a2fo::refit_menu::skin_native::shell_draw(2, self, dc);
}
