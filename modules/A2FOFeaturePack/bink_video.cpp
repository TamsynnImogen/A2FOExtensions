/*
 * File: modules/A2FOFeaturePack/bink_video.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bink scaling hooks for viewport and movie rendering paths (D3D texture and GDI) to avoid stretched UI artifacts.
 */

#include "bink_video.hpp"

#include <d3d9.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace a2fo {
namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";

// TBinkIntro's playback loop calls its frame renderer with the object in EAX.
// The supported FleetOpsHook build has only this one call site.
constexpr std::uintptr_t kBinkRenderRva = 0x1aa470;
constexpr std::uintptr_t kBinkRenderCallRva = 0x1aa6ab;
constexpr std::size_t kBinkDeviceOffset = 0x44;

// Armada's original movie renderer decodes to a top-down DIB and displays it
// with SetDIBitsToDevice.  That API cannot scale, so a resized movie window
// leaves the native-sized frame in its upper-left corner.
constexpr std::uintptr_t kArmadaBinkSetDibitsCallRva = 0x0639ba;

// Armada's menu/campaign movie player uses a separate decoder thread and
// uploads each frame to a D3D8 texture.  The renderer is handed a transformed
// UI rectangle, which retains the movie's native dimensions after Gamescope
// resizes the game surface.  Both callers use the same cdecl renderer.
constexpr std::uintptr_t kArmadaBinkTextureRenderRva = 0x064350;
constexpr std::uintptr_t kArmadaBinkTextureRenderCallRva1 = 0x0e5da5;
constexpr std::uintptr_t kArmadaBinkTextureRenderCallRva2 = 0x0e6153;

// This engine global points to the active viewport owner.  Its four-float
// rectangle at +0x1b0 is also the source used by Armada's own UI transform at
// 0x51b210, so it is the authoritative full render area for this movie path.
constexpr std::uintptr_t kArmadaViewportOwnerGlobalRva = 0x365010;
constexpr std::size_t kArmadaViewportRectOffset = 0x1b0;

const std::uint8_t kExpectedBinkRenderCall[] =
    {0xe8, 0xc0, 0xfd, 0xff, 0xff};
const std::uint8_t kExpectedBinkRender[] =
    {0x53, 0x83, 0xc4, 0x80, 0x8b, 0xd8};
const std::uint8_t kExpectedArmadaBinkSetDibitsCall[] =
    {0xff, 0x15, 0x98, 0x7a, 0x7b, 0x00};
const std::uint8_t kExpectedArmadaBinkTextureRender[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedArmadaBinkTextureRenderCall1[] =
    {0xe8, 0xa6, 0xe5, 0xf7, 0xff};
const std::uint8_t kExpectedArmadaBinkTextureRenderCall2[] =
    {0xe8, 0xf8, 0xe1, 0xf7, 0xff};

using BinkRenderFunction =
    void (__attribute__((regparm(1))) *)(void* bink_intro);

struct ArmadaFloatRect {
    float x;
    float y;
    float width;
    float height;
};

using ArmadaBinkTextureRenderFunction =
    void (__cdecl *)(const ArmadaFloatRect* rectangle);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
BinkRenderFunction g_bink_render = nullptr;
ArmadaBinkTextureRenderFunction g_armada_bink_texture_render = nullptr;
volatile LONG g_viewport_log_count = 0;
volatile LONG g_gdi_log_count = 0;
volatile LONG g_texture_log_count = 0;

void* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<std::uint8_t*>(module) + rva;
}

void log_message(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

void prepare_bink_viewport(void* bink_intro) noexcept {
    if (!bink_intro) return;

    auto* object = static_cast<std::uint8_t*>(bink_intro);
    IDirect3DDevice9* device =
        *reinterpret_cast<IDirect3DDevice9**>(object + kBinkDeviceOffset);
    if (!device) return;

    D3DVIEWPORT9 viewport{};
    if (FAILED(device->GetViewport(&viewport))) return;

    IDirect3DSurface9* render_target = nullptr;
    D3DSURFACE_DESC target_desc{};
    if (FAILED(device->GetRenderTarget(0, &render_target)) ||
        !render_target) {
        return;
    }
    const HRESULT desc_result = render_target->GetDesc(&target_desc);
    render_target->Release();
    if (FAILED(desc_result) || target_desc.Width == 0 ||
        target_desc.Height == 0) {
        return;
    }

    D3DPRESENT_PARAMETERS present{};
    IDirect3DSwapChain9* swap_chain = nullptr;
    unsigned client_width = 0;
    unsigned client_height = 0;
    if (SUCCEEDED(device->GetSwapChain(0, &swap_chain)) && swap_chain) {
        if (SUCCEEDED(swap_chain->GetPresentParameters(&present)) &&
            present.hDeviceWindow) {
            RECT client{};
            if (GetClientRect(present.hDeviceWindow, &client) &&
                client.right > client.left && client.bottom > client.top) {
                client_width = static_cast<unsigned>(client.right - client.left);
                client_height = static_cast<unsigned>(client.bottom - client.top);
            }
        }
        swap_chain->Release();
    }

    const bool needs_full_target =
        viewport.X != 0 || viewport.Y != 0 ||
        viewport.Width != target_desc.Width ||
        viewport.Height != target_desc.Height;
    HRESULT set_result = D3D_OK;
    if (needs_full_target) {
        D3DVIEWPORT9 full_target{};
        full_target.Width = target_desc.Width;
        full_target.Height = target_desc.Height;
        full_target.MinZ = 0.0f;
        full_target.MaxZ = 1.0f;
        set_result = device->SetViewport(&full_target);
    }

    if (InterlockedIncrement(&g_viewport_log_count) == 1) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Bink viewport: %lux%lu at %lu,%lu; target=%lux%lu; "
            "backbuffer=%lux%lu; client=%ux%u; full-target=%s (hr=%08lx)",
            static_cast<unsigned long>(viewport.Width),
            static_cast<unsigned long>(viewport.Height),
            static_cast<unsigned long>(viewport.X),
            static_cast<unsigned long>(viewport.Y),
            static_cast<unsigned long>(target_desc.Width),
            static_cast<unsigned long>(target_desc.Height),
            static_cast<unsigned long>(present.BackBufferWidth),
            static_cast<unsigned long>(present.BackBufferHeight),
            client_width, client_height,
            needs_full_target ? "applied" : "already full",
            static_cast<unsigned long>(set_result));
        log_message(message);
    }
}

void __attribute__((regparm(1))) bink_render_hook(void* bink_intro) noexcept {
    prepare_bink_viewport(bink_intro);
    g_bink_render(bink_intro);
}

int WINAPI armada_bink_set_dibits_scaled(
    HDC device_context, int destination_x, int destination_y,
    DWORD width, DWORD height, int source_x, int source_y,
    UINT first_scan, UINT scan_count, const void* pixels,
    const BITMAPINFO* bitmap_info, UINT color_use) noexcept {
    if (!device_context || !pixels || !bitmap_info || width == 0 ||
        height == 0 || first_scan != 0 || scan_count != height) {
        return SetDIBitsToDevice(
            device_context, destination_x, destination_y, width, height,
            source_x, source_y, first_scan, scan_count, pixels, bitmap_info,
            color_use);
    }

    RECT target{};
    HWND window = WindowFromDC(device_context);
    bool have_target = window && GetClientRect(window, &target) &&
        target.right > target.left && target.bottom > target.top;
    if (!have_target) {
        have_target = GetClipBox(device_context, &target) != ERROR &&
            target.right > target.left && target.bottom > target.top;
    }
    if (!have_target) {
        return SetDIBitsToDevice(
            device_context, destination_x, destination_y, width, height,
            source_x, source_y, first_scan, scan_count, pixels, bitmap_info,
            color_use);
    }

    const unsigned target_width =
        static_cast<unsigned>(target.right - target.left);
    const unsigned target_height =
        static_cast<unsigned>(target.bottom - target.top);
    unsigned fitted_width = target_width;
    unsigned fitted_height = static_cast<unsigned>(
        (static_cast<unsigned long long>(height) * target_width) / width);
    if (fitted_height > target_height) {
        fitted_height = target_height;
        fitted_width = static_cast<unsigned>(
            (static_cast<unsigned long long>(width) * target_height) / height);
    }
    if (fitted_width == 0 || fitted_height == 0) {
        return 0;
    }

    const int fitted_x = target.left +
        static_cast<int>((target_width - fitted_width) / 2);
    const int fitted_y = target.top +
        static_cast<int>((target_height - fitted_height) / 2);
    if (InterlockedIncrement(&g_gdi_log_count) == 1) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "Armada Bink GDI scaling: source=%lux%lu; client=%ux%u; "
            "destination=%ux%u at %d,%d",
            static_cast<unsigned long>(width),
            static_cast<unsigned long>(height), target_width, target_height,
            fitted_width, fitted_height, fitted_x, fitted_y);
        log_message(message);
    }

    return StretchDIBits(
        device_context, fitted_x, fitted_y,
        static_cast<int>(fitted_width), static_cast<int>(fitted_height),
        source_x, source_y, static_cast<int>(width), static_cast<int>(height),
        pixels, bitmap_info, color_use, SRCCOPY);
}

bool is_sane_viewport(const ArmadaFloatRect& rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
        std::isfinite(rectangle.width) &&
        std::isfinite(rectangle.height) && rectangle.width >= 320.0f &&
        rectangle.height >= 200.0f && rectangle.width <= 8192.0f &&
        rectangle.height <= 8192.0f;
}

void __cdecl armada_bink_texture_render_scaled(
    const ArmadaFloatRect* requested_rectangle) noexcept {
    if (!g_armada_bink_texture_render || !requested_rectangle) return;

    const ArmadaFloatRect* render_rectangle = requested_rectangle;
    ArmadaFloatRect full_viewport{};
    auto* viewport_owner = *reinterpret_cast<std::uint8_t**>(
        at(g_armada, kArmadaViewportOwnerGlobalRva));
    if (viewport_owner) {
        std::memcpy(&full_viewport,
                    viewport_owner + kArmadaViewportRectOffset,
                    sizeof(full_viewport));
        if (is_sane_viewport(full_viewport)) {
            render_rectangle = &full_viewport;
        }
    }

    if (InterlockedIncrement(&g_texture_log_count) == 1) {
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Armada Bink texture scaling: requested=%.0fx%.0f at %.0f,%.0f; "
            "viewport=%.0fx%.0f at %.0f,%.0f; applied=%s",
            requested_rectangle->width, requested_rectangle->height,
            requested_rectangle->x, requested_rectangle->y,
            full_viewport.width, full_viewport.height,
            full_viewport.x, full_viewport.y,
            render_rectangle == &full_viewport ? "yes" : "no");
        log_message(message);
    }

    g_armada_bink_texture_render(render_rectangle);
}

}  // namespace

bool initialize_bink_video_scaling(const A2FO_ModuleApi* api,
                                   HMODULE armada) noexcept {
    if (!api || !armada || !api->patch_call || !api->log) {
        if (api && api->log) {
            api->log(kModuleName,
                     "Bink viewport scaling unavailable in this core");
        }
        return false;
    }
    HMODULE fleet_ops =
        api->fleetops_module ? static_cast<HMODULE>(api->fleetops_module())
                             : nullptr;
    if (!fleet_ops) return false;

    void* render = at(fleet_ops, kBinkRenderRva);
    void* call_site = at(fleet_ops, kBinkRenderCallRva);
    void* armada_gdi_call = at(armada, kArmadaBinkSetDibitsCallRva);
    void* armada_texture_render =
        at(armada, kArmadaBinkTextureRenderRva);
    void* armada_texture_call1 =
        at(armada, kArmadaBinkTextureRenderCallRva1);
    void* armada_texture_call2 =
        at(armada, kArmadaBinkTextureRenderCallRva2);
    if (std::memcmp(render, kExpectedBinkRender,
                    sizeof(kExpectedBinkRender)) != 0 ||
        std::memcmp(call_site, kExpectedBinkRenderCall,
                    sizeof(kExpectedBinkRenderCall)) != 0 ||
        std::memcmp(armada_gdi_call, kExpectedArmadaBinkSetDibitsCall,
                    sizeof(kExpectedArmadaBinkSetDibitsCall)) != 0 ||
        std::memcmp(armada_texture_render,
                    kExpectedArmadaBinkTextureRender,
                    sizeof(kExpectedArmadaBinkTextureRender)) != 0 ||
        std::memcmp(armada_texture_call1,
                    kExpectedArmadaBinkTextureRenderCall1,
                    sizeof(kExpectedArmadaBinkTextureRenderCall1)) != 0 ||
        std::memcmp(armada_texture_call2,
                    kExpectedArmadaBinkTextureRenderCall2,
                    sizeof(kExpectedArmadaBinkTextureRenderCall2)) != 0) {
        api->log(kModuleName,
                 "Bink renderer signatures mismatch; viewport fix disabled");
        return false;
    }

    g_api = api;
    g_armada = armada;
    g_bink_render = reinterpret_cast<BinkRenderFunction>(render);
    g_armada_bink_texture_render =
        reinterpret_cast<ArmadaBinkTextureRenderFunction>(
            armada_texture_render);
    if (!api->patch_call(call_site,
                         reinterpret_cast<void*>(&bink_render_hook),
                         kExpectedBinkRenderCall,
                         sizeof(kExpectedBinkRenderCall)) ||
        !api->patch_call(
            armada_gdi_call,
            reinterpret_cast<void*>(&armada_bink_set_dibits_scaled),
            kExpectedArmadaBinkSetDibitsCall,
            sizeof(kExpectedArmadaBinkSetDibitsCall)) ||
        !api->patch_call(
            armada_texture_call1,
            reinterpret_cast<void*>(&armada_bink_texture_render_scaled),
            kExpectedArmadaBinkTextureRenderCall1,
            sizeof(kExpectedArmadaBinkTextureRenderCall1)) ||
        !api->patch_call(
            armada_texture_call2,
            reinterpret_cast<void*>(&armada_bink_texture_render_scaled),
            kExpectedArmadaBinkTextureRenderCall2,
            sizeof(kExpectedArmadaBinkTextureRenderCall2))) {
        api->log(kModuleName,
                 "Bink renderer call patch failed; viewport fix disabled");
        return false;
    }

    api->log(kModuleName,
             "Fleet Ops D3D and Armada GDI/texture Bink scaling enabled");
    return true;
}

}  // namespace a2fo
