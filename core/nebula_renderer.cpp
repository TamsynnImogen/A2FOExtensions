/*
 * Fleet Operations DX8 per-pixel lighting derived from armadaNebulaPatch.
 *
 * The original project used a second startup proxy, MinHook, and an unsafe
 * middle-of-function jump which skipped Fleet Operations' final alpha draws.
 * This core subsystem uses the A2FOExtensions checked hook primitives. Its alpha
 * transition detour disables the pixel shader and then resumes the displaced
 * Fleet Operations instructions through a gateway.
 *
 * Upstream: https://github.com/FNSOIDATHQ/armadaNebulaPatch
 * Copyright (c) 2024 dev gao, used under the MIT License.
 */

#include "nebula_renderer.hpp"
#include "nebula_emissive.hpp"
#include "hook.hpp"

#include <windows.h>
#include <d3d8.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#undef INTERFACE
#define INTERFACE ID3DXBuffer
DECLARE_INTERFACE_(ID3DXBuffer, IUnknown) {
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** object) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD_(void*, GetBufferPointer)(THIS) PURE;
    STDMETHOD_(DWORD, GetBufferSize)(THIS) PURE;
};
#undef INTERFACE

extern "C" {
std::uintptr_t __cdecl a2fo_nebula_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
void a2fo_nebula_set_pixel_shader_hook();
void a2fo_nebula_alpha_hook();
void a2fo_nebula_standard_pre_hook();
void a2fo_nebula_standard_post_hook();
void a2fo_nebula_nonvb_pre_hook();
void a2fo_nebula_nonvb_post_hook();
void a2fo_nebula_workspace_dx8_draw_hook();
void a2fo_nebula_frame_bloom_hook();
void a2fo_nebula_device_reset_hook();
void a2fo_nebula_dot3_draw_hook();

// Read by the assembly continuation after its helper has restored all
// registers and flags from the original Fleet Operations render function.
void* g_a2fo_nebula_alpha_gateway = nullptr;
void* g_a2fo_nebula_standard_pre_gateway = nullptr;
void* g_a2fo_nebula_standard_post_gateway = nullptr;
void* g_a2fo_nebula_nonvb_pre_gateway = nullptr;
void* g_a2fo_nebula_nonvb_post_gateway = nullptr;
void* g_a2fo_nebula_workspace_dx8_draw_gateway = nullptr;
void* g_a2fo_nebula_frame_bloom_gateway = nullptr;
void* g_a2fo_nebula_device_reset_gateway = nullptr;
void* g_a2fo_nebula_dot3_draw_gateway = nullptr;
}

namespace {

using AssembleShaderFromFile = HRESULT (WINAPI*)(
    LPCSTR file_name, DWORD flags, ID3DXBuffer** constants,
    ID3DXBuffer** compiled_shader, ID3DXBuffer** compilation_errors);
using MatrixInverse = D3DMATRIX* (WINAPI*)(
    D3DMATRIX* output, float* determinant, const D3DMATRIX* input);
using MatrixMultiply = D3DMATRIX* (WINAPI*)(
    D3DMATRIX* output, const D3DMATRIX* left, const D3DMATRIX* right);
using MatrixTranspose = D3DMATRIX* (WINAPI*)(
    D3DMATRIX* output, const D3DMATRIX* input);
using CreateTextureFromFileEx = HRESULT (WINAPI*)(
    IDirect3DDevice8* device, LPCSTR file_name, UINT width, UINT height,
    UINT mip_levels, DWORD usage, D3DFORMAT format, D3DPOOL pool,
    DWORD filter, DWORD mip_filter, D3DCOLOR colour_key,
    void* source_info, PALETTEENTRY* palette,
    IDirect3DTexture8** texture);
using SaveSurfaceToFile = HRESULT (WINAPI*)(
    LPCSTR file_name, int file_format, IDirect3DSurface8* surface,
    const PALETTEENTRY* palette, const RECT* source_rectangle);
using CompileDot3Mesh = void* (__cdecl*)(const void* mesh);

constexpr const char* kModuleName = "A2FONebulaRenderer";
constexpr std::uint32_t kArmadaTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kArmadaImageSize = 0x00403999;
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

// Supported Armada II 1.1 / Fleet Operations Roots renderer locations.
constexpr std::uintptr_t kVertexShaderPathRva = 0x0032b580;
constexpr std::uintptr_t kCompileDot3MeshRva = 0x00226e50;
constexpr std::uintptr_t kGetShaderHandleRva = 0x0022c270;
constexpr std::uintptr_t kGetShaderHandleRouteRva = 0x00210bb4;
constexpr std::uintptr_t kCameraToNodeRva = 0x003ad5e0;
constexpr std::uintptr_t kGraphicsEnginePointerRva = 0x003ad508;
constexpr std::uintptr_t kAlphaTransitionRva = 0x001e67d1;
// Armada2.map section offsets 0x23d4ea/0x23d5aa plus the PE .text RVA.
constexpr std::uintptr_t kStandardMeshPreDrawRva = 0x0023e4ea;
constexpr std::uintptr_t kStandardMeshPostDrawRva = 0x0023e5aa;
// ST3D_Mesh::RenderInternalNonVB builds clipped/inside face batches, calls
// Workspace::Flush (+8) to calculate their counts, applies a texture-material
// pass, and only then calls the selected workspace's Submit (+0x18) to issue
// the Direct3D draw. Scope the emissive stage around that final Submit call.
constexpr std::uintptr_t kNonVbPreDrawRva = 0x00232585;
constexpr std::uintptr_t kNonVbPostDrawRva = 0x0023258c;
// ST3D_WorkspaceDirectX8::Submit's actual DrawIndexedPrimitive call. Capturing
// here guarantees its rolling GPU buffers and start index still describe the
// exact batch being submitted, rather than reconstructing them afterward.
constexpr std::uintptr_t kWorkspaceDx8DrawRva = 0x00248bfb;
// ST3D_DeviceDirectX8::EndScene, immediately before the native device
// EndScene call. At this point all world and interface rendering for the
// frame has completed, but Direct3D still accepts our final composite draw.
constexpr std::uintptr_t kFrameBloomRva = 0x00223ee9;
// Device-lost recovery immediately before IDirect3DDevice8::Reset. Default-pool
// bloom targets must be released or Direct3D rejects the native reset.
constexpr std::uintptr_t kDeviceResetRva = 0x00223ce4;
// ST3D_Dot3_MeshVB::Render's primary indexed draw. The call is intercepted
// before execution while its vertex/index streams and custom shader are live;
// the helper restores them before the original call is replayed.
constexpr std::uintptr_t kDot3DrawRva = 0x002279af;
constexpr std::size_t kCurrentDeviceIndexOffset = 0xc0;
constexpr std::size_t kDeviceWrapperTableOffset = 0xcc;
constexpr std::size_t kStormDeviceOffset = 0x90;
constexpr std::uint32_t kMaximumStormDeviceCount = 2;
constexpr std::size_t kRequiredD3D8VtableEntries = 93;
constexpr std::size_t kStorm3DEngineOffset = 0x44;
constexpr std::size_t kStorm3DTextureRegistryOffset =
    (5u * 3u + 0x12u) * sizeof(void*);
constexpr std::size_t kStorm3DTextureNameOffset = 0x08;
constexpr std::size_t kStorm3DTextureDeviceArrayOffset = 0x40;
constexpr std::size_t kStorm3DDeviceTextureNativeOffset = 0x04;
constexpr std::size_t kMaximumStorm3DRegistryEntries = 65536;
constexpr std::size_t kMaximumStorm3DTextureName = 1024;
constexpr UINT kD3dxDefault = 0xffffffffu;
constexpr std::size_t kCraftClassOffset = 0x40;
constexpr std::size_t kCraftSubsystemsOffset = 0x1e0;
constexpr std::size_t kSubsystemRecordSize = 0x30;
constexpr std::size_t kSensorsRecord = 0;
constexpr std::size_t kEnginesRecord = 1;
constexpr std::size_t kWeaponsRecord = 2;
constexpr std::size_t kLifeSupportRecord = 3;
constexpr std::size_t kShieldsRecord = 4;
constexpr std::size_t kCraftRenderStackCapacity = 16;
constexpr std::size_t kStandardStateStackCapacity = 16;
constexpr std::uintptr_t kWorkspaceDirectX8VtableRva = 0x002bcd8c;
constexpr std::uintptr_t kWorkspaceDirectX8NonVbVtableRva = 0x002bcdb4;
// Fixed-point DX8 render targets cannot retain HDR energy above 1.0. Repeating
// the colour-preserving additive outer-halo pass gives the broad blur the
// sprite-like intensity that a floating-point post-process would otherwise
// provide, without replacing Fleet Operations' D3D8 device.
constexpr std::size_t kHaloCompositePasses = 3;

constexpr std::array<std::uint8_t, 32> kExpectedVertexShaderPath{
    0x73, 0x68, 0x61, 0x64, 0x65, 0x72, 0x73, 0x5c,
    0x64, 0x6f, 0x74, 0x33, 0x5f, 0x64, 0x69, 0x72,
    0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x61, 0x6c,
    0x2e, 0x6e, 0x76, 0x76, 0x00, 0x00, 0x00, 0x00};

constexpr std::array<std::uint8_t, 32> make_vertex_shader_path() {
    std::array<std::uint8_t, 32> result{};
    constexpr char path[] = "shaders\\dx8\\vertex\\vs.nvv";
    for (std::size_t index = 0; index < sizeof(path); ++index) {
        result[index] = static_cast<std::uint8_t>(path[index]);
    }
    return result;
}
constexpr auto kVertexShaderPath = make_vertex_shader_path();

constexpr std::array<std::uint8_t, 10> kExpectedCompileDot3Mesh{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0xcb, 0xba, 0x6a, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedGetShaderHandle{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::array<std::uint8_t, 13> kExpectedAlphaTransition{
    0x8b, 0x40, 0x0c,
    0xf7, 0x80, 0x2c, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 8> kExpectedStandardMeshPreDraw{
    0x8b, 0x16, 0x8b, 0x0d, 0x08, 0xd5, 0x7a, 0x00};
constexpr std::array<std::uint8_t, 5> kExpectedStandardMeshPostDraw{
    0x5f, 0x5e, 0x85, 0xc0, 0x5b};
constexpr std::array<std::uint8_t, 7> kExpectedNonVbPreDraw{
    0x8b, 0x03, 0x8b, 0xcb, 0xff, 0x50, 0x18};
constexpr std::array<std::uint8_t, 6> kExpectedNonVbPostDraw{
    0x8b, 0x45, 0xfc, 0x46, 0x3b, 0xf0};
constexpr std::array<std::uint8_t, 6> kExpectedWorkspaceDx8Draw{
    0xff, 0x92, 0x1c, 0x01, 0x00, 0x00};
constexpr std::array<std::uint8_t, 9> kExpectedFrameBloom{
    0x56, 0x8b, 0x06, 0xff, 0x90, 0x8c, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 7> kExpectedDeviceReset{
    0x8b, 0x10, 0x51, 0x50, 0xff, 0x52, 0x38};
constexpr std::array<std::uint8_t, 6> kExpectedDot3Draw{
    0xff, 0x92, 0x1c, 0x01, 0x00, 0x00};

a2fo::NebulaRendererLog g_log = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
HMODULE g_d3dx = nullptr;
bool g_d3dx_owned = false;
volatile LONG g_runtime_enabled = 0;
volatile LONG g_activation_state = 0;
volatile LONG g_logged_set_failure = 0;
volatile LONG g_logged_compile_failure = 0;
volatile LONG g_logged_create_failure = 0;
volatile LONG g_logged_device_failure = 0;
a2fo::InlineHook g_compile_hook{};
a2fo::InlineHook g_alpha_hook{};
a2fo::InlineHook g_standard_pre_hook{};
a2fo::InlineHook g_standard_post_hook{};
a2fo::InlineHook g_nonvb_pre_hook{};
a2fo::InlineHook g_nonvb_post_hook{};
a2fo::InlineHook g_workspace_dx8_draw_hook{};
a2fo::InlineHook g_frame_bloom_hook{};
a2fo::InlineHook g_device_reset_hook{};
a2fo::InlineHook g_dot3_draw_hook{};
bool g_hooks_ready = false;
void* g_compile_original = nullptr;
void* g_get_shader_handle_original = nullptr;
IDirect3DDevice8* g_device = nullptr;
DWORD g_pixel_shader = 0;
ID3DXBuffer* g_compiled_pixel_shader = nullptr;
AssembleShaderFromFile g_assemble_shader = nullptr;
MatrixInverse g_matrix_inverse = nullptr;
MatrixMultiply g_matrix_multiply = nullptr;
MatrixTranspose g_matrix_transpose = nullptr;
CreateTextureFromFileEx g_create_texture_from_file = nullptr;
SaveSurfaceToFile g_save_surface_to_file = nullptr;
std::string g_pixel_shader_path;
std::string g_root_directory;

struct EmissiveMaterialPolicy {
    // Empty only for the legacy unnumbered ODF commands, which intentionally
    // remain a wildcard applying to every material on that class.
    std::string diffuse_key;
    std::array<std::string, a2fo::nebula::kEmissiveSystemCount> paths{};
    std::array<std::vector<std::uint32_t>,
               a2fo::nebula::kEmissiveSystemCount> pixels{};
    std::array<bool, a2fo::nebula::kEmissiveSystemCount> source_attempted{};
    std::array<bool, a2fo::nebula::kEmissiveSystemCount> source_loaded{};
    std::unordered_map<std::uint8_t, IDirect3DTexture8*> composites;
    UINT width = 0;
    UINT height = 0;
    std::uint8_t path_mask = 0;
};

struct EmissiveClassPolicy {
    std::vector<std::unique_ptr<EmissiveMaterialPolicy>> materials;
};

std::unordered_map<void*, std::unique_ptr<EmissiveClassPolicy>>
    g_emissive_policies;
IDirect3DTexture8* g_black_emissive_texture = nullptr;
std::unordered_map<IDirect3DBaseTexture8*, std::string>
    g_diffuse_texture_keys;
thread_local std::array<void*, kCraftRenderStackCapacity>
    g_craft_render_stack{};
thread_local std::size_t g_craft_render_depth = 0;
thread_local std::size_t g_craft_render_overflow = 0;
volatile LONG g_logged_emissive_loader_unavailable = 0;
volatile LONG g_logged_emissive_create_failure = 0;
volatile LONG g_logged_standard_emissive = 0;
volatile LONG g_logged_standard_hook_reached = 0;
volatile LONG g_logged_nonvb_hook_reached = 0;
volatile LONG g_logged_registered_craft_context = 0;
volatile LONG g_logged_fixed_function_without_context = 0;
volatile LONG g_logged_diffuse_lookup_failure = 0;
volatile LONG g_logged_indexed_diffuse_binding = 0;

struct StandardTextureStageState {
    IDirect3DDevice8* device = nullptr;
    IDirect3DBaseTexture8* texture = nullptr;
    IDirect3DTexture8* emissive = nullptr;
    DWORD colour_operation = D3DTOP_DISABLE;
    DWORD colour_argument1 = D3DTA_TEXTURE;
    DWORD colour_argument2 = D3DTA_CURRENT;
    DWORD alpha_operation = D3DTOP_DISABLE;
    DWORD alpha_argument1 = D3DTA_TEXTURE;
    DWORD alpha_argument2 = D3DTA_CURRENT;
    DWORD coordinate_index = 1;
    DWORD minimum_filter = D3DTEXF_POINT;
    DWORD magnification_filter = D3DTEXF_POINT;
    DWORD mipmap_filter = D3DTEXF_NONE;
    bool active = false;
};

thread_local std::array<StandardTextureStageState,
                        kStandardStateStackCapacity>
    g_standard_state_stack{};
thread_local std::size_t g_standard_state_depth = 0;
thread_local std::size_t g_standard_state_overflow = 0;
thread_local bool g_emissive_mask_draw_active = false;
volatile LONG g_logged_emissive_mask_draw = 0;
volatile LONG g_logged_emissive_bloom_composite = 0;
volatile LONG g_logged_emissive_bloom_failure = 0;
volatile LONG g_logged_nonvb_mask_workspace = 0;
volatile LONG g_logged_workspace_context_fallback = 0;

void release_bloom_resources() noexcept;

void log_line(const char* message) noexcept {
    if (g_log && message) {
        g_log(std::string("[") + kModuleName + "] " + message);
    }
}

void log_hresult(const char* operation, HRESULT result) noexcept {
    char message[192]{};
    std::snprintf(message, sizeof(message), "%s failed (HRESULT 0x%08lx)",
                  operation,
                  static_cast<unsigned long>(
                      static_cast<std::uint32_t>(result)));
    log_line(message);
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (size > static_cast<std::uintptr_t>(-1) - start) return false;
    const std::uintptr_t requested_end = start + size;
    std::uintptr_t current = start;
    while (current < requested_end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(current), &info,
                         sizeof(info)) != sizeof(info) ||
            info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
            (info.Protect & PAGE_NOACCESS) != 0) {
            return false;
        }
        const auto region_start =
            reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (info.RegionSize > static_cast<std::uintptr_t>(-1) - region_start) {
            return false;
        }
        const std::uintptr_t region_end = region_start + info.RegionSize;
        if (current < region_start || region_end <= current) return false;
        current = std::min(region_end, requested_end);
    }
    return true;
}

template <typename T>
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void* at(HMODULE module, std::uintptr_t rva) noexcept;

void release_emissive_gpu_cache(EmissiveMaterialPolicy& policy) noexcept {
    for (auto& entry : policy.composites) {
        if (entry.second) entry.second->Release();
    }
    policy.composites.clear();
}

void release_all_emissive_gpu_caches() noexcept {
    if (g_black_emissive_texture) {
        g_black_emissive_texture->Release();
        g_black_emissive_texture = nullptr;
    }
    for (auto& entry : g_emissive_policies) {
        if (!entry.second) continue;
        for (auto& material : entry.second->materials) {
            if (material) release_emissive_gpu_cache(*material);
        }
    }
    g_diffuse_texture_keys.clear();
}

IDirect3DTexture8* ensure_black_emissive_texture(
    IDirect3DDevice8* device) noexcept {
    if (g_black_emissive_texture) return g_black_emissive_texture;
    if (!device) return nullptr;
    IDirect3DTexture8* texture = nullptr;
    HRESULT result = device->CreateTexture(
        1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture);
    if (FAILED(result) || !texture) return nullptr;
    D3DLOCKED_RECT locked{};
    result = texture->LockRect(0, &locked, nullptr, 0);
    if (FAILED(result) || !locked.pBits) {
        texture->Release();
        return nullptr;
    }
    *static_cast<std::uint32_t*>(locked.pBits) = 0xff000000u;
    texture->UnlockRect(0);
    g_black_emissive_texture = texture;
    return texture;
}

bool load_emissive_source(EmissiveMaterialPolicy& policy,
                          std::size_t index,
                          IDirect3DDevice8* device) noexcept {
    if (index >= policy.paths.size() || policy.paths[index].empty()) {
        return false;
    }
    if (policy.source_attempted[index]) return policy.source_loaded[index];
    policy.source_attempted[index] = true;
    if (!g_create_texture_from_file) {
        if (InterlockedCompareExchange(
                &g_logged_emissive_loader_unavailable, 1, 0) == 0) {
            log_line("D3DX texture loader is unavailable; ODF emissive maps "
                     "are inactive");
        }
        return false;
    }

    IDirect3DTexture8* source = nullptr;
    const UINT width = policy.width ? policy.width : kD3dxDefault;
    const UINT height = policy.height ? policy.height : kD3dxDefault;
    const HRESULT load_result = g_create_texture_from_file(
        device, policy.paths[index].c_str(), width, height, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, kD3dxDefault, kD3dxDefault,
        0, nullptr, nullptr, &source);
    if (FAILED(load_result) || !source) {
        char message[1400]{};
        std::snprintf(message, sizeof(message),
                      "Could not load emissive texture '%s' (HRESULT 0x%08lx)",
                      policy.paths[index].c_str(),
                      static_cast<unsigned long>(
                          static_cast<std::uint32_t>(load_result)));
        log_line(message);
        if (source) source->Release();
        return false;
    }

    D3DSURFACE_DESC description{};
    HRESULT result = source->GetLevelDesc(0, &description);
    if (FAILED(result) || description.Width == 0 || description.Height == 0) {
        source->Release();
        return false;
    }
    if (policy.width == 0 || policy.height == 0) {
        policy.width = description.Width;
        policy.height = description.Height;
    }

    D3DLOCKED_RECT locked{};
    result = source->LockRect(0, &locked, nullptr, D3DLOCK_READONLY);
    if (FAILED(result) || !locked.pBits) {
        source->Release();
        return false;
    }
    try {
        auto& pixels = policy.pixels[index];
        pixels.resize(static_cast<std::size_t>(policy.width) * policy.height);
        const auto* source_bytes = static_cast<const std::uint8_t*>(
            locked.pBits);
        for (UINT y = 0; y < policy.height; ++y) {
            std::memcpy(
                pixels.data() + static_cast<std::size_t>(y) * policy.width,
                source_bytes + static_cast<std::size_t>(y) * locked.Pitch,
                static_cast<std::size_t>(policy.width) *
                    sizeof(std::uint32_t));
        }
        // Keep the authored RGB values sharp. The old UV-space expansion was
        // useful while the renderer had no framebuffer halo, but feeding that
        // already-expanded image into the real blur washed neighbouring
        // colours together and drove bright channels towards white.
        policy.source_loaded[index] = true;
    } catch (...) {
        policy.pixels[index].clear();
    }
    source->UnlockRect(0);
    source->Release();
    return policy.source_loaded[index];
}

bool upload_emissive_mip_chain(
    IDirect3DTexture8* texture,
    const std::vector<std::uint32_t>& base_pixels,
    UINT base_width, UINT base_height) noexcept {
    if (!texture || base_pixels.size() !=
            static_cast<std::size_t>(base_width) * base_height) {
        return false;
    }
    try {
        std::vector<std::uint32_t> pixels = base_pixels;
        UINT width = base_width;
        UINT height = base_height;
        const DWORD levels = texture->GetLevelCount();
        for (DWORD level = 0; level < levels; ++level) {
            D3DLOCKED_RECT locked{};
            const HRESULT result = texture->LockRect(
                level, &locked, nullptr, 0);
            if (FAILED(result) || !locked.pBits) return false;
            for (UINT y = 0; y < height; ++y) {
                std::memcpy(
                    static_cast<std::uint8_t*>(locked.pBits) +
                        static_cast<std::size_t>(y) * locked.Pitch,
                    pixels.data() + static_cast<std::size_t>(y) * width,
                    static_cast<std::size_t>(width) *
                        sizeof(std::uint32_t));
            }
            texture->UnlockRect(level);
            if (level + 1 >= levels) break;

            const UINT next_width = std::max<UINT>(1, width / 2);
            const UINT next_height = std::max<UINT>(1, height / 2);
            std::vector<std::uint32_t> next(
                static_cast<std::size_t>(next_width) * next_height);
            for (UINT y = 0; y < next_height; ++y) {
                for (UINT x = 0; x < next_width; ++x) {
                    std::array<std::uint32_t, 4> samples{};
                    std::size_t sample_index = 0;
                    for (UINT dy = 0; dy < 2; ++dy) {
                        for (UINT dx = 0; dx < 2; ++dx) {
                            const UINT source_x = std::min(
                                width - 1, x * 2 + dx);
                            const UINT source_y = std::min(
                                height - 1, y * 2 + dy);
                            samples[sample_index++] = pixels[
                                static_cast<std::size_t>(source_y) * width +
                                source_x];
                        }
                    }
                    std::uint32_t averaged = 0;
                    for (unsigned shift = 0; shift < 32; shift += 8) {
                        const std::uint32_t channel =
                            ((samples[0] >> shift) & 0xffu) +
                            ((samples[1] >> shift) & 0xffu) +
                            ((samples[2] >> shift) & 0xffu) +
                            ((samples[3] >> shift) & 0xffu);
                        averaged |= ((channel + 2u) / 4u) << shift;
                    }
                    next[static_cast<std::size_t>(y) * next_width + x] =
                        averaged;
                }
            }
            pixels.swap(next);
            width = next_width;
            height = next_height;
        }
        return true;
    } catch (...) {
        return false;
    }
}

IDirect3DTexture8* build_emissive_composite(
    EmissiveMaterialPolicy& policy, std::uint8_t requested_mask,
    IDirect3DDevice8* device) noexcept {
    const auto existing = policy.composites.find(requested_mask);
    if (existing != policy.composites.end()) return existing->second;

    std::uint8_t loaded_mask = requested_mask & policy.path_mask;
    for (std::size_t index = 0; index < policy.paths.size(); ++index) {
        const std::uint8_t bit = static_cast<std::uint8_t>(1u << index);
        if ((loaded_mask & bit) != 0 &&
            !load_emissive_source(policy, index, device)) {
            loaded_mask = static_cast<std::uint8_t>(loaded_mask & ~bit);
        }
    }
    if (loaded_mask == 0 || policy.width == 0 || policy.height == 0) {
        return ensure_black_emissive_texture(device);
    }

    std::vector<std::uint32_t> base_pixels;
    try {
        base_pixels.resize(
            static_cast<std::size_t>(policy.width) * policy.height);
        for (UINT y = 0; y < policy.height; ++y) {
            for (UINT x = 0; x < policy.width; ++x) {
                const std::size_t pixel_index =
                    static_cast<std::size_t>(y) * policy.width + x;
                std::array<std::uint32_t,
                           a2fo::nebula::kEmissiveSystemCount> sources{};
                for (std::size_t source_index = 0;
                     source_index < sources.size(); ++source_index) {
                    if ((loaded_mask & (1u << source_index)) != 0) {
                        sources[source_index] =
                            policy.pixels[source_index][pixel_index];
                    }
                }
                base_pixels[pixel_index] =
                    a2fo::nebula::combine_emissive_pixel(
                        sources, loaded_mask);
            }
        }
    } catch (...) {
        return ensure_black_emissive_texture(device);
    }

    IDirect3DTexture8* texture = nullptr;
    HRESULT result = device->CreateTexture(
        policy.width, policy.height, 0, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, &texture);
    if (FAILED(result) || !texture) {
        if (InterlockedCompareExchange(
                &g_logged_emissive_create_failure, 1, 0) == 0) {
            log_hresult("Create emissive composite texture", result);
        }
        return ensure_black_emissive_texture(device);
    }

    if (!upload_emissive_mip_chain(
            texture, base_pixels, policy.width, policy.height)) {
        texture->Release();
        return ensure_black_emissive_texture(device);
    }
    policy.composites.emplace(requested_mask, texture);
    return texture;
}

a2fo::nebula::SubsystemLightState subsystem_emissive_state(
    const void* subsystem_records, std::size_t record_index) noexcept {
    if (!subsystem_records) {
        return a2fo::nebula::SubsystemLightState::operational;
    }
    const auto* record = static_cast<const std::uint8_t*>(
        subsystem_records) + record_index * kSubsystemRecordSize;
    // Fail open if an unfamiliar Craft layout is encountered. A false light
    // is safer than dereferencing invalid state or blacking out every craft.
    if (!readable_range(record, kSubsystemRecordSize)) {
        return a2fo::nebula::SubsystemLightState::operational;
    }

    const bool operational = record[0] != 0;
    const bool forced_disabled = record[1] != 0;
    std::int32_t maximum_hitpoints = 0;
    double current_hitpoints = 0.0;
    float disable_time = 0.0f;
    std::memcpy(&maximum_hitpoints, record + 0x04,
                sizeof(maximum_hitpoints));
    std::memcpy(&current_hitpoints, record + 0x18,
                sizeof(current_hitpoints));
    std::memcpy(&disable_time, record + 0x28,
                sizeof(disable_time));
    return a2fo::nebula::classify_subsystem_light(
        operational, forced_disabled, maximum_hitpoints,
        current_hitpoints, disable_time);
}

bool disabled_emissive_visible(const void* craft,
                               std::size_t record_index) noexcept {
    // Hold each irregular decision long enough to read as an electrical
    // flicker rather than frame-rate noise. Craft and subsystem bits keep
    // simultaneous failures from blinking in lockstep.
    std::uint32_t value = GetTickCount() / 90u;
    value ^= static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(craft) >> 4);
    value ^= static_cast<std::uint32_t>(record_index) * 0x9e3779b9u;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return (value & 7u) < 5u;
}

std::uint8_t current_emissive_mask(
    const EmissiveMaterialPolicy& policy, const void* craft) noexcept {
    if (!craft) return 0;
    const void* subsystems = read_at<const void*>(
        craft, kCraftSubsystemsOffset, nullptr);
    std::uint8_t mask = policy.path_mask;
    const auto apply_system = [&](std::size_t record_index,
                                  std::uint8_t channel_bits) {
        const a2fo::nebula::SubsystemLightState state =
            subsystem_emissive_state(
                subsystems, record_index);
        if (state == a2fo::nebula::SubsystemLightState::destroyed ||
            (state == a2fo::nebula::SubsystemLightState::disabled &&
             !disabled_emissive_visible(craft, record_index))) {
            mask = static_cast<std::uint8_t>(mask & ~channel_bits);
        }
    };
    apply_system(
        kEnginesRecord,
        static_cast<std::uint8_t>(
            (1u << a2fo::nebula::warp) |
            (1u << a2fo::nebula::impulse)));
    apply_system(
        kShieldsRecord,
        static_cast<std::uint8_t>(1u << a2fo::nebula::shields));
    apply_system(
        kLifeSupportRecord,
        static_cast<std::uint8_t>(1u << a2fo::nebula::life_support));
    apply_system(
        kSensorsRecord,
        static_cast<std::uint8_t>(1u << a2fo::nebula::sensors));
    apply_system(
        kWeaponsRecord,
        static_cast<std::uint8_t>(1u << a2fo::nebula::weapons));
    return mask;
}

void* current_render_craft() noexcept {
    return g_craft_render_depth != 0
        ? g_craft_render_stack[g_craft_render_depth - 1] : nullptr;
}

bool bounded_texture_name(const char* source, std::string* name) noexcept {
    if (!source || !name) return false;
    try {
        name->clear();
        for (std::size_t index = 0;
             index < kMaximumStorm3DTextureName; ++index) {
            if (!readable_range(source + index, 1)) return false;
            const char character = source[index];
            if (character == '\0') return !name->empty();
            name->push_back(character);
        }
    } catch (...) {
        name->clear();
    }
    return false;
}

std::string current_diffuse_texture_key(
    IDirect3DDevice8* device) noexcept {
    if (!device || !g_armada) return {};
    IDirect3DBaseTexture8* active_texture = nullptr;
    if (FAILED(device->GetTexture(0, &active_texture)) || !active_texture) {
        return {};
    }

    std::string key;
    try {
        const auto cached = g_diffuse_texture_keys.find(active_texture);
        if (cached != g_diffuse_texture_keys.end()) {
            key = cached->second;
        } else {
            void* renderer = read_at<void*>(
                at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
            const std::uint32_t device_index = read_at<std::uint32_t>(
                renderer, kCurrentDeviceIndexOffset,
                kMaximumStormDeviceCount);
            void* engine = read_at<void*>(
                renderer, kStorm3DEngineOffset, nullptr);
            void* sentinel = read_at<void*>(
                engine, kStorm3DTextureRegistryOffset, nullptr);
            void* node = read_at<void*>(sentinel, 0, nullptr);
            for (std::size_t count = 0;
                 node && node != sentinel &&
                 count < kMaximumStorm3DRegistryEntries; ++count) {
                void* texture = read_at<void*>(node, 0x08, nullptr);
                void* device_texture = device_index < kMaximumStormDeviceCount
                    ? read_at<void*>(
                          texture,
                          kStorm3DTextureDeviceArrayOffset +
                              device_index * sizeof(void*),
                          nullptr)
                    : nullptr;
                IDirect3DBaseTexture8* native_texture = read_at<
                    IDirect3DBaseTexture8*>(
                        device_texture,
                        kStorm3DDeviceTextureNativeOffset, nullptr);
                if (native_texture == active_texture) {
                    const char* source_name = read_at<const char*>(
                        texture, kStorm3DTextureNameOffset, nullptr);
                    std::string native_name;
                    if (bounded_texture_name(source_name, &native_name)) {
                        key = a2fo::nebula::normalize_texture_key(native_name);
                        if (!key.empty()) {
                            g_diffuse_texture_keys.emplace(
                                active_texture, key);
                        }
                    }
                    break;
                }
                node = read_at<void*>(node, 0, nullptr);
            }
        }
    } catch (...) {
        key.clear();
    }
    active_texture->Release();
    return key;
}

EmissiveMaterialPolicy* select_current_emissive_material(
    EmissiveClassPolicy& policy, IDirect3DDevice8* device) noexcept {
    EmissiveMaterialPolicy* wildcard = nullptr;
    bool has_indexed_material = false;
    for (auto& material : policy.materials) {
        if (!material) continue;
        if (material->diffuse_key.empty()) {
            wildcard = material.get();
        } else {
            has_indexed_material = true;
        }
    }
    if (!has_indexed_material) return wildcard;

    const std::string diffuse_key = current_diffuse_texture_key(device);
    if (!diffuse_key.empty()) {
        for (auto& material : policy.materials) {
            if (material && material->diffuse_key == diffuse_key) {
                if (InterlockedCompareExchange(
                        &g_logged_indexed_diffuse_binding, 1, 0) == 0) {
                    char message[320]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Indexed emissive material matched live diffuse '%s'",
                        diffuse_key.c_str());
                    log_line(message);
                }
                return material.get();
            }
        }
    } else if (InterlockedCompareExchange(
                   &g_logged_diffuse_lookup_failure, 1, 0) == 0) {
        log_line("Could not identify a live Storm3D diffuse texture; indexed "
                 "emissive maps skipped for that material");
    }
    return wildcard;
}

bool select_current_emissive_texture(
    IDirect3DDevice8* device, IDirect3DTexture8** selected) noexcept {
    if (selected) *selected = nullptr;
    void* craft = current_render_craft();
    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    const auto found = g_emissive_policies.find(object_class);
    if (found == g_emissive_policies.end() || !found->second) {
        return false;
    }
    EmissiveMaterialPolicy* material = select_current_emissive_material(
        *found->second, device);
    if (!material) return false;
    const std::uint8_t mask = current_emissive_mask(*material, craft);
    if (mask == 0) return false;
    IDirect3DTexture8* texture = build_emissive_composite(
        *material, mask, device);
    if (!texture || texture == g_black_emissive_texture) return false;
    if (selected) *selected = texture;
    return true;
}

IDirect3DTexture8* current_emissive_texture(
    IDirect3DDevice8* device) noexcept {
    IDirect3DTexture8* selected = nullptr;
    return select_current_emissive_texture(device, &selected)
        ? selected : ensure_black_emissive_texture(device);
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

bool validate_module(HMODULE module, std::uint32_t timestamp,
                     std::uint32_t image_size, const char* name) noexcept {
    if (!module) {
        char message[128]{};
        std::snprintf(message, sizeof(message), "%s is unavailable",
                      name ? name : "Renderer image");
        log_line(message);
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (!readable_range(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        log_line("Renderer module has an invalid DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != timestamp ||
        nt->OptionalHeader.SizeOfImage != image_size) {
        char message[224]{};
        std::snprintf(message, sizeof(message),
                      "%s is not the supported Fleet Operations build",
                      name ? name : "Renderer image");
        log_line(message);
        return false;
    }
    return true;
}

template <typename Function>
void* function_address(Function function) noexcept {
    static_assert(sizeof(function) == sizeof(void*),
                  "32-bit function and object pointers must match");
    void* address = nullptr;
    std::memcpy(&address, &function, sizeof(address));
    return address;
}

template <typename Function>
Function function_from_address(void* address) noexcept {
    static_assert(sizeof(Function) == sizeof(void*),
                  "32-bit function and object pointers must match");
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

template <typename Function>
Function imported_function(HMODULE module, const char* name) noexcept {
    FARPROC procedure = module ? GetProcAddress(module, name) : nullptr;
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(procedure),
                  "32-bit function pointers must match FARPROC");
    std::memcpy(&function, &procedure, sizeof(function));
    return function;
}

bool command_line_requests_dx9() {
    const wchar_t* command_line = GetCommandLineW();
    if (!command_line) return false;
    std::wstring token;
    bool quoted = false;
    const auto inspect = [&token]() {
        if (token.size() != 5) return false;
        std::wstring normalized = token;
        for (wchar_t& character : normalized) {
            if (character >= L'A' && character <= L'Z') {
                character = static_cast<wchar_t>(character - L'A' + L'a');
            }
        }
        return normalized == L"/d3d9" || normalized == L"-d3d9";
    };
    for (const wchar_t* cursor = command_line;; ++cursor) {
        const wchar_t character = *cursor;
        if (character == L'\"') {
            quoted = !quoted;
        } else if (character == L'\0' ||
                   (!quoted && (character == L' ' || character == L'\t'))) {
            if (!token.empty() && inspect()) return true;
            token.clear();
            if (character == L'\0') break;
        } else {
            token.push_back(character);
        }
    }
    return false;
}

std::string join_path(const char* left, const char* right) {
    std::string result = left ? left : "";
    if (!result.empty() && result.back() != '\\' && result.back() != '/') {
        result.push_back('\\');
    }
    result += right ? right : "";
    return result;
}

bool shader_assets_available() {
    const char* root = g_root_directory.c_str();
    g_pixel_shader_path = join_path(root, "Shaders\\dx8\\pixel\\ps.nvv");
    const std::string vertex_path =
        join_path(root, "Shaders\\dx8\\vertex\\vs.nvv");
    if (GetFileAttributesA(vertex_path.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesA(g_pixel_shader_path.c_str()) ==
            INVALID_FILE_ATTRIBUTES) {
        log_line("Required DX8 custom shader assets are missing from "
                 "Data\\Shaders; Nebula renderer inactive");
        return false;
    }
    return true;
}

bool controller_is_installed() {
    const std::string controller = join_path(
        g_root_directory.c_str(), "modules\\A2FONebulaRenderer.dll");
    return GetFileAttributesA(controller.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool load_d3dx() noexcept {
    g_d3dx = GetModuleHandleA("D3DX81ab.dll");
    if (!g_d3dx) {
        g_d3dx = LoadLibraryA("D3DX81ab.dll");
        g_d3dx_owned = g_d3dx != nullptr;
    }
    if (!g_d3dx) {
        log_line("D3DX81ab.dll is unavailable; Nebula renderer inactive");
        return false;
    }
    g_assemble_shader = imported_function<AssembleShaderFromFile>(
        g_d3dx, "D3DXAssembleShaderFromFileA");
    g_matrix_inverse = imported_function<MatrixInverse>(
        g_d3dx, "D3DXMatrixInverse");
    g_matrix_multiply = imported_function<MatrixMultiply>(
        g_d3dx, "D3DXMatrixMultiply");
    g_matrix_transpose = imported_function<MatrixTranspose>(
        g_d3dx, "D3DXMatrixTranspose");
    g_create_texture_from_file = imported_function<CreateTextureFromFileEx>(
        g_d3dx, "D3DXCreateTextureFromFileExA");
    g_save_surface_to_file = imported_function<SaveSurfaceToFile>(
        g_d3dx, "D3DXSaveSurfaceToFileA");
    if (!g_assemble_shader || !g_matrix_inverse || !g_matrix_multiply ||
        !g_matrix_transpose) {
        log_line("D3DX81ab.dll lacks required DX8 shader/matrix exports");
        return false;
    }
    if (!g_create_texture_from_file) {
        log_line("D3DX81ab.dll lacks D3DXCreateTextureFromFileExA; the "
                 "lighting shader remains available but ODF emissive maps "
                 "will be inactive");
    }
    return true;
}

bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t* expected,
                       std::size_t size) noexcept {
    const void* site = at(module, rva);
    return readable_range(site, size) &&
        std::memcmp(site, expected, size) == 0;
}

bool preflight_signatures() noexcept {
    if (!signature_matches(g_armada, kVertexShaderPathRva,
                           kExpectedVertexShaderPath.data(),
                           kExpectedVertexShaderPath.size()) ||
        !signature_matches(g_armada, kCompileDot3MeshRva,
                           kExpectedCompileDot3Mesh.data(),
                           kExpectedCompileDot3Mesh.size()) ||
        !signature_matches(g_armada, kGetShaderHandleRva,
                           kExpectedGetShaderHandle.data(),
                           kExpectedGetShaderHandle.size()) ||
        !signature_matches(g_armada, kStandardMeshPreDrawRva,
                           kExpectedStandardMeshPreDraw.data(),
                           kExpectedStandardMeshPreDraw.size()) ||
        !signature_matches(g_armada, kStandardMeshPostDrawRva,
                           kExpectedStandardMeshPostDraw.data(),
                           kExpectedStandardMeshPostDraw.size()) ||
        !signature_matches(g_armada, kNonVbPreDrawRva,
                           kExpectedNonVbPreDraw.data(),
                           kExpectedNonVbPreDraw.size()) ||
        !signature_matches(g_armada, kNonVbPostDrawRva,
                           kExpectedNonVbPostDraw.data(),
                           kExpectedNonVbPostDraw.size()) ||
        !signature_matches(g_armada, kWorkspaceDx8DrawRva,
                           kExpectedWorkspaceDx8Draw.data(),
                           kExpectedWorkspaceDx8Draw.size()) ||
        !signature_matches(g_armada, kFrameBloomRva,
                           kExpectedFrameBloom.data(),
                           kExpectedFrameBloom.size()) ||
        !signature_matches(g_armada, kDeviceResetRva,
                           kExpectedDeviceReset.data(),
                           kExpectedDeviceReset.size()) ||
        !signature_matches(g_armada, kDot3DrawRva,
                           kExpectedDot3Draw.data(),
                           kExpectedDot3Draw.size()) ||
        !signature_matches(g_fleet_ops, kAlphaTransitionRva,
                           kExpectedAlphaTransition.data(),
                           kExpectedAlphaTransition.size())) {
        log_line("A Nebula renderer code/data signature differs from the "
                 "supported binaries; runtime inactive");
        return false;
    }

    void* expected = at(g_armada, kGetShaderHandleRva);
    void* route = at(g_fleet_ops, kGetShaderHandleRouteRva);
    void* current = nullptr;
    if (!readable_range(route, sizeof(current))) {
        log_line("Fleet Ops DOT3 shader-handle route is unreadable");
        return false;
    }
    std::memcpy(&current, route, sizeof(current));
    if (current != expected) {
        log_line("Fleet Ops DOT3 shader-handle route is already modified; "
                 "Nebula renderer inactive");
        return false;
    }
    g_get_shader_handle_original = expected;
    return true;
}

bool install_get_shader_route() noexcept {
    void* route = at(g_fleet_ops, kGetShaderHandleRouteRva);
    void* expected = g_get_shader_handle_original;
    void* replacement = function_address(&a2fo_nebula_set_pixel_shader_hook);
    if (!a2fo::patch_bytes(
            route,
            reinterpret_cast<const std::uint8_t*>(&replacement),
            reinterpret_cast<const std::uint8_t*>(&expected),
            sizeof(replacement))) {
        log_line("Could not route Fleet Ops' DOT3 shader-handle call; "
                 "native rendering retained");
        return false;
    }
    log_line("Fleet Ops DOT3 shader-handle route installed");
    return true;
}

bool compile_pixel_shader() noexcept {
    if (g_compiled_pixel_shader) return true;
    if (!g_assemble_shader || g_pixel_shader_path.empty()) return false;
    ID3DXBuffer* compiled = nullptr;
    ID3DXBuffer* errors = nullptr;
    const HRESULT result = g_assemble_shader(
        g_pixel_shader_path.c_str(), 0, nullptr, &compiled, &errors);
    if (FAILED(result) || !compiled || !compiled->GetBufferPointer()) {
        if (InterlockedCompareExchange(&g_logged_compile_failure, 1, 0) == 0) {
            log_hresult("D3DXAssembleShaderFromFileA", result);
            if (errors && errors->GetBufferPointer()) {
                char message[384]{};
                const char* text = static_cast<const char*>(
                    errors->GetBufferPointer());
                const int text_size = static_cast<int>(
                    errors->GetBufferSize() < 320
                        ? errors->GetBufferSize() : 320);
                std::snprintf(message, sizeof(message),
                              "Pixel shader assembler: %.*s", text_size,
                              text);
                log_line(message);
            }
        }
        if (compiled) compiled->Release();
        if (errors) errors->Release();
        return false;
    }
    if (errors) errors->Release();
    g_compiled_pixel_shader = compiled;
    log_line("DX8 Nebula pixel shader assembled");
    return true;
}

IDirect3DDevice8* resolve_live_device(void* renderer) noexcept {
    if (!renderer) return nullptr;

    const auto* bytes = static_cast<const std::uint8_t*>(renderer);
    if (!readable_range(bytes + kCurrentDeviceIndexOffset,
                        sizeof(std::uint32_t))) {
        return nullptr;
    }
    std::uint32_t device_index = 0;
    std::memcpy(&device_index, bytes + kCurrentDeviceIndexOffset,
                sizeof(device_index));
    if (device_index >= kMaximumStormDeviceCount) return nullptr;

    void* wrapper = nullptr;
    const void* wrapper_slot =
        bytes + kDeviceWrapperTableOffset + device_index * sizeof(wrapper);
    if (!readable_range(wrapper_slot, sizeof(wrapper))) return nullptr;
    std::memcpy(&wrapper, wrapper_slot, sizeof(wrapper));
    if (!wrapper || !readable_range(
            static_cast<const std::uint8_t*>(wrapper) + kStormDeviceOffset,
            sizeof(IDirect3DDevice8*))) {
        return nullptr;
    }

    IDirect3DDevice8* device = nullptr;
    std::memcpy(&device,
                static_cast<const std::uint8_t*>(wrapper) +
                    kStormDeviceOffset,
                sizeof(device));
    if (!device || !readable_range(device, sizeof(void*))) return nullptr;

    void* vtable = nullptr;
    std::memcpy(&vtable, device, sizeof(vtable));
    return readable_range(vtable,
                          sizeof(void*) * kRequiredD3D8VtableEntries)
        ? device : nullptr;
}

void adopt_live_device(IDirect3DDevice8* device) noexcept {
    if (!device || g_device == device) return;
    // Release our references from the previous renderer before switching.
    // Decoded source pixels remain valid and are recombined lazily.
    if (g_device) {
        release_bloom_resources();
        g_device->SetTexture(1, nullptr);
        if (g_pixel_shader != 0) {
            g_device->DeletePixelShader(g_pixel_shader);
        }
    }
    g_pixel_shader = 0;
    release_all_emissive_gpu_caches();
    g_device = device;
}

void standard_emissive_pre_draw() noexcept {
    if (g_standard_state_depth >= g_standard_state_stack.size()) {
        ++g_standard_state_overflow;
        return;
    }
    StandardTextureStageState& state =
        g_standard_state_stack[g_standard_state_depth++];
    state = StandardTextureStageState{};
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0) return;
    if (!current_render_craft()) {
        if (InterlockedCompareExchange(
                &g_logged_fixed_function_without_context, 1, 0) == 0) {
            log_line("Fixed-function draw had no enclosing Craft render context");
        }
        return;
    }

    void* renderer = read_at<void*>(
        at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
    IDirect3DDevice8* device = resolve_live_device(renderer);
    if (!device) return;
    adopt_live_device(device);

    IDirect3DTexture8* emissive = nullptr;
    if (!select_current_emissive_texture(device, &emissive) || !emissive) {
        return;
    }

    state.device = device;
    if (FAILED(device->GetTexture(1, &state.texture)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_COLOROP, &state.colour_operation)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_COLORARG1, &state.colour_argument1)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_COLORARG2, &state.colour_argument2)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_ALPHAOP, &state.alpha_operation)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_ALPHAARG1, &state.alpha_argument1)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_ALPHAARG2, &state.alpha_argument2)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_TEXCOORDINDEX, &state.coordinate_index)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_MINFILTER, &state.minimum_filter)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_MAGFILTER, &state.magnification_filter)) ||
        FAILED(device->GetTextureStageState(
            1, D3DTSS_MIPFILTER, &state.mipmap_filter))) {
        if (state.texture) {
            state.texture->Release();
            state.texture = nullptr;
        }
        return;
    }

    device->SetPixelShader(0);
    device->SetTexture(1, emissive);
    device->SetTextureStageState(
        1, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        1, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_CURRENT));
    device->SetTextureStageState(
        1, D3DTSS_COLORARG2, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_ADD));
    // Preserve the material's stage-0 alpha. Emissive RGB never makes an
    // otherwise transparent texel opaque.
    device->SetTextureStageState(
        1, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_CURRENT));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    state.emissive = emissive;
    state.active = true;
    if (InterlockedCompareExchange(
            &g_logged_standard_emissive, 1, 0) == 0) {
        log_line("Fixed-function emissive texture stage activated");
    }
}

struct BloomTarget {
    IDirect3DTexture8* texture = nullptr;
    IDirect3DSurface8* surface = nullptr;
    UINT texture_width = 0;
    UINT texture_height = 0;
    UINT active_width = 0;
    UINT active_height = 0;
};

struct BloomResources {
    IDirect3DDevice8* device = nullptr;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    UINT frame_width = 0;
    UINT frame_height = 0;
    BloomTarget mask{};
    BloomTarget blur_a{};
    BloomTarget blur_b{};
    bool mask_prepared = false;
    bool mask_dirty = false;
    UINT diagnostic_draw_count = 0;
    std::uint64_t diagnostic_triangle_count = 0;
};

BloomResources g_bloom{};
std::uint32_t g_diagnostic_frame_count = 0;
std::uint32_t g_diagnostic_capture_count = 0;

void release_bloom_target(BloomTarget& target) noexcept {
    if (target.surface) target.surface->Release();
    if (target.texture) target.texture->Release();
    target = BloomTarget{};
}

void release_bloom_resources() noexcept {
    release_bloom_target(g_bloom.mask);
    release_bloom_target(g_bloom.blur_a);
    release_bloom_target(g_bloom.blur_b);
    g_bloom = BloomResources{};
}

UINT next_power_of_two(UINT value) noexcept {
    if (value <= 1) return 1;
    if (value > 0x80000000u) return 0;
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}

bool create_bloom_target(IDirect3DDevice8* device, UINT active_width,
                         UINT active_height, D3DFORMAT format,
                         BloomTarget& target) noexcept {
    if (!device || active_width == 0 || active_height == 0) return false;
    const UINT power_width = next_power_of_two(active_width);
    const UINT power_height = next_power_of_two(active_height);
    const std::array<std::array<UINT, 2>, 2> attempts{{
        {{active_width, active_height}}, {{power_width, power_height}}}};
    for (const auto& dimensions : attempts) {
        if (dimensions[0] == 0 || dimensions[1] == 0 ||
            (target.texture && dimensions[0] == target.texture_width &&
             dimensions[1] == target.texture_height)) {
            continue;
        }
        IDirect3DTexture8* texture = nullptr;
        HRESULT result = device->CreateTexture(
            dimensions[0], dimensions[1], 1, D3DUSAGE_RENDERTARGET,
            format, D3DPOOL_DEFAULT, &texture);
        if (FAILED(result) || !texture) continue;
        IDirect3DSurface8* surface = nullptr;
        result = texture->GetSurfaceLevel(0, &surface);
        if (FAILED(result) || !surface) {
            texture->Release();
            continue;
        }
        target.texture = texture;
        target.surface = surface;
        target.texture_width = dimensions[0];
        target.texture_height = dimensions[1];
        target.active_width = active_width;
        target.active_height = active_height;
        return true;
    }
    return false;
}

bool ensure_bloom_resources(IDirect3DDevice8* device) noexcept {
    if (!device) return false;
    IDirect3DSurface8* render_target = nullptr;
    HRESULT result = device->GetRenderTarget(&render_target);
    if (FAILED(result) || !render_target) return false;
    D3DSURFACE_DESC description{};
    result = render_target->GetDesc(&description);
    render_target->Release();
    if (FAILED(result) || description.Width == 0 || description.Height == 0) {
        return false;
    }
    if (g_bloom.device == device &&
        g_bloom.frame_width == description.Width &&
        g_bloom.frame_height == description.Height &&
        g_bloom.format == description.Format && g_bloom.mask.texture &&
        g_bloom.blur_a.texture && g_bloom.blur_b.texture) {
        return true;
    }

    release_bloom_resources();
    g_bloom.device = device;
    g_bloom.format = description.Format;
    g_bloom.frame_width = description.Width;
    g_bloom.frame_height = description.Height;
    // Half resolution keeps one-pixel nacelle strips and windows stable while
    // moving. Quarter-resolution sampling visibly flickered when those thin
    // emitters crossed its four-screen-pixel grid.
    const UINT blur_width = std::max<UINT>(1, (description.Width + 1) / 2);
    const UINT blur_height = std::max<UINT>(1, (description.Height + 1) / 2);
    if (!create_bloom_target(device, description.Width, description.Height,
                             description.Format, g_bloom.mask) ||
        !create_bloom_target(device, blur_width, blur_height,
                             description.Format, g_bloom.blur_a) ||
        !create_bloom_target(device, blur_width, blur_height,
                             description.Format, g_bloom.blur_b)) {
        release_bloom_resources();
        if (InterlockedCompareExchange(
                &g_logged_emissive_bloom_failure, 1, 0) == 0) {
            log_line("Could not create native D3D8 bloom render targets");
        }
        return false;
    }
    return true;
}

struct SavedDeviceDrawState {
    DWORD state_block = 0;
    IDirect3DSurface8* render_target = nullptr;
    IDirect3DSurface8* depth_stencil = nullptr;
    D3DVIEWPORT8 viewport{};
    bool valid = false;
};

bool save_device_draw_state(IDirect3DDevice8* device,
                            SavedDeviceDrawState& saved) noexcept {
    if (!device) return false;
    HRESULT result = device->CreateStateBlock(D3DSBT_ALL,
                                               &saved.state_block);
    if (FAILED(result) || saved.state_block == 0 ||
        FAILED(device->GetRenderTarget(&saved.render_target)) ||
        !saved.render_target || FAILED(device->GetViewport(&saved.viewport))) {
        if (saved.render_target) saved.render_target->Release();
        if (saved.depth_stencil) saved.depth_stencil->Release();
        if (saved.state_block) device->DeleteStateBlock(saved.state_block);
        saved = SavedDeviceDrawState{};
        return false;
    }
    // A depth surface is optional (menus and some off-screen passes have none).
    if (FAILED(device->GetDepthStencilSurface(&saved.depth_stencil))) {
        saved.depth_stencil = nullptr;
    }
    saved.valid = true;
    return true;
}

void restore_device_draw_state(IDirect3DDevice8* device,
                               SavedDeviceDrawState& saved) noexcept {
    if (!device || !saved.valid) return;
    device->SetTexture(0, nullptr);
    device->SetTexture(1, nullptr);
    device->SetRenderTarget(saved.render_target, saved.depth_stencil);
    device->ApplyStateBlock(saved.state_block);
    device->SetViewport(&saved.viewport);
    device->DeleteStateBlock(saved.state_block);
    if (saved.depth_stencil) saved.depth_stencil->Release();
    if (saved.render_target) saved.render_target->Release();
    saved = SavedDeviceDrawState{};
}

void set_target_viewport(IDirect3DDevice8* device,
                         const BloomTarget& target) noexcept {
    D3DVIEWPORT8 viewport{};
    viewport.Width = target.active_width;
    viewport.Height = target.active_height;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    device->SetViewport(&viewport);
}

void configure_emissive_mask_state(IDirect3DDevice8* device,
                                   IDirect3DTexture8* emissive) noexcept {
    device->SetPixelShader(0);
    device->SetTexture(0, nullptr);
    device->SetTexture(1, emissive);
    device->SetTexture(2, nullptr);
    // Keep stage 0 enabled as a neutral white input and sample the isolated
    // authored emissive map on stage 1, matching the visible emissive layer.
    device->SetTextureStageState(
        0, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_TFACTOR));
    device->SetTextureStageState(
        0, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_TFACTOR));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetRenderState(
        D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(255, 255, 255, 255));

    device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        1, D3DTSS_TEXTURETRANSFORMFLAGS,
        static_cast<DWORD>(D3DTTFF_DISABLE));
    device->SetTextureStageState(
        1, D3DTSS_ADDRESSU, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        1, D3DTSS_ADDRESSV, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        1, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(
        2, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetTextureStageState(
        2, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(
        D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_DESTBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_ZENABLE, static_cast<DWORD>(D3DZB_FALSE));
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_WRAP1, 0);
}

template <typename Draw>
void accumulate_emissive_mask(StandardTextureStageState& state,
                              UINT diagnostic_primitive_count,
                              Draw&& draw) noexcept {
    if (!state.active || !state.device || !state.emissive ||
        g_emissive_mask_draw_active ||
        !ensure_bloom_resources(state.device)) {
        return;
    }
    SavedDeviceDrawState saved{};
    if (!save_device_draw_state(state.device, saved)) return;

    IDirect3DDevice8* device = state.device;
    g_emissive_mask_draw_active = true;
    device->SetTexture(0, nullptr);
    device->SetTexture(1, nullptr);
    HRESULT result = device->SetRenderTarget(g_bloom.mask.surface, nullptr);
    if (SUCCEEDED(result)) {
        set_target_viewport(device, g_bloom.mask);
        if (!g_bloom.mask_prepared) {
            result = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                                   D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (SUCCEEDED(result)) g_bloom.mask_prepared = true;
        }
    }
    if (SUCCEEDED(result)) {
        configure_emissive_mask_state(device, state.emissive);
        result = draw(device);
        if (SUCCEEDED(result)) {
            g_bloom.mask_dirty = true;
            ++g_bloom.diagnostic_draw_count;
            g_bloom.diagnostic_triangle_count += diagnostic_primitive_count;
            if (InterlockedCompareExchange(
                    &g_logged_emissive_mask_draw, 1, 0) == 0) {
                log_line("ODF emissive geometry accumulated for native bloom");
            }
        }
    }
    restore_device_draw_state(device, saved);
    g_emissive_mask_draw_active = false;
    if (FAILED(result) && InterlockedCompareExchange(
            &g_logged_emissive_bloom_failure, 1, 0) == 0) {
        log_hresult("Accumulate native emissive bloom mask", result);
    }
}

void standard_emissive_mask_draw(const void* mesh_stream) noexcept {
    if (!mesh_stream || g_standard_state_depth == 0) return;
    const UINT vertex_count = read_at<UINT>(mesh_stream, 0x10, 0);
    const UINT primitive_count = read_at<UINT>(mesh_stream, 0x14, 0);
    if (vertex_count == 0 || primitive_count == 0) return;
    StandardTextureStageState& state =
        g_standard_state_stack[g_standard_state_depth - 1];
    accumulate_emissive_mask(
        state, primitive_count,
        [vertex_count, primitive_count](IDirect3DDevice8* device) {
            return device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST, 0, vertex_count, 0, primitive_count);
        });
}

void workspace_dx8_emissive_mask_draw(IDirect3DDevice8* live_device,
                                      void* workspace,
                                      UINT native_vertex_count,
                                      UINT native_start_index,
                                      UINT native_primitive_count) noexcept {
    if (!live_device || !workspace ||
        read_at<const void*>(workspace, 0, nullptr) !=
            at(g_armada, kWorkspaceDirectX8VtableRva)) {
        return;
    }

    StandardTextureStageState local_state{};
    StandardTextureStageState* selected_state = nullptr;
    if (g_standard_state_depth != 0) {
        StandardTextureStageState& scoped_state =
            g_standard_state_stack[g_standard_state_depth - 1];
        if (scoped_state.active && scoped_state.device == live_device) {
            selected_state = &scoped_state;
        }
    }
    if (!selected_state) {
        IDirect3DTexture8* emissive = nullptr;
        if (!select_current_emissive_texture(live_device, &emissive) ||
            !emissive) {
            return;
        }
        local_state.device = live_device;
        local_state.emissive = emissive;
        local_state.active = true;
        selected_state = &local_state;
        if (InterlockedCompareExchange(
                &g_logged_workspace_context_fallback, 1, 0) == 0) {
            log_line("Emissive mask captured a craft workspace outside the "
                     "narrow material-pass hook");
        }
    }

    // This helper runs at Submit's actual DrawIndexedPrimitive instruction.
    // The exact rolling vertex/index buffers, base vertex, and FVF are selected
    // already and cannot be replaced by a later workspace submission yet.
    const UINT vertex_format = read_at<UINT>(workspace, 0x98, 0);
    const UINT stride = read_at<UINT>(workspace, 0x9c, 0);
    const std::uintptr_t vertex_begin = read_at<std::uintptr_t>(
        workspace, 0x04, 0);
    const std::uintptr_t vertex_end = read_at<std::uintptr_t>(
        workspace, 0x30, 0);
    const std::uintptr_t index_begin = read_at<std::uintptr_t>(
        workspace, 0x40, 0);
    const std::uintptr_t index_end = read_at<std::uintptr_t>(
        workspace, 0x2c, 0);
    const UINT workspace_start_index = read_at<UINT>(workspace, 0xbc, 0);
    if (stride == 0 || vertex_end < vertex_begin || index_end < index_begin) {
        return;
    }
    const std::uintptr_t vertex_bytes = vertex_end - vertex_begin;
    const std::uintptr_t index_bytes = index_end - index_begin;
    if (vertex_bytes % stride != 0 || index_bytes % 6u != 0) return;
    const std::uintptr_t vertex_count_wide = vertex_bytes / stride;
    const std::uintptr_t primitive_count_wide = index_bytes / 6u;
    if (vertex_count_wide == 0 || vertex_count_wide > 3000000u ||
        primitive_count_wide == 0 || primitive_count_wide > 1000000u ||
        (stride != 28u && stride != 32u) ||
        (vertex_format != 0x144u && vertex_format != 0x1c4u)) {
        return;
    }
    const UINT derived_vertex_count = static_cast<UINT>(vertex_count_wide);
    const UINT derived_primitive_count =
        static_cast<UINT>(primitive_count_wide);
    if (native_vertex_count == 0 || native_vertex_count > 3000000u ||
        native_primitive_count == 0 || native_primitive_count > 1000000u) {
        return;
    }
    if (InterlockedCompareExchange(
            &g_logged_nonvb_mask_workspace, 1, 0) == 0) {
        char message[480]{};
        std::snprintf(
            message, sizeof(message),
            "Active classic DX8 workspace at native draw: native "
            "triangles=%lu, vertices=%lu, startIndex=%lu; derived "
            "triangles=%lu, vertices=%lu, startIndex=%lu; stride=%lu, "
            "FVF=0x%08lx",
            static_cast<unsigned long>(native_primitive_count),
            static_cast<unsigned long>(native_vertex_count),
            static_cast<unsigned long>(native_start_index),
            static_cast<unsigned long>(derived_primitive_count),
            static_cast<unsigned long>(derived_vertex_count),
            static_cast<unsigned long>(workspace_start_index),
            static_cast<unsigned long>(stride),
            static_cast<unsigned long>(vertex_format));
        log_line(message);
    }
    accumulate_emissive_mask(
        *selected_state, native_primitive_count,
        [native_vertex_count, native_start_index,
                native_primitive_count](IDirect3DDevice8* device) {
            return device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST, 0, native_vertex_count,
                native_start_index, native_primitive_count);
        });
}

void nonvb_emissive_mask_draw(void* workspace) noexcept {
    if (!workspace || g_standard_state_depth == 0) return;
    StandardTextureStageState& state =
        g_standard_state_stack[g_standard_state_depth - 1];
    if (!state.active) return;

    const void* vtable = read_at<const void*>(workspace, 0, nullptr);
    if (vtable == at(g_armada, kWorkspaceDirectX8VtableRva)) {
        // The exact in-Submit draw hook owns this GPU-buffer subclass. The
        // outer post hook remains responsible only for restoring texture state.
        return;
    }
    if (vtable != at(g_armada, kWorkspaceDirectX8NonVbVtableRva)) {
        return;
    }

    const UINT primitive_count = read_at<UINT>(workspace, 0x9c, 0);
    const UINT stride = read_at<UINT>(workspace, 0x98, 0);
    const DWORD vertex_format = read_at<DWORD>(workspace, 0xa4, 0);
    const void* vertex_data = read_at<const void*>(workspace, 0xa8, nullptr);
    const void* index_data = read_at<const void*>(workspace, 0xb4, nullptr);
    if (InterlockedCompareExchange(
            &g_logged_nonvb_mask_workspace, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Active legacy emissive workspace: triangles=%lu, stride=%lu, "
            "FVF=0x%08lx, vertexData=%p, indexData=%p",
            static_cast<unsigned long>(primitive_count),
            static_cast<unsigned long>(stride),
            static_cast<unsigned long>(vertex_format), vertex_data,
            index_data);
        log_line(message);
    }
    if (primitive_count == 0 || primitive_count > 1000000u ||
        (stride != 28u && stride != 32u) ||
        (vertex_format != 0x144u && vertex_format != 0x1c4u) ||
        !vertex_data || !index_data) {
        return;
    }
    accumulate_emissive_mask(
        state, primitive_count,
        [primitive_count, stride, vertex_data,
                index_data](IDirect3DDevice8* device) {
            return device->DrawIndexedPrimitiveUP(
                D3DPT_TRIANGLELIST, 0, primitive_count * 3u,
                primitive_count, index_data, D3DFMT_INDEX16,
                vertex_data, stride);
        });
}

void dot3_emissive_mask_draw(IDirect3DDevice8* device,
                             const void* mesh_stream,
                             UINT primitive_count) noexcept {
    if (!device || !mesh_stream || primitive_count == 0 ||
        primitive_count > 1000000u ||
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        !current_render_craft()) {
        return;
    }
    const UINT vertex_count = read_at<UINT>(mesh_stream, 0x10, 0);
    if (vertex_count == 0) return;
    adopt_live_device(device);
    IDirect3DTexture8* emissive = nullptr;
    if (!select_current_emissive_texture(device, &emissive) || !emissive) {
        return;
    }
    StandardTextureStageState state{};
    state.device = device;
    state.emissive = emissive;
    state.active = true;
    accumulate_emissive_mask(
        state, primitive_count,
        [vertex_count, primitive_count](IDirect3DDevice8* live_device) {
            return live_device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST, 0, vertex_count, 0, primitive_count);
        });
}

struct QuadVertex {
    float x;
    float y;
    float z;
    float rhw;
    D3DCOLOR colour;
    float u;
    float v;
};

void configure_quad_state(IDirect3DDevice8* device) noexcept {
    device->SetPixelShader(0);
    device->SetVertexShader(
        D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSU, static_cast<DWORD>(D3DTADDRESS_CLAMP));
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSV, static_cast<DWORD>(D3DTADDRESS_CLAMP));
    device->SetTextureStageState(
        0, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_NONE));
    device->SetTextureStageState(
        0, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_COLORARG2, static_cast<DWORD>(D3DTA_TFACTOR));
    device->SetTextureStageState(
        0, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_MODULATE));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAARG2, static_cast<DWORD>(D3DTA_TFACTOR));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_MODULATE));
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ZENABLE, static_cast<DWORD>(D3DZB_FALSE));
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(
        D3DRS_CULLMODE, static_cast<DWORD>(D3DCULL_NONE));
}

HRESULT draw_textured_quad(IDirect3DDevice8* device,
                           const D3DVIEWPORT8& viewport,
                           float u0, float v0, float u1, float v1,
                           std::uint8_t factor) noexcept {
    const float left = static_cast<float>(viewport.X) - 0.5f;
    const float top = static_cast<float>(viewport.Y) - 0.5f;
    const float right = left + static_cast<float>(viewport.Width);
    const float bottom = top + static_cast<float>(viewport.Height);
    const std::array<QuadVertex, 4> vertices{{
        {left, top, 0.0f, 1.0f, 0xffffffffu, u0, v0},
        {right, top, 0.0f, 1.0f, 0xffffffffu, u1, v0},
        {left, bottom, 0.0f, 1.0f, 0xffffffffu, u0, v1},
        {right, bottom, 0.0f, 1.0f, 0xffffffffu, u1, v1},
    }};
    device->SetRenderState(
        D3DRS_TEXTUREFACTOR,
        D3DCOLOR_ARGB(255, factor, factor, factor));
    return device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(QuadVertex));
}

bool draw_blur_pass(IDirect3DDevice8* device,
                    const BloomTarget& source, BloomTarget& destination,
                    bool horizontal) noexcept {
    device->SetTexture(0, nullptr);
    HRESULT result = device->SetRenderTarget(destination.surface, nullptr);
    if (FAILED(result)) return false;
    set_target_viewport(device, destination);
    result = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                           D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(result)) return false;
    device->SetTexture(0, source.texture);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(
        D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_DESTBLEND, static_cast<DWORD>(D3DBLEND_ONE));

    // Linear filtering combines adjacent samples from a dense 13-tap
    // Gaussian into seven weighted taps. Two horizontal/vertical iterations
    // build a smooth halo without the checkerboard produced by the former
    // widely spaced kernel. The weights total 256 and apply equally to R/G/B.
    constexpr std::array<float, 7> offsets{{
        -5.4f, -3.3823529f, -1.4461538f, 0.0f,
         1.4461538f, 3.3823529f, 5.4f}};
    constexpr std::array<std::uint8_t, 7> weights{{
        10, 34, 65, 38, 65, 34, 10}};
    const float maximum_u = static_cast<float>(source.active_width) /
        static_cast<float>(source.texture_width);
    const float maximum_v = static_cast<float>(source.active_height) /
        static_cast<float>(source.texture_height);
    D3DVIEWPORT8 viewport{};
    viewport.Width = destination.active_width;
    viewport.Height = destination.active_height;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const float u_offset = horizontal
            ? offsets[index] / static_cast<float>(source.texture_width)
            : 0.0f;
        const float v_offset = horizontal
            ? 0.0f
            : offsets[index] / static_cast<float>(source.texture_height);
        result = draw_textured_quad(
            device, viewport, u_offset, v_offset,
            maximum_u + u_offset, maximum_v + v_offset, weights[index]);
        if (FAILED(result)) return false;
    }
    return true;
}

bool draw_downsample_pass(IDirect3DDevice8* device,
                          BloomTarget& destination) noexcept {
    if (!device || !g_bloom.mask.texture || !destination.surface) {
        return false;
    }
    device->SetTexture(0, nullptr);
    HRESULT result = device->SetRenderTarget(destination.surface, nullptr);
    if (FAILED(result)) return false;
    set_target_viewport(device, destination);
    result = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                           D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    if (FAILED(result)) return false;

    device->SetTexture(0, g_bloom.mask.texture);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(
        D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_DESTBLEND, static_cast<DWORD>(D3DBLEND_ONE));

    // Four bilinear taps cover each 2x2 full-resolution footprint. The former
    // quarter-resolution reduction could phase thin windows and nacelle strips
    // in and out as they moved. Equal contributions retain energy and hue.
    constexpr std::array<std::array<float, 2>, 4> offsets{{
        {{-0.5f, -0.5f}}, {{0.5f, -0.5f}},
        {{-0.5f,  0.5f}}, {{0.5f,  0.5f}}}};
    const float maximum_u = static_cast<float>(g_bloom.mask.active_width) /
        static_cast<float>(g_bloom.mask.texture_width);
    const float maximum_v = static_cast<float>(g_bloom.mask.active_height) /
        static_cast<float>(g_bloom.mask.texture_height);
    D3DVIEWPORT8 viewport{};
    viewport.Width = destination.active_width;
    viewport.Height = destination.active_height;
    viewport.MinZ = 0.0f;
    viewport.MaxZ = 1.0f;
    for (const auto& offset : offsets) {
        const float u_offset = offset[0] /
            static_cast<float>(g_bloom.mask.texture_width);
        const float v_offset = offset[1] /
            static_cast<float>(g_bloom.mask.texture_height);
        result = draw_textured_quad(
            device, viewport, u_offset, v_offset,
            maximum_u + u_offset, maximum_v + v_offset, 64);
        if (FAILED(result)) return false;
    }
    return true;
}

void configure_halo_composite_state(IDirect3DDevice8* device,
                                    const BloomTarget& halo) noexcept {
    // Composite the continuous colour blur directly. The earlier stage-1
    // `blur - sharp` operation behaved like a moving threshold: sub-pixel
    // changes could zero one thin nacelle while retaining the other. Screen
    // blending below controls highlight clipping without that instability.
    device->SetTexture(0, halo.texture);
    device->SetTexture(1, nullptr);
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_DISABLE));
}

void save_diagnostic_surface(IDirect3DSurface8* surface,
                             const char* label,
                             std::uint32_t index) noexcept {
    if (!surface || !label || !g_save_surface_to_file ||
        g_root_directory.empty()) {
        return;
    }
    const bool has_separator = g_root_directory.back() == '\\' ||
        g_root_directory.back() == '/';
    char path[1800]{};
    std::snprintf(path, sizeof(path), "%s%sA2FOBloom-%02lu-%s.bmp",
                  g_root_directory.c_str(), has_separator ? "" : "\\",
                  static_cast<unsigned long>(index), label);
    // D3DXIFF_BMP is enum value zero in D3DX8. Capture only the first few
    // emissive frames; this is a bounded diagnostic, not a per-frame feature.
    const HRESULT result = g_save_surface_to_file(
        path, 0, surface, nullptr, nullptr);
    if (FAILED(result)) {
        log_hresult("Save diagnostic emissive surface", result);
    } else {
        char message[1900]{};
        std::snprintf(message, sizeof(message),
                      "Saved diagnostic emissive surface: %s", path);
        log_line(message);
    }
}

void composite_native_bloom(IDirect3DDevice8* device) noexcept {
    if (!device || device != g_bloom.device || g_emissive_mask_draw_active) {
        return;
    }
    if (!g_bloom.mask_dirty) return;
    SavedDeviceDrawState saved{};
    if (!save_device_draw_state(device, saved)) return;

    const std::uint32_t diagnostic_index = g_diagnostic_capture_count;
    const bool capture_diagnostic = diagnostic_index < 4;
    if (capture_diagnostic) {
        save_diagnostic_surface(
            g_bloom.mask.surface, "mask", diagnostic_index);
    }

    if (g_diagnostic_frame_count < 16) {
        char message[240]{};
        std::snprintf(
            message, sizeof(message),
            "Emissive mask frame %lu: %lu draw(s), %llu triangle(s)",
            static_cast<unsigned long>(g_diagnostic_frame_count),
            static_cast<unsigned long>(g_bloom.diagnostic_draw_count),
            static_cast<unsigned long long>(
                g_bloom.diagnostic_triangle_count));
        log_line(message);
    }
    ++g_diagnostic_frame_count;

    bool succeeded = false;
    configure_quad_state(device);
    HRESULT result = draw_downsample_pass(device, g_bloom.blur_a)
        ? D3D_OK : E_FAIL;
    bool filtered = SUCCEEDED(result);
    // Two dense separable iterations yield a broad, continuous Gaussian while
    // using only two ping-pong targets.
    for (std::size_t iteration = 0; iteration < 2 && filtered; ++iteration) {
        filtered = draw_blur_pass(
            device, g_bloom.blur_a, g_bloom.blur_b, true) &&
            draw_blur_pass(
                device, g_bloom.blur_b, g_bloom.blur_a, false);
    }
    if (!filtered) result = E_FAIL;
    if (filtered) {
        if (capture_diagnostic) {
            save_diagnostic_surface(
                g_bloom.blur_a.surface, "blur", diagnostic_index);
            ++g_diagnostic_capture_count;
        }
        BloomTarget* halo = &g_bloom.blur_a;
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
        result = device->SetRenderTarget(
            saved.render_target, saved.depth_stencil);
        if (SUCCEEDED(result)) {
            device->SetViewport(&saved.viewport);
            configure_quad_state(device);
            configure_halo_composite_state(device, *halo);
            device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device->SetRenderState(
                D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
            device->SetRenderState(
                D3DRS_DESTBLEND,
                static_cast<DWORD>(D3DBLEND_INVSRCCOLOR));
            for (std::size_t pass = 0;
                 pass < kHaloCompositePasses && SUCCEEDED(result); ++pass) {
                result = draw_textured_quad(
                    device, saved.viewport, 0.0f, 0.0f,
                    static_cast<float>(halo->active_width) /
                        static_cast<float>(halo->texture_width),
                    static_cast<float>(halo->active_height) /
                        static_cast<float>(halo->texture_height),
                    255);
            }
            succeeded = SUCCEEDED(result);
            if (succeeded && capture_diagnostic) {
                save_diagnostic_surface(
                    saved.render_target, "final", diagnostic_index);
            }
        }
    }

    restore_device_draw_state(device, saved);
    g_bloom.mask_dirty = false;
    g_bloom.mask_prepared = false;
    g_bloom.diagnostic_draw_count = 0;
    g_bloom.diagnostic_triangle_count = 0;
    if (succeeded) {
        if (InterlockedCompareExchange(
                &g_logged_emissive_bloom_composite, 1, 0) == 0) {
            log_line("Native D3D8 emissive framebuffer bloom composited");
        }
    } else if (InterlockedCompareExchange(
                   &g_logged_emissive_bloom_failure, 1, 0) == 0) {
        log_hresult("Composite native emissive bloom", result);
    }
}

void standard_emissive_post_draw() noexcept {
    if (g_standard_state_overflow != 0) {
        --g_standard_state_overflow;
        return;
    }
    if (g_standard_state_depth == 0) return;
    StandardTextureStageState& state =
        g_standard_state_stack[--g_standard_state_depth];
    if (!state.active || !state.device) {
        state = StandardTextureStageState{};
        return;
    }
    IDirect3DDevice8* device = state.device;
    device->SetTexture(1, state.texture);
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, state.colour_operation);
    device->SetTextureStageState(
        1, D3DTSS_COLORARG1, state.colour_argument1);
    device->SetTextureStageState(
        1, D3DTSS_COLORARG2, state.colour_argument2);
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, state.alpha_operation);
    device->SetTextureStageState(
        1, D3DTSS_ALPHAARG1, state.alpha_argument1);
    device->SetTextureStageState(
        1, D3DTSS_ALPHAARG2, state.alpha_argument2);
    device->SetTextureStageState(
        1, D3DTSS_TEXCOORDINDEX, state.coordinate_index);
    device->SetTextureStageState(
        1, D3DTSS_MINFILTER, state.minimum_filter);
    device->SetTextureStageState(
        1, D3DTSS_MAGFILTER, state.magnification_filter);
    device->SetTextureStageState(
        1, D3DTSS_MIPFILTER, state.mipmap_filter);
    if (state.texture) state.texture->Release();
    state = StandardTextureStageState{};
}

bool ensure_pixel_shader(IDirect3DDevice8* device) noexcept {
    if (!device || !compile_pixel_shader()) return false;
    if (g_device == device && g_pixel_shader != 0) return true;

    adopt_live_device(device);

    // Armada has just resolved/created the matching custom vertex shader in
    // the original GetShaderHandle gateway. Create only its pixel half here,
    // using the actual live device selected by the renderer rather than the
    // static wrapper vtable assumed by the upstream patch.
    DWORD pixel_shader = 0;
    const HRESULT result = device->CreatePixelShader(
        static_cast<const DWORD*>(g_compiled_pixel_shader->GetBufferPointer()),
        &pixel_shader);
    if (FAILED(result) || pixel_shader == 0) {
        if (InterlockedCompareExchange(&g_logged_create_failure, 1, 0) == 0) {
            log_hresult("IDirect3DDevice8::CreatePixelShader", result);
        }
        return false;
    }
    g_pixel_shader = pixel_shader;
    log_line("DX8 Nebula pixel shader created on Armada's live device");
    return true;
}

std::uintptr_t set_pixel_shader_impl(
    void* self, std::uintptr_t shader_id) noexcept {
    const std::uintptr_t original = g_get_shader_handle_original
        ? a2fo_nebula_call_thiscall_1(
              g_get_shader_handle_original, self, shader_id)
        : 0;
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        !g_matrix_transpose ||
        !g_matrix_multiply || !g_matrix_inverse) {
        return original;
    }

    IDirect3DDevice8* device = resolve_live_device(self);
    if (!device) {
        if (InterlockedCompareExchange(&g_logged_device_failure, 1, 0) == 0) {
            log_line("Could not resolve Armada's live DX8 device from the "
                     "shader renderer");
        }
        return original;
    }
    if (!ensure_pixel_shader(device)) return original;

    D3DMATRIX world{};
    D3DMATRIX view{};
    D3DMATRIX transposed{};
    D3DMATRIX multiplied{};
    D3DMATRIX inverse{};
    HRESULT result = device->GetTransform(D3DTS_WORLD, &world);
    if (FAILED(result)) return original;
    result = device->GetTransform(D3DTS_VIEW, &view);
    if (FAILED(result)) return original;

    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        1, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        1, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTexture(1, current_emissive_texture(device));

    g_matrix_transpose(&transposed, &world);
    device->SetVertexShaderConstant(7, &transposed, 4);

    g_matrix_multiply(&multiplied, &world, &view);
    g_matrix_transpose(&transposed, &multiplied);
    device->SetVertexShaderConstant(11, &transposed, 4);

    if (!g_matrix_inverse(&inverse, nullptr, &world)) return original;
    g_matrix_transpose(&transposed, &inverse);
    device->SetVertexShaderConstant(15, &transposed, 4);

    const auto* camera_front = static_cast<const float*>(
        at(g_armada, kCameraToNodeRva));
    if (!readable_range(camera_front, sizeof(float) * 3)) return original;
    const float camera_direction[4]{
        camera_front[0], camera_front[1], camera_front[2], 0.0f};
    device->SetVertexShaderConstant(19, camera_direction, 1);

    result = device->SetPixelShader(g_pixel_shader);
    if (FAILED(result)) {
        if (InterlockedCompareExchange(&g_logged_set_failure, 1, 0) == 0) {
            log_hresult("IDirect3DDevice8::SetPixelShader", result);
        }
        // A device reset can invalidate a shader handle without changing the
        // wrapper pointer. Recreate it on the next DOT3 draw.
        g_pixel_shader = 0;
        device->SetTexture(1, nullptr);
    }
    return original;
}

void disable_pixel_shader_impl() noexcept {
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) != 0 &&
        g_device) {
        // Resume Fleet Operations immediately after this call. Unlike the
        // upstream naked epilogue, its alpha geometry is not discarded.
        g_device->SetPixelShader(0);
        g_device->SetTexture(1, nullptr);
        g_device->SetTextureStageState(
            1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    }
}

void* __cdecl compile_dot3_mesh_hook(const void* mesh) noexcept {
    // This is the first safe point after process attach and before Armada reads
    // the vertex-shader pathname. Activate here so no file/D3DX work occurs
    // under the Windows loader lock, while the path still changes in time for
    // this one-time shared DOT3 shader construction.
    const LONG state = InterlockedCompareExchange(&g_activation_state, 1, 0);
    if (state == 0) {
        bool activated = false;
        if (!controller_is_installed()) {
            log_line("Controller DLL is absent; early renderer remains native");
        } else if (command_line_requests_dx9()) {
            log_line("/d3d9 detected; the initial Nebula integration supports "
                     "DX8 only and remains inactive");
        } else if (!shader_assets_available() || !load_d3dx() ||
                   !compile_pixel_shader()) {
            log_line("DX8 shader activation failed; native rendering retained");
        } else if (!install_get_shader_route()) {
            log_line("DX8 draw-time shader routing failed; native rendering "
                     "retained");
        } else if (!a2fo::patch_bytes(
                       at(g_armada, kVertexShaderPathRva),
                       kVertexShaderPath.data(),
                       kExpectedVertexShaderPath.data(),
                       kVertexShaderPath.size())) {
            log_line("Could not patch the checked DX8 vertex-shader path; "
                     "native rendering retained");
        } else {
            InterlockedExchange(&g_runtime_enabled, 1);
            activated = true;
            log_line("Nebula Patch DX8 per-pixel lighting activated before "
                     "Armada's shared DOT3 shader compilation");
        }
        InterlockedExchange(&g_activation_state, activated ? 2 : -1);
    }

    CompileDot3Mesh original =
        function_from_address<CompileDot3Mesh>(g_compile_original);
    return original ? original(mesh) : nullptr;
}

bool install_hooks_early() noexcept {
    if (!preflight_signatures()) return false;

    // These replacements are installed under process attach but remain pure
    // pass-throughs. The compile hook performs all file/D3DX work later at the
    // first DOT3 mesh, before calling its original gateway.
    bool installed = a2fo::install_inline_hook(
        at(g_armada, kCompileDot3MeshRva),
        function_address(&compile_dot3_mesh_hook),
        kExpectedCompileDot3Mesh.size(), kExpectedCompileDot3Mesh.data(),
        g_compile_hook);
    if (installed) g_compile_original = g_compile_hook.gateway;

    installed = installed && a2fo::install_inline_hook(
        at(g_fleet_ops, kAlphaTransitionRva),
        function_address(&a2fo_nebula_alpha_hook),
        kExpectedAlphaTransition.size(), kExpectedAlphaTransition.data(),
        g_alpha_hook);
    if (installed) g_a2fo_nebula_alpha_gateway = g_alpha_hook.gateway;

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kStandardMeshPreDrawRva),
        function_address(&a2fo_nebula_standard_pre_hook),
        kExpectedStandardMeshPreDraw.size(),
        kExpectedStandardMeshPreDraw.data(), g_standard_pre_hook);
    if (installed) {
        g_a2fo_nebula_standard_pre_gateway = g_standard_pre_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kStandardMeshPostDrawRva),
        function_address(&a2fo_nebula_standard_post_hook),
        kExpectedStandardMeshPostDraw.size(),
        kExpectedStandardMeshPostDraw.data(), g_standard_post_hook);
    if (installed) {
        g_a2fo_nebula_standard_post_gateway = g_standard_post_hook.gateway;
    }

    // Older Armada SODs bypass both DOT3 and ST3D_Standard_MeshVB. Their face
    // routines and Workspace::Flush only populate/finalize CPU-side batches.
    // Keep the additive stage alive across Workspace::Submit, after the native
    // texture-material pass has configured its state, then restore it before
    // the pass loop advances.
    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kNonVbPreDrawRva),
        function_address(&a2fo_nebula_nonvb_pre_hook),
        kExpectedNonVbPreDraw.size(), kExpectedNonVbPreDraw.data(),
        g_nonvb_pre_hook);
    if (installed) {
        g_a2fo_nebula_nonvb_pre_gateway = g_nonvb_pre_hook.gateway;
    }
    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kNonVbPostDrawRva),
        function_address(&a2fo_nebula_nonvb_post_hook),
        kExpectedNonVbPostDraw.size(), kExpectedNonVbPostDraw.data(),
        g_nonvb_post_hook);
    if (installed) {
        g_a2fo_nebula_nonvb_post_gateway = g_nonvb_post_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kWorkspaceDx8DrawRva),
        function_address(&a2fo_nebula_workspace_dx8_draw_hook),
        kExpectedWorkspaceDx8Draw.size(),
        kExpectedWorkspaceDx8Draw.data(), g_workspace_dx8_draw_hook);
    if (installed) {
        g_a2fo_nebula_workspace_dx8_draw_gateway =
            g_workspace_dx8_draw_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kFrameBloomRva),
        function_address(&a2fo_nebula_frame_bloom_hook),
        kExpectedFrameBloom.size(), kExpectedFrameBloom.data(),
        g_frame_bloom_hook);
    if (installed) {
        g_a2fo_nebula_frame_bloom_gateway = g_frame_bloom_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kDeviceResetRva),
        function_address(&a2fo_nebula_device_reset_hook),
        kExpectedDeviceReset.size(), kExpectedDeviceReset.data(),
        g_device_reset_hook);
    if (installed) {
        g_a2fo_nebula_device_reset_gateway = g_device_reset_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kDot3DrawRva),
        function_address(&a2fo_nebula_dot3_draw_hook),
        kExpectedDot3Draw.size(), kExpectedDot3Draw.data(),
        g_dot3_draw_hook);
    if (installed) {
        g_a2fo_nebula_dot3_draw_gateway = g_dot3_draw_hook.gateway;
    }

    if (!installed) {
        log_line("A Nebula renderer inline hook could not be installed; "
                 "installed core hooks remain pass-through");
        return false;
    }

    g_hooks_ready = true;
    return true;
}

}  // namespace

extern "C" std::uintptr_t __cdecl a2fo_nebula_set_pixel_shader(
    void* self, std::uintptr_t shader_id) {
    return set_pixel_shader_impl(self, shader_id);
}

extern "C" void __cdecl a2fo_nebula_disable_pixel_shader() {
    disable_pixel_shader_impl();
}

extern "C" void __cdecl a2fo_nebula_standard_pre() {
    if (InterlockedCompareExchange(
            &g_logged_standard_hook_reached, 1, 0) == 0) {
        log_line("Standard MeshVB draw boundary reached");
    }
    standard_emissive_pre_draw();
}

extern "C" void __cdecl a2fo_nebula_standard_post(void* mesh_stream) {
    standard_emissive_mask_draw(mesh_stream);
    standard_emissive_post_draw();
}

extern "C" void __cdecl a2fo_nebula_nonvb_pre() {
    if (InterlockedCompareExchange(
            &g_logged_nonvb_hook_reached, 1, 0) == 0) {
        log_line("Legacy non-VB direct-submit draw boundary reached");
    }
    standard_emissive_pre_draw();
}

extern "C" void __cdecl a2fo_nebula_nonvb_post(void* workspace) {
    nonvb_emissive_mask_draw(workspace);
    standard_emissive_post_draw();
}

extern "C" void __cdecl a2fo_nebula_workspace_dx8_draw(
    IDirect3DDevice8* device, void* workspace, UINT vertex_count,
    UINT start_index, UINT primitive_count) {
    workspace_dx8_emissive_mask_draw(
        device, workspace, vertex_count, start_index, primitive_count);
}

extern "C" void __cdecl a2fo_nebula_frame_bloom(
    IDirect3DDevice8* device) {
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) != 0) {
        composite_native_bloom(device);
    }
}

extern "C" void __cdecl a2fo_nebula_before_device_reset(
    IDirect3DDevice8* device) {
    if (device && device == g_bloom.device) release_bloom_resources();
}

extern "C" void __cdecl a2fo_nebula_dot3_draw(
    IDirect3DDevice8* device, const void* mesh_stream,
    UINT primitive_count) {
    dot3_emissive_mask_draw(device, mesh_stream, primitive_count);
}

namespace a2fo {

bool install_nebula_renderer_early(HMODULE armada, HMODULE fleet_ops,
                                   const std::string& root_directory,
                                   NebulaRendererLog log) {
    g_armada = armada;
    g_fleet_ops = fleet_ops;
    g_root_directory = root_directory;
    g_log = log;
    if (!validate_module(g_armada, kArmadaTimestamp, kArmadaImageSize,
                         "ArmadaL.exe") ||
        !validate_module(g_fleet_ops, kFleetOpsTimestamp, kFleetOpsImageSize,
                         "FleetOpsHook.dll") ||
        !install_hooks_early()) {
        InterlockedExchange(&g_activation_state, -1);
        log_line("Early DX8 renderer hooks unavailable; native rendering retained");
        return false;
    }
    log_line("Early DX8 renderer hooks armed before DOT3 shader creation");
    return true;
}

int nebula_renderer_status() noexcept {
    if (!g_hooks_ready) return 0;
    const LONG state = InterlockedCompareExchange(&g_activation_state, 0, 0);
    if (state == 2) return 2;
    if (state < 0) return -1;
    return 1;
}

bool register_nebula_emissive_class(
    void* object_class, const char* const* texture_paths,
    std::uint32_t texture_path_count) noexcept {
    return register_nebula_emissive_materials(
        object_class, nullptr, texture_paths, 1, texture_path_count);
}

bool register_nebula_emissive_materials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count,
    std::uint32_t texture_paths_per_material) noexcept {
    if (!object_class || !texture_paths || material_count == 0 ||
        material_count > 256 || texture_paths_per_material == 0) {
        return false;
    }
    try {
        auto policy = std::make_unique<EmissiveClassPolicy>();
        policy->materials.reserve(material_count);
        for (std::size_t material_index = 0;
             material_index < material_count; ++material_index) {
            auto material = std::make_unique<EmissiveMaterialPolicy>();
            if (diffuse_names) {
                const char* diffuse_name = diffuse_names[material_index];
                material->diffuse_key = a2fo::nebula::normalize_texture_key(
                    diffuse_name ? diffuse_name : "");
                if (material->diffuse_key.empty()) continue;
            }
            const std::size_t count = std::min<std::size_t>(
                texture_paths_per_material, material->paths.size());
            const std::size_t first_path =
                material_index * texture_paths_per_material;
            for (std::size_t index = 0; index < count; ++index) {
                const char* path = texture_paths[first_path + index];
                if (!path || !*path) continue;
                material->paths[index] = path;
                material->path_mask = static_cast<std::uint8_t>(
                    material->path_mask | (1u << index));
            }
            if (material->path_mask != 0) {
                policy->materials.push_back(std::move(material));
            }
        }
        if (policy->materials.empty()) return false;

        const auto existing = g_emissive_policies.find(object_class);
        if (existing != g_emissive_policies.end() && existing->second) {
            for (auto& material : existing->second->materials) {
                if (material) release_emissive_gpu_cache(*material);
            }
        }
        g_emissive_policies[object_class] = std::move(policy);
        return true;
    } catch (...) {
        log_line("Could not retain a CraftClass emissive-map policy");
        return false;
    }
}

void nebula_begin_craft_render(void* craft) noexcept {
    if (g_craft_render_depth < g_craft_render_stack.size()) {
        g_craft_render_stack[g_craft_render_depth++] = craft;
    } else {
        ++g_craft_render_overflow;
    }
    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    if (g_emissive_policies.find(object_class) !=
            g_emissive_policies.end() &&
        InterlockedCompareExchange(
            &g_logged_registered_craft_context, 1, 0) == 0) {
        log_line("Craft render context reached a registered emissive class");
    }
}

void nebula_end_craft_render(void*) noexcept {
    if (g_craft_render_overflow != 0) {
        --g_craft_render_overflow;
        return;
    }
    if (g_craft_render_depth != 0) {
        --g_craft_render_depth;
        g_craft_render_stack[g_craft_render_depth] = nullptr;
    }
}

}  // namespace a2fo

extern "C" __declspec(dllexport)
int __cdecl A2FO_NebulaRendererStatus() {
    return a2fo::nebula_renderer_status();
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_NebulaRegisterEmissiveClass(
    void* object_class, const char* const* texture_paths,
    std::uint32_t texture_path_count) {
    return a2fo::register_nebula_emissive_class(
        object_class, texture_paths, texture_path_count) ? 1 : 0;
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_NebulaRegisterEmissiveMaterials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count,
    std::uint32_t texture_paths_per_material) {
    return a2fo::register_nebula_emissive_materials(
        object_class, diffuse_names, texture_paths, material_count,
        texture_paths_per_material) ? 1 : 0;
}

extern "C" __declspec(dllexport)
void __cdecl A2FO_NebulaBeginCraftRender(void* craft) {
    a2fo::nebula_begin_craft_render(craft);
}

extern "C" __declspec(dllexport)
void __cdecl A2FO_NebulaEndCraftRender(void* craft) {
    a2fo::nebula_end_craft_render(craft);
}
