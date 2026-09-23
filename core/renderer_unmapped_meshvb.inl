// Included by nebula_renderer.cpp after its private renderer helpers. This
// deliberately shares the existing shader/device lifecycle, rather than
// installing another renderer or changing the user's bump-map setting.

extern "C" {
void a2fo_nebula_unmapped_meshvb_create_hook();
void* g_a2fo_unmapped_standard_create_return = nullptr;
void* g_a2fo_unmapped_create_done_return = nullptr;
std::uint32_t __cdecl a2fo_nebula_create_unmapped_meshvb(void* mesh) noexcept;
}

namespace {

constexpr std::uintptr_t kUnmappedCreateBranchRva = 0x00231d7e;
constexpr std::uintptr_t kUnmappedDot3FactoryRva = 0x00226e50;
constexpr std::uintptr_t kUnmappedDot3VtableRva = 0x002bc7d4;
constexpr std::size_t kUnmappedMeshBytes = 0x130;
constexpr std::size_t kUnmappedGroupBytes = 0x1c;
constexpr std::size_t kUnmappedTriangleBytes = 0x28;

using UnmappedFactory = void* (__cdecl*)(void*);
using UnmappedRender = void (__attribute__((thiscall))*)(
    void*, int, const void*, const void*, const void*);
using UnmappedDelete = void* (__attribute__((thiscall))*)(void*, unsigned);

struct UnmappedTextureArray {
    void** data;
    int capacity;
    int size;
};
static_assert(sizeof(UnmappedTextureArray) == 12,
              "Storm3D texture arrays require the 32-bit layout");

bool g_unmapped_meshvb_installed = false;
// Preserve the native RTTI pointer immediately preceding the five slots.
std::array<std::uintptr_t, 6> g_unmapped_vtable{};
UnmappedRender g_unmapped_native_render = nullptr;
volatile LONG g_unmapped_meshes_created = 0;
volatile LONG g_unmapped_visible_logged = 0;
volatile LONG g_unmapped_cloaked_logged = 0;

bool unmapped_diffuse_texture(const void* texture_array,
                             void** diffuse) noexcept {
    if (!diffuse || !readable_range(texture_array, 12)) return false;
    const auto array = read_at<UnmappedTextureArray>(
        texture_array, 0, UnmappedTextureArray{});
    if (array.size < 1 || array.size > 16 ||
        array.capacity < array.size ||
        !readable_range(array.data, array.size * sizeof(void*))) {
        return false;
    }
    // Do not flatten authored normal maps or additional texture layouts.
    if (array.size > 2 ||
        (array.size == 2 && array.data[1] != nullptr)) return false;
    *diffuse = array.data[0];
    return *diffuse && readable_range(*diffuse, 0x48);
}

bool unmapped_geometry_compatible(void* mesh) noexcept {
    if (!readable_range(mesh, 0x130) ||
        read_at<void*>(mesh, 0x128, nullptr) ||
        (read_at<DWORD>(mesh, 0x12c, 0) & 2u) != 0) return false;
    void* diffuse = nullptr;
    if (!unmapped_diffuse_texture(
            static_cast<const unsigned char*>(mesh) + 0x114, &diffuse)) return false;
    const DWORD vertices = read_at<DWORD>(mesh, 0xc8, 0);
    const int groups = read_at<int>(mesh, 0x100, 0);
    const void* positions = read_at<const void*>(mesh, 0xc0, nullptr);
    const void* normals = read_at<const void*>(mesh, 0xc4, nullptr);
    const void* texcoords = read_at<const void*>(mesh, 0x124, nullptr);
    const void* group_data = read_at<const void*>(mesh, 0x104, nullptr);
    if (!vertices || vertices > 65535 || groups <= 0 || groups > 4096 ||
        !readable_range(positions, vertices * 12u) ||
        !readable_range(normals, vertices * 12u) ||
        !readable_range(group_data, static_cast<std::size_t>(groups) * 0x1c)) return false;
    for (DWORD vertex = 0; vertex < vertices; ++vertex) {
        float length = 0.0f;
        for (unsigned component = 0; component < 3; ++component) {
            const std::size_t offset = vertex * 12u + component * 4u;
            const float normal = read_at<float>(normals, offset, 0.0f);
            if (!std::isfinite(read_at<float>(positions, offset, 0.0f)) ||
                !std::isfinite(normal)) return false;
            length += normal * normal;
        }
        if (!std::isfinite(length) || length <= 1.0e-12f) return false;
    }
    unsigned maximum_uv = 0;
    for (int group = 0; group < groups; ++group) {
        const auto* entry = static_cast<const unsigned char*>(group_data) + group * 0x1c;
        const void* lighting = read_at<const void*>(entry, 0x14, nullptr);
        const int model = read_at<int>(lighting, 0x40, -1);
        // Constant, Lambert and Phong all have valid native GPU geometry.
        // Their lighting differences are handled by the single-pass shader.
        if (model < 0 || model > 2) return false;
        const int triangles = read_at<int>(entry, 0xc, 0);
        const void* data = read_at<const void*>(entry, 8, nullptr);
        if (triangles <= 0 || triangles > 21845 ||
            !readable_range(data, static_cast<std::size_t>(triangles) * 40u)) return false;
        for (int triangle = 0; triangle < triangles; ++triangle) {
            for (unsigned corner = 0; corner < 3; ++corner) {
                const std::size_t offset = triangle * 40u + corner * 2u;
                if (read_at<unsigned short>(data, offset, 0) >= vertices) return false;
                maximum_uv = std::max(maximum_uv, static_cast<unsigned>(
                    read_at<unsigned short>(data, offset + 6u, 0)));
            }
        }
    }
    if (!readable_range(texcoords, (maximum_uv + 1u) * 8u)) return false;
    for (unsigned index = 0; index <= maximum_uv; ++index) {
        if (!std::isfinite(read_at<float>(texcoords, index * 8u, 0.0f)) ||
            !std::isfinite(read_at<float>(texcoords, index * 8u + 4u, 0.0f))) return false;
    }
    return true;
}

int __fastcall unmapped_meshvb_can_render(
    void* meshvb, void*, void* wrapper) noexcept {
    if (!g_hooks_ready || !g_dxvk_backend_active ||
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0 ||
        g_craft_render_overflow != 0 || false /* Non-Craft meshes use the same GPU path. */) return 0;
    const auto* mesh = read_at<const std::uint8_t*>(meshvb, 0x0c, nullptr);
    void* diffuse = nullptr;
    if (!readable_range(mesh, kUnmappedMeshBytes) ||
        !unmapped_diffuse_texture(mesh + 0x114, &diffuse)) return 0;
    // Match native DOT3 CanRender's capability bit, bypassing only its
    // irrelevant "disable bump mapping" option for these unmapped meshes.
    const auto* capabilities = read_at<const void*>(wrapper, 0x78, nullptr);
    if ((read_at<std::uint32_t>(capabilities, 0x20, 0) & 0x10u) == 0) return 0;
    auto* device = read_at<IDirect3DDevice8*>(wrapper, kStormDeviceOffset, nullptr);
    if (!device || active_storm_device_wrapper(device) != wrapper) return 0;
    adopt_live_device(device);
    if (!ensure_fast_nonbump_vertex_shader(device)) return 0;
    return 1;
}

void __fastcall unmapped_meshvb_render(
    void* meshvb, void*, int group, const void* lighting,
    const void* material, const void* textures) noexcept {
    void* diffuse = nullptr;
    if (!g_unmapped_native_render ||
        !unmapped_diffuse_texture(textures, &diffuse)) return;

    // FO dereferences textures[1] even when our per-light shader will not
    // sample it. Supply a valid, borrowed texture in a call-local array;
    // never append a fake bump map to the shared SOD or its texture registry.
    void* borrowed_textures[2] = {diffuse, diffuse};
    const UnmappedTextureArray draw_textures{borrowed_textures, 2, 2};
    struct DrawScope {
        DrawScope() noexcept { ++g_fast_unmapped_draw_depth; }
        ~DrawScope() { --g_fast_unmapped_draw_depth; }
    } scope;

    const auto cloak = current_craft_cloak_state(current_render_craft());
    auto* logged = cloak >= 1 && cloak <= 3
        ? &g_unmapped_cloaked_logged : &g_unmapped_visible_logged;
    if (InterlockedCompareExchange(logged, 1, 0) == 0) {
        log_line(cloak >= 1 && cloak <= 3
            ? "Unmapped craft MeshVB draw active while cloaked or transitioning"
            : "Unmapped craft MeshVB draw active while visible");
    }
    g_unmapped_native_render(meshvb, group, lighting, material, &draw_textures);
}

void install_unmapped_meshvb_policy() noexcept {
    if (g_unmapped_meshvb_installed) return;
    const auto ini = renderer_ini_path();
    if (GetPrivateProfileIntA(
            "Compatibility", "FastUnmappedMeshVB", 1, ini.c_str()) == 0) {
        log_line("Automatic non-bump MeshVB creation disabled by FastUnmappedMeshVB=0");
        return;
    }
    if (!g_dxvk_backend_active || command_line_requests_dx9() ||
        !validate_armada_module(g_armada) ||
        !validate_module(g_fleet_ops, kFleetOpsTimestamp,
                         kFleetOpsImageSize, "FleetOpsHook.dll") ||
        !fast_nonbump_vertex_shader_asset_available()) {
        log_line("Automatic non-bump MeshVB creation unavailable; native mesh creation retained");
        return;
    }

    auto* site = at(g_armada, kUnmappedCreateBranchRva);
    const std::uint8_t expected[] = {0xa8, 0x01, 0x74, 0x0f, 0x56};
    auto* factory = at(g_armada, kUnmappedDot3FactoryRva);
    const std::uint8_t factory_prefix[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
    auto* native_vtable = reinterpret_cast<const std::uintptr_t*>(
        at(g_armada, kUnmappedDot3VtableRva));
    if (!readable_range(site, sizeof(expected)) ||
        std::memcmp(site, expected, sizeof(expected)) != 0 ||
        !readable_range(factory, sizeof(factory_prefix)) ||
        std::memcmp(factory, factory_prefix, sizeof(factory_prefix)) != 0 ||
        !readable_range(native_vtable - 1, sizeof(g_unmapped_vtable))) {
        log_line("Automatic non-bump MeshVB signatures differ; native mesh creation retained");
        return;
    }
    const std::uintptr_t expected_methods[] = {
        0x00227a60, 0x00226f50, 0x002272a0, 0x002275a0, 0x002271b0};
    for (unsigned slot = 0; slot < 5; ++slot) {
        if (native_vtable[slot] != reinterpret_cast<std::uintptr_t>(
                at(g_armada, expected_methods[slot]))) {
            log_line("Automatic non-bump MeshVB vtable differs; native mesh creation retained");
            return;
        }
    }
    std::memcpy(g_unmapped_vtable.data(), native_vtable - 1,
                sizeof(g_unmapped_vtable));
    g_unmapped_native_render = reinterpret_cast<UnmappedRender>(native_vtable[3]);
    g_unmapped_vtable[2] = reinterpret_cast<std::uintptr_t>(
        &unmapped_meshvb_can_render);
    g_unmapped_vtable[4] = reinterpret_cast<std::uintptr_t>(&unmapped_meshvb_render);
    g_a2fo_unmapped_standard_create_return = at(g_armada, 0x00231d83);
    g_a2fo_unmapped_create_done_return = at(g_armada, 0x00231d91);

    std::uint8_t patch[5] = {0xe9, 0, 0, 0, 0};
    const auto relative = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&a2fo_nebula_unmapped_meshvb_create_hook) -
        reinterpret_cast<std::uintptr_t>(site) - sizeof(patch));
    std::memcpy(patch + 1, &relative, sizeof(relative));
    DWORD previous_protection = 0;
    if (!VirtualProtect(site, sizeof(patch), PAGE_EXECUTE_READWRITE,
                        &previous_protection)) {
        log_line("Automatic non-bump MeshVB hook could not obtain write access");
        return;
    }
    std::memcpy(site, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), site, sizeof(patch));
    DWORD ignored = 0;
    if (!VirtualProtect(site, sizeof(patch), previous_protection, &ignored)) {
        log_line("Automatic non-bump MeshVB hook installed; page-protection restore failed");
    }
    g_unmapped_meshvb_installed = true;
    log_line("Automatic non-bump MeshVB creation enabled for compatible Lambert craft meshes");
}

}  // namespace

extern "C" std::uint32_t __cdecl a2fo_nebula_create_unmapped_meshvb(
    void* mesh) noexcept {
    if (!g_unmapped_meshvb_installed || !g_hooks_ready ||
        !g_dxvk_backend_active || !unmapped_geometry_compatible(mesh)) return 0;
    // The native factory builds seam-aware vertex/index buffers and tangent
    // data, and triggers the existing early DOT3 shader-creation hook. It is
    // also the native owner of buffer rebuild/destruction on mesh changes.
    const auto create = reinterpret_cast<UnmappedFactory>(
        at(g_armada, kUnmappedDot3FactoryRva));
    void* meshvb = create(mesh);
    if (!meshvb) return 0;
    const auto* native_vtable = reinterpret_cast<const std::uintptr_t*>(
        at(g_armada, kUnmappedDot3VtableRva));
    if (read_at<const std::uintptr_t*>(meshvb, 0, nullptr) != native_vtable ||
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0) {
        const auto destroy = reinterpret_cast<UnmappedDelete>(native_vtable[0]);
        destroy(meshvb, 1);
        return 0;
    }
    auto* vtable = g_unmapped_vtable.data() + 1;
    std::memcpy(meshvb, &vtable, sizeof(vtable));
    std::memcpy(static_cast<std::uint8_t*>(mesh) + 0x128,
                &meshvb, sizeof(meshvb));
    if (InterlockedIncrement(&g_unmapped_meshes_created) <= 8) {
        log_line("Prepared native MeshVB buffers for a model without a bump map");
    }
    return 1;
}
