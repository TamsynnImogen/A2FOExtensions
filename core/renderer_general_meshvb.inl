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
    const auto step=+[](void*,std::uint32_t op,std::uintptr_t) noexcept -> std::uintptr_t {
        switch(op){
        case 0:return g_hooks_ready;
        case 1:return g_dxvk_backend_active;
        case 2:return InterlockedCompareExchange(&g_runtime_enabled,0,0)!=0;
        case 3:return InterlockedCompareExchange(&g_fast_nonbump_shader_rejected,0,0)==0;
        case 4:return g_craft_render_overflow==0;
        default:return 0;
        }
    };
    return a2fo_rs_stage10_core_render_flow(0,nullptr,step,0)!=0;
}

extern "C" void __cdecl a2fo_nebula_general_mesh_selector(
    unsigned reason,void* mesh,void* wrapper) noexcept {
    g_general_render_mesh=nullptr;
    g_m3d_active_render_mesh=nullptr;
    struct Context {unsigned reason;void* mesh;void* wrapper;void* material=nullptr;IDirect3DDevice8* device=nullptr;};
    Context context{reason,mesh,wrapper};
    const auto step=+[](void* opaque,std::uint32_t op,std::uintptr_t arg) noexcept -> std::uintptr_t {
        auto& c=*static_cast<Context*>(opaque);
        switch(op){
        case 0:return a2fo_nebula_general_mesh_enabled();
        case 1:return readable_range(c.mesh,0x130);
        case 2:return refit_cloak_diagnostics_enabled();
        case 3:report_refit_cloak_mesh(current_render_craft(),c.reason,general_mesh_uses_dot3(c.mesh),registered_m3d_mesh(c.mesh));return 0;
        case 4:return InterlockedCompareExchange(&g_general_mesh_logged,1,0)==0;
        case 5:log_line("General fast MeshVB routing enabled: complete material state, preserved alpha selection, per-group transparent index sorting");return 0;
        case 7:return InterlockedIncrement(&g_general_mesh_fallback_logs)<=16;
        case 8:{
            const void* groups=read_at<const void*>(c.mesh,0x104,nullptr);
            const void* lighting=read_at<const void*>(groups,0x14,nullptr);
            char message[256]{};
            std::snprintf(message,sizeof(message),
                "General MeshVB unsupported route: reason=%u flags=0x%lx model=%d vertices=%lu groups=%d cloak=%d craft=%p mesh=%p",
                c.reason,static_cast<unsigned long>(read_at<DWORD>(c.mesh,0x12c,0)),
                read_at<int>(lighting,0x40,-1),static_cast<unsigned long>(read_at<DWORD>(c.mesh,0xc8,0)),
                read_at<int>(c.mesh,0x100,0),static_cast<int>(current_craft_cloak_state(current_render_craft())),current_render_craft(),c.mesh);
            log_line(message);return 0;}
        case 9:g_general_render_mesh=c.mesh;return 0;
        case 10:return registered_m3d_mesh(c.mesh);
        case 11:g_m3d_active_render_mesh=c.mesh;return 0;
        case 12:c.material=read_at<void*>(c.wrapper,kStormCurrentMaterialOffset,nullptr);return 0;
        case 13:return g_cloak_composite.selected;
        case 14:return g_cloak_composite.material==c.material;
        case 15:g_cloak_composite.alpha_sort=true;return 0;
        case 16:return general_mesh_uses_dot3(c.mesh);
        case 17:clear_cloak_composite_selection();return 0;
        case 18:return static_cast<std::uint32_t>(static_cast<int>(current_craft_cloak_state(current_render_craft())));
        case 19:return general_mesh_has_bump(c.mesh);
        case 20:return read_at<DWORD>(c.material,0x20,D3DBLEND_ZERO)==D3DBLEND_ZERO;
        case 21:return fast_nonbump_requested_for_draw();
        case 22:c.device=read_at<IDirect3DDevice8*>(c.wrapper,kStormDeviceOffset,nullptr);return c.device!=nullptr;
        case 23:return active_storm_device_wrapper(c.device)==c.wrapper;
        case 24:adopt_live_device(c.device);return 0;
        case 25:return c.material!=nullptr;
        case 26:return prepare_cloak_composite(c.material);
        case 27:g_cloak_composite.alpha_sort=arg!=0;return 0;
        default:return 0;
        }
    };
    a2fo_rs_stage10_core_render_flow(1,&context,step,reason);
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
