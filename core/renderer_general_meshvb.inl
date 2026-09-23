// Shared MeshVB admission and material handling for visible and cloaked
// objects. Included after the compositor so its selection is scoped to the
// same native mesh, not to a Craft instance (effects need not be Crafts).
namespace {

thread_local void* g_general_render_mesh = nullptr;
volatile LONG g_general_mesh_logged = 0;
volatile LONG g_general_mesh_fallback_logs = 0;
volatile LONG g_general_mesh_material_logged[4]{};

bool general_mesh_has_bump(const void* mesh) noexcept {
    const auto* textures = static_cast<const unsigned char*>(mesh) + 0x114;
    const int count = read_at<int>(textures, 8, 0);
    const void* data = read_at<const void*>(textures, 0, nullptr);
    return count >= 2 && readable_range(data, 2 * sizeof(void*)) &&
        read_at<const void*>(data, sizeof(void*), nullptr) != nullptr;
}

bool general_mesh_uses_dot3(const void* mesh) noexcept {
    const void* meshvb = read_at<const void*>(mesh, 0x128, nullptr);
    const void* table = read_at<const void*>(meshvb, 0, nullptr);
    // Fleet Ops can replace methods in Armada's original DOT3 table. Its
    // table identity is still valid even when PrepareVertices is detoured.
    // Extension clones retain the original preparation method.
    return table == at(g_armada, 0x002bc7d4) ||
        read_at<const void*>(table, 0x10, nullptr) ==
        at(g_armada, 0x002271b0);
}

float general_mesh_colour(const void* source, std::size_t offset) noexcept {
    const float value = read_at<float>(source, offset, 0.0f);
    return std::isfinite(value) ? std::max(0.0f, std::min(1.0f, value)) : 0.0f;
}

}  // namespace

extern "C" bool a2fo_nebula_general_mesh_enabled() noexcept {
    return g_hooks_ready && g_dxvk_backend_active &&
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) != 0 &&
        InterlockedCompareExchange(&g_fast_nonbump_shader_rejected, 0, 0) == 0 &&
        g_craft_render_overflow == 0;
}

extern "C" void __cdecl a2fo_nebula_general_mesh_selector(
    unsigned reason, void* mesh, void* wrapper) noexcept {
    g_general_render_mesh = nullptr;
    if (!a2fo_nebula_general_mesh_enabled() ||
        !readable_range(mesh, 0x130)) return;

    if (InterlockedCompareExchange(&g_general_mesh_logged, 1, 0) == 0) {
        log_line("General fast MeshVB routing enabled: complete material state, preserved alpha selection, per-group transparent index sorting");
    }
    if (reason != 4 && reason != 5) {
        if (InterlockedIncrement(&g_general_mesh_fallback_logs) <= 16) {
            const void* groups = read_at<const void*>(mesh, 0x104, nullptr);
            const void* lighting = read_at<const void*>(groups, 0x14, nullptr);
            char message[256]{};
            std::snprintf(message, sizeof(message),
                "General MeshVB unsupported route: reason=%u flags=0x%lx model=%d vertices=%lu groups=%d cloak=%d craft=%p mesh=%p",
                reason, static_cast<unsigned long>(read_at<DWORD>(mesh, 0x12c, 0)),
                read_at<int>(lighting, 0x40, -1),
                static_cast<unsigned long>(read_at<DWORD>(mesh, 0xc8, 0)),
                read_at<int>(mesh, 0x100, 0),
                static_cast<int>(current_craft_cloak_state(current_render_craft())),
                current_render_craft(), mesh);
            log_line(message);
        }
        return;
    }
    g_general_render_mesh = mesh;
    void* material = read_at<void*>(wrapper, kStormCurrentMaterialOffset, nullptr);
    if (reason == 5 && g_cloak_composite.selected &&
        g_cloak_composite.material == material) {
        // Alpha admission already preflighted the compositor. Do not erase
        // it because Fleet Ops substituted a MeshVB virtual method. Only
        // the actual FO DOT3 draw hooks consume this 68-byte shader; native
        // StandardMeshVB draws do not enter those hooks.
        g_cloak_composite.alpha_sort = true;
        return;
    }
    if (!general_mesh_uses_dot3(mesh)) {
        // Never select the 68-byte compositor on a native 32-byte stream.
        clear_cloak_composite_selection();
        return;
    }

    const int cloak = static_cast<int>(
        current_craft_cloak_state(current_render_craft()));
    if (reason == 4 && general_mesh_has_bump(mesh) &&
        read_at<DWORD>(material, 0x20, D3DBLEND_ZERO) == D3DBLEND_ZERO &&
        !(cloak >= 1 && cloak <= 3) &&
        !fast_nonbump_requested_for_draw()) {
        // Already GPU MeshVB: retain visible normal-map lighting instead of
        // replacing authored bump maps with the cloak's geometric normal.
        return;
    }

    auto* device = read_at<IDirect3DDevice8*>(wrapper, kStormDeviceOffset, nullptr);
    if (!device || active_storm_device_wrapper(device) != wrapper) return;
    adopt_live_device(device);
    if (!material || !prepare_cloak_composite(material)) return;
    g_cloak_composite.alpha_sort = reason == 5;
}

extern "C" void __cdecl a2fo_nebula_general_mesh_material(
    IDirect3DDevice8* device, const void* lighting) noexcept {
    if (!cloak_composite_selected(device) || !readable_range(lighting, 0x46)) return;
    auto& state = g_cloak_composite;
    const int model = read_at<int>(lighting, 0x40, 1);
    void* engine = phong_engine();
    const bool team = read_at<unsigned char>(lighting, 0x45, 0) != 0 &&
        read_at<unsigned char>(engine, 0x40, 0) != 0;
    const int cloak = static_cast<int>(current_craft_cloak_state(state.craft));
    auto& base = state.constants[kCloakCompositeConstantCount - 1];
    base = {};
    for (std::size_t component = 0; component < 3; ++component) {
        const std::size_t offset = component * sizeof(float);
        if (model == 0) {
            // LightVertices_Constant uses diffuse/team colour, not ambient
            // or directional lights. Keep shield/effect materials self-lit.
            base[component] = team
                ? general_mesh_colour(engine, 0x34 + offset)
                : general_mesh_colour(lighting, 0x24 + offset);
        } else if (!(cloak >= 1 && cloak <= 3) && !state.alpha_sort) {
            // Match the non-VB Lambert/Phong ambient initialization. Cloak
            // intentionally retains the neutral-bump path's black ambient.
            base[component] = general_mesh_colour(engine, 0x28 + offset) +
                (team ? 0.0f : general_mesh_colour(lighting, 0x18 + offset));
        }
    }
    if (model == 0) {
        for (std::size_t light = 0; light < state.count; ++light)
            state.constants[light * 2 + 1] = {};
    }

    if (g_general_render_mesh) {
        // SetRenderState(pass, textures) only binds the texture and blending.
        // Restore the complete native state as well: culling/depth policy
        // and the material/global alpha updater are separate native calls.
        void* wrapper = active_storm_device_wrapper(device);
        const void* table = read_at<const void*>(state.material, 0, nullptr);
        using SetMaterial = void(__attribute__((thiscall))*)(void*, int, const void*);
        const auto set_material = reinterpret_cast<SetMaterial>(
            read_at<void*>(table, 0x10, nullptr));
        if (wrapper && set_material && writable_range(
                static_cast<unsigned char*>(wrapper) + kStormCurrentMaterialOffset,
                sizeof(void*))) {
            void* empty = nullptr;
            std::memcpy(static_cast<unsigned char*>(wrapper) +
                kStormCurrentMaterialOffset, &empty, sizeof(empty));
            a2fo_nebula_call_thiscall_0(
                at(g_armada, 0x00244810), state.material);
            set_material(state.material, 0,
                static_cast<unsigned char*>(g_general_render_mesh) + 0x114);
            // Sorted draws reapply their native alpha policy in the core's
            // final-alpha hook after this ordinary state has been restored.
        }
    }

    const unsigned category = model == 0 ? 3u :
        (state.alpha_sort ? 1u : (state.craft ? 0u : 2u));
    if (InterlockedCompareExchange(&g_general_mesh_material_logged[category], 1, 0) == 0) {
        char message[176]{};
        std::snprintf(message, sizeof(message),
            "General single-pass MeshVB material: category=%u model=%d cloak=%d sorted-alpha=%d",
            category, model, cloak, state.alpha_sort ? 1 : 0);
        log_line(message);
    }
}
