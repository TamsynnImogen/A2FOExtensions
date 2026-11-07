// Native intro frame hook tests use a memory DIB and synthetic game images.
// No HWND, original game method, or physical display is accessed.
#include "../modules/A2FOShellDisplay/native_intro.hpp"
#include <windows.h>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
BOOL WINAPI host_GetClientRect(HWND, LPRECT);
HDC WINAPI host_GetDC(HWND);
int WINAPI host_ReleaseDC(HWND, HDC);
HMODULE WINAPI host_GetModuleHandleA(LPCSTR);
FARPROC WINAPI host_GetProcAddress(HMODULE, LPCSTR);
BOOL WINAPI host_PatBlt(HDC, int, int, int, int, DWORD);
int WINAPI host_StretchDIBits(HDC, int, int, int, int, int, int, int, int,
                             const VOID*, const BITMAPINFO*, UINT, DWORD);
BOOL WINAPI host_BitBlt(HDC, int, int, int, int, HDC, int, int, DWORD);
BOOL WINAPI host_GdiFlush(void);
auto g_real_pat_blt = &::PatBlt;
auto g_real_stretch_dibits = &::StretchDIBits;
auto g_real_bit_blt = &::BitBlt;
auto g_real_gdi_flush = &::GdiFlush;
}
#undef GetClientRect
#undef GetDC
#undef ReleaseDC
#undef GetModuleHandleA
#undef GetProcAddress
#undef PatBlt
#undef StretchDIBits
#undef BitBlt
#undef GdiFlush
#define GetClientRect host_GetClientRect
#define GetDC host_GetDC
#define ReleaseDC host_ReleaseDC
#define GetModuleHandleA host_GetModuleHandleA
#define GetProcAddress host_GetProcAddress
#define PatBlt host_PatBlt
#define StretchDIBits host_StretchDIBits
#define BitBlt host_BitBlt
#define GdiFlush host_GdiFlush
#include "../modules/A2FOShellDisplay/native_intro.cpp"

namespace native = a2fo::shell_display::native_intro;
namespace {
constexpr std::uintptr_t kNextFrameRva = 0x637f0;
constexpr std::uintptr_t kCopyCallRva = 0x638af;
constexpr std::uintptr_t kNextFrameSlot = 0x210874;
constexpr std::uintptr_t kEngineSlot = 0x3ad508;
constexpr std::uint8_t kNextBytes[] = {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28};
constexpr std::uint8_t kCopyBytes[] = {0xff, 0x15, 0xa0, 0x82, 0x7b, 0x00};
constexpr unsigned kFormat32 = 3;
constexpr unsigned kFormat16 = 23;
constexpr unsigned kSourceWidth = 640, kSourceHeight = 480;
constexpr int kPaddedPitch = 1280 * 4;

HMODULE g_armada{}, g_fleetops{}, g_bink{};
void* g_next_hook{};
void* g_copy_hook{};
HDC g_dib_dc{};
HWND g_window = reinterpret_cast<HWND>(0x1234);
int g_client_width = 1920, g_client_height = 1080;
bool g_dc_available = true;
bool g_fail_final_blt{};
bool g_fail_copy_flush{}, g_fail_present_flush{};
unsigned g_dc_gets{}, g_dc_releases{};
unsigned g_patblt_calls{}, g_stretch_calls{}, g_final_blits{};
HDC g_frame_composition_dc{}, g_last_frame_composition_dc{};
unsigned g_next_calls{}, g_copy_calls{}, g_flip_calls{};
unsigned g_patch_call_calls{}, g_patch_bytes_calls{};
bool g_patch_call_succeeds = true, g_patch_bytes_succeeds = true;
bool g_next_show{}, g_next_decode{};
int g_next_return = 37, g_copy_return{};
int g_last_copy_result{};
int g_repeat_value{};
void* g_seen_bink{};
bool g_seen_next_decode{}, g_seen_copy{};
unsigned g_copy_pitch{}, g_copy_height{}, g_copy_format{};
int g_frame_pitch = kPaddedPitch;
unsigned g_frame_height = kSourceHeight;
void* g_frame_pixels{};
std::vector<std::uint8_t> g_pixels;
bool g_fake_next_active{};

void* at(HMODULE image, std::uintptr_t offset) {
    return reinterpret_cast<std::uint8_t*>(image) + offset;
}

int WINAPI fake_copy(void* bink, void* buffer, int pitch, unsigned height,
                     unsigned x, unsigned y, unsigned flags) {
    ++g_copy_calls; std::printf("copy %d %u %u %u %u %u\n",pitch,height,x,y,flags,g_copy_calls);
    g_seen_bink = bink;
    g_copy_pitch = static_cast<unsigned>(pitch);
    g_copy_height = height;
    g_copy_format = flags;
    g_seen_copy = x == 0 && y == 0 && buffer == g_frame_pixels;
    return g_copy_return;
}

int __cdecl fake_next(void* bink, bool show, int* repeats, bool decode) {
    ++g_next_calls; std::printf("next %u %u\n",unsigned(show),unsigned(decode));
    assert(!g_fake_next_active);
    g_fake_next_active = true;
    g_frame_composition_dc = nullptr;
    g_seen_bink = bink;
    g_next_show = show;
    g_next_decode = decode;
    g_seen_next_decode = repeats != nullptr;
    if (repeats) *repeats = g_repeat_value;
    const auto copy = reinterpret_cast<int (WINAPI*)(void*, void*, int, unsigned,
                                                      unsigned, unsigned, unsigned)>(g_copy_hook);
    if (copy) g_last_copy_result = copy(bink, g_frame_pixels, g_frame_pitch,
                                        g_frame_height, 0, 0, g_copy_format);
    g_last_frame_composition_dc = g_frame_composition_dc;
    g_fake_next_active = false;
    return g_next_return;
}

void __attribute__((fastcall)) fake_flip(void* self, void*) {
    assert(self != nullptr);
    ++g_flip_calls; std::puts("native-flip");
}

bool patch_call(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length) {
    ++g_patch_call_calls; std::printf("patch-copy %u\n",unsigned(g_patch_call_succeeds));
    assert(target == at(g_armada, kCopyCallRva));
    assert(length == sizeof(kCopyBytes) &&
           std::memcmp(target, expected, length) == 0 &&
           std::memcmp(expected, kCopyBytes, length) == 0);
    g_copy_hook = replacement;
    return g_patch_call_succeeds;
}

bool patch_bytes(void* target, const std::uint8_t* replacement,
                 const std::uint8_t* expected, std::size_t length) {
    ++g_patch_bytes_calls; std::printf("patch-slot %u\n",unsigned(g_patch_bytes_succeeds));
    assert(target == at(g_fleetops, kNextFrameSlot));
    assert(length == sizeof(void*) &&
           std::memcmp(target, expected, length) == 0);
    if (!g_patch_bytes_succeeds) return false;
    std::memcpy(&g_next_hook, replacement, sizeof(g_next_hook));
    return true;
}

void log_message(const char*, const char*) {}

BOOL WINAPI host_GetClientRect(HWND window, LPRECT rect) {
    if (window != g_window || !rect) return FALSE;
    *rect = RECT{0, 0, g_client_width, g_client_height};
    return TRUE;
}

HDC WINAPI host_GetDC(HWND window) {
    ++g_dc_gets; std::puts("getdc");
    return window == g_window && g_dc_available ? g_dib_dc : nullptr;
}

int WINAPI host_ReleaseDC(HWND window, HDC dc) {
    assert(window == g_window && dc == g_dib_dc);
    ++g_dc_releases; std::puts("releasedc");
    return 1;
}

HMODULE WINAPI host_GetModuleHandleA(LPCSTR name) {
    return name && std::strcmp(name, "binkw32.dll") == 0 ? g_bink : nullptr;
}

FARPROC WINAPI host_GetProcAddress(HMODULE module, LPCSTR name) {
    if (module != g_bink || !name ||
        std::strcmp(name, "_BinkCopyToBuffer@28") != 0) return nullptr;
    FARPROC result{};
    auto copy = &fake_copy;
    static_assert(sizeof(result) == sizeof(copy));
    std::memcpy(&result, &copy, sizeof(result));
    return result;
}

BOOL WINAPI host_PatBlt(HDC dc, int x, int y, int width, int height, DWORD rop) {
    ++g_patblt_calls; std::printf("clear %d %d %d %d\n",x,y,width,height);
    assert(g_fake_next_active && dc != g_dib_dc);
    if (!g_frame_composition_dc) g_frame_composition_dc = dc;
    assert(dc == g_frame_composition_dc);
    return g_real_pat_blt(dc, x, y, width, height, rop);
}

int WINAPI host_StretchDIBits(HDC dc, int x, int y, int width, int height,
                              int source_x, int source_y, int source_width,
                              int source_height, const VOID* pixels,
                              const BITMAPINFO* info, UINT usage, DWORD rop) {
    ++g_stretch_calls; std::printf("stretch %d %d %d %d %d %d %d %d\n",x,y,width,height,source_x,source_y,source_width,source_height);
    assert(g_fake_next_active && dc != g_dib_dc && pixels == g_frame_pixels && info);
    if (!g_frame_composition_dc) g_frame_composition_dc = dc;
    assert(dc == g_frame_composition_dc);
    return g_real_stretch_dibits(dc, x, y, width, height,
        source_x, source_y, source_width, source_height, pixels, info, usage, rop);
}

BOOL WINAPI host_BitBlt(HDC destination, int x, int y, int width, int height,
                        HDC source, int source_x, int source_y, DWORD rop) {
    ++g_final_blits; std::printf("present %d %d %d %d\n",x,y,width,height);
    assert(!g_fake_next_active && destination == g_dib_dc && source != destination);
    assert(source == g_last_frame_composition_dc);
    if (g_fail_final_blt) return FALSE;
    return g_real_bit_blt(destination, x, y, width, height,
                          source, source_x, source_y, rop);
}

BOOL WINAPI host_GdiFlush(void) {
    std::puts("flush"); const BOOL result = g_real_gdi_flush();
    if ((g_fake_next_active && g_fail_copy_flush) ||
        (!g_fake_next_active && g_fail_present_flush)) return FALSE;
    return result;
}

struct Dib {
    HDC dc{};
    HBITMAP bitmap{};
    HGDIOBJ previous{};
    std::uint8_t* bits{};
    Dib() {
        dc = CreateCompatibleDC(nullptr);
        assert(dc);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = 1920;
        info.bmiHeader.biHeight = -1080;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS,
                                  reinterpret_cast<void**>(&bits), nullptr, 0);
        assert(bitmap && bits);
        previous = SelectObject(dc, bitmap);
        assert(previous && previous != HGDI_ERROR);
        g_dib_dc = dc;
    }
    ~Dib() {
        g_dib_dc = nullptr;
        SelectObject(dc, previous);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
    std::uint8_t* pixel(int x, int y) { return bits + (y * 1920 + x) * 4; }
};

void make_images() {
    g_armada = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x3b8300,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    g_fleetops = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x211000,
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    g_bink = reinterpret_cast<HMODULE>(0x87654321);
    assert(g_armada && g_fleetops);
    auto* next = static_cast<std::uint8_t*>(at(g_armada, kNextFrameRva));
    std::memcpy(next, kNextBytes, sizeof(kNextBytes));
    // Preserve the verified prologue, then restore the entry stack and tail
    // jump to the synthetic original NextBinkFrame implementation.
    next[6] = 0x89; next[7] = 0xec; next[8] = 0x5d; next[9] = 0xe9;
    const auto jump = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(&fake_next) -
        reinterpret_cast<std::uintptr_t>(next + 14));
    std::memcpy(next + 10, &jump, sizeof(jump));
    std::memcpy(at(g_armada, kCopyCallRva), kCopyBytes, sizeof(kCopyBytes));
    void* next_pointer = next;
    std::memcpy(at(g_fleetops, kNextFrameSlot), &next_pointer, sizeof(next_pointer));
    void* copy_pointer = reinterpret_cast<void*>(&fake_copy);
    std::memcpy(at(g_armada, 0x3b82a0), &copy_pointer, sizeof(copy_pointer));
    g_pixels.resize(static_cast<std::size_t>(kPaddedPitch) * kSourceHeight);
    g_frame_pixels = g_pixels.data();
    for (unsigned y = 0; y < kSourceHeight; ++y) {
        auto* row = g_pixels.data() + static_cast<std::size_t>(y) * kPaddedPitch;
        // Valid image pixels are red above and blue below; padded columns are
        // green, making any source-stride bleed visible in the output.
        const std::uint32_t color = y < kSourceHeight / 2 ? 0x00ff0000 : 0x000000ff;
        for (unsigned x = 0; x < 1280; ++x) {
            const std::uint32_t value = x < kSourceWidth ? color : 0x0000ff00;
            std::memcpy(row + x * 4, &value, sizeof(value));
        }
    }

    static void* output_table[27];
    std::memset(output_table, 0, sizeof(output_table));
    output_table[0x68 / sizeof(void*)] = reinterpret_cast<void*>(&fake_flip);
    static void* output_object[1];
    output_object[0] = output_table;
    static std::uint8_t engine[0xd0]{};
    const unsigned display = 0;
    std::memcpy(engine + 0xc0, &display, sizeof(display));
    void* output = output_object;
    std::memcpy(engine + 0xcc, &output, sizeof(output));
    void* engine_pointer = engine;
    std::memcpy(at(g_armada, kEngineSlot), &engine_pointer, sizeof(engine_pointer));
}

void clear_dib(Dib& dib) {
    std::memset(dib.bits, 0xaa, 1920u * 1080u * 4u);
}

int call_next_frame(void* bink, bool show, bool decode, int* repeats) {
    return reinterpret_cast<int (__cdecl*)(void*, bool, int*, bool)>(g_next_hook)(
        bink, show, repeats, decode);
}

void run_frame(void* bink, HWND window, bool enabled, bool show = true) {
    native::Scope scope(bink, window, enabled);
    int repeats = -1;
    const int result = call_next_frame(bink, show, true, &repeats);
    assert(result == g_next_return);
    assert(repeats == g_repeat_value);
}

void assert_rgb(Dib& dib, int x, int y, std::uint8_t red,
                std::uint8_t green, std::uint8_t blue) {
    const auto* pixel = dib.pixel(x, y);
    assert(pixel[0] == blue && pixel[1] == green && pixel[2] == red);
}
}

int main() {
    make_images();
    Dib dib;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = log_message;
    api.patch_call = patch_call;
    api.patch_bytes = patch_bytes;
    std::array<std::uint32_t, 2> bink_state{kSourceWidth, kSourceHeight};
    const void* bink = bink_state.data();
    g_copy_format = kFormat32;
    g_repeat_value = 9;
    g_next_return = 37;
    g_copy_return = 0;

    // Signature and ownership preflight failures must stop before either patch.
    auto* next_bytes = static_cast<std::uint8_t*>(at(g_armada, kNextFrameRva));
    const auto saved_opcode = next_bytes[0];
    next_bytes[0] ^= 1;
    assert(!native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 0 && g_patch_bytes_calls == 0);
    next_bytes[0] = saved_opcode;

    void* slot = at(g_fleetops, kNextFrameSlot);
    void* saved_next{};
    std::memcpy(&saved_next, slot, sizeof(saved_next));
    void* foreign_next = reinterpret_cast<void*>(0x10203040);
    std::memcpy(slot, &foreign_next, sizeof(foreign_next));
    assert(!native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 0 && g_patch_bytes_calls == 0);
    std::memcpy(slot, &saved_next, sizeof(saved_next));

    void* imported_copy_slot = at(g_armada, 0x3b82a0);
    void* saved_copy{};
    std::memcpy(&saved_copy, imported_copy_slot, sizeof(saved_copy));
    void* foreign_copy = reinterpret_cast<void*>(0x50607080);
    std::memcpy(imported_copy_slot, &foreign_copy, sizeof(foreign_copy));
    assert(!native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 0 && g_patch_bytes_calls == 0);
    std::memcpy(imported_copy_slot, &saved_copy, sizeof(saved_copy));

    // A failed CALL patch prevents the slot patch. Its captured copy wrapper
    // still forwards while readiness remains false and performs no GDI work.
    g_patch_call_succeeds = false;
    assert(!native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 1 && g_patch_bytes_calls == 0 && g_copy_hook);
    const unsigned dc_before_failed_install = g_dc_gets;
    const unsigned copies_before_failed_install = g_copy_calls;
    {
        native::Scope scope(const_cast<void*>(bink), g_window, true);
        const auto copy = reinterpret_cast<int (WINAPI*)(void*, void*, int, unsigned,
            unsigned, unsigned, unsigned)>(g_copy_hook);
        assert(copy(const_cast<void*>(bink), g_frame_pixels, kPaddedPitch,
                    kSourceHeight, 0, 0, kFormat32) == g_copy_return);
    }
    assert(g_copy_calls == copies_before_failed_install + 1 &&
           g_dc_gets == dc_before_failed_install);

    // A failed NextFrame slot write leaves the already captured copy wrapper
    // forwarding to Bink, with GDI presentation still disabled.
    g_patch_call_succeeds = true;
    g_patch_bytes_succeeds = false;
    g_patch_call_calls = g_patch_bytes_calls = 0;
    assert(!native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 1 && g_patch_bytes_calls == 1);
    const unsigned copies_before_slot_failure = g_copy_calls;
    const unsigned dc_before_slot_failure = g_dc_gets;
    {
        native::Scope scope(const_cast<void*>(bink), g_window, true);
        const auto copy = reinterpret_cast<int (WINAPI*)(void*, void*, int, unsigned,
            unsigned, unsigned, unsigned)>(g_copy_hook);
        assert(copy(const_cast<void*>(bink), g_frame_pixels, kPaddedPitch,
                    kSourceHeight, 0, 0, kFormat32) == g_copy_return);
    }
    assert(g_copy_calls == copies_before_slot_failure + 1 &&
           g_dc_gets == dc_before_slot_failure);

    // Retrying both installs succeeds and enables the same hook for drawing.
    g_patch_bytes_succeeds = true;
    g_patch_call_calls = g_patch_bytes_calls = 0;
    assert(native::initialize(&api, g_armada, g_fleetops));
    assert(g_patch_call_calls == 1 && g_patch_bytes_calls == 1 &&
           g_next_hook && g_copy_hook);

    // 640x480 into 1920x1080 is 1440x1080, centred at x=240.
    clear_dib(dib);
    g_client_width = 1920; g_client_height = 1080;
    const unsigned copies_before_draw = g_copy_calls;
    const unsigned blits_before_draw = g_final_blits;
    const unsigned clears_before_draw = g_patblt_calls;
    const unsigned stretches_before_draw = g_stretch_calls;
    run_frame(const_cast<void*>(bink), g_window, true);
    GdiFlush();
    assert(g_next_show == false && g_next_decode && g_seen_next_decode);
    assert(g_copy_calls == copies_before_draw + 1 && g_seen_copy && g_copy_pitch == kPaddedPitch &&
           g_copy_height == kSourceHeight && g_copy_format == kFormat32);
    assert(g_flip_calls == 0 && g_dc_gets == 1 && g_dc_releases == 1);
    assert(g_final_blits == blits_before_draw + 1 &&
           g_patblt_calls == clears_before_draw + 1 &&
           g_stretch_calls == stretches_before_draw + 1 &&
           g_last_frame_composition_dc != g_dib_dc);
    assert_rgb(dib, 0, 0, 0, 0, 0);
    assert_rgb(dib, 239, 540, 0, 0, 0);
    assert_rgb(dib, 1680, 540, 0, 0, 0);
    assert_rgb(dib, 600, 10, 255, 0, 0);
    assert_rgb(dib, 600, 1070, 0, 0, 255);
    assert_rgb(dib, 1678, 100, 255, 0, 0); // rightmost image pixels avoid padding
    assert(g_last_copy_result == g_copy_return);

    // Two frames in the same movie scope reuse one offscreen DIB and each
    // performs exactly one visible blit after the native callback returns.
    const unsigned repeat_blits_before = g_final_blits;
    const unsigned repeat_dc_before = g_dc_gets;
    HDC reused_composition_dc{};
    {
        native::Scope scope(const_cast<void*>(bink), g_window, true);
        int repeats = -1;
        assert(call_next_frame(const_cast<void*>(bink), true, true, &repeats) == g_next_return);
        assert(repeats == g_repeat_value);
        reused_composition_dc = g_last_frame_composition_dc;
        assert(call_next_frame(const_cast<void*>(bink), true, true, &repeats) == g_next_return);
        assert(repeats == g_repeat_value &&
               g_last_frame_composition_dc == reused_composition_dc);
    }
    assert(g_final_blits == repeat_blits_before + 2 &&
           g_dc_gets == repeat_dc_before + 2);

    // A 1600x900 client gets a centred 1200x900 image at x=200.
    clear_dib(dib);
    g_client_width = 1600; g_client_height = 900;
    const unsigned blits_before_1600 = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    GdiFlush();
    assert(g_final_blits == blits_before_1600 + 1);
    assert_rgb(dib, 199, 450, 0, 0, 0);
    assert_rgb(dib, 1400, 450, 0, 0, 0);
    assert_rgb(dib, 800, 10, 255, 0, 0);
    assert_rgb(dib, 800, 890, 0, 0, 255);

    // At 800x600 the 4:3 source fills the client.
    clear_dib(dib);
    g_client_width = 800; g_client_height = 600;
    const unsigned blits_before_800 = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    GdiFlush();
    assert(g_final_blits == blits_before_800 + 1);
    assert_rgb(dib, 0, 0, 255, 0, 0);
    assert_rgb(dib, 799, 599, 0, 0, 255);

    // Outside native-movie scope, show=false and another Bink object, forward
    // all caller arguments unchanged and do not copy or present.
    const unsigned copies_before = g_copy_calls;
    const unsigned dc_before = g_dc_gets;
    run_frame(const_cast<void*>(bink), g_window, false);
    assert(g_next_show && g_copy_calls == copies_before + 1 && g_dc_gets == dc_before);
    int repeats = -1;
    const unsigned copy_before_show = g_copy_calls;
    reinterpret_cast<int (__cdecl*)(void*, bool, int*, bool)>(g_next_hook)(
        const_cast<void*>(bink), false, &repeats, false);
    assert(g_next_show == false && !g_next_decode && repeats == g_repeat_value &&
           g_copy_calls == copy_before_show + 1 && g_dc_gets == dc_before);
    const unsigned copy_before_scoped_show = g_copy_calls;
    const unsigned flips_before_scoped_show = g_flip_calls;
    {
        native::Scope scope(const_cast<void*>(bink), g_window, true);
        repeats = -1;
        const int result = reinterpret_cast<int (__cdecl*)(void*, bool, int*, bool)>(g_next_hook)(
            const_cast<void*>(bink), false, &repeats, true);
        assert(result == g_next_return && repeats == g_repeat_value);
    }
    assert(!g_next_show && g_next_decode && g_copy_calls == copy_before_scoped_show + 1 &&
           g_flip_calls == flips_before_scoped_show && g_dc_gets == dc_before);
    const unsigned copy_before_other = g_copy_calls;
    {
        native::Scope scope(const_cast<void*>(bink), g_window, true);
        int other_repeats = -1;
        const int result = reinterpret_cast<int (__cdecl*)(void*, bool, int*, bool)>(g_next_hook)(
            reinterpret_cast<void*>(0x99887766), true, &other_repeats, true);
        assert(result == g_next_return && other_repeats == g_repeat_value);
    }
    assert(g_copy_calls == copy_before_other + 1 && g_dc_gets == dc_before);

    // Unsupported formats and a missing DC fall back exactly once after a
    // successful original copy. A failed copy never flips.
    g_client_width = 1920; g_client_height = 1080;
    g_copy_format = kFormat16;
    const unsigned flips_before_format = g_flip_calls;
    const unsigned blits_before_format = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    assert(g_flip_calls == flips_before_format + 1 && g_final_blits == blits_before_format);
    g_copy_format = kFormat32;
    g_dc_available = false;
    const unsigned flips_before_dc = g_flip_calls;
    const unsigned blits_before_dc = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    assert(g_flip_calls == flips_before_dc + 1 && g_final_blits == blits_before_dc);
    g_dc_available = true;
    g_copy_return = 1;
    const unsigned flips_before_copy_failure = g_flip_calls;
    const unsigned blits_before_copy_failure = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    assert(g_flip_calls == flips_before_copy_failure &&
           g_final_blits == blits_before_copy_failure);
    g_copy_return = 0;
    g_frame_pitch = 3;
    const unsigned flips_before_bad_pitch = g_flip_calls;
    const unsigned blits_before_bad_pitch = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    assert(g_flip_calls == flips_before_bad_pitch + 1 &&
           g_final_blits == blits_before_bad_pitch);
    g_frame_pitch = kPaddedPitch;
    g_frame_pixels = nullptr;
    const unsigned flips_before_null_pixels = g_flip_calls;
    const unsigned blits_before_null_pixels = g_final_blits;
    run_frame(const_cast<void*>(bink), g_window, true);
    assert(g_flip_calls == flips_before_null_pixels + 1 &&
           g_final_blits == blits_before_null_pixels);
    g_frame_pixels = g_pixels.data();

    // If the single final blit fails, the destination DIB stays untouched and
    // the native output receives exactly one fallback flip.
    clear_dib(dib);
    const unsigned flips_before_blt_failure = g_flip_calls;
    const unsigned blits_before_blt_failure = g_final_blits;
    g_fail_final_blt = true;
    run_frame(const_cast<void*>(bink), g_window, true);
    g_fail_final_blt = false;
    assert(g_final_blits == blits_before_blt_failure + 1 &&
           g_flip_calls == flips_before_blt_failure + 1);
    assert_rgb(dib, 600, 100, 170, 170, 170);

    // A failed offscreen flush prevents presentation and falls back once.
    clear_dib(dib);
    const unsigned blits_before_compose_flush = g_final_blits;
    const unsigned flips_before_compose_flush = g_flip_calls;
    g_fail_copy_flush = true;
    run_frame(const_cast<void*>(bink), g_window, true);
    g_fail_copy_flush = false;
    assert(g_final_blits == blits_before_compose_flush &&
           g_flip_calls == flips_before_compose_flush + 1);
    assert_rgb(dib, 600, 100, 170, 170, 170);

    // A failed final flush is also reported as a failed presentation and uses
    // one native fallback flip after the single visible blit attempt.
    const unsigned blits_before_present_flush = g_final_blits;
    const unsigned flips_before_present_flush = g_flip_calls;
    g_fail_present_flush = true;
    run_frame(const_cast<void*>(bink), g_window, true);
    g_fail_present_flush = false;
    assert(g_final_blits == blits_before_present_flush + 1 &&
           g_flip_calls == flips_before_present_flush + 1);
    assert_rgb(dib, 600, 100, 255, 0, 0);

    native::shutdown();
    VirtualFree(g_armada, 0, MEM_RELEASE);
    VirtualFree(g_fleetops, 0, MEM_RELEASE);
    std::puts("native intro memory-DIB fitting, scoped forwarding and fallback passed");
}
