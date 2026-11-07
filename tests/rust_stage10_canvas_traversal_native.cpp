// Full production canvas traversal fixture. Build once per MODULE_SOURCE.
// HWND/HDC values are inert tokens; no window, input, or GDI calls are made.
#include <windows.h>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <vector>

#ifndef MODULE_SOURCE
#error MODULE_SOURCE is required
#endif

BOOL WINAPI host_GetClientRect(HWND window, LPRECT rect);
int WINAPI host_MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count);
#define GetClientRect host_GetClientRect
#define MapWindowPoints host_MapWindowPoints
#include MODULE_SOURCE
#undef MapWindowPoints
#undef GetClientRect

namespace canvas = a2fo::shell_display::canvas;

namespace {
void require(bool condition) { if (!condition) std::abort(); }
HWND token(std::uintptr_t value) { return reinterpret_cast<HWND>(value); }
HDC dc_token(std::uintptr_t value) { return reinterpret_cast<HDC>(value); }

HWND source_handle = token(0x1000);
HWND target_handle = token(0x2000);
RECT source_client{0, 0, 200, 200};
RECT target_client{0, 0, 400, 400};
unsigned native_calls = 0;
UINT native_count = 0;
std::array<POINT, 2> native_input{};
int native_result = 23;

void reset() {
    canvas::g_leases.clear();
    canvas::g_windows.clear();
    canvas::g_mode = {};
    canvas::g_ready.store(false, std::memory_order_release);
    native_calls = 0;
    native_count = 0;
    native_input = {};
    native_result = 23;
}

std::shared_ptr<canvas::Window> window(HWND handle, HWND parent = nullptr) {
    auto record = std::make_shared<canvas::Window>();
    record->handle = handle;
    record->root = handle;
    record->parent = parent;
    record->width = 100;
    record->height = 100;
    canvas::g_windows[handle] = record;
    return record;
}

void ready() {
    canvas::g_mode = {1920, 1080, 32};
    canvas::g_ready.store(true, std::memory_order_release);
}

void lease(const std::shared_ptr<canvas::Window>& record, HDC logical) {
    canvas::g_leases.push_back(
        canvas::Lease{record, dc_token(0x9000), logical, {}, false, 0, {}});
}

void descendants() {
    reset();
    for (std::uintptr_t index = 1; index <= 66; ++index) {
        window(token(index), index == 66 ? nullptr : token(index + 1));
    }
    require(canvas::descendant(token(1), token(65)));
    require(!canvas::descendant(token(1), token(66)));
    require(!canvas::descendant(token(1), token(1)));
    require(!canvas::descendant(token(70), token(65)));
    auto self = window(token(80), token(80));
    require(!canvas::descendant(self->handle, token(80)));
    std::puts("row:descendant-found-missing-self-bound64-bound65");
}

void leases() {
    reset();
    const auto older = window(token(101));
    const auto newer = window(token(102));
    const HDC logical = dc_token(0x3000);
    ready();
    lease(older, logical);
    lease(newer, logical);
    require(A2FO_ShellCanvasWindowV1(logical) == newer->handle);
    require(!A2FO_ShellCanvasWindowV1(dc_token(0x3001)));
    require(!A2FO_ShellCanvasWindowV1(nullptr));
    canvas::g_ready.store(false, std::memory_order_release);
    require(!A2FO_ShellCanvasWindowV1(logical));
    ready();
    canvas::g_mode = {};
    require(!A2FO_ShellCanvasWindowV1(logical));
    ready();
    canvas::g_leases.clear();
    auto stale = std::make_shared<canvas::Window>();
    stale->handle = older->handle;
    lease(stale, logical);
    require(!A2FO_ShellCanvasWindowV1(logical));
    canvas::g_leases.clear();
    lease(older, logical);
    lease(stale, logical);
    require(A2FO_ShellCanvasWindowV1(logical) == older->handle);
    std::puts("row:lease-reverse-scope-inactive-null");
}

void mapping() {
    reset();
    window(source_handle);
    window(target_handle);
    ready();
    POINT points[2]{{7, 9}, {11, 13}};
    require(canvas::map_points(source_handle, target_handle, points, 2) == 23);
    require(native_calls == 1 && native_count == 2);
    require(native_input[0].x == 14 && native_input[0].y == 18);
    require(native_input[1].x == 22 && native_input[1].y == 26);
    // Native stub adds (+10,-6); target inverse scale divides by four.
    require(points[0].x == 6 && points[0].y == 3);
    require(points[1].x == 8 && points[1].y == 5);

    native_calls = 0;
    require(canvas::map_points(source_handle, target_handle, points, 0) == 23);
    require(native_calls == 1 && native_count == 0);

    native_calls = 0;
    std::vector<POINT> many(4097, POINT{7, 9});
    require(canvas::map_points(source_handle, target_handle,
                              many.data(), 4097) == 23);
    require(native_calls == 1 && native_count == 4097);
    require(native_input[0].x == 7 && native_input[0].y == 9);
    require(many[0].x == 17 && many[0].y == 3);

    native_calls = 0;
    POINT failed{7, 9};
    native_result = -1;
    require(canvas::map_points(source_handle, target_handle, &failed, 1) == -1);
    require(native_calls == 1 && native_input[0].x == 14 &&
           native_input[0].y == 18);
    require(failed.x == 6 && failed.y == 3);
    std::puts("row:map-source-native-target-zero-bypass-error");
}
} // namespace

BOOL WINAPI host_GetClientRect(HWND handle, LPRECT rect) {
    if (!rect) return FALSE;
    if (handle == source_handle) *rect = source_client;
    else if (handle == target_handle) *rect = target_client;
    else return FALSE;
    return TRUE;
}

int WINAPI host_MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count) {
    require(from == source_handle && to == target_handle);
    ++native_calls;
    native_count = count;
    if (count > 0) native_input[0] = points[0];
    if (count > 1) native_input[1] = points[1];
    for (UINT index = 0; index < count; ++index) {
        points[index].x += 10;
        points[index].y -= 6;
    }
    return native_result;
}

int main() {
    std::puts("ready:canvas-traversal");
    descendants();
    leases();
    mapping();
    reset();
    std::puts("completed:canvas-traversal");
    return 0;
}
