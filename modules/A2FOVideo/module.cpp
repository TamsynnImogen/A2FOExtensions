#include "../../rust/native_hooks_ffi.h"
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../sdk/include/a2fo_supported_fleetops.hpp"
#include "../../rust/media_ffi.h"
#include "../../rust/stage9_video_runtime_ffi.h"
#include "../../rust/stage10_video_completion_ffi.h"

#include <windows.h>
#include <mmsystem.h>


#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

extern "C" void __cdecl a2fo_video_set_client(void*, void*, int);

namespace {
constexpr char kName[] = "A2FOVideo";
constexpr std::uintptr_t kArmadaPlayMovieRva = 0x00086020;
constexpr std::uintptr_t kFleetOpsPlayMovieRva = 0x001d3cf0;

std::atomic<const A2FO_ModuleApi*> g_api{nullptr};
std::atomic<unsigned> g_active_hooks{0};
thread_local bool g_playing = false;
thread_local unsigned g_hook_depth = 0;
const A2FO_MediaApiV1* g_media = nullptr;
HMODULE g_media_library = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fo = nullptr;
using NativePlayMovie = bool (WINAPI*)(const char* filename);
NativePlayMovie g_native_play_movie = nullptr;
std::atomic<bool> g_enabled{false};

void* at(HMODULE image, std::uintptr_t rva) noexcept {
    return reinterpret_cast<std::uint8_t*>(image) + rva;
}

void log(const char* text) noexcept {
    const auto* api = g_api.load(std::memory_order_acquire);
    if (api && api->log) api->log(kName, text);
}

void logf(const char* format, const char* value) noexcept {
    char buffer[768]{};
    std::snprintf(buffer, sizeof(buffer), format, value ? value : "");
    log(buffer);
}

bool file_exists(const std::string& path) noexcept {
    const DWORD attrs = GetFileAttributesA(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool is_absolute_path(const std::string& path) noexcept {
    return path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    std::string result = left;
    if (result.back() != '\\' && result.back() != '/') result.push_back('\\');
    if (right.front() == '\\' || right.front() == '/') result += right.substr(1);
    else result += right;
    return result;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string extension_of(const std::string& filename) {
    const auto slash = filename.find_last_of("/\\");
    const auto dot = filename.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return {};
    return lowercase(filename.substr(dot));
}

std::string resolve_movie(const char* filename) {
    if (!filename || !*filename) return {};
    struct Context {
        std::string name, resolved;
        const char* root = nullptr;
        std::exception_ptr failure;
        std::uint32_t root_count = 0;
        const A2FO_ModuleApi* api = nullptr;
    } context{filename, {}, nullptr, {}, 0, g_api.load(std::memory_order_acquire)};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) {
                if (file_exists(ctx.name)) { ctx.resolved = ctx.name; return 1; }
            } else if (action == 1) {
                if (ctx.api && A2FO_MODULE_API_HAS(ctx.api, extension_root_count) &&
                    ctx.api->extension_root_count && ctx.api->extension_root)
                    ctx.root_count = ctx.api->extension_root_count();
                return 0;
            } else if (action == 2) {
                const char* root = ctx.api->extension_root(static_cast<std::uint32_t>(index));
                if (!root || !*root) return 2;
                ctx.root = root;
                const std::string animation = join_path(join_path(ctx.root, "animations"), ctx.name);
                if (file_exists(animation)) { ctx.resolved = animation; return 1; }
            } else if (action == 3) {
                const std::string direct = join_path(ctx.root, ctx.name);
                if (file_exists(direct)) { ctx.resolved = direct; return 1; }
            } else if (action == 4) {
                if (!ctx.api || !ctx.api->root_directory) return 2;
                const char* root = ctx.api->root_directory();
                if (!root || !*root) return 2;
                ctx.root = root;
                const std::string animation = join_path(join_path(ctx.root, "animations"), ctx.name);
                if (file_exists(animation)) { ctx.resolved = animation; return 1; }
            } else if (action == 5) {
                const std::string direct = join_path(ctx.root, ctx.name);
                if (file_exists(direct)) { ctx.resolved = direct; return 1; }
            }
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
        return 0;
    };
    const bool has_path = is_absolute_path(context.name) ||
        context.name.find('\\') != std::string::npos || context.name.find('/') != std::string::npos;
    a2fo_rs_stage9_video_resolve(step, &context, has_path, &context.root_count);
    if (context.failure) std::rethrow_exception(context.failure);
    return context.resolved;
}

// Settings/form offsets and Delphi setters are only used after whole-file
// FleetOpsHook identity validation (the same three profiles as ShellDisplay).
bool readable(const void* address, std::size_t size) noexcept {
    if (!address || !size) return false;
    auto begin = reinterpret_cast<std::uintptr_t>(address);
    const auto end = begin + size;
    if (end < begin) return false;
    while (begin < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(reinterpret_cast<void*>(begin), &info, sizeof(info)) ||
            info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
        const DWORD protection = info.Protect & 0xff;
        if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
            protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
            protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY) return false;
        const auto next = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        if (next <= begin) return false;
        begin = next;
    }
    return true;
}

template<class T> T read(const void* address, std::size_t offset = 0) noexcept {
    T result{};
    const auto* source = static_cast<const std::uint8_t*>(address);
    if (source && readable(source + offset, sizeof(T)))
        std::memcpy(&result, source + offset, sizeof(T));
    return result;
}

bool known_fleetops() noexcept {
    char path[32768]{};
    const DWORD length = GetModuleFileNameA(g_fo, path, sizeof(path));
    if (!length || length >= sizeof(path)) return false;
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info) ||
        info.nFileSizeHigh || info.nFileSizeLow != 3043840) return false;
    std::uint32_t crc = 0;
    const bool known_crc =
        a2fo::supported_armada::file_range_crc32(g_fo, 0, info.nFileSizeLow, crc) &&
        (crc == 0x07bd1e4f || crc == 0x3ed28fbe || crc == 0x46a0dd96);
    return known_crc || a2fo::supported_fleetops::approved_profile(g_fo);
}

void* main_form() noexcept {
    return g_fo ? read<void*>(at(g_fo, 0x24485c)) : nullptr;
}

bool own_visible_window(HWND window) noexcept {
    DWORD pid = 0;
    if (!window || !IsWindowVisible(window)) return false;
    GetWindowThreadProcessId(window, &pid);
    return pid == GetCurrentProcessId();
}

using NativeMovieWindow = HWND (__cdecl*)();
NativeMovieWindow g_native_movie_window = nullptr;

void log_window(const char* label, HWND window) noexcept {
    char name[64]{};
    RECT outer{}, client{};
    GetClassNameA(window, name, sizeof(name));
    GetWindowRect(window, &outer);
    GetClientRect(window, &client);
    char message[400]{};
    std::snprintf(message, sizeof(message),
        "Movie HWND %s: handle=%p; class=%s; outer=%ld,%ld %ldx%ld; client=%ldx%ld; style=0x%lx; classstyle=0x%lx",
        label, static_cast<void*>(window), name, outer.left, outer.top,
        outer.right - outer.left, outer.bottom - outer.top, client.right, client.bottom,
        static_cast<unsigned long>(GetWindowLongPtrA(window, GWL_STYLE)),
        static_cast<unsigned long>(GetClassLongPtrA(window, GCL_STYLE)));
    log(message);
}

struct WindowSearch {
    DWORD pid = 0;
    HWND result = nullptr;
    long long best_area = -1;
};

BOOL CALLBACK enum_game_windows(HWND hwnd, LPARAM value) noexcept {
    auto* state = reinterpret_cast<WindowSearch*>(value);
    if (!state) return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != state->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr)
        return TRUE;

    RECT client{};
    if (!GetClientRect(hwnd, &client)) return TRUE;
    const long long width = std::max<LONG>(0, client.right - client.left);
    const long long height = std::max<LONG>(0, client.bottom - client.top);
    const long long area = width * height;
    if (area > state->best_area) {
        state->best_area = area;
        state->result = hwnd;
    }
    return TRUE;
}

HWND find_game_window() noexcept {
    // Match the native Bink renderer's verified HWND getter. The VCL main
    // form and graphics output can be separate windows during startup.
    HWND native = g_native_movie_window ? g_native_movie_window() : nullptr;
    log_window("native getter", native);
    if (own_visible_window(native) && !(GetWindowLongPtrA(native, GWL_STYLE) & WS_CHILD)) return native;

    // Prefer the actual VCL main form, even when another process-owned
    // window happens to be larger. No native getters or constructors run here.
    HWND main = read<HWND>(main_form(), 0x1b4);
    if (own_visible_window(main) && !(GetWindowLongPtrA(main, GWL_STYLE) & WS_CHILD)) return main;

    // During intro playback Fleet Ops can make the authored movie form the
    // active/foreground window.  That form may still be 1280x960 even when the
    // shell itself is running at a larger selected resolution.  Prefer the
    // largest visible ownerless top-level window belonging to this process.
    WindowSearch search{GetCurrentProcessId(), nullptr, -1};
    EnumWindows(enum_game_windows, reinterpret_cast<LPARAM>(&search));
    if (search.result) return search.result;

    HWND window = GetActiveWindow();
    if (own_visible_window(window)) return window;
    window = GetForegroundWindow();
    if (window) {
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid == GetCurrentProcessId()) return window;
    }
    return nullptr;
}

struct SavedDisplayMode {
    int width = 0;
    int height = 0;
    int depth = 32;
    bool windowed = true;
    int adapter = 0;
};

bool saved_display_mode(SavedDisplayMode& out) noexcept {
    if (!g_fo) return false;
    void* settings = read<void*>(at(g_fo, 0x20e644));
    if (!readable(settings, 0x73)) return false;

    const auto* bytes = static_cast<const std::uint8_t*>(settings);
    auto read_i32 = [bytes](std::size_t offset) noexcept {
        std::int32_t value = 0;
        std::memcpy(&value, bytes + offset, sizeof(value));
        return value;
    };

    out.width = read_i32(0x30);
    out.height = read_i32(0x34);
    out.depth = read_i32(0x38);
    out.adapter = read_i32(0x64);
    out.windowed = bytes[0x72] != 0;

    if (out.width <= 0 || out.height <= 0) {
        DEVMODEA mode{};
        mode.dmSize = sizeof(mode);
        DISPLAY_DEVICEA display{};
        display.cb = sizeof(display);
        if (out.adapter >= 0 && EnumDisplayDevicesA(nullptr, static_cast<DWORD>(out.adapter), &display, 0) &&
            EnumDisplaySettingsA(display.DeviceName, ENUM_CURRENT_SETTINGS, &mode)) {
            out.width = static_cast<int>(mode.dmPelsWidth);
            out.height = static_cast<int>(mode.dmPelsHeight);
            if (out.depth != 16 && out.depth != 32) out.depth = static_cast<int>(mode.dmBitsPerPel);
        } else {
            out.width = GetSystemMetrics(SM_CXSCREEN);
            out.height = GetSystemMetrics(SM_CYSCREEN);
        }
    }
    if (out.depth != 16 && out.depth != 32) out.depth = 32;
    return out.width >= 640 && out.width <= 16384 &&
           out.height >= 480 && out.height <= 16384;
}

bool apply_movie_window(HWND window, const SavedDisplayMode& saved) noexcept {
    // Match ShellDisplay's intro scope: nested menus/gameplay keep their layout.
    if (!g_armada || !own_visible_window(window) ||
        read<std::int32_t>(at(g_armada, 0x3a8980)) > 1) return false;
    if (GetWindowLongPtrA(window, GWL_STYLE) & WS_CHILD) {
        log("Movie geometry left native: child HWND has no verified top-level positioning contract");
        return false;
    }

    LONG_PTR style = GetWindowLongPtrA(window, GWL_STYLE);
    LONG_PTR ex_style = GetWindowLongPtrA(window, GWL_EXSTYLE);
    const LONG_PTR frame_mask = WS_POPUP | WS_CAPTION | WS_THICKFRAME |
        WS_BORDER | WS_DLGFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    const LONG_PTR edge_mask = WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE |
        WS_EX_CLIENTEDGE | WS_EX_STATICEDGE;
    style = (style & ~frame_mask) | (saved.windowed
        ? WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER : WS_POPUP);
    ex_style = (ex_style & ~edge_mask) | (saved.windowed ? WS_EX_WINDOWEDGE : 0);
    RECT outer{0, 0, saved.width, saved.height};
    if (saved.windowed && !AdjustWindowRectEx(&outer, static_cast<DWORD>(style),
                                             GetMenu(window) != nullptr,
                                             static_cast<DWORD>(ex_style))) return false;

    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);
    DISPLAY_DEVICEA display{};
    display.cb = sizeof(display);
    DEVMODEA mode{};
    mode.dmSize = sizeof(mode);
    if (saved.adapter >= 0 && EnumDisplayDevicesA(nullptr, saved.adapter, &display, 0) &&
        EnumDisplaySettingsA(display.DeviceName, ENUM_CURRENT_SETTINGS, &mode) &&
        (mode.dmFields & DM_POSITION)) {
        const POINT center{mode.dmPosition.x + static_cast<LONG>(mode.dmPelsWidth / 2),
                           mode.dmPosition.y + static_cast<LONG>(mode.dmPelsHeight / 2)};
        monitor = MonitorFromPoint(center, MONITOR_DEFAULTTOPRIMARY);
    }
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!monitor || !GetMonitorInfoA(monitor, &info)) return false;
    const RECT area = saved.windowed ? info.rcWork : info.rcMonitor;
    const int width = outer.right - outer.left;
    const int height = outer.bottom - outer.top;
    // Keep an oversized window's title bar accessible. Borderless still uses
    // the selected resolution rather than silently substituting desktop size.
    const int x = area.left + std::max(0, static_cast<int>(area.right - area.left) - width) / 2;
    const int y = area.top + std::max(0, static_cast<int>(area.bottom - area.top) - height) / 2;
    SetLastError(0);
    const LONG_PTR old_style = SetWindowLongPtrA(window, GWL_STYLE, style);
    if (!old_style && GetLastError()) return false;
    SetLastError(0);
    const LONG_PTR old_ex_style = SetWindowLongPtrA(window, GWL_EXSTYLE, ex_style);
    if (!old_ex_style && GetLastError()) {
        SetWindowLongPtrA(window, GWL_STYLE, old_style);
        return false;
    }

    // HWND-only sizing can be undone by VCL's cached client constraints.
    // These are the original setters already verified by ShellDisplay.
    void* form = main_form();
    if (read<HWND>(form, 0x1b4) == window) {
        a2fo_video_set_client(at(g_fo, 0xad390), form, saved.height);
        a2fo_video_set_client(at(g_fo, 0xad35c), form, saved.width);
    }
    if (!SetWindowPos(window, nullptr, x, y, width, height,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_FRAMECHANGED)) return false;
    RECT actual{};
    RECT positioned{};
    if (GetWindowRect(window, &positioned)) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
            "Movie window: %s; position=%ld,%ld; outer=%ldx%ld; adapter=%d",
            saved.windowed ? "windowed" : "borderless", positioned.left, positioned.top,
            positioned.right - positioned.left, positioned.bottom - positioned.top, saved.adapter);
        log(message);
    }
    return GetClientRect(window, &actual) && actual.right - actual.left == saved.width &&
        actual.bottom - actual.top == saved.height;
}

bool load_media_backend() {
    if (g_media) return g_media->startup() != 0;
    HMODULE owner = GetModuleHandleW(L"A2FOExtensions.dll");
    const bool standalone = owner == nullptr;
    if (!owner && !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&load_media_backend), &owner)) return false;
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(owner, path, 32768);
    if (!length || length >= 32768) return false;
    std::wstring base(path, length);
    auto slash = base.find_last_of(L"/\\");
    if (slash == std::wstring::npos) return false;
    base.resize(slash);
    if (standalone) {
        slash = base.find_last_of(L"/\\");
        if (slash != std::wstring::npos && _wcsicmp(base.c_str()+slash+1, L"modules") == 0)
            base.resize(slash);
    }
    const std::wstring dll = base + L"\\A2FOMedia.dll";
    HMODULE media = LoadLibraryW(dll.c_str());
    if (!media) { log("Rust media backend could not be loaded beside the extension DLL"); return false; }
    const FARPROC symbol = GetProcAddress(media, "A2FOMedia_GetApiV1");
    A2FO_MediaGetApi entry = nullptr;
    static_assert(sizeof(entry) == sizeof(symbol), "function pointer width");
    std::memcpy(&entry, &symbol, sizeof(entry));
    const auto* api = entry ? entry(A2FO_MEDIA_VERSION) : nullptr;
    if (!api || api->size < sizeof(*api) || api->version != A2FO_MEDIA_VERSION ||
        !api->open || !api->info || !api->play || !api->seek || !api->cancel ||
        !api->close || !api->shutdown || !api->startup || !api->startup()) {
        FreeLibrary(media); log("Rust media backend API mismatch"); return false;
    }
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(entry), &pinned)) { api->shutdown(); FreeLibrary(media); return false; }
    g_media_library = media;
    g_media = api;
    return true;
}

bool play_rust_movie(const std::string& movie) {
    if (!g_media) return false;
    const int count = MultiByteToWideChar(CP_ACP, 0, movie.c_str(), -1, nullptr, 0);
    if (count <= 1) return false;
    std::vector<wchar_t> path(static_cast<std::size_t>(count));
    if (!MultiByteToWideChar(CP_ACP, 0, movie.c_str(), -1, path.data(), count)) return false;
    A2FO_MediaOpen request{};
    request.size = sizeof(request); request.version = A2FO_MEDIA_VERSION;
    request.path = reinterpret_cast<const std::uint16_t*>(path.data());
    request.path_length = static_cast<std::uint32_t>(count - 1); request.threads = 4;
    request.log = +[](void*, const char* message) { log(message); };
    const std::uint32_t session = g_media->open(&request);
    if (!session) return false;
    struct Session { std::uint32_t id; ~Session() { g_media->close(id); } } guard{session};
    A2FO_MediaInfo info{}; info.size = sizeof(info);
    if (!g_media->info(session, &info) || !info.width || !info.height) return false;
    if (!g_enabled.load(std::memory_order_seq_cst)) { g_media->cancel(session); return true; }
    HWND window = find_game_window();
    if (!window) { log("Modern movie playback could not find an owned output window"); return false; }
    SavedDisplayMode saved{};
    if (saved_display_mode(saved)) apply_movie_window(window, saved);
    A2FO_MediaPlay play{};
    play.size = sizeof(play); play.version = A2FO_MEDIA_VERSION;
    play.window = reinterpret_cast<std::uintptr_t>(window);
    return g_media->play(session, &play) != 0;
}

std::string modern_replacement_for(const char* filename) {
    if (!filename || !*filename) return {};
    const std::string requested(filename);
    const std::string ext = extension_of(requested);
    const std::uint8_t kind = a2fo_rs_stage9_video_replacement_kind(ext.c_str());
    struct Context {
        const char* filename;
        std::string stem, resolved;
        std::exception_ptr failure;
    } context{filename, {}, {}, {}};
    if (kind != 1) {
        context.stem = requested;
        if (ext == ".bik") context.stem.resize(context.stem.size() - ext.size());
    }
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) ctx.resolved = resolve_movie(ctx.filename);
            else if (action == 1) {
                static constexpr const char* extensions[] = {".mp4", ".webm", ".mkv"};
                const std::string candidate = ctx.stem + extensions[index];
                ctx.resolved = resolve_movie(candidate.c_str());
            }
            return !ctx.resolved.empty();
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
    };
    a2fo_rs_stage9_video_replacement(step, &context, kind);
    if (context.failure) std::rethrow_exception(context.failure);
    return context.resolved;
}

bool WINAPI play_movie_hook(const char* filename) {
    struct HookScope {
        bool counted = true;
        HookScope() { ++g_hook_depth; g_active_hooks.fetch_add(1, std::memory_order_seq_cst); }
        void release() noexcept {
            if (!counted) return;
            counted = false;
            if (g_active_hooks.fetch_sub(1, std::memory_order_seq_cst) == 1 &&
                !g_enabled.load(std::memory_order_seq_cst)) g_api.store(nullptr, std::memory_order_release);
        }
        ~HookScope() { --g_hook_depth; release(); }
    } hook_scope;
    if (!g_enabled.load(std::memory_order_seq_cst)) {
        hook_scope.release();
        return g_native_play_movie ? g_native_play_movie(filename) : false;
    }

    logf("Movie request intercepted: %s", filename ? filename : "<null>");

    // PeekMessage/DispatchMessage may reenter a game WndProc. A nested movie
    // must not open another decoder/native playback inside the active one.
    bool& playing = g_playing;
    static thread_local ULONGLONG previous_finish = 0;
    if (playing) {
        log("Reentrant movie request declined during active playback");
        return false;
    }
    const ULONGLONG entered_at = GetTickCount64();
    if (previous_finish) {
        char boundary[128]{};
        std::snprintf(boundary, sizeof(boundary), "Movie transition: gap before request=%llu ms",
                      static_cast<unsigned long long>(entered_at - previous_finish));
        log(boundary);
    }
    struct PlaybackScope {
        bool& active;
        ULONGLONG& finished;
        ULONGLONG started;
        PlaybackScope(bool& flag, ULONGLONG& end, ULONGLONG begin)
            : active(flag), finished(end), started(begin) { active = true; }
        ~PlaybackScope() {
            active = false;
            finished = GetTickCount64();
            char boundary[128]{};
            std::snprintf(boundary, sizeof(boundary), "Movie request finished: occupied=%llu ms",
                          static_cast<unsigned long long>(finished - started));
            log(boundary);
        }
    } scope(playing, previous_finish, entered_at);

    const ULONGLONG lookup_at = GetTickCount64();
    struct Context {
        const char* filename;
        ULONGLONG lookup_at;
        std::string modern;
        std::exception_ptr failure;
    } context{filename, lookup_at, {}, {}};
    const auto step = +[](void* opaque, std::uint32_t action, std::uintptr_t had_modern) noexcept -> std::uintptr_t {
        auto& ctx = *static_cast<Context*>(opaque);
        try {
            if (action == 0) {
                ctx.modern = modern_replacement_for(ctx.filename);
                return !ctx.modern.empty();
            }
            if (action == 1) {
                char lookup[128]{};
                std::snprintf(lookup, sizeof(lookup), "Modern movie lookup: %llu ms",
                              static_cast<unsigned long long>(GetTickCount64() - ctx.lookup_at));
                log(lookup);
                logf("Modern movie selected: %s", ctx.modern.c_str());
                return play_rust_movie(ctx.modern);
            }
            // Shutdown can cancel an in-flight open. Consume that request
            // instead of starting a second, uncancellable native movie.
            if (!g_enabled.load(std::memory_order_seq_cst)) return 1;
            if (had_modern)
                log("Modern movie playback failed; falling back to native Fleet Ops/Bink lookup");
            return g_native_play_movie ? g_native_play_movie(ctx.filename) : false;
        } catch (...) { ctx.failure = std::current_exception(); return 3; }
    };
    const bool result = a2fo_rs_stage9_video_play(step, &context);
    if (context.failure) { if (!g_enabled.load(std::memory_order_seq_cst)) return true; log("Modern movie routing failed; returning to native lookup"); return g_native_play_movie ? g_native_play_movie(filename) : false; }
    return result;
}

constexpr std::size_t kFleetOpsPlayMoviePrologueSize = 6;
constexpr std::uint8_t kFleetOpsPlayMoviePrologue[kFleetOpsPlayMoviePrologueSize] = {
    0x55,
    0x8b, 0xec,
    0x83, 0xc4, 0xf4
};

[[maybe_unused]] bool write_rel32_jump(std::uint8_t* site, const void* destination,
                                       std::size_t span) noexcept {
    return a2fo_rs_windows_jump_unchecked(site,destination,span)!=0;
}

bool install_fleetops_trampoline() {
    auto* site = static_cast<std::uint8_t*>(at(g_fo, kFleetOpsPlayMovieRva));
    const auto publish=[](void*,void* gateway) noexcept {
        g_native_play_movie = reinterpret_cast<NativePlayMovie>(gateway);
    };
    A2foRsVideoInstall result{};
    if (a2fo_rs_windows_video_install(site,reinterpret_cast<void*>(&play_movie_hook),
            kFleetOpsPlayMoviePrologueSize,kFleetOpsPlayMoviePrologue,&result,nullptr,publish))
        return true;
    switch(result.status) {
    case 1:log("Fleet Ops Program_PlayMovie_New prologue did not match; video hook not installed");break;
    case 2:log("Failed to allocate Program_PlayMovie_New trampoline");break;
    default:log("Failed to patch Fleet Ops Program_PlayMovie_New");break;
    }
    return false;
}

bool preflight() {
    if (!g_armada || !g_fo || !known_fleetops() ||
        a2fo::supported_armada::identify(g_armada) == a2fo::supported_armada::Identity::unsupported) {
        log("Unsupported Armada/Fleet Ops binary identity; video hook not installed");
        return false;
    }

    auto* armada_site = static_cast<std::uint8_t*>(at(g_armada, kArmadaPlayMovieRva));
    if (armada_site[0] != 0xe9) {
        log("Armada movie entry is not owned by Fleet Ops; video hook not installed");
        return false;
    }
    std::int32_t displacement = 0;
    std::memcpy(&displacement, armada_site + 1, sizeof(displacement));
    void* destination = armada_site + 5 + displacement;
    if (destination != at(g_fo, kFleetOpsPlayMovieRva)) {
        log("Armada movie entry has an unexpected owner; video hook not installed");
        return false;
    }
    return true;
}
} // namespace

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->fleetops_module) return false;
    g_api.store(api, std::memory_order_release);
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fo = static_cast<HMODULE>(api->fleetops_module());
    if (!preflight()) return false;

    if (!load_media_backend()) return false;
    // The installed trampoline remains process-wide; its code must remain live.
    HMODULE pinned_owner = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR>(&play_movie_hook), &pinned_owner)) return false;

    g_native_movie_window = reinterpret_cast<NativeMovieWindow>(at(g_fo, 0x1ddb94));
    if (!g_native_play_movie && !install_fleetops_trampoline()) return false;
    g_enabled.store(true, std::memory_order_seq_cst);

    log("Initialized v12: Rust H.264/VP9 + AAC/Opus/Vorbis playback; native BIK fallback preserved");
    return true;
}

extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    g_enabled.store(false, std::memory_order_seq_cst);
    if (g_media && !g_media->shutdown() && !g_hook_depth)
        log("Rust media shutdown retained driver-owned buffers; cleanup will be retried before playback");
    if (g_hook_depth) return; // Reentrant callback: outer hook owns deferred cleanup.
    while (g_active_hooks.load(std::memory_order_seq_cst)) Sleep(1);
    g_api.store(nullptr, std::memory_order_release);
}
