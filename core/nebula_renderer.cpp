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
#include "com_owner.hpp"
#include "nebula_emissive.hpp"
#include "renderer_options.hpp"
#include "decal_math.hpp"
#include "hook.hpp"
#include "../sdk/include/a2fo_supported_armada.hpp"

#include <windows.h>
#include <d3d8.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
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
std::uintptr_t __cdecl a2fo_nebula_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
void a2fo_nebula_set_pixel_shader_hook();
void a2fo_nebula_alpha_hook();
void a2fo_nebula_standard_pre_hook();
void a2fo_nebula_standard_post_hook();
void a2fo_nebula_nonvb_pre_hook();
void a2fo_nebula_nonvb_post_hook();
void a2fo_nebula_workspace_dx8_draw_hook();
void a2fo_nebula_frame_begin_hook();
void a2fo_nebula_frame_bloom_hook();
void a2fo_nebula_device_reset_hook();
void a2fo_nebula_device_destroy_hook();
void a2fo_nebula_dot3_draw_hook();
void a2fo_nebula_fleetops_dot3_draw_hook();

// Read by the assembly continuation after its helper has restored all
// registers and flags from the original Fleet Operations render function.
void* g_a2fo_nebula_alpha_gateway = nullptr;
void* g_a2fo_nebula_standard_pre_gateway = nullptr;
void* g_a2fo_nebula_standard_post_gateway = nullptr;
void* g_a2fo_nebula_nonvb_pre_gateway = nullptr;
void* g_a2fo_nebula_nonvb_post_gateway = nullptr;
void* g_a2fo_nebula_workspace_dx8_draw_gateway = nullptr;
void* g_a2fo_nebula_workspace_dx8_draw_return = nullptr;
void* g_a2fo_nebula_frame_begin_gateway = nullptr;
void* g_a2fo_nebula_frame_bloom_gateway = nullptr;
void* g_a2fo_nebula_device_reset_gateway = nullptr;
void* g_a2fo_nebula_device_destroy_gateway = nullptr;
void* g_a2fo_nebula_dot3_draw_gateway = nullptr;
void* g_a2fo_nebula_dot3_draw_return = nullptr;
void* g_a2fo_nebula_fleetops_dot3_draw_return = nullptr;
}

namespace {

using AssembleShaderFromFile = HRESULT (WINAPI*)(
    LPCSTR file_name, DWORD flags, ID3DXBuffer** constants,
    ID3DXBuffer** compiled_shader, ID3DXBuffer** compilation_errors);
using CreateTextureFromFileEx = HRESULT (WINAPI*)(
    IDirect3DDevice8* device, LPCSTR file_name, UINT width, UINT height,
    UINT mip_levels, DWORD usage, D3DFORMAT format, D3DPOOL pool,
    DWORD filter, DWORD mip_filter, D3DCOLOR colour_key,
    void* source_info, PALETTEENTRY* palette,
    IDirect3DTexture8** texture);
using CompileDot3Mesh = void* (__cdecl*)(const void* mesh);

constexpr const char* kModuleName = "A2FONebulaRenderer";
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

// Supported Armada II 1.1 / Fleet Operations Roots renderer locations.
constexpr std::uintptr_t kCompileDot3MeshRva = 0x00226e50;
constexpr std::uintptr_t kGetShaderHandleRva = 0x0022c270;
constexpr std::uintptr_t kGetShaderHandleRouteRva = 0x00210bb4;
constexpr std::uintptr_t kGraphicsEnginePointerRva = 0x003ad508;
constexpr std::uintptr_t kAlphaTransitionRva = 0x001e67d1;
// Fleet Operations replaces ST3D_Dot3_MeshVB::Render and submits its primary
// indexed draw here. Its final diffuse bind occurs immediately before this
// call; hooking Armada's original DOT3 draw therefore cannot see FO meshes.
constexpr std::uintptr_t kFleetOpsDot3DrawRva = 0x001e67c8;
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
// ST3D_DeviceDirectX8::BeginScene's native device call. Reset the selective
// emissive-mask lifecycle here so it can never retain geometry from an older
// presented frame if an EndScene path is skipped or reordered.
constexpr std::uintptr_t kFrameBeginRva = 0x00223e9c;
// ST3D_DeviceDirectX8::RefreshDisplay's final native EndScene call, immediately
// before Present. The earlier ST3D_DeviceDirectX8::EndScene method can run more
// than once per displayed frame; compositing there occasionally leaves a short
// duplicate image when a later scene is presented on the same backbuffer.
constexpr std::uintptr_t kFrameBloomRva = 0x00224710;
// Device-lost recovery immediately before IDirect3DDevice8::Reset. Default-pool
// bloom targets must be released or Direct3D rejects the native reset.
constexpr std::uintptr_t kDeviceResetRva = 0x00223ce4;
// Fleet Operations already owns the supported ST3D_DeviceDirectX8 destructor
// detour. Enter its callback before the original Armada DestroyDevice call so
// extension GPU objects and the retained COM reference are released while the
// dxwrapper device is still fully callable.
constexpr std::uintptr_t kFleetOpsDeviceDestroyCallbackRva = 0x001fd0a0;
// ST3D_Dot3_MeshVB::Render's primary indexed draw. The call is intercepted
// before execution while its vertex/index streams and custom shader are live;
// the helper restores them before the original call is replayed.
constexpr std::uintptr_t kDot3DrawRva = 0x002279af;
// Fleet Operations performs one or more native DOT3 light-accumulation draws
// after resolving its vertex-shader handle and only binds the diffuse texture
// immediately before the final material draw. Selecting an extension pixel
// shader at the handle lookup therefore interprets the blue normal map as
// diffuse colour during those earlier draws. Preserve FO's complete native
// bump sequence for now; bumped emissives use the scoped fixed-function stage
// fallback at the final indexed draw and specular rendering remains deferred.
constexpr bool kPreserveFleetOpsNativeDot3 = true;
// The Windows dxwrapper/d3d8to9 chain can invalidate its internal vertex-
// shader object when an otherwise redundant pixel-shader state change occurs
// between Fleet Operations' GetShaderHandle and SetVertexShader calls. Native
// DOT3 mode does not need that interception: emissive/specular work is scoped
// at the final draw instead. Keep this policy tied to the native-DOT3 switch
// so enabling an experimental pixel-shader path remains an explicit change.
constexpr bool kUseFleetOpsShaderHandleRoute =
    !kPreserveFleetOpsNativeDot3;
static_assert(!kPreserveFleetOpsNativeDot3 ||
                  !kUseFleetOpsShaderHandleRoute,
              "Native DOT3 must leave Fleet Ops' shader-handle route native");
// Specular maps are authored as material-intensity masks. Apply a restrained
// quarter-strength additive overlay after the native DOT3 material draw; this
// is visible without turning bright masks into self-lit white hulls.
constexpr DWORD kSpecularOverlayTextureFactor = 0xff404040u;
constexpr std::size_t kCurrentDeviceIndexOffset = 0xc0;
constexpr std::size_t kDeviceWrapperTableOffset = 0xcc;
constexpr std::size_t kStormDeviceOffset = 0x90;
constexpr std::uint32_t kMaximumStormDeviceCount = 2;
constexpr std::size_t kRequiredD3D8VtableEntries = 93;
constexpr std::size_t kSetTextureVtableIndex = 61;
constexpr std::size_t kSetPixelShaderVtableIndex = 88;
constexpr std::size_t kDeletePixelShaderVtableIndex = 90;
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
// GameObject::IsCloaked/IsFullyCloaked/IsDecloaking read the active cloak
// controller at +0x108 and its state at +0x3c. Values observed in Armada are
// 0=visible, 1=cloaking, 2=fully cloaked, and 3=decloaking.
constexpr std::size_t kGameObjectCloakControllerOffset = 0x108;
constexpr std::size_t kCloakControllerStateOffset = 0x3c;
constexpr std::size_t kCraftNameIndexOffset = 0x218;
// FleetOpsHook encodes a private enhancement-record pointer across four spare
// CraftClass bytes. The wrapper's +4 sidecar owns logoFileNames and a parallel
// ST3D_Texture pointer table in the same row order.
constexpr std::size_t kCraftClassEnhancementPointerByte0Offset = 0x185;
constexpr std::size_t kCraftClassEnhancementPointerByte1Offset = 0x186;
constexpr std::size_t kCraftClassEnhancementPointerByte2Offset = 0x187;
constexpr std::size_t kCraftClassEnhancementPointerByte3Offset = 0x19f;
constexpr std::size_t kEnhancementWrapperSidecarOffset = 0x04;
constexpr std::size_t kEnhancementSidecarOwnerOffset = 0x00;
constexpr std::size_t kEnhancementLogoNamesBeginOffset = 0x148;
constexpr std::size_t kEnhancementLogoNamesEndOffset = 0x14c;
constexpr std::size_t kEnhancementLogoTexturesOffset = 0x154;
constexpr std::size_t kEnhancementMinimumReadableSize = 0x158;
constexpr std::size_t kMaximumCraftLogoCount = 256;
constexpr std::size_t kGameObjectCurrentHealthOffset = 0x15c;
constexpr std::size_t kGameObjectMaximumHealthOffset = 0x160;
constexpr std::size_t kGameObjectVelocityOffset = 0xdc;
constexpr std::size_t kCraftPhysicsOffset = 0x1b0;
constexpr std::size_t kCraftSubsystemsOffset = 0x1e0;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::size_t kCraftClassImpulseSpeedOffset = 0x3ec;
constexpr std::uintptr_t kTrekPhysicsVtableRva = 0x002b28a4;
constexpr std::size_t kTrekPhysicsWarpEffectStateOffset = 0x20;
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
// The selective framebuffer compositor replays only registered emissive
// geometry into private D3D8 render targets. That path was unstable through the
// old dxwrapper -> d3d8to9 -> ReShade chain, but the managed DXVK backend does
// not need ReShade and has a predictable device/reset implementation. Keep the
// feature restart-applied and limited to that backend; system-renderer users
// retain the sharp native emissive material without the private targets.
bool g_native_framebuffer_bloom_enabled = false;
bool g_dxvk_backend_active = false;
bool g_dxvk_backend_ini_claimed = false;
bool g_dxvk_backend_payload_detected = false;
bool g_mapped_texture_cloak_diagnostics_enabled = false;
// Derived emissive composites are D3DPOOL_MANAGED, so every cached surface can
// occupy both process RAM and VRAM. Keep common live states hot without
// retaining every subsystem-mask/motion combination ever observed.
constexpr std::size_t kMaximumEmissiveCompositesPerMaterial = 8;
constexpr std::size_t kEmissiveCompositeCacheBudgetBytes =
    96u * 1024u * 1024u;

constexpr std::array<std::uint8_t, 10> kExpectedCompileDot3Mesh{
    0x55, 0x8b, 0xec, 0x6a, 0xff,
    0x68, 0xcb, 0xba, 0x6a, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedGetShaderHandle{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::array<std::uint8_t, 13> kExpectedAlphaTransition{
    0x8b, 0x40, 0x0c,
    0xf7, 0x80, 0x2c, 0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedFleetOpsDot3Draw{
    0xff, 0x90, 0x1c, 0x01, 0x00, 0x00};
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
constexpr std::array<std::uint8_t, 9> kExpectedFrameBegin{
    0x56, 0x8b, 0x0e, 0xff, 0x91, 0x88, 0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 7> kExpectedFrameBloom{
    0x8b, 0x86, 0x90, 0x00, 0x00, 0x00, 0x50};
constexpr std::array<std::uint8_t, 7> kExpectedDeviceReset{
    0x8b, 0x10, 0x51, 0x50, 0xff, 0x52, 0x38};
constexpr std::array<std::uint8_t, 7> kExpectedFleetOpsDeviceDestroyCallback{
    0x55, 0x8b, 0xec, 0x51, 0x89, 0x4d, 0xfc};
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
volatile LONG g_logged_device_switch = 0;
volatile LONG g_logged_device_reset_invalidation = 0;
volatile LONG g_logged_device_destroy_release = 0;
volatile LONG g_logged_device_cleanup_method_unavailable = 0;
volatile LONG g_logged_native_dot3_preserved = 0;
volatile LONG g_pixel_shader_rejected = 0;
a2fo::InlineHook g_compile_hook{};
a2fo::InlineHook g_alpha_hook{};
a2fo::InlineHook g_standard_pre_hook{};
a2fo::InlineHook g_standard_post_hook{};
a2fo::InlineHook g_nonvb_pre_hook{};
a2fo::InlineHook g_nonvb_post_hook{};
a2fo::InlineHook g_workspace_dx8_draw_hook{};
a2fo::InlineHook g_frame_begin_hook{};
a2fo::InlineHook g_frame_bloom_hook{};
a2fo::InlineHook g_device_reset_hook{};
a2fo::InlineHook g_device_destroy_hook{};
a2fo::InlineHook g_dot3_draw_hook{};
a2fo::InlineHook g_fleetops_dot3_draw_hook{};
bool g_hooks_ready = false;
void* g_compile_original = nullptr;
void* g_get_shader_handle_original = nullptr;
IDirect3DDevice8* g_device = nullptr;
DWORD g_pixel_shader = 0;
DWORD g_specular_pixel_shader = 0;
float g_emissive_bump_multiplier = 1.0f;
float g_bump_light_bias = 0.2f;
float g_emissive_diffuse_restore = 0.0f;
ID3DXBuffer* g_compiled_pixel_shader = nullptr;
ID3DXBuffer* g_compiled_specular_pixel_shader = nullptr;
AssembleShaderFromFile g_assemble_shader = nullptr;
CreateTextureFromFileEx g_create_texture_from_file = nullptr;
std::string g_pixel_shader_path;
std::string g_specular_pixel_shader_path;
std::string g_root_directory;

struct SparseEmissivePixel {
    std::uint32_t index = 0;
    std::uint32_t pixel = 0xff000000u;
};

struct EmissiveSourcePixels {
    std::vector<std::uint32_t> dense;
    std::vector<SparseEmissivePixel> sparse;

    void clear() noexcept {
        std::vector<std::uint32_t>().swap(dense);
        std::vector<SparseEmissivePixel>().swap(sparse);
    }
};

struct EmissiveComposite {
    IDirect3DTexture8* texture = nullptr;
    std::size_t estimated_bytes = 0;
    std::uint64_t last_used = 0;
};

struct EmissiveMaterialPolicy {
    // Empty only for the legacy unnumbered ODF commands, which intentionally
    // remain a wildcard applying to every material on that class.
    std::string diffuse_key;
    std::array<std::string, a2fo::nebula::kEmissiveSystemCount> paths{};
    std::array<EmissiveSourcePixels,
               a2fo::nebula::kEmissiveSystemCount> pixels{};
    std::array<bool, a2fo::nebula::kEmissiveSystemCount> source_attempted{};
    std::array<bool, a2fo::nebula::kEmissiveSystemCount> source_loaded{};
    // Low byte: active subsystem mask. High byte: motion-light profile.
    std::unordered_map<std::uint16_t, EmissiveComposite> composites;
    UINT width = 0;
    UINT height = 0;
    std::uint8_t path_mask = 0;
};

struct EmissiveClassPolicy {
    std::vector<std::unique_ptr<EmissiveMaterialPolicy>> materials;
};

struct SpecularMaterialPolicy {
    std::string diffuse_key;
    std::string path;
    IDirect3DTexture8* texture = nullptr;
    IDirect3DDevice8* device = nullptr;
    bool load_attempted = false;
};

struct SpecularClassPolicy {
    std::vector<std::unique_ptr<SpecularMaterialPolicy>> materials;
};

std::unordered_map<void*, std::unique_ptr<EmissiveClassPolicy>>
    g_emissive_policies;
std::unordered_map<void*, std::unique_ptr<SpecularClassPolicy>>
    g_specular_policies;
std::size_t g_emissive_composite_cache_bytes = 0;
std::uint64_t g_emissive_composite_use_clock = 0;
volatile LONG g_logged_emissive_cache_eviction = 0;
volatile LONG g_logged_emissive_cache_floor = 0;

struct DamageDecal {
    std::uint32_t system_index = 0;
    std::uint32_t threshold_index = 1;
    void* node = nullptr;
    std::string texture_path;
    std::array<float, 3> offset{};
    std::array<float, 3> rotation_degrees{};
    std::array<float, 2> size{{1.0f, 1.0f}};
};

struct DamageDecalClassPolicy {
    float damage_threshold = 0.1f;
    std::vector<DamageDecal> decals;
};

std::unordered_map<void*, std::unique_ptr<DamageDecalClassPolicy>>
    g_damage_decal_policies;

struct LogoDecal {
    void* node = nullptr;
    // Empty means use Fleet Operations' native logoFileNames texture table.
    std::vector<std::string> texture_paths;
    bool use_colour_key = false;
    D3DCOLOR colour_key = 0;
    bool flip_u = false;
    std::array<float, 3> offset{};
    std::array<float, 3> rotation_degrees{};
    std::array<float, 2> size{{1.0f, 1.0f}};
};

struct LogoDecalClassPolicy {
    std::vector<LogoDecal> decals;
};

std::unordered_map<void*, std::unique_ptr<LogoDecalClassPolicy>>
    g_logo_decal_policies;
std::unordered_map<std::string, IDirect3DTexture8*> g_damage_decal_textures;
volatile LONG g_logged_damage_decal_draw = 0;
volatile LONG g_logged_damage_decal_texture_failure = 0;
volatile LONG g_logged_damage_decal_texture_unavailable = 0;
volatile LONG g_logged_damage_decal_policy_render = 0;
volatile LONG g_logged_damage_decal_threshold_active = 0;
volatile LONG g_logged_damage_decal_device_unavailable = 0;
volatile LONG g_logged_damage_decal_state_failure = 0;
volatile LONG g_logged_damage_decal_transform_failure = 0;
volatile LONG g_logged_damage_decal_draw_failure = 0;
volatile LONG g_logged_logo_decal_draw = 0;
volatile LONG g_logged_logo_decal_selection_failure = 0;
volatile LONG g_logged_logo_decal_policy_render = 0;
IDirect3DTexture8* g_black_emissive_texture = nullptr;
std::unordered_map<IDirect3DBaseTexture8*, std::string>
    g_diffuse_texture_keys;
thread_local std::array<void*, kCraftRenderStackCapacity>
    g_craft_render_stack{};
thread_local std::size_t g_craft_render_depth = 0;
thread_local std::size_t g_craft_render_overflow = 0;
thread_local bool g_specular_shader_selected = false;
volatile LONG g_logged_emissive_loader_unavailable = 0;
volatile LONG g_logged_emissive_create_failure = 0;
volatile LONG g_logged_standard_emissive = 0;
volatile LONG g_logged_standard_hook_reached = 0;
volatile LONG g_logged_nonvb_hook_reached = 0;
volatile LONG g_logged_nonvb_registered_passes = 0;
volatile LONG g_mapped_texture_cloak_diagnostic_mask = 0;
volatile LONG g_logged_registered_craft_context = 0;
volatile LONG g_logged_fixed_function_without_context = 0;
volatile LONG g_logged_diffuse_lookup_failure = 0;
volatile LONG g_logged_indexed_diffuse_binding = 0;
volatile LONG g_logged_dot3_emissive_fallback = 0;
volatile LONG g_logged_dot3_hook_reached = 0;
volatile LONG g_logged_fleetops_dot3_hook_reached = 0;
volatile LONG g_logged_dot3_without_context = 0;
volatile LONG g_logged_dot3_without_emissive = 0;
volatile LONG g_logged_specular_shader_failure = 0;
volatile LONG g_specular_shader_rejected = 0;
volatile LONG g_logged_specular_binding = 0;
volatile LONG g_logged_specular_draw = 0;

struct StandardTextureStageState {
    IDirect3DDevice8* device = nullptr;
    IDirect3DBaseTexture8* texture = nullptr;
    IDirect3DBaseTexture8* secondary_texture = nullptr;
    IDirect3DTexture8* emissive = nullptr;
    IDirect3DTexture8* specular = nullptr;
    UINT vertex_count = 0;
    UINT primitive_count = 0;
    DWORD stage = 1;
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
    DWORD pixel_shader = 0;
    bool stage_state_modified = false;
    bool pixel_shader_modified = false;
    bool active = false;
};

thread_local std::array<StandardTextureStageState,
                        kStandardStateStackCapacity>
    g_standard_state_stack{};
thread_local std::size_t g_standard_state_depth = 0;
thread_local std::size_t g_standard_state_overflow = 0;
thread_local std::array<StandardTextureStageState,
                        kStandardStateStackCapacity>
    g_dot3_state_stack{};
thread_local std::size_t g_dot3_state_depth = 0;
thread_local std::size_t g_dot3_state_overflow = 0;
thread_local bool g_emissive_mask_draw_active = false;
volatile LONG g_logged_emissive_mask_draw = 0;
volatile LONG g_logged_emissive_bloom_composite = 0;
volatile LONG g_logged_emissive_bloom_failure = 0;
volatile LONG g_logged_emissive_offscreen_skip = 0;
volatile LONG g_logged_nonvb_mask_workspace = 0;
volatile LONG g_logged_workspace_context_fallback = 0;

void release_bloom_resources() noexcept;
bool ensure_specular_pixel_shader(IDirect3DDevice8* device) noexcept;

std::string renderer_ini_path() {
    if (g_root_directory.empty()) return "A2FORenderer.ini";
    const char separator = g_root_directory.back();
    return g_root_directory +
        ((separator == '\\' || separator == '/') ? "" : "\\") +
        "A2FORenderer.ini";
}

std::string renderer_data_path(const char* relative_path) {
    if (!relative_path || !*relative_path) return g_root_directory;
    if (g_root_directory.empty()) return relative_path;
    const char separator = g_root_directory.back();
    return g_root_directory +
        ((separator == '\\' || separator == '/') ? "" : "\\") +
        relative_path;
}

bool same_file_contents(const std::string& left,
                        const std::string& right) noexcept {
    HANDLE left_file = CreateFileA(
        left.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (left_file == INVALID_HANDLE_VALUE) return false;
    HANDLE right_file = CreateFileA(
        right.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (right_file == INVALID_HANDLE_VALUE) {
        CloseHandle(left_file);
        return false;
    }

    LARGE_INTEGER left_size{};
    LARGE_INTEGER right_size{};
    bool equal = GetFileSizeEx(left_file, &left_size) &&
                 GetFileSizeEx(right_file, &right_size) &&
                 left_size.QuadPart == right_size.QuadPart;
    std::array<std::uint8_t, 16 * 1024> left_buffer{};
    std::array<std::uint8_t, 16 * 1024> right_buffer{};
    while (equal) {
        DWORD left_read = 0;
        DWORD right_read = 0;
        if (!ReadFile(left_file, left_buffer.data(), left_buffer.size(),
                      &left_read, nullptr) ||
            !ReadFile(right_file, right_buffer.data(), right_buffer.size(),
                      &right_read, nullptr) ||
            left_read != right_read ||
            std::memcmp(left_buffer.data(), right_buffer.data(), left_read) !=
                0) {
            equal = false;
            break;
        }
        if (left_read == 0) break;
    }
    CloseHandle(right_file);
    CloseHandle(left_file);
    return equal;
}

bool load_dxvk_backend_policy() noexcept {
    char backend[32]{};
    const std::string ini = renderer_ini_path();
    GetPrivateProfileStringA(
        "Renderer", "AppliedBackend", "system", backend,
        sizeof(backend), ini.c_str());
    g_dxvk_backend_ini_claimed = _stricmp(backend, "dxvk") == 0;

    // AppliedBackend is diagnostic state written by the post-exit helper; it
    // can be stale after files are copied between testers or after a crash.
    // The live renderer choice is the managed Data\d3d9.dll itself. Require it
    // to be byte-identical to the packaged DXVK payload so unrelated wrappers
    // (ReShade, dgVoodoo, dxwrapper) never enable the DXVK-only DOT3 hooks.
    g_dxvk_backend_payload_detected = same_file_contents(
        renderer_data_path("d3d9.dll"),
        renderer_data_path("renderers\\dxvk\\d3d9.dll"));
    return g_dxvk_backend_payload_detected;
}

bool load_native_framebuffer_bloom_policy() noexcept {
    const std::string ini = renderer_ini_path();
    const bool requested = GetPrivateProfileIntA(
        "Effects", "EmissiveBloom", 1, ini.c_str()) != 0;
    return requested && g_dxvk_backend_active;
}

bool load_mapped_texture_cloak_diagnostics_policy() noexcept {
    const std::string ini = renderer_ini_path();
    return GetPrivateProfileIntA(
        "Diagnostics", "MappedTextureCloak", 0, ini.c_str()) != 0;
}

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

bool executable_address(const void* address) noexcept {
    if (!address) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info) ||
        info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
        (info.Protect & PAGE_NOACCESS) != 0) {
        return false;
    }
    const DWORD protection = info.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
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

bool device_method_is_callable(IDirect3DDevice8* device,
                               std::size_t method_index) noexcept {
    void* vtable = read_at<void*>(device, 0, nullptr);
    void* method = read_at<void*>(
        vtable, method_index * sizeof(void*), nullptr);
    return executable_address(method);
}

template <typename T>
T read_live_at(const void* object, std::size_t offset,
               T fallback = T{}) noexcept {
    if (!object) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

void* at(HMODULE module, std::uintptr_t rva) noexcept;

void release_emissive_composite(EmissiveComposite& composite) noexcept {
    if (composite.texture) composite.texture->Release();
    if (composite.estimated_bytes <= g_emissive_composite_cache_bytes) {
        g_emissive_composite_cache_bytes -= composite.estimated_bytes;
    } else {
        g_emissive_composite_cache_bytes = 0;
    }
    composite = EmissiveComposite{};
}

void release_emissive_gpu_cache(EmissiveMaterialPolicy& policy) noexcept {
    for (auto& entry : policy.composites) {
        release_emissive_composite(entry.second);
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
    g_emissive_composite_cache_bytes = 0;
}

void release_all_specular_gpu_caches() noexcept {
    for (auto& class_entry : g_specular_policies) {
        if (!class_entry.second) continue;
        for (auto& material : class_entry.second->materials) {
            if (!material) continue;
            if (material->texture) material->texture->Release();
            material->texture = nullptr;
            material->device = nullptr;
            material->load_attempted = false;
        }
    }
}

std::size_t emissive_texture_bytes(UINT width, UINT height) noexcept {
    std::size_t total = 0;
    while (width != 0 && height != 0) {
        total += static_cast<std::size_t>(width) * height *
            sizeof(std::uint32_t);
        if (width == 1 && height == 1) break;
        width = std::max<UINT>(1, width / 2);
        height = std::max<UINT>(1, height / 2);
    }
    return total;
}

bool evict_oldest_emissive_composite(
    EmissiveMaterialPolicy& policy) noexcept {
    if (policy.composites.empty()) return false;
    auto oldest = policy.composites.begin();
    for (auto candidate = std::next(policy.composites.begin());
         candidate != policy.composites.end(); ++candidate) {
        if (candidate->second.last_used < oldest->second.last_used) {
            oldest = candidate;
        }
    }
    release_emissive_composite(oldest->second);
    policy.composites.erase(oldest);
    if (InterlockedCompareExchange(
            &g_logged_emissive_cache_eviction, 1, 0) == 0) {
        log_line("Bounded emissive composite cache evicted an inactive "
                 "derived texture");
    }
    return true;
}

void trim_emissive_composite_cache() noexcept {
    while (g_emissive_composite_cache_bytes >
           kEmissiveCompositeCacheBudgetBytes) {
        EmissiveMaterialPolicy* oldest_policy = nullptr;
        std::uint64_t oldest_use = static_cast<std::uint64_t>(-1);
        // Keep one composite per material to avoid rebuilding the ordinary
        // visible state every frame when many ship classes share the scene.
        for (auto& class_entry : g_emissive_policies) {
            if (!class_entry.second) continue;
            for (auto& material : class_entry.second->materials) {
                if (!material || material->composites.size() <= 1) continue;
                for (const auto& composite : material->composites) {
                    if (composite.second.last_used < oldest_use) {
                        oldest_use = composite.second.last_used;
                        oldest_policy = material.get();
                    }
                }
            }
        }
        if (!oldest_policy ||
            !evict_oldest_emissive_composite(*oldest_policy)) {
            if (InterlockedCompareExchange(
                    &g_logged_emissive_cache_floor, 1, 0) == 0) {
                log_line("Emissive cache budget reached its one-texture-per-"
                         "material floor");
            }
            break;
        }
    }
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
        pixels.clear();
        const std::size_t pixel_count =
            static_cast<std::size_t>(policy.width) * policy.height;
        const auto* source_bytes = static_cast<const std::uint8_t*>(
            locked.pBits);
        std::size_t non_black_count = 0;
        for (UINT y = 0; y < policy.height; ++y) {
            const auto* row = reinterpret_cast<const std::uint32_t*>(
                source_bytes + static_cast<std::size_t>(y) * locked.Pitch);
            for (UINT x = 0; x < policy.width; ++x) {
                if ((row[x] & 0x00ffffffu) != 0) ++non_black_count;
            }
        }
        const bool sparse_is_smaller =
            non_black_count * sizeof(SparseEmissivePixel) <
            pixel_count * sizeof(std::uint32_t);
        if (sparse_is_smaller) {
            pixels.sparse.reserve(non_black_count);
            for (UINT y = 0; y < policy.height; ++y) {
                const auto* row = reinterpret_cast<const std::uint32_t*>(
                    source_bytes +
                    static_cast<std::size_t>(y) * locked.Pitch);
                for (UINT x = 0; x < policy.width; ++x) {
                    const std::uint32_t pixel = row[x];
                    if ((pixel & 0x00ffffffu) == 0) continue;
                    pixels.sparse.push_back(SparseEmissivePixel{
                        static_cast<std::uint32_t>(
                            static_cast<std::size_t>(y) * policy.width + x),
                        pixel});
                }
            }
        } else {
            pixels.dense.resize(pixel_count);
            for (UINT y = 0; y < policy.height; ++y) {
                std::memcpy(
                    pixels.dense.data() +
                        static_cast<std::size_t>(y) * policy.width,
                    source_bytes +
                        static_cast<std::size_t>(y) * locked.Pitch,
                    static_cast<std::size_t>(policy.width) *
                        sizeof(std::uint32_t));
            }
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
    std::vector<std::uint32_t> pixels,
    UINT base_width, UINT base_height) noexcept {
    if (!texture || pixels.size() !=
            static_cast<std::size_t>(base_width) * base_height) {
        return false;
    }
    try {
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
    a2fo::nebula::CraftMotionLightState motion_state,
    IDirect3DDevice8* device) noexcept {
    const std::uint16_t cache_key = static_cast<std::uint16_t>(
        requested_mask |
        (static_cast<std::uint16_t>(motion_state) << 8));
    auto existing = policy.composites.find(cache_key);
    if (existing != policy.composites.end()) {
        existing->second.last_used = ++g_emissive_composite_use_clock;
        return existing->second.texture;
    }

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
        const auto intensity_percent =
            a2fo::nebula::emissive_intensity_percent(motion_state);
        const std::size_t pixel_count =
            static_cast<std::size_t>(policy.width) * policy.height;
        base_pixels.assign(pixel_count, 0xff000000u);
        for (std::size_t source_index = 0;
             source_index < policy.pixels.size(); ++source_index) {
            if ((loaded_mask & (1u << source_index)) == 0) continue;
            const EmissiveSourcePixels& source =
                policy.pixels[source_index];
            const std::uint32_t percent = intensity_percent[source_index];
            if (!source.dense.empty()) {
                for (std::size_t pixel_index = 0;
                     pixel_index < pixel_count; ++pixel_index) {
                    base_pixels[pixel_index] =
                        a2fo::nebula::merge_emissive_pixel(
                            base_pixels[pixel_index],
                            source.dense[pixel_index], percent);
                }
            } else {
                for (const SparseEmissivePixel& pixel : source.sparse) {
                    if (pixel.index >= base_pixels.size()) continue;
                    base_pixels[pixel.index] =
                        a2fo::nebula::merge_emissive_pixel(
                            base_pixels[pixel.index], pixel.pixel, percent);
                }
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
            texture, std::move(base_pixels),
            policy.width, policy.height)) {
        texture->Release();
        return ensure_black_emissive_texture(device);
    }
    while (policy.composites.size() >=
           kMaximumEmissiveCompositesPerMaterial) {
        if (!evict_oldest_emissive_composite(policy)) break;
    }
    const std::size_t estimated_bytes = emissive_texture_bytes(
        policy.width, policy.height);
    policy.composites.emplace(
        cache_key, EmissiveComposite{
            texture, estimated_bytes, ++g_emissive_composite_use_clock});
    g_emissive_composite_cache_bytes += estimated_bytes;
    trim_emissive_composite_cache();
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

a2fo::nebula::CraftMotionLightState current_craft_motion_light(
    const void* craft) noexcept {
    if (!craft) return a2fo::nebula::CraftMotionLightState::idle;

    float velocity_squared = -1.0f;
    const auto* velocity = static_cast<const std::uint8_t*>(craft) +
        kGameObjectVelocityOffset;
    if (readable_range(velocity, sizeof(float) * 3u)) {
        float components[3]{};
        std::memcpy(components, velocity, sizeof(components));
        velocity_squared = components[0] * components[0] +
            components[1] * components[1] +
            components[2] * components[2];
    }

    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    const float maximum_impulse_speed = read_at<float>(
        object_class, kCraftClassImpulseSpeedOffset, -1.0f);

    std::uint32_t warp_effect_state =
        a2fo::nebula::kUnknownWarpEffectState;
    void* physics = read_at<void*>(craft, kCraftPhysicsOffset, nullptr);
    void* physics_vtable = read_at<void*>(physics, 0, nullptr);
    if (physics_vtable == at(g_armada, kTrekPhysicsVtableRva)) {
        warp_effect_state = read_at<std::uint32_t>(
            physics, kTrekPhysicsWarpEffectStateOffset,
            a2fo::nebula::kUnknownWarpEffectState);
    }

    return a2fo::nebula::classify_craft_motion_light(
        warp_effect_state, velocity_squared, maximum_impulse_speed);
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

std::string current_texture_key(
    IDirect3DDevice8* device, DWORD stage) noexcept {
    if (!device || !g_armada) return {};
    IDirect3DBaseTexture8* active_texture = nullptr;
    if (FAILED(device->GetTexture(stage, &active_texture)) ||
        !active_texture) {
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

std::uint32_t current_craft_cloak_state(void* craft) noexcept {
    void* controller = read_at<void*>(
        craft, kGameObjectCloakControllerOffset, nullptr);
    return controller
        ? read_at<std::uint32_t>(
              controller, kCloakControllerStateOffset, 0)
        : 0;
}

bool claim_mapped_texture_cloak_diagnostic(std::uint32_t bit) noexcept {
    const LONG flag = static_cast<LONG>(1u << bit);
    LONG observed = InterlockedCompareExchange(
        &g_mapped_texture_cloak_diagnostic_mask, 0, 0);
    while ((observed & flag) == 0) {
        const LONG replaced = InterlockedCompareExchange(
            &g_mapped_texture_cloak_diagnostic_mask,
            observed | flag, observed);
        if (replaced == observed) return true;
        observed = replaced;
    }
    return false;
}

void log_mapped_texture_cloak_draw_state(
    IDirect3DDevice8* device, bool dot3_route) noexcept {
    if (!g_mapped_texture_cloak_diagnostics_enabled || !device) return;
    void* craft = current_render_craft();
    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    if (!object_class ||
        (g_emissive_policies.find(object_class) ==
             g_emissive_policies.end() &&
         g_specular_policies.find(object_class) ==
             g_specular_policies.end())) {
        return;
    }

    const std::uint32_t raw_cloak_state = current_craft_cloak_state(craft);
    const std::uint32_t cloak_bucket = std::min<std::uint32_t>(
        raw_cloak_state, 4);
    const std::uint32_t bit = (dot3_route ? 5u : 0u) + cloak_bucket;
    if (!claim_mapped_texture_cloak_diagnostic(bit)) return;

    IDirect3DBaseTexture8* textures[4]{};
    std::array<std::string, 4> texture_keys{};
    DWORD colour_operations[4]{};
    for (DWORD stage = 0; stage < 4; ++stage) {
        device->GetTexture(stage, &textures[stage]);
        texture_keys[stage] = current_texture_key(device, stage);
        device->GetTextureStageState(
            stage, D3DTSS_COLOROP, &colour_operations[stage]);
    }
    DWORD pixel_shader = 0;
    DWORD vertex_shader = 0;
    DWORD alpha_blend = 0;
    DWORD alpha_test = 0;
    DWORD source_blend = 0;
    DWORD destination_blend = 0;
    DWORD z_write = 0;
    device->GetPixelShader(&pixel_shader);
    device->GetVertexShader(&vertex_shader);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &alpha_blend);
    device->GetRenderState(D3DRS_ALPHATESTENABLE, &alpha_test);
    device->GetRenderState(D3DRS_SRCBLEND, &source_blend);
    device->GetRenderState(D3DRS_DESTBLEND, &destination_blend);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &z_write);

    char message[1200]{};
    std::snprintf(
        message, sizeof(message),
        "Mapped texture cloak diagnostic: state=%lu route=%s, "
        "stage0='%s'(%s) stage1='%s'(%s) stage2='%s'(%s) "
        "stage3='%s'(%s), colourOps=%lu/%lu/%lu/%lu, "
        "PS=0x%08lx VS=0x%08lx, blend=%lu alphaTest=%lu "
        "src=%lu dst=%lu zwrite=%lu",
        static_cast<unsigned long>(raw_cloak_state),
        dot3_route ? "DOT3" : "fixed/workspace",
        texture_keys[0].empty() ? "<unknown>" : texture_keys[0].c_str(),
        textures[0] ? "set" : "empty",
        texture_keys[1].empty() ? "<unknown>" : texture_keys[1].c_str(),
        textures[1] ? "set" : "empty",
        texture_keys[2].empty() ? "<unknown>" : texture_keys[2].c_str(),
        textures[2] ? "set" : "empty",
        texture_keys[3].empty() ? "<unknown>" : texture_keys[3].c_str(),
        textures[3] ? "set" : "empty",
        static_cast<unsigned long>(colour_operations[0]),
        static_cast<unsigned long>(colour_operations[1]),
        static_cast<unsigned long>(colour_operations[2]),
        static_cast<unsigned long>(colour_operations[3]),
        static_cast<unsigned long>(pixel_shader),
        static_cast<unsigned long>(vertex_shader),
        static_cast<unsigned long>(alpha_blend),
        static_cast<unsigned long>(alpha_test),
        static_cast<unsigned long>(source_blend),
        static_cast<unsigned long>(destination_blend),
        static_cast<unsigned long>(z_write));
    log_line(message);
    for (IDirect3DBaseTexture8* texture : textures) {
        if (texture) texture->Release();
    }
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

    // Armada's ordinary material path places the diffuse texture on stage 0.
    // Its DOT3 path instead places the normal map on stage 0 and the diffuse
    // texture on stage 1 before requesting the pixel-shader handle. Check both
    // so the same indexed emissive policy works for bumped and unbumped SODs.
    bool identified_live_texture = false;
    for (DWORD stage = 0; stage < 2; ++stage) {
        const std::string diffuse_key = current_texture_key(device, stage);
        if (diffuse_key.empty()) continue;
        identified_live_texture = true;
        for (auto& material : policy.materials) {
            if (material && material->diffuse_key == diffuse_key) {
                if (InterlockedCompareExchange(
                        &g_logged_indexed_diffuse_binding, 1, 0) == 0) {
                    char message[320]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Indexed emissive material matched live diffuse '%s' "
                        "on texture stage %lu",
                        diffuse_key.c_str(),
                        static_cast<unsigned long>(stage));
                    log_line(message);
                }
                return material.get();
            }
        }
    }
    if (!identified_live_texture && InterlockedCompareExchange(
                   &g_logged_diffuse_lookup_failure, 1, 0) == 0) {
        log_line("Could not identify a live Storm3D diffuse texture; indexed "
                 "emissive maps skipped for that material");
    }
    return wildcard;
}

bool select_current_emissive_texture(
    IDirect3DDevice8* device, IDirect3DTexture8** selected) noexcept {
    if (selected) *selected = nullptr;
    if (!a2fo::renderer_emissive_maps_enabled()) return false;
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
    const a2fo::nebula::CraftMotionLightState motion_state =
        current_craft_motion_light(craft);
    IDirect3DTexture8* texture = build_emissive_composite(
        *material, mask, motion_state, device);
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

SpecularMaterialPolicy* select_current_specular_material(
    IDirect3DDevice8* device) noexcept {
    if (!device || !a2fo::renderer_specular_maps_enabled()) return nullptr;
    void* craft = current_render_craft();
    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    const auto found = g_specular_policies.find(object_class);
    if (found == g_specular_policies.end() || !found->second) return nullptr;

    for (DWORD stage = 0; stage < 2; ++stage) {
        const std::string diffuse_key = current_texture_key(device, stage);
        if (diffuse_key.empty()) continue;
        for (auto& material : found->second->materials) {
            if (material && material->diffuse_key == diffuse_key) {
                if (InterlockedCompareExchange(
                        &g_logged_specular_binding, 1, 0) == 0) {
                    char message[320]{};
                    std::snprintf(
                        message, sizeof(message),
                        "Indexed specular material matched live diffuse '%s' "
                        "on texture stage %lu",
                        diffuse_key.c_str(),
                        static_cast<unsigned long>(stage));
                    log_line(message);
                }
                return material.get();
            }
        }
    }
    return nullptr;
}

IDirect3DTexture8* load_specular_texture(
    SpecularMaterialPolicy& material,
    IDirect3DDevice8* device) noexcept {
    if (!device || material.path.empty() || !g_create_texture_from_file) {
        return nullptr;
    }
    if (material.device == device) {
        if (material.texture) return material.texture;
        if (material.load_attempted) return nullptr;
    } else {
        if (material.texture) material.texture->Release();
        material.texture = nullptr;
        material.device = device;
        material.load_attempted = false;
    }

    material.load_attempted = true;
    IDirect3DTexture8* texture = nullptr;
    const HRESULT result = g_create_texture_from_file(
        device, material.path.c_str(), kD3dxDefault, kD3dxDefault,
        kD3dxDefault, 0, D3DFMT_UNKNOWN, D3DPOOL_MANAGED,
        kD3dxDefault, kD3dxDefault, 0, nullptr, nullptr, &texture);
    if (FAILED(result) || !texture) {
        char message[1400]{};
        std::snprintf(
            message, sizeof(message),
            "Could not load specular texture '%s' (HRESULT 0x%08lx)",
            material.path.c_str(),
            static_cast<unsigned long>(static_cast<std::uint32_t>(result)));
        log_line(message);
        if (texture) texture->Release();
        return nullptr;
    }
    material.texture = texture;
    return texture;
}

IDirect3DTexture8* current_specular_texture(
    IDirect3DDevice8* device) noexcept {
    SpecularMaterialPolicy* material =
        select_current_specular_material(device);
    return material ? load_specular_texture(*material, device) : nullptr;
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

bool validate_armada_module(HMODULE module) noexcept {
    const auto identity = a2fo::supported_armada::identify(module);
    if (identity == a2fo::supported_armada::Identity::unsupported) {
        log_line("ArmadaL.exe is not the supported Fleet Operations build");
        return false;
    }
    if (identity == a2fo::supported_armada::Identity::normalized) {
        log_line(
            "Accepted normalized ArmadaL.exe header for early DX8 hooks");
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
    g_specular_pixel_shader_path = join_path(
        root, "Shaders\\dx8\\pixel\\ps_specular.nvv");
    if (GetFileAttributesA(g_pixel_shader_path.c_str()) ==
            INVALID_FILE_ATTRIBUTES) {
        log_line("Required DX8 pixel-shader assets are missing from "
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
    g_create_texture_from_file = imported_function<CreateTextureFromFileEx>(
        g_d3dx, "D3DXCreateTextureFromFileExA");
    if (!g_assemble_shader) {
        log_line("D3DX81ab.dll lacks the required DX8 shader assembler");
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
    if (!signature_matches(g_armada, kCompileDot3MeshRva,
                           kExpectedCompileDot3Mesh.data(),
                           kExpectedCompileDot3Mesh.size()) ||
        (kUseFleetOpsShaderHandleRoute &&
         !signature_matches(g_armada, kGetShaderHandleRva,
                            kExpectedGetShaderHandle.data(),
                            kExpectedGetShaderHandle.size())) ||
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
        (g_dxvk_backend_active &&
         !signature_matches(g_armada, kDot3DrawRva,
                            kExpectedDot3Draw.data(),
                            kExpectedDot3Draw.size())) ||
        !signature_matches(g_armada, kWorkspaceDx8DrawRva,
                           kExpectedWorkspaceDx8Draw.data(),
                           kExpectedWorkspaceDx8Draw.size()) ||
        (g_native_framebuffer_bloom_enabled &&
         !signature_matches(g_armada, kFrameBloomRva,
                            kExpectedFrameBloom.data(),
                            kExpectedFrameBloom.size())) ||
        !signature_matches(g_armada, kDeviceResetRva,
                           kExpectedDeviceReset.data(),
                           kExpectedDeviceReset.size()) ||
        !signature_matches(g_fleet_ops, kFleetOpsDeviceDestroyCallbackRva,
                           kExpectedFleetOpsDeviceDestroyCallback.data(),
                           kExpectedFleetOpsDeviceDestroyCallback.size()) ||
        !signature_matches(g_fleet_ops, kAlphaTransitionRva,
                           kExpectedAlphaTransition.data(),
                           kExpectedAlphaTransition.size()) ||
        (g_dxvk_backend_active &&
         !signature_matches(g_fleet_ops, kFleetOpsDot3DrawRva,
                            kExpectedFleetOpsDot3Draw.data(),
                            kExpectedFleetOpsDot3Draw.size()))) {
        log_line("A Nebula renderer code/data signature differs from the "
                 "supported binaries; runtime inactive");
        return false;
    }

    if (!kUseFleetOpsShaderHandleRoute) {
        g_get_shader_handle_original = nullptr;
        return true;
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

bool compile_specular_pixel_shader() noexcept {
    if (g_compiled_specular_pixel_shader) return true;
    if (!g_assemble_shader || g_specular_pixel_shader_path.empty() ||
        GetFileAttributesA(g_specular_pixel_shader_path.c_str()) ==
            INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    ID3DXBuffer* compiled = nullptr;
    ID3DXBuffer* errors = nullptr;
    const HRESULT result = g_assemble_shader(
        g_specular_pixel_shader_path.c_str(), 0, nullptr, &compiled, &errors);
    if (FAILED(result) || !compiled || !compiled->GetBufferPointer()) {
        if (InterlockedCompareExchange(
                &g_logged_specular_shader_failure, 1, 0) == 0) {
            log_hresult("Assemble specular DX8 pixel shader", result);
            if (errors && errors->GetBufferPointer()) {
                char message[384]{};
                const char* error_text = static_cast<const char*>(
                    errors->GetBufferPointer());
                const int text_size = static_cast<int>(
                    errors->GetBufferSize() < 320
                        ? errors->GetBufferSize() : 320);
                std::snprintf(message, sizeof(message),
                              "Specular pixel shader assembler: %.*s",
                              text_size, error_text);
                log_line(message);
            }
        }
        if (compiled) compiled->Release();
        if (errors) errors->Release();
        return false;
    }
    if (errors) errors->Release();
    g_compiled_specular_pixel_shader = compiled;
    log_line("DX8 bump/emissive/specular pixel shader assembled");
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

void release_damage_decal_textures() noexcept {
    for (auto& entry : g_damage_decal_textures) {
        if (entry.second) entry.second->Release();
    }
    g_damage_decal_textures.clear();
}

void invalidate_device_resources(IDirect3DDevice8* device) noexcept {
    release_bloom_resources();
    if (device) {
        // No extension texture or shader may remain bound while its backing
        // cache is released or while Armada resets this device.
        const bool can_set_pixel_shader = device_method_is_callable(
            device, kSetPixelShaderVtableIndex);
        const bool can_set_texture = device_method_is_callable(
            device, kSetTextureVtableIndex);
        const bool can_delete_pixel_shader = device_method_is_callable(
            device, kDeletePixelShaderVtableIndex);
        if (can_set_pixel_shader) device->SetPixelShader(0);
        if (can_set_texture) {
            device->SetTexture(1, nullptr);
            device->SetTexture(2, nullptr);
            device->SetTexture(3, nullptr);
        }
        if (can_delete_pixel_shader) {
            if (g_pixel_shader != 0) {
                device->DeletePixelShader(g_pixel_shader);
            }
            if (g_specular_pixel_shader != 0) {
                device->DeletePixelShader(g_specular_pixel_shader);
            }
        } else if ((g_pixel_shader != 0 || g_specular_pixel_shader != 0) &&
                   InterlockedCompareExchange(
                       &g_logged_device_cleanup_method_unavailable,
                       1, 0) == 0) {
            log_line(
                "DX8 device cleanup skipped an unavailable pixel-shader "
                "deletion route");
        }
    }
    g_pixel_shader = 0;
    g_specular_pixel_shader = 0;
    g_specular_shader_selected = false;
    InterlockedExchange(&g_pixel_shader_rejected, 0);
    InterlockedExchange(&g_specular_shader_rejected, 0);
    release_all_emissive_gpu_caches();
    release_all_specular_gpu_caches();
    release_damage_decal_textures();
}

void adopt_live_device(IDirect3DDevice8* device) noexcept {
    const bool switched = a2fo::adopt_com_owner(
        g_device, device, [](IDirect3DDevice8* previous) noexcept {
            // The owned reference guarantees that every virtual call and
            // resource release remains valid even after Armada discarded its
            // last reference during a gameplay/editor mode transition.
            invalidate_device_resources(previous);
        });
    if (switched && InterlockedCompareExchange(
            &g_logged_device_switch, 1, 0) == 0) {
        log_line("Retained Armada's live DX8 device with owned COM lifetime");
    }
}

void release_destroying_device(void* wrapper) noexcept {
    IDirect3DDevice8* device = read_at<IDirect3DDevice8*>(
        wrapper, kStormDeviceOffset, nullptr);
    const bool released = a2fo::release_matching_com_owner(
        g_device, device, [](IDirect3DDevice8* previous) noexcept {
            invalidate_device_resources(previous);
        });
    if (released && InterlockedCompareExchange(
            &g_logged_device_destroy_release, 1, 0) == 0) {
        log_line("Released retained DX8 device before Fleet Ops destruction");
    }
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
    log_mapped_texture_cloak_draw_state(device, false);

    IDirect3DTexture8* emissive = nullptr;
    if (!select_current_emissive_texture(device, &emissive) || !emissive) {
        return;
    }

    IDirect3DBaseTexture8* native_stage_one = nullptr;
    if (FAILED(device->GetTexture(1, &native_stage_one))) return;
    const DWORD stage = native_stage_one ? 2 : 1;
    if (native_stage_one) native_stage_one->Release();

    state.device = device;
    state.stage = stage;
    if (FAILED(device->GetTexture(stage, &state.texture)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLOROP, &state.colour_operation)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLORARG1, &state.colour_argument1)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLORARG2, &state.colour_argument2)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAOP, &state.alpha_operation)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAARG1, &state.alpha_argument1)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAARG2, &state.alpha_argument2)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_TEXCOORDINDEX, &state.coordinate_index)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MINFILTER, &state.minimum_filter)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MAGFILTER, &state.magnification_filter)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MIPFILTER, &state.mipmap_filter))) {
        if (state.texture) {
            state.texture->Release();
            state.texture = nullptr;
        }
        return;
    }

    // The system dxwrapper -> d3d8to9 -> ReShade chain can invalidate Fleet
    // Ops' cached DOT3 vertex-shader object when a redundant SetPixelShader(0)
    // occurs between materials. The standard native path is already fixed
    // function, so preserve its shader state on that backend. DXVK tolerates
    // the explicit reset and uses it to isolate extension material stages.
    if (g_dxvk_backend_active) device->SetPixelShader(0);
    device->SetTexture(stage, emissive);
    device->SetTextureStageState(
        stage, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        stage, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        stage, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        stage, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        stage, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_CURRENT));
    device->SetTextureStageState(
        stage, D3DTSS_COLORARG2, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        stage, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_ADD));
    // Preserve the material's stage-0 alpha. Emissive RGB never makes an
    // otherwise transparent texel opaque.
    device->SetTextureStageState(
        stage, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_CURRENT));
    device->SetTextureStageState(
        stage, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    state.emissive = emissive;
    state.stage_state_modified = true;
    state.active = true;
    if (InterlockedCompareExchange(
            &g_logged_standard_emissive, 1, 0) == 0) {
        log_line(stage == 2
            ? "Fixed-function emissive stage 2 activated; native stage 1 texture preserved"
            : "Fixed-function emissive texture stage 1 activated");
    }
}

void log_nonvb_registered_pass(UINT pass_index, UINT pass_count) noexcept {
    if (!g_mapped_texture_cloak_diagnostics_enabled) return;
    void* craft = current_render_craft();
    void* object_class = read_at<void*>(craft, kCraftClassOffset, nullptr);
    const auto policy = g_emissive_policies.find(object_class);
    if (policy == g_emissive_policies.end() || !policy->second) return;

    const LONG diagnostic_index = InterlockedIncrement(
        &g_logged_nonvb_registered_passes);
    if (diagnostic_index > 24) return;

    void* renderer = read_at<void*>(
        at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
    IDirect3DDevice8* device = resolve_live_device(renderer);
    if (!device) return;

    IDirect3DBaseTexture8* textures[3]{};
    DWORD colour_operations[3]{D3DTOP_DISABLE, D3DTOP_DISABLE,
                               D3DTOP_DISABLE};
    for (DWORD stage = 0; stage < 3; ++stage) {
        device->GetTexture(stage, &textures[stage]);
        device->GetTextureStageState(
            stage, D3DTSS_COLOROP, &colour_operations[stage]);
    }
    DWORD pixel_shader = 0;
    DWORD vertex_shader = 0;
    DWORD alpha_blend = 0;
    DWORD source_blend = 0;
    DWORD destination_blend = 0;
    DWORD z_write = 0;
    device->GetPixelShader(&pixel_shader);
    device->GetVertexShader(&vertex_shader);
    device->GetRenderState(D3DRS_ALPHABLENDENABLE, &alpha_blend);
    device->GetRenderState(D3DRS_SRCBLEND, &source_blend);
    device->GetRenderState(D3DRS_DESTBLEND, &destination_blend);
    device->GetRenderState(D3DRS_ZWRITEENABLE, &z_write);
    const std::string stage_zero_key = current_texture_key(device, 0);

    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Registered legacy pass %lu/%lu: stage0='%s', textures=%s/%s/%s, "
        "colourOps=%lu/%lu/%lu, PS=0x%08lx VS=0x%08lx, "
        "blend=%lu src=%lu dst=%lu zwrite=%lu",
        static_cast<unsigned long>(pass_index + 1),
        static_cast<unsigned long>(pass_count),
        stage_zero_key.empty() ? "<unknown>" : stage_zero_key.c_str(),
        textures[0] ? "set" : "empty",
        textures[1] ? "set" : "empty",
        textures[2] ? "set" : "empty",
        static_cast<unsigned long>(colour_operations[0]),
        static_cast<unsigned long>(colour_operations[1]),
        static_cast<unsigned long>(colour_operations[2]),
        static_cast<unsigned long>(pixel_shader),
        static_cast<unsigned long>(vertex_shader),
        static_cast<unsigned long>(alpha_blend),
        static_cast<unsigned long>(source_blend),
        static_cast<unsigned long>(destination_blend),
        static_cast<unsigned long>(z_write));
    log_line(message);
    for (IDirect3DBaseTexture8* texture : textures) {
        if (texture) texture->Release();
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
    IDirect3DSurface8* back_buffer = nullptr;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    UINT frame_width = 0;
    UINT frame_height = 0;
    BloomTarget mask{};
    BloomTarget blur_a{};
    BloomTarget blur_b{};
    bool mask_prepared = false;
    bool mask_dirty = false;
};

BloomResources g_bloom{};

void release_bloom_target(BloomTarget& target) noexcept {
    if (target.surface) target.surface->Release();
    if (target.texture) target.texture->Release();
    target = BloomTarget{};
}

void release_bloom_resources() noexcept {
    release_bloom_target(g_bloom.mask);
    release_bloom_target(g_bloom.blur_a);
    release_bloom_target(g_bloom.blur_b);
    if (g_bloom.back_buffer) g_bloom.back_buffer->Release();
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
    IDirect3DSurface8* back_buffer = nullptr;
    HRESULT result = device->GetBackBuffer(
        0, D3DBACKBUFFER_TYPE_MONO, &back_buffer);
    if (FAILED(result) || !back_buffer) return false;
    D3DSURFACE_DESC description{};
    result = back_buffer->GetDesc(&description);
    if (FAILED(result) || description.Width == 0 || description.Height == 0) {
        back_buffer->Release();
        return false;
    }
    if (g_bloom.device == device &&
        g_bloom.back_buffer == back_buffer &&
        g_bloom.frame_width == description.Width &&
        g_bloom.frame_height == description.Height &&
        g_bloom.format == description.Format && g_bloom.mask.texture &&
        g_bloom.blur_a.texture && g_bloom.blur_b.texture) {
        back_buffer->Release();
        return true;
    }

    release_bloom_resources();
    g_bloom.device = device;
    g_bloom.back_buffer = back_buffer;
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

bool primary_back_buffer_is_current(IDirect3DDevice8* device,
                                    bool log_offscreen) noexcept {
    if (!device || device != g_bloom.device || !g_bloom.back_buffer) {
        return false;
    }
    IDirect3DSurface8* render_target = nullptr;
    const HRESULT result = device->GetRenderTarget(&render_target);
    if (FAILED(result) || !render_target) return false;
    const bool primary = render_target == g_bloom.back_buffer;
    render_target->Release();
    if (!primary && log_offscreen && InterlockedCompareExchange(
            &g_logged_emissive_offscreen_skip, 1, 0) == 0) {
        log_line("Skipped an off-screen emissive render while collecting the "
                 "primary framebuffer bloom mask");
    }
    return primary;
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
    device->SetTexture(0, emissive);
    device->SetTexture(1, nullptr);
    device->SetTexture(2, nullptr);
    // Fleet Operations keeps its programmable DOT3 vertex shader selected at
    // this draw. That shader emits only oT0. With programmable vertex
    // processing, D3DTSS_TEXCOORDINDEX cannot redirect oT0 into fixed-function
    // texture stage 1, so the old stage-1 mask path sampled an absent oT1 and
    // produced a completely black render target. Sample the isolated authored
    // emissive map directly from stage 0/oT0 instead.
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        0, D3DTSS_TEXTURETRANSFORMFLAGS,
        static_cast<DWORD>(D3DTTFF_DISABLE));
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSU, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSV, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        0, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(
        1, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    device->SetTextureStageState(
        1, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_DISABLE));
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
    device->SetRenderState(D3DRS_WRAP0, 0);
}

void configure_specular_overlay_state(
    IDirect3DDevice8* device, IDirect3DTexture8* specular) noexcept {
    device->SetPixelShader(0);
    device->SetTexture(0, specular);
    device->SetTexture(1, nullptr);
    device->SetTexture(2, nullptr);
    device->SetTexture(3, nullptr);
    device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        0, D3DTSS_TEXTURETRANSFORMFLAGS,
        static_cast<DWORD>(D3DTTFF_DISABLE));
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSU, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        0, D3DTSS_ADDRESSV, static_cast<DWORD>(D3DTADDRESS_WRAP));
    device->SetTextureStageState(
        0, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        0, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_COLORARG2, static_cast<DWORD>(D3DTA_TFACTOR));
    device->SetTextureStageState(
        0, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_MODULATE));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_TEXTURE));
    device->SetTextureStageState(
        0, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    for (DWORD stage = 1; stage < 4; ++stage) {
        device->SetTextureStageState(
            stage, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_DISABLE));
    }
    device->SetRenderState(
        D3DRS_TEXTUREFACTOR, kSpecularOverlayTextureFactor);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(
        D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_DESTBLEND, static_cast<DWORD>(D3DBLEND_ONE));
    device->SetRenderState(
        D3DRS_ZENABLE, static_cast<DWORD>(D3DZB_TRUE));
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(
        D3DRS_ZFUNC, static_cast<DWORD>(D3DCMP_LESSEQUAL));
    device->SetRenderState(D3DRS_WRAP0, 0);
}

void draw_dot3_specular_overlay(StandardTextureStageState& state) noexcept {
    if (!state.device || !state.specular || state.vertex_count == 0 ||
        state.vertex_count > 3000000u || state.primitive_count == 0 ||
        state.primitive_count > 1000000u) {
        return;
    }
    SavedDeviceDrawState saved{};
    if (!save_device_draw_state(state.device, saved)) return;

    IDirect3DDevice8* device = state.device;
    configure_specular_overlay_state(device, state.specular);
    const HRESULT result = device->DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST, 0, state.vertex_count, 0,
        state.primitive_count);
    restore_device_draw_state(device, saved);
    if (SUCCEEDED(result)) {
        if (InterlockedCompareExchange(
                &g_logged_specular_draw, 1, 0) == 0) {
            log_line("Native-bump-safe specular overlay drawn after the "
                     "final DOT3 material pass");
        }
    } else if (InterlockedCompareExchange(
                   &g_logged_specular_shader_failure, 1, 0) == 0) {
        log_hresult("Draw native-bump-safe specular overlay", result);
    }
}

template <typename Draw>
void accumulate_emissive_mask(StandardTextureStageState& state,
                              Draw&& draw) noexcept {
    if (!state.active || !state.device || !state.emissive ||
        g_emissive_mask_draw_active) {
        return;
    }
    if (!ensure_bloom_resources(state.device) ||
        !primary_back_buffer_is_current(state.device, true)) return;
    SavedDeviceDrawState saved{};
    if (!save_device_draw_state(state.device, saved)) return;

    IDirect3DDevice8* device = state.device;
    g_emissive_mask_draw_active = true;
    device->SetTexture(0, nullptr);
    device->SetTexture(1, nullptr);
    HRESULT result = device->SetRenderTarget(g_bloom.mask.surface, nullptr);
    if (SUCCEEDED(result) && !g_bloom.mask_prepared) {
        // D3D8 Clear is viewport-scoped. Clear the complete physical mask
        // before restoring the scene's possibly smaller viewport, otherwise
        // pixels outside the first viewport can survive from an older frame.
        set_target_viewport(device, g_bloom.mask);
        result = device->Clear(0, nullptr, D3DCLEAR_TARGET,
                               D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
        if (SUCCEEDED(result)) g_bloom.mask_prepared = true;
    }
    if (SUCCEEDED(result)) {
        // The scene renderer can use a viewport smaller than the physical
        // backbuffer. Replaying its unchanged projection through a forced
        // full-screen viewport scales and shifts the bloom geometry toward
        // the upper-left. The mask matches the backbuffer dimensions, so the
        // draw must retain its exact native viewport.
        result = device->SetViewport(&saved.viewport);
    }
    if (SUCCEEDED(result)) {
        configure_emissive_mask_state(device, state.emissive);
        result = draw(device);
        if (SUCCEEDED(result)) {
            g_bloom.mask_dirty = true;
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
        state,
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
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        !live_device || !workspace ||
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
        *selected_state,
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
        state,
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
        state,
        [vertex_count, primitive_count](IDirect3DDevice8* live_device) {
            return live_device->DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST, 0, vertex_count, 0, primitive_count);
        });
}

void dot3_emissive_mask_draw_counts(IDirect3DDevice8* device,
                                    UINT vertex_count,
                                    UINT primitive_count) noexcept {
    if (!device || vertex_count == 0 || primitive_count == 0 ||
        primitive_count > 1000000u ||
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        !current_render_craft()) {
        return;
    }
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
        state,
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

struct Matrix34 {
    float values[12]{};
};

struct DamageDecalVertex {
    float x;
    float y;
    float z;
    D3DCOLOR colour;
    float u;
    float v;
};

std::array<float, 3> transform_decal_point(
    const Matrix34& matrix, const std::array<float, 3>& point) noexcept {
    return {
        matrix.values[0] * point[0] + matrix.values[3] * point[1] +
            matrix.values[6] * point[2] + matrix.values[9],
        matrix.values[1] * point[0] + matrix.values[4] * point[1] +
            matrix.values[7] * point[2] + matrix.values[10],
        matrix.values[2] * point[0] + matrix.values[5] * point[1] +
            matrix.values[8] * point[2] + matrix.values[11],
    };
}

IDirect3DTexture8* damage_decal_texture(
    IDirect3DDevice8* device, const std::string& path,
    bool use_colour_key = false, D3DCOLOR colour_key = 0) noexcept {
    char colour_key_text[16]{};
    std::snprintf(
        colour_key_text, sizeof(colour_key_text), "#%c%06lx",
        use_colour_key ? 'k' : 'n',
        static_cast<unsigned long>(colour_key & 0x00ffffffu));
    const std::string cache_key = path + colour_key_text;
    const auto found = g_damage_decal_textures.find(cache_key);
    if (found != g_damage_decal_textures.end()) return found->second;
    if (!device || !g_create_texture_from_file || path.empty()) {
        if (InterlockedCompareExchange(
                &g_logged_damage_decal_texture_unavailable, 1, 0) == 0) {
            char message[256]{};
            std::snprintf(
                message, sizeof(message),
                "Damage decal texture loader unavailable: device=%s loader=%s path=%s",
                device ? "ready" : "missing",
                g_create_texture_from_file ? "ready" : "missing",
                path.empty() ? "empty" : "ready");
            log_line(message);
        }
        return nullptr;
    }
    IDirect3DTexture8* texture = nullptr;
    const HRESULT result = g_create_texture_from_file(
        device, path.c_str(), kD3dxDefault, kD3dxDefault, 0, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, kD3dxDefault, kD3dxDefault,
        use_colour_key ? colour_key : 0, nullptr, nullptr, &texture);
    if (FAILED(result) || !texture) {
        if (InterlockedCompareExchange(
                &g_logged_damage_decal_texture_failure, 1, 0) == 0) {
            char message[1400]{};
            std::snprintf(message, sizeof(message),
                          "Could not load damage decal texture '%s' "
                          "(HRESULT 0x%08lx)", path.c_str(),
                          static_cast<unsigned long>(
                              static_cast<std::uint32_t>(result)));
            log_line(message);
        }
        if (texture) texture->Release();
        return nullptr;
    }
    g_damage_decal_textures.emplace(cache_key, texture);
    return texture;
}

float craft_damage_fraction(void* craft, void* object_class,
                            std::uint32_t system_index) noexcept {
    if (!craft || !object_class) return 0.0f;
    double current = 0.0;
    double maximum = 0.0;
    if (system_index < kSubsystemRecordSize / kSubsystemRecordSize * 5u) {
        void* systems = read_at<void*>(
            craft, kCraftSubsystemsOffset, nullptr);
        const auto* record = systems
            ? static_cast<const std::uint8_t*>(systems) +
                system_index * kSubsystemRecordSize : nullptr;
        const std::int32_t maximum_value = read_at<std::int32_t>(
            record, 0x04, 0);
        current = read_at<double>(record, 0x18, 0.0);
        maximum = static_cast<double>(maximum_value);
    } else if (system_index == 5u) {
        current = static_cast<double>(read_at<float>(
            craft, kGameObjectCurrentHealthOffset, 0.0f));
        maximum = static_cast<double>(read_at<float>(
            craft, kGameObjectMaximumHealthOffset, 0.0f));
    }
    if (!std::isfinite(current) || !std::isfinite(maximum) ||
        maximum <= 0.0001) return 0.0f;
    return std::clamp(
        static_cast<float>(1.0 - current / maximum), 0.0f, 1.0f);
}

bool draw_decal_quad(
    void* craft, void* node, const std::array<float, 3>& offset,
    const std::array<float, 3>& rotation_degrees,
    const std::array<float, 2>& size, IDirect3DTexture8* texture,
    IDirect3DDevice8* device, bool flip_u = false) noexcept {
    if (!craft || !node || !texture || !device) return false;
    Matrix34 transform{};
    const std::uintptr_t result = a2fo_nebula_call_thiscall_2(
        at(g_armada, kEntityGetWorldTransformRva), craft,
        reinterpret_cast<std::uintptr_t>(&transform),
        reinterpret_cast<std::uintptr_t>(node));
    if (result != reinterpret_cast<std::uintptr_t>(&transform)) {
        if (InterlockedCompareExchange(
                &g_logged_damage_decal_transform_failure, 1, 0) == 0) {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                          "Decal hardpoint transform failed: craft=%p node=%p result=%p",
                          craft, node,
                          reinterpret_cast<void*>(result));
            log_line(message);
        }
        return false;
    }

    const float half_width = size[0] * 0.5f;
    const float half_height = size[1] * 0.5f;
    const float lift = std::max(size[0], size[1]) * 0.0025f;
    const std::array<std::array<float, 3>, 4> corners{{
        {{-half_width, half_height, lift}},
        {{half_width, half_height, lift}},
        {{-half_width, -half_height, lift}},
        {{half_width, -half_height, lift}},
    }};
    const float left_u = flip_u ? 1.0f : 0.0f;
    const float right_u = flip_u ? 0.0f : 1.0f;
    const std::array<std::array<float, 2>, 4> uv{{
        {{left_u, 0.0f}}, {{right_u, 0.0f}},
        {{left_u, 1.0f}}, {{right_u, 1.0f}},
    }};
    std::array<DamageDecalVertex, 4> vertices{};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        auto local = a2fo::decal::rotate_xyz(
            corners[index], rotation_degrees);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            local[axis] += offset[axis];
        }
        const auto world = transform_decal_point(transform, local);
        vertices[index] = {world[0], world[1], world[2], 0xffffffffu,
                           uv[index][0], uv[index][1]};
    }
    device->SetTexture(0, texture);
    const HRESULT draw_result = device->DrawPrimitiveUP(
        D3DPT_TRIANGLESTRIP, 2, vertices.data(),
        sizeof(DamageDecalVertex));
    if (FAILED(draw_result) && InterlockedCompareExchange(
            &g_logged_damage_decal_draw_failure, 1, 0) == 0) {
        log_hresult("Decal DrawPrimitiveUP", draw_result);
    }
    return SUCCEEDED(draw_result);
}

bool draw_damage_decal(void* craft, const DamageDecal& decal,
                       IDirect3DDevice8* device) noexcept {
    IDirect3DTexture8* texture = damage_decal_texture(
        device, decal.texture_path);
    return draw_decal_quad(
        craft, decal.node, decal.offset, decal.rotation_degrees,
        decal.size, texture, device);
}

bool prepare_decal_render_state(
    IDirect3DDevice8* device, DWORD* state_block) noexcept {
    if (!device || !state_block) return false;
    *state_block = 0;
    const HRESULT state_result =
        device->CreateStateBlock(D3DSBT_ALL, state_block);
    if (FAILED(state_result)) {
        if (InterlockedCompareExchange(
                &g_logged_damage_decal_state_failure, 1, 0) == 0) {
            log_hresult("Decal CreateStateBlock", state_result);
        }
        return false;
    }
    device->SetPixelShader(0);
    device->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    D3DMATRIX identity{};
    identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
    device->SetTransform(D3DTS_WORLD, &identity);
    device->SetRenderState(D3DRS_LIGHTING, FALSE);
    device->SetRenderState(D3DRS_FOGENABLE, FALSE);
    device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    device->SetRenderState(D3DRS_ZBIAS, 1);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    device->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    return true;
}

void render_damage_decals(void* craft) noexcept {
    if (!craft || g_damage_decal_policies.empty()) return;
    void* object_class = read_live_at<void*>(
        craft, kCraftClassOffset, nullptr);
    const auto found = g_damage_decal_policies.find(object_class);
    if (found == g_damage_decal_policies.end() || !found->second) return;
    const DamageDecalClassPolicy& policy = *found->second;
    if (policy.decals.empty() || policy.damage_threshold <= 0.0f) return;
    if (InterlockedCompareExchange(
            &g_logged_damage_decal_policy_render, 1, 0) == 0) {
        const float current_hull = read_at<float>(
            craft, kGameObjectCurrentHealthOffset, 0.0f);
        const float maximum_hull = read_at<float>(
            craft, kGameObjectMaximumHealthOffset, 0.0f);
        char message[256]{};
        std::snprintf(
            message, sizeof(message),
            "Registered decal craft reached render: craft=%p class=%p hull=%.3f/%.3f threshold=%.3f decals=%u",
            craft, object_class, static_cast<double>(current_hull),
            static_cast<double>(maximum_hull),
            static_cast<double>(policy.damage_threshold),
            static_cast<unsigned>(policy.decals.size()));
        log_line(message);
    }
    std::array<std::uint32_t, 6> active_bands{};
    std::array<float, 6> damage_fractions{};
    for (std::size_t index = 0; index < active_bands.size(); ++index) {
        damage_fractions[index] = craft_damage_fraction(
            craft, object_class, static_cast<std::uint32_t>(index));
        active_bands[index] = static_cast<std::uint32_t>(
            std::floor((damage_fractions[index] + 0.00001f) /
                       policy.damage_threshold));
    }
    const DamageDecal* first_active = nullptr;
    for (const DamageDecal& decal : policy.decals) {
        if (decal.system_index < active_bands.size() &&
            (decal.threshold_index == 0 ||
             decal.threshold_index <= active_bands[decal.system_index])) {
            first_active = &decal;
            break;
        }
    }
    if (!first_active) return;
    if (InterlockedCompareExchange(
            &g_logged_damage_decal_threshold_active, 1, 0) == 0) {
        char message[320]{};
        std::snprintf(
            message, sizeof(message),
            "Damage decal became active: system=%u damage=%.3f band=%u required=%u%s",
            static_cast<unsigned>(first_active->system_index),
            static_cast<double>(
                damage_fractions[first_active->system_index]),
            static_cast<unsigned>(
                active_bands[first_active->system_index]),
            static_cast<unsigned>(first_active->threshold_index),
            first_active->threshold_index == 0 ? " (preview)" : "");
        log_line(message);
    }
    if (!g_device) {
        void* renderer = read_at<void*>(
            at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
        IDirect3DDevice8* device = resolve_live_device(renderer);
        if (device) adopt_live_device(device);
    }
    if (!g_device) {
        if (InterlockedCompareExchange(
                &g_logged_damage_decal_device_unavailable, 1, 0) == 0) {
            log_line("Damage decal is active, but the live DX8 device could not be resolved");
        }
        return;
    }
    if (!g_create_texture_from_file && !load_d3dx()) return;
    DWORD state_block = 0;
    if (!prepare_decal_render_state(g_device, &state_block)) return;

    std::size_t drawn = 0;
    for (const DamageDecal& decal : policy.decals) {
        if (decal.system_index >= active_bands.size() ||
            (decal.threshold_index != 0 &&
             decal.threshold_index > active_bands[decal.system_index])) {
            continue;
        }
        if (draw_damage_decal(craft, decal, g_device)) ++drawn;
    }
    g_device->ApplyStateBlock(state_block);
    g_device->DeleteStateBlock(state_block);
    if (drawn != 0 && InterlockedCompareExchange(
            &g_logged_damage_decal_draw, 1, 0) == 0) {
        log_line("Rendered the first subsystem/hull damage decal");
    }
}

IDirect3DTexture8* native_storm_texture(void* texture) noexcept {
    if (!texture || !g_armada) return nullptr;
    void* renderer = read_at<void*>(
        at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
    const std::uint32_t device_index = read_at<std::uint32_t>(
        renderer, kCurrentDeviceIndexOffset, kMaximumStormDeviceCount);
    if (device_index >= kMaximumStormDeviceCount) return nullptr;
    void* device_texture = read_at<void*>(
        texture, kStorm3DTextureDeviceArrayOffset +
            device_index * sizeof(void*), nullptr);
    IDirect3DTexture8* native = read_at<IDirect3DTexture8*>(
        device_texture, kStorm3DDeviceTextureNativeOffset, nullptr);
    return native && readable_range(native, sizeof(void*)) ? native : nullptr;
}

void* fleet_ops_craft_class_enhancement(void* object_class) noexcept {
    if (!object_class) return nullptr;
    const std::uint32_t byte0 = read_at<std::uint8_t>(
        object_class, kCraftClassEnhancementPointerByte0Offset, 0);
    const std::uint32_t byte1 = read_at<std::uint8_t>(
        object_class, kCraftClassEnhancementPointerByte1Offset, 0);
    const std::uint32_t byte2 = read_at<std::uint8_t>(
        object_class, kCraftClassEnhancementPointerByte2Offset, 0);
    const std::uint32_t byte3 = read_at<std::uint8_t>(
        object_class, kCraftClassEnhancementPointerByte3Offset, 0);
    const std::uintptr_t encoded = byte0 | (byte1 << 8u) |
        (byte2 << 16u) | (byte3 << 24u);
    void* wrapper = reinterpret_cast<void*>(encoded);
    if (!readable_range(
            wrapper, kEnhancementWrapperSidecarOffset + sizeof(void*))) {
        return nullptr;
    }
    void* sidecar = read_at<void*>(
        wrapper, kEnhancementWrapperSidecarOffset, nullptr);
    if (!readable_range(sidecar, kEnhancementMinimumReadableSize) ||
        read_at<void*>(sidecar, kEnhancementSidecarOwnerOffset, nullptr) !=
            object_class) {
        return nullptr;
    }
    return sidecar;
}

IDirect3DTexture8* selected_logo_texture(
    void* craft, void* object_class, const LogoDecal& decal,
    IDirect3DDevice8* device, std::int32_t* selected_index,
    std::size_t* available_count) noexcept {
    if (selected_index) *selected_index = -1;
    if (available_count) *available_count = 0;
    const std::int32_t index = read_at<std::int32_t>(
        craft, kCraftNameIndexOffset, -1);
    if (selected_index) *selected_index = index;
    if (index < 0) return nullptr;

    if (!decal.texture_paths.empty()) {
        if (available_count) *available_count = decal.texture_paths.size();
        if (static_cast<std::size_t>(index) < decal.texture_paths.size()) {
            const std::string& path = decal.texture_paths[index];
            if (!path.empty() &&
                (g_create_texture_from_file || load_d3dx())) {
                IDirect3DTexture8* texture = damage_decal_texture(
                    device, path, decal.use_colour_key,
                    decal.colour_key);
                if (texture) return texture;
            }
        }
        // An empty/missing loose row may still be a valid FPQ asset already
        // loaded by Fleet Operations. Continue into its native texture table.
    }

    void* enhancement = fleet_ops_craft_class_enhancement(object_class);
    if (!enhancement) return nullptr;
    const std::uintptr_t begin = read_at<std::uintptr_t>(
        enhancement, kEnhancementLogoNamesBeginOffset, 0);
    const std::uintptr_t end = read_at<std::uintptr_t>(
        enhancement, kEnhancementLogoNamesEndOffset, 0);
    if (end < begin || (end - begin) % sizeof(void*) != 0) return nullptr;
    const std::size_t count = (end - begin) / sizeof(void*);
    if (available_count) *available_count = count;
    if (count > kMaximumCraftLogoCount ||
        static_cast<std::size_t>(index) >= count) {
        return nullptr;
    }
    void* table = read_at<void*>(
        enhancement, kEnhancementLogoTexturesOffset, nullptr);
    void* storm_texture = read_at<void*>(
        table, static_cast<std::size_t>(index) * sizeof(void*), nullptr);
    return native_storm_texture(storm_texture);
}

void render_logo_decals(void* craft) noexcept {
    if (!craft || g_logo_decal_policies.empty()) return;
    void* object_class = read_live_at<void*>(
        craft, kCraftClassOffset, nullptr);
    const auto found = g_logo_decal_policies.find(object_class);
    if (found == g_logo_decal_policies.end() || !found->second ||
        found->second->decals.empty()) {
        return;
    }
    const LogoDecalClassPolicy& policy = *found->second;
    if (InterlockedCompareExchange(
            &g_logged_logo_decal_policy_render, 1, 0) == 0) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "Registered logo-decal craft reached render: craft=%p class=%p placements=%u",
            craft, object_class,
            static_cast<unsigned>(policy.decals.size()));
        log_line(message);
    }
    if (!g_device) {
        void* renderer = read_at<void*>(
            at(g_armada, kGraphicsEnginePointerRva), 0, nullptr);
        IDirect3DDevice8* device = resolve_live_device(renderer);
        if (device) adopt_live_device(device);
    }
    if (!g_device) return;

    DWORD state_block = 0;
    if (!prepare_decal_render_state(g_device, &state_block)) return;
    std::size_t drawn = 0;
    std::int32_t failed_index = -1;
    std::size_t failed_count = 0;
    for (const LogoDecal& decal : policy.decals) {
        IDirect3DTexture8* texture = selected_logo_texture(
            craft, object_class, decal, g_device, &failed_index,
            &failed_count);
        if (!texture) continue;
        if (draw_decal_quad(
                craft, decal.node, decal.offset, decal.rotation_degrees,
                decal.size, texture, g_device, decal.flip_u)) {
            ++drawn;
        }
    }
    g_device->ApplyStateBlock(state_block);
    g_device->DeleteStateBlock(state_block);
    if (drawn != 0 && InterlockedCompareExchange(
            &g_logged_logo_decal_draw, 1, 0) == 0) {
        log_line("Rendered the first selected craft logo decal");
    } else if (drawn == 0 && InterlockedCompareExchange(
            &g_logged_logo_decal_selection_failure, 1, 0) == 0) {
        char message[224]{};
        std::snprintf(
            message, sizeof(message),
            "Logo decal had no usable selected texture: possibleCraftNames index=%ld available logo rows=%u",
            static_cast<long>(failed_index),
            static_cast<unsigned>(failed_count));
        log_line(message);
    }
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

void composite_native_bloom(IDirect3DDevice8* device) noexcept {
    if (!device || device != g_bloom.device || g_emissive_mask_draw_active) {
        return;
    }
    if (!primary_back_buffer_is_current(device, false)) return;
    if (!g_bloom.mask_dirty) return;
    SavedDeviceDrawState saved{};
    if (!save_device_draw_state(device, saved)) return;

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
        BloomTarget* halo = &g_bloom.blur_a;
        device->SetTexture(0, nullptr);
        device->SetTexture(1, nullptr);
        result = device->SetRenderTarget(
            saved.render_target, saved.depth_stencil);
        if (SUCCEEDED(result)) {
            // RefreshDisplay can inherit the last scene/UI viewport. The mask
            // is stored in physical backbuffer coordinates; mapping all of it
            // into that smaller viewport creates a large displaced "ghost".
            // Composite over the complete primary backbuffer, then let the
            // saved state restore Fleet Ops' viewport below.
            D3DVIEWPORT8 frame_viewport{};
            frame_viewport.Width = g_bloom.frame_width;
            frame_viewport.Height = g_bloom.frame_height;
            frame_viewport.MinZ = 0.0f;
            frame_viewport.MaxZ = 1.0f;
            result = device->SetViewport(&frame_viewport);
            if (SUCCEEDED(result)) {
                configure_quad_state(device);
                configure_halo_composite_state(device, *halo);
                device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                device->SetRenderState(
                    D3DRS_SRCBLEND, static_cast<DWORD>(D3DBLEND_ONE));
                device->SetRenderState(
                    D3DRS_DESTBLEND,
                    static_cast<DWORD>(D3DBLEND_INVSRCCOLOR));
            }
            for (std::size_t pass = 0;
                 pass < kHaloCompositePasses && SUCCEEDED(result); ++pass) {
                result = draw_textured_quad(
                    device, frame_viewport, 0.0f, 0.0f,
                    static_cast<float>(halo->active_width) /
                        static_cast<float>(halo->texture_width),
                    static_cast<float>(halo->active_height) /
                        static_cast<float>(halo->texture_height),
                    255);
            }
            succeeded = SUCCEEDED(result);
        }
    }

    restore_device_draw_state(device, saved);
    g_bloom.mask_dirty = false;
    g_bloom.mask_prepared = false;
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
    const DWORD stage = state.stage;
    device->SetTexture(stage, state.texture);
    if (state.stage_state_modified) {
        device->SetTextureStageState(
            stage, D3DTSS_COLOROP, state.colour_operation);
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG1, state.colour_argument1);
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG2, state.colour_argument2);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAOP, state.alpha_operation);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAARG1, state.alpha_argument1);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAARG2, state.alpha_argument2);
        device->SetTextureStageState(
            stage, D3DTSS_TEXCOORDINDEX, state.coordinate_index);
        device->SetTextureStageState(
            stage, D3DTSS_MINFILTER, state.minimum_filter);
        device->SetTextureStageState(
            stage, D3DTSS_MAGFILTER, state.magnification_filter);
        device->SetTextureStageState(
            stage, D3DTSS_MIPFILTER, state.mipmap_filter);
    }
    if (state.texture) state.texture->Release();
    state = StandardTextureStageState{};
}

void dot3_emissive_pre_draw(IDirect3DDevice8* device, UINT vertex_count,
                            UINT primitive_count) noexcept {
    if (g_dot3_state_depth >= g_dot3_state_stack.size()) {
        ++g_dot3_state_overflow;
        return;
    }
    StandardTextureStageState& state =
        g_dot3_state_stack[g_dot3_state_depth++];
    state = StandardTextureStageState{};
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        !device) {
        return;
    }
    if (!current_render_craft()) {
        if (InterlockedCompareExchange(
                &g_logged_dot3_without_context, 1, 0) == 0) {
            log_line("DOT3 indexed draw had no enclosing Craft render context");
        }
        return;
    }

    adopt_live_device(device);
    log_mapped_texture_cloak_draw_state(device, true);
    IDirect3DTexture8* emissive = nullptr;
    const bool has_emissive =
        select_current_emissive_texture(device, &emissive) && emissive;
    IDirect3DTexture8* specular = current_specular_texture(device);
    const bool fixed_function_fallback = kPreserveFleetOpsNativeDot3 ||
        InterlockedCompareExchange(&g_pixel_shader_rejected, 0, 0) != 0;
    if (specular && !fixed_function_fallback &&
        !ensure_specular_pixel_shader(device)) {
        specular = nullptr;
    }
    if (!has_emissive && !specular) {
        if (InterlockedCompareExchange(
                &g_logged_dot3_without_emissive, 1, 0) == 0) {
            log_line("DOT3 indexed draw had no matching emissive or specular texture");
        }
        return;
    }

    state.device = device;
    state.specular = specular;
    state.vertex_count = vertex_count;
    state.primitive_count = primitive_count;
    if (fixed_function_fallback && !has_emissive) {
        // A specular-only material needs no changes around FO's native draw.
        // Retain the scoped state solely so post-draw can add its isolated
        // overlay after the native bump/diffuse result is complete.
        state.active = true;
        if (InterlockedCompareExchange(
                &g_logged_dot3_emissive_fallback, 1, 0) == 0) {
            log_line("Native DOT3 scoped specular overlay activated");
        }
        return;
    }
    constexpr DWORD stage = 2;
    state.stage = stage;
    if (FAILED(device->GetTexture(stage, &state.texture))) return;

    if (!fixed_function_fallback &&
        FAILED(device->GetTexture(3, &state.secondary_texture))) {
        if (state.texture) {
            state.texture->Release();
            state.texture = nullptr;
        }
        return;
    }
    if (fixed_function_fallback &&
        (FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLOROP, &state.colour_operation)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLORARG1, &state.colour_argument1)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_COLORARG2, &state.colour_argument2)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAOP, &state.alpha_operation)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAARG1, &state.alpha_argument1)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_ALPHAARG2, &state.alpha_argument2)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_TEXCOORDINDEX, &state.coordinate_index)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MINFILTER, &state.minimum_filter)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MAGFILTER, &state.magnification_filter)) ||
        FAILED(device->GetTextureStageState(
            stage, D3DTSS_MIPFILTER, &state.mipmap_filter)))) {
        if (state.texture) {
            state.texture->Release();
            state.texture = nullptr;
        }
        return;
    }

    if (specular && !fixed_function_fallback) {
        if (FAILED(device->GetPixelShader(&state.pixel_shader)) ||
            FAILED(device->SetPixelShader(g_specular_pixel_shader))) {
            specular = nullptr;
            if (!has_emissive) {
                if (state.texture) state.texture->Release();
                if (state.secondary_texture) {
                    state.secondary_texture->Release();
                }
                state = StandardTextureStageState{};
                return;
            }
        } else {
            state.pixel_shader_modified = true;
            if (InterlockedCompareExchange(
                    &g_logged_specular_draw, 1, 0) == 0) {
                log_line("DOT3 specular pixel shader bound at the indexed draw");
            }
        }
    }

    device->SetTexture(
        stage, has_emissive ? emissive : ensure_black_emissive_texture(device));
    if (fixed_function_fallback) {
        device->SetPixelShader(0);
        device->SetTextureStageState(stage, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(
            stage, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
        device->SetTextureStageState(
            stage, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
        device->SetTextureStageState(
            stage, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG1, static_cast<DWORD>(D3DTA_CURRENT));
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG2, static_cast<DWORD>(D3DTA_TEXTURE));
        device->SetTextureStageState(
            stage, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_ADD));
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAARG1, static_cast<DWORD>(D3DTA_CURRENT));
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAOP, static_cast<DWORD>(D3DTOP_SELECTARG1));
        state.stage_state_modified = true;
    } else {
        device->SetTexture(3, specular);
        const float light_bias[4]{
            g_bump_light_bias, g_bump_light_bias,
            g_bump_light_bias, g_bump_light_bias};
        const float multiplier[4]{
            g_emissive_bump_multiplier, g_emissive_bump_multiplier,
            g_emissive_bump_multiplier, g_emissive_bump_multiplier};
        const float diffuse_restore[4]{
            g_emissive_diffuse_restore, g_emissive_diffuse_restore,
            g_emissive_diffuse_restore, g_emissive_diffuse_restore};
        device->SetPixelShaderConstant(0, light_bias, 1);
        device->SetPixelShaderConstant(1, multiplier, 1);
        device->SetPixelShaderConstant(2, diffuse_restore, 1);
    }
    state.emissive = emissive;
    state.active = true;
    if (InterlockedCompareExchange(
            &g_logged_dot3_emissive_fallback, 1, 0) == 0) {
        log_line(fixed_function_fallback
            ? specular
                ? has_emissive
                    ? "Native DOT3 emissive fallback and scoped specular overlay activated"
                    : "Native DOT3 scoped specular overlay activated"
                : "Native DOT3 emissive texture-stage fallback activated"
            : specular
                ? "DOT3 emissive/specular samplers bound at the indexed draw"
                : "DOT3 emissive sampler bound at the indexed draw");
    }
}

void dot3_emissive_post_draw() noexcept {
    if (g_dot3_state_overflow != 0) {
        --g_dot3_state_overflow;
        return;
    }
    if (g_dot3_state_depth == 0) return;
    StandardTextureStageState& state =
        g_dot3_state_stack[--g_dot3_state_depth];
    if (!state.active || !state.device) {
        state = StandardTextureStageState{};
        return;
    }
    constexpr DWORD stage = 2;
    IDirect3DDevice8* device = state.device;
    draw_dot3_specular_overlay(state);
    if (state.stage_state_modified || state.pixel_shader_modified) {
        device->SetTexture(stage, state.texture);
        device->SetTexture(3, state.secondary_texture);
    }
    if (state.pixel_shader_modified) {
        device->SetPixelShader(state.pixel_shader);
    }
    if (state.stage_state_modified) {
        device->SetTextureStageState(
            stage, D3DTSS_COLOROP, state.colour_operation);
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG1, state.colour_argument1);
        device->SetTextureStageState(
            stage, D3DTSS_COLORARG2, state.colour_argument2);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAOP, state.alpha_operation);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAARG1, state.alpha_argument1);
        device->SetTextureStageState(
            stage, D3DTSS_ALPHAARG2, state.alpha_argument2);
        device->SetTextureStageState(
            stage, D3DTSS_TEXCOORDINDEX, state.coordinate_index);
        device->SetTextureStageState(
            stage, D3DTSS_MINFILTER, state.minimum_filter);
        device->SetTextureStageState(
            stage, D3DTSS_MAGFILTER, state.magnification_filter);
        device->SetTextureStageState(
            stage, D3DTSS_MIPFILTER, state.mipmap_filter);
    }
    if (state.texture) state.texture->Release();
    if (state.secondary_texture) state.secondary_texture->Release();
    state = StandardTextureStageState{};
}

bool ensure_pixel_shader(IDirect3DDevice8* device) noexcept {
    if (!device || !compile_pixel_shader()) return false;
    if (g_device != device) adopt_live_device(device);
    if (g_pixel_shader != 0) return true;
    if (InterlockedCompareExchange(
            &g_pixel_shader_rejected, 0, 0) != 0) {
        return false;
    }

    // Armada has just resolved/created Fleet Operations' stock DOT3 vertex
    // shader in the original GetShaderHandle gateway. Add only the extension
    // pixel stage here, using the live device selected by the renderer.
    DWORD pixel_shader = 0;
    const HRESULT result = device->CreatePixelShader(
        static_cast<const DWORD*>(g_compiled_pixel_shader->GetBufferPointer()),
        &pixel_shader);
    if (FAILED(result) || pixel_shader == 0) {
        if (InterlockedCompareExchange(&g_logged_create_failure, 1, 0) == 0) {
            log_hresult("IDirect3DDevice8::CreatePixelShader", result);
            log_line("Live DX8 device rejected the optional Nebula pixel "
                     "shader; native DOT3 and emissive fallback retained");
        }
        // Avoid repeating the expensive failing driver call on every DOT3
        // material. Fixed-function and DOT3-stage emissives remain active.
        InterlockedExchange(&g_pixel_shader_rejected, 1);
        return false;
    }
    g_pixel_shader = pixel_shader;
    log_line("DX8 Nebula pixel shader created on Armada's live device");
    return true;
}

bool ensure_specular_pixel_shader(IDirect3DDevice8* device) noexcept {
    if (!device || InterlockedCompareExchange(
            &g_specular_shader_rejected, 0, 0) != 0) {
        return false;
    }
    if (!compile_specular_pixel_shader()) {
        InterlockedExchange(&g_specular_shader_rejected, 1);
        return false;
    }
    if (g_device != device) adopt_live_device(device);
    if (g_specular_pixel_shader != 0) return true;

    DWORD shader = 0;
    const HRESULT result = device->CreatePixelShader(
        static_cast<const DWORD*>(
            g_compiled_specular_pixel_shader->GetBufferPointer()),
        &shader);
    if (FAILED(result) || shader == 0) {
        if (InterlockedCompareExchange(
                &g_logged_specular_shader_failure, 1, 0) == 0) {
            log_hresult("Create specular DX8 pixel shader", result);
        }
        InterlockedExchange(&g_specular_shader_rejected, 1);
        return false;
    }
    g_specular_pixel_shader = shader;
    log_line("DX8 bump/emissive/specular pixel shader created");
    return true;
}

std::uintptr_t set_pixel_shader_impl(
    void* self, std::uintptr_t shader_id) noexcept {
    g_specular_shader_selected = false;
    const std::uintptr_t original = g_get_shader_handle_original
        ? a2fo_nebula_call_thiscall_1(
              g_get_shader_handle_original, self, shader_id)
        : 0;
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0) {
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

    if (kPreserveFleetOpsNativeDot3) {
        // This hook runs while FO still has the normal map on stage 0 and
        // before its native per-light DOT3 draws. A pixel shader selected here
        // survives into those draws and leaks normal-map RGB into the hull.
        // Leave the complete native sequence untouched; the exact final-draw
        // hook can still add a mapped emissive texture through stage 2.
        device->SetPixelShader(0);
        device->SetTexture(2, nullptr);
        device->SetTexture(3, nullptr);
        if (InterlockedCompareExchange(
                &g_logged_native_dot3_preserved, 1, 0) == 0) {
            log_line("Fleet Operations native DOT3 bump lighting preserved; "
                     "extension pixel shaders deferred");
        }
        return original;
    }
    if (!ensure_pixel_shader(device)) return original;

    IDirect3DTexture8* specular = current_specular_texture(device);
    DWORD selected_pixel_shader = g_pixel_shader;
    if (specular && ensure_specular_pixel_shader(device)) {
        selected_pixel_shader = g_specular_pixel_shader;
    } else {
        specular = nullptr;
    }

    device->SetTextureStageState(
        2, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_SELECTARG1));
    device->SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 0);
    device->SetTextureStageState(
        2, D3DTSS_MINFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        2, D3DTSS_MAGFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTextureStageState(
        2, D3DTSS_MIPFILTER, static_cast<DWORD>(D3DTEXF_LINEAR));
    device->SetTexture(2, current_emissive_texture(device));
    device->SetTexture(3, specular);

    const float emissive_multiplier[4]{
        g_emissive_bump_multiplier, g_emissive_bump_multiplier,
        g_emissive_bump_multiplier, g_emissive_bump_multiplier};
    const float bump_light_bias[4]{
        g_bump_light_bias, g_bump_light_bias,
        g_bump_light_bias, g_bump_light_bias};
    const float emissive_diffuse_restore[4]{
        g_emissive_diffuse_restore, g_emissive_diffuse_restore,
        g_emissive_diffuse_restore, g_emissive_diffuse_restore};
    device->SetPixelShaderConstant(0, bump_light_bias, 1);
    device->SetPixelShaderConstant(1, emissive_multiplier, 1);
    device->SetPixelShaderConstant(2, emissive_diffuse_restore, 1);

    HRESULT result = device->SetPixelShader(selected_pixel_shader);
    if (FAILED(result) && selected_pixel_shader == g_specular_pixel_shader) {
        if (g_specular_pixel_shader != 0) {
            device->DeletePixelShader(g_specular_pixel_shader);
            g_specular_pixel_shader = 0;
        }
        InterlockedExchange(&g_specular_shader_rejected, 1);
        device->SetTexture(3, nullptr);
        result = device->SetPixelShader(g_pixel_shader);
    }
    if (FAILED(result)) {
        if (InterlockedCompareExchange(&g_logged_set_failure, 1, 0) == 0) {
            log_hresult("IDirect3DDevice8::SetPixelShader", result);
        }
        // A device reset can invalidate a shader handle without changing the
        // wrapper pointer. Keep native DOT3 and the scoped emissive fallback
        // for this device rather than retrying an invalid handle each draw.
        g_pixel_shader = 0;
        InterlockedExchange(&g_pixel_shader_rejected, 1);
        device->SetTexture(2, nullptr);
        device->SetTexture(3, nullptr);
        return original;
    }
    g_specular_shader_selected =
        selected_pixel_shader == g_specular_pixel_shader &&
        g_specular_pixel_shader != 0;
    // This Fleet Ops route resolves its stock DOT3 vertex-shader handle. Keep
    // returning that native handle: our pixel shader is selected only through
    // the side-effect above. Returning g_pixel_shader here makes the caller
    // pass a pixel-shader handle to SetVertexShader and drops geometry.
    return original;
}

void disable_pixel_shader_impl() noexcept {
    g_specular_shader_selected = false;
    if (!kUseFleetOpsShaderHandleRoute) return;
    if (InterlockedCompareExchange(&g_runtime_enabled, 0, 0) != 0 &&
        g_device) {
        // Resume Fleet Operations immediately after this call. Unlike the
        // upstream naked epilogue, its alpha geometry is not discarded.
        g_device->SetPixelShader(0);
        g_device->SetTexture(2, nullptr);
        g_device->SetTexture(3, nullptr);
        g_device->SetTextureStageState(
            2, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
        g_device->SetTextureStageState(
            3, D3DTSS_COLOROP, static_cast<DWORD>(D3DTOP_DISABLE));
    }
}

void* __cdecl compile_dot3_mesh_hook(const void* mesh) noexcept {
    // This is the first safe point after process attach. Activate here so no
    // file/D3DX work occurs under the Windows loader lock, while Fleet Ops'
    // stock DOT3 vertex shader remains untouched.
    const LONG state = InterlockedCompareExchange(&g_activation_state, 1, 0);
    if (state == 0) {
        bool activated = false;
        if (!controller_is_installed()) {
            log_line("Controller DLL is absent; early renderer remains native");
        } else if (command_line_requests_dx9()) {
            log_line("/d3d9 detected; the initial Nebula integration supports "
                     "DX8 only and remains inactive");
        } else if (!load_d3dx()) {
            log_line("DX8 mapped-texture loading failed; native rendering "
                     "retained");
        } else if (!kUseFleetOpsShaderHandleRoute) {
            InterlockedExchange(&g_runtime_enabled, 1);
            activated = true;
            log_line("DX8 mapped-material lighting activated in native DOT3 "
                     "compatibility mode; Fleet Ops shader-handle route left "
                     "untouched");
        } else if (!shader_assets_available() || !compile_pixel_shader()) {
            log_line("DX8 shader activation failed; native rendering retained");
        } else if (!install_get_shader_route()) {
            log_line("DX8 draw-time shader routing failed; native rendering "
                     "retained");
        } else {
            InterlockedExchange(&g_runtime_enabled, 1);
            activated = true;
            log_line("DX8 mapped-material lighting activated with Fleet "
                     "Operations' stock DOT3 vertex shader");
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
        at(g_fleet_ops, kFleetOpsDeviceDestroyCallbackRva),
        function_address(&a2fo_nebula_device_destroy_hook),
        kExpectedFleetOpsDeviceDestroyCallback.size(),
        kExpectedFleetOpsDeviceDestroyCallback.data(),
        g_device_destroy_hook);
    if (installed) {
        g_a2fo_nebula_device_destroy_gateway =
            g_device_destroy_hook.gateway;
    }

    installed = installed && a2fo::install_inline_hook(
        at(g_fleet_ops, kAlphaTransitionRva),
        function_address(&a2fo_nebula_alpha_hook),
        kExpectedAlphaTransition.size(), kExpectedAlphaTransition.data(),
        g_alpha_hook);
    if (installed) g_a2fo_nebula_alpha_gateway = g_alpha_hook.gateway;

    if (g_dxvk_backend_active) {
        installed = installed && a2fo::install_inline_hook(
            at(g_fleet_ops, kFleetOpsDot3DrawRva),
            function_address(&a2fo_nebula_fleetops_dot3_draw_hook),
            kExpectedFleetOpsDot3Draw.size(),
            kExpectedFleetOpsDot3Draw.data(), g_fleetops_dot3_draw_hook);
        if (installed) {
            g_a2fo_nebula_fleetops_dot3_draw_return =
                at(g_fleet_ops,
                   kFleetOpsDot3DrawRva +
                       kExpectedFleetOpsDot3Draw.size());
        }
    }

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

    // Older Armada SODs bypass both DOT3 and ST3D_Standard_MeshVB. Keep these
    // pass-loop hooks for material diagnostics and the optional mask replay;
    // the visible emissive stage is scoped at WorkspaceDirectX8's exact draw
    // below, after Submit has finished changing device state.
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
        g_a2fo_nebula_workspace_dx8_draw_return =
            at(g_armada,
               kWorkspaceDx8DrawRva + kExpectedWorkspaceDx8Draw.size());
    }

    // DOT3 materials need a native fixed-function emissive fallback on DXVK.
    // Do not interpose either DOT3 draw on the system backend: Windows'
    // dxwrapper/D3D8 chain has been observed to retain driver-private draw
    // state across this boundary and crash in its native callback even when
    // the extension makes no texture changes. Native Fleet Ops bumps remain
    // fully enabled; only extension emissive/specular work on bumped materials
    // is unavailable on that backend.
    if (g_dxvk_backend_active) {
        installed = installed && a2fo::install_inline_hook(
            at(g_armada, kDot3DrawRva),
            function_address(&a2fo_nebula_dot3_draw_hook),
            kExpectedDot3Draw.size(), kExpectedDot3Draw.data(),
            g_dot3_draw_hook);
        if (installed) {
            g_a2fo_nebula_dot3_draw_gateway = g_dot3_draw_hook.gateway;
            g_a2fo_nebula_dot3_draw_return =
                at(g_armada, kDot3DrawRva + kExpectedDot3Draw.size());
        }
    } else {
        log_line("System renderer keeps native Fleet Operations DOT3 bump "
                 "draws unintercepted");
    }

    if (g_native_framebuffer_bloom_enabled) {
        installed = installed && a2fo::install_inline_hook(
            at(g_armada, kFrameBeginRva),
            function_address(&a2fo_nebula_frame_begin_hook),
            kExpectedFrameBegin.size(), kExpectedFrameBegin.data(),
            g_frame_begin_hook);
        if (installed) {
            g_a2fo_nebula_frame_begin_gateway =
                g_frame_begin_hook.gateway;
        }

        installed = installed && a2fo::install_inline_hook(
            at(g_armada, kFrameBloomRva),
            function_address(&a2fo_nebula_frame_bloom_hook),
            kExpectedFrameBloom.size(), kExpectedFrameBloom.data(),
            g_frame_bloom_hook);
        if (installed) {
            g_a2fo_nebula_frame_bloom_gateway =
                g_frame_bloom_hook.gateway;
        }

    } else {
        log_line("Native framebuffer bloom is unavailable on the active "
                 "system renderer; ODF emissive material rendering remains active");
    }

    // Device ownership and cache invalidation apply to every DX8 backend, not
    // only DXVK's optional framebuffer-bloom path.
    installed = installed && a2fo::install_inline_hook(
        at(g_armada, kDeviceResetRva),
        function_address(&a2fo_nebula_device_reset_hook),
        kExpectedDeviceReset.size(), kExpectedDeviceReset.data(),
        g_device_reset_hook);
    if (installed) {
        g_a2fo_nebula_device_reset_gateway =
            g_device_reset_hook.gateway;
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
    if (g_native_framebuffer_bloom_enabled) {
        standard_emissive_mask_draw(mesh_stream);
    }
    standard_emissive_post_draw();
}

extern "C" void __cdecl a2fo_nebula_nonvb_pre(
    UINT pass_index, UINT pass_count) {
    if (InterlockedCompareExchange(
            &g_logged_nonvb_hook_reached, 1, 0) == 0) {
        log_line("Legacy non-VB direct-submit draw boundary reached");
    }
    log_nonvb_registered_pass(pass_index, pass_count);
}

extern "C" void __cdecl a2fo_nebula_nonvb_post(void* workspace) {
    if (g_native_framebuffer_bloom_enabled) {
        nonvb_emissive_mask_draw(workspace);
    }
}

extern "C" void __cdecl a2fo_nebula_workspace_dx8_draw(
    IDirect3DDevice8* device, void* workspace, UINT vertex_count,
    UINT start_index, UINT primitive_count) {
    standard_emissive_pre_draw();
    if (g_native_framebuffer_bloom_enabled) {
        workspace_dx8_emissive_mask_draw(
            device, workspace, vertex_count, start_index, primitive_count);
    }
}

extern "C" void __cdecl a2fo_nebula_workspace_dx8_post() {
    standard_emissive_post_draw();
}

extern "C" void __cdecl a2fo_nebula_frame_begin(
    IDirect3DDevice8* device) {
    if (!g_native_framebuffer_bloom_enabled || !device) return;
    // EndScene normally consumes and resets the mask. BeginScene is the
    // authoritative safety boundary: discard any stale logical contents before
    // the first emissive replay clears the private target for this frame.
    g_bloom.mask_prepared = false;
    g_bloom.mask_dirty = false;
}

extern "C" void __cdecl a2fo_nebula_frame_bloom(
    IDirect3DDevice8* device) {
    if (g_native_framebuffer_bloom_enabled &&
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) != 0) {
        composite_native_bloom(device);
    }
}

extern "C" void __cdecl a2fo_nebula_before_device_reset(
    IDirect3DDevice8* device) {
    if (!device) return;
    adopt_live_device(device);
    if (device == g_device) {
        invalidate_device_resources(device);
        if (InterlockedCompareExchange(
                &g_logged_device_reset_invalidation, 1, 0) == 0) {
            log_line("Invalidated extension GPU caches before DX8 device reset");
        }
    }
}

extern "C" void __cdecl a2fo_nebula_before_device_destroy(void* wrapper) {
    release_destroying_device(wrapper);
}

extern "C" void __cdecl a2fo_nebula_dot3_draw(
    IDirect3DDevice8* device, const void* mesh_stream,
    UINT primitive_count) {
    if (InterlockedCompareExchange(
            &g_logged_dot3_hook_reached, 1, 0) == 0) {
        log_line("DOT3 indexed draw boundary reached");
    }
    const UINT vertex_count = read_at<UINT>(mesh_stream, 0x10, 0);
    dot3_emissive_pre_draw(device, vertex_count, primitive_count);
    if (g_native_framebuffer_bloom_enabled) {
        dot3_emissive_mask_draw(device, mesh_stream, primitive_count);
    }
}

extern "C" void __cdecl a2fo_nebula_dot3_post() {
    dot3_emissive_post_draw();
}

extern "C" void __cdecl a2fo_nebula_fleetops_dot3_draw(
    IDirect3DDevice8* device, UINT vertex_count, UINT primitive_count) {
    if (InterlockedCompareExchange(
            &g_logged_fleetops_dot3_hook_reached, 1, 0) == 0) {
        log_line("Fleet Ops DOT3 indexed draw boundary reached");
    }
    dot3_emissive_pre_draw(device, vertex_count, primitive_count);
    if (g_native_framebuffer_bloom_enabled) {
        dot3_emissive_mask_draw_counts(
            device, vertex_count, primitive_count);
    }
}

extern "C" void __cdecl a2fo_nebula_fleetops_dot3_post() {
    dot3_emissive_post_draw();
}

namespace a2fo {

bool install_nebula_renderer_early(HMODULE armada, HMODULE fleet_ops,
                                   const std::string& root_directory,
                                   NebulaRendererLog log) {
    g_armada = armada;
    g_fleet_ops = fleet_ops;
    g_root_directory = root_directory;
    g_log = log;
    g_dxvk_backend_active = load_dxvk_backend_policy();
    if (g_dxvk_backend_payload_detected) {
        log_line(g_dxvk_backend_ini_claimed
                     ? "Managed DXVK payload detected as the active renderer"
                     : "Managed DXVK payload detected; overriding stale "
                       "AppliedBackend=system");
    } else if (g_dxvk_backend_ini_claimed) {
        log_line("AppliedBackend=dxvk is stale; the managed DXVK payload is "
                 "not active, so system-renderer safety policy is in use");
    }
    if (!g_dxvk_backend_active) {
        // Native Windows reaches the vendor D3D9 driver through
        // dxwrapper/d3d8to9. An AMD failure captured at Fleet Ops'
        // unintercepted DOT3 DrawIndexedPrimitive showed that merely retaining
        // and inspecting the shared DX8 device from adjacent material hooks
        // can destabilize this chain. Keep the system backend completely
        // native: no compile, material, workspace, reset, or device-lifetime
        // hook is installed, and the controller can use status() to avoid SOD
        // texture mutations as well.
        g_native_framebuffer_bloom_enabled = false;
        g_hooks_ready = false;
        InterlockedExchange(&g_runtime_enabled, 0);
        InterlockedExchange(&g_activation_state, -1);
        log_line("System renderer safety isolation active; A2FO DX8 "
                 "mapped-material hooks are not installed and Fleet "
                 "Operations retains its complete native render path");
        return true;
    }
    g_native_framebuffer_bloom_enabled =
        load_native_framebuffer_bloom_policy();
    g_mapped_texture_cloak_diagnostics_enabled =
        load_mapped_texture_cloak_diagnostics_policy();
    InterlockedExchange(&g_mapped_texture_cloak_diagnostic_mask, 0);
    if (g_native_framebuffer_bloom_enabled) {
        log_line("Native selective emissive framebuffer bloom enabled for DXVK");
    }
    if (g_mapped_texture_cloak_diagnostics_enabled) {
        log_line("Mapped-texture cloak diagnostics enabled");
    }
    if (!validate_armada_module(g_armada) ||
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

void shutdown_nebula_renderer() noexcept {
    InterlockedExchange(&g_runtime_enabled, 0);
    release_com_owner(
        g_device, [](IDirect3DDevice8* previous) noexcept {
            invalidate_device_resources(previous);
        });
}

int nebula_renderer_status() noexcept {
    const LONG state = InterlockedCompareExchange(&g_activation_state, 0, 0);
    if (state < 0) return -1;
    if (!g_hooks_ready) return 0;
    if (state == 2) return 2;
    return 1;
}

bool set_nebula_emissive_bump_multiplier(float multiplier) noexcept {
    if (!std::isfinite(multiplier) || multiplier < 0.0f ||
        multiplier > 8.0f) {
        return false;
    }
    g_emissive_bump_multiplier = multiplier;
    return true;
}

bool set_nebula_bump_light_bias(float bias) noexcept {
    if (!std::isfinite(bias) || bias < 0.0f || bias > 1.0f) {
        return false;
    }
    g_bump_light_bias = bias;
    return true;
}

bool set_nebula_emissive_diffuse_restore(float amount) noexcept {
    if (!std::isfinite(amount) || amount < 0.0f || amount > 2.0f) {
        return false;
    }
    g_emissive_diffuse_restore = amount;
    return true;
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

bool register_nebula_specular_materials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count) noexcept {
    if (!object_class || !diffuse_names || !texture_paths ||
        material_count == 0 || material_count > 256) {
        return false;
    }
    try {
        auto policy = std::make_unique<SpecularClassPolicy>();
        policy->materials.reserve(material_count);
        for (std::uint32_t index = 0; index < material_count; ++index) {
            const char* diffuse_name = diffuse_names[index];
            const char* texture_path = texture_paths[index];
            if (!diffuse_name || !*diffuse_name ||
                !texture_path || !*texture_path) {
                continue;
            }
            auto material = std::make_unique<SpecularMaterialPolicy>();
            material->diffuse_key = a2fo::nebula::normalize_texture_key(
                diffuse_name);
            if (material->diffuse_key.empty()) continue;
            material->path = texture_path;
            policy->materials.push_back(std::move(material));
        }
        if (policy->materials.empty()) return false;

        const auto existing = g_specular_policies.find(object_class);
        if (existing != g_specular_policies.end() && existing->second) {
            for (auto& material : existing->second->materials) {
                if (!material || !material->texture) continue;
                material->texture->Release();
                material->texture = nullptr;
            }
        }
        g_specular_policies[object_class] = std::move(policy);
        return true;
    } catch (...) {
        log_line("Could not retain a CraftClass specular-map policy");
        return false;
    }
}

bool register_damage_decal_class(
    void* object_class, float damage_threshold,
    const DecalDescriptor* descriptors, std::uint32_t descriptor_count)
    noexcept {
    if (!object_class || !descriptors || descriptor_count == 0 ||
        descriptor_count > 384 || !std::isfinite(damage_threshold) ||
        damage_threshold <= 0.0f || damage_threshold > 1.0f) {
        return false;
    }
    try {
        auto policy = std::make_unique<DamageDecalClassPolicy>();
        policy->damage_threshold = damage_threshold;
        policy->decals.reserve(descriptor_count);
        for (std::uint32_t index = 0; index < descriptor_count; ++index) {
            const DecalDescriptor& source = descriptors[index];
            if (source.struct_size < sizeof(DecalDescriptor) ||
                source.system_index > 5 ||
                !source.node || !source.texture_path ||
                !*source.texture_path ||
                !std::isfinite(source.size[0]) ||
                !std::isfinite(source.size[1]) ||
                source.size[0] <= 0.0f || source.size[1] <= 0.0f) {
                continue;
            }
            DamageDecal decal;
            decal.system_index = source.system_index;
            decal.threshold_index = source.threshold_index;
            decal.node = source.node;
            decal.texture_path = source.texture_path;
            std::copy_n(source.offset, 3, decal.offset.begin());
            std::copy_n(source.rotation_degrees, 3,
                        decal.rotation_degrees.begin());
            std::copy_n(source.size, 2, decal.size.begin());
            const bool finite = std::all_of(
                decal.offset.begin(), decal.offset.end(),
                [](float value) { return std::isfinite(value); }) &&
                std::all_of(
                    decal.rotation_degrees.begin(),
                    decal.rotation_degrees.end(),
                    [](float value) { return std::isfinite(value); });
            if (finite) policy->decals.push_back(std::move(decal));
        }
        if (policy->decals.empty()) return false;
        g_damage_decal_policies[object_class] = std::move(policy);
        return true;
    } catch (...) {
        log_line("Could not retain a CraftClass damage-decal policy");
        return false;
    }
}

bool register_logo_decal_class(
    void* object_class, const LogoDecalDescriptor* descriptors,
    std::uint32_t descriptor_count) noexcept {
    if (!object_class || !descriptors || descriptor_count == 0 ||
        descriptor_count > 64) {
        return false;
    }
    try {
        auto policy = std::make_unique<LogoDecalClassPolicy>();
        policy->decals.reserve(descriptor_count);
        for (std::uint32_t index = 0; index < descriptor_count; ++index) {
            const LogoDecalDescriptor& source = descriptors[index];
            if (source.struct_size < sizeof(LogoDecalDescriptor) ||
                !source.node || source.texture_path_count > 256 ||
                (source.texture_path_count != 0 && !source.texture_paths) ||
                !std::isfinite(source.size[0]) ||
                !std::isfinite(source.size[1]) ||
                source.size[0] <= 0.0f || source.size[1] <= 0.0f) {
                continue;
            }
            LogoDecal decal;
            decal.node = source.node;
            decal.use_colour_key = source.use_colour_key != 0;
            decal.colour_key = source.colour_key & 0x00ffffffu;
            decal.flip_u = source.flip_u != 0;
            std::copy_n(source.offset, 3, decal.offset.begin());
            std::copy_n(source.rotation_degrees, 3,
                        decal.rotation_degrees.begin());
            std::copy_n(source.size, 2, decal.size.begin());
            const bool finite = std::all_of(
                decal.offset.begin(), decal.offset.end(),
                [](float value) { return std::isfinite(value); }) &&
                std::all_of(
                    decal.rotation_degrees.begin(),
                    decal.rotation_degrees.end(),
                    [](float value) { return std::isfinite(value); });
            if (!finite) continue;
            decal.texture_paths.reserve(source.texture_path_count);
            for (std::uint32_t row = 0;
                 row < source.texture_path_count; ++row) {
                const char* path = source.texture_paths[row];
                decal.texture_paths.emplace_back(path ? path : "");
            }
            policy->decals.push_back(std::move(decal));
        }
        if (policy->decals.empty()) return false;
        g_logo_decal_policies[object_class] = std::move(policy);
        return true;
    } catch (...) {
        log_line("Could not retain a CraftClass logo-decal policy");
        return false;
    }
}

void nebula_begin_craft_render(void* craft) noexcept {
    if (g_craft_render_depth < g_craft_render_stack.size()) {
        g_craft_render_stack[g_craft_render_depth++] = craft;
    } else {
        ++g_craft_render_overflow;
    }
    if (g_emissive_policies.empty() && g_specular_policies.empty()) return;
    void* object_class = read_live_at<void*>(
        craft, kCraftClassOffset, nullptr);
    const bool mapped_class =
        g_emissive_policies.find(object_class) !=
            g_emissive_policies.end() ||
        g_specular_policies.find(object_class) !=
            g_specular_policies.end();
    if (mapped_class &&
        InterlockedCompareExchange(
            &g_logged_registered_craft_context, 1, 0) == 0) {
        log_line("Craft render context reached a registered mapped-lighting class");
    }
    if (mapped_class && g_mapped_texture_cloak_diagnostics_enabled) {
        const std::uint32_t cloak_state = current_craft_cloak_state(craft);
        const std::uint32_t cloak_bucket = std::min<std::uint32_t>(
            cloak_state, 4);
        if (claim_mapped_texture_cloak_diagnostic(16u + cloak_bucket)) {
            char message[180]{};
            std::snprintf(
                message, sizeof(message),
                "Mapped craft render context reached cloak state %lu",
                static_cast<unsigned long>(cloak_state));
            log_line(message);
        }
    }
}

void nebula_end_craft_render(void* craft) noexcept {
    render_logo_decals(craft);
    render_damage_decals(craft);
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
int __cdecl A2FO_NebulaSetEmissiveBumpMultiplier(float multiplier) {
    return a2fo::set_nebula_emissive_bump_multiplier(multiplier) ? 1 : 0;
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_NebulaSetBumpLightBias(float bias) {
    return a2fo::set_nebula_bump_light_bias(bias) ? 1 : 0;
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_NebulaSetEmissiveDiffuseRestore(float amount) {
    return a2fo::set_nebula_emissive_diffuse_restore(amount) ? 1 : 0;
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
int __cdecl A2FO_NebulaRegisterSpecularMaterials(
    void* object_class, const char* const* diffuse_names,
    const char* const* texture_paths, std::uint32_t material_count) {
    return a2fo::register_nebula_specular_materials(
        object_class, diffuse_names, texture_paths, material_count) ? 1 : 0;
}

extern "C" __declspec(dllexport)
void __cdecl A2FO_NebulaBeginCraftRender(void* craft) {
    a2fo::nebula_begin_craft_render(craft);
}

extern "C" __declspec(dllexport)
void __cdecl A2FO_NebulaEndCraftRender(void* craft) {
    a2fo::nebula_end_craft_render(craft);
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_DecalRegisterClass(
    void* object_class, float damage_threshold,
    const a2fo::DecalDescriptor* descriptors,
    std::uint32_t descriptor_count) {
    return a2fo::register_damage_decal_class(
        object_class, damage_threshold, descriptors, descriptor_count)
        ? 1 : 0;
}

extern "C" __declspec(dllexport)
int __cdecl A2FO_LogoDecalRegisterClass(
    void* object_class, const a2fo::LogoDecalDescriptor* descriptors,
    std::uint32_t descriptor_count) {
    return a2fo::register_logo_decal_class(
        object_class, descriptors, descriptor_count) ? 1 : 0;
}
