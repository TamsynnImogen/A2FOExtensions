// Included after renderer_unmapped_meshvb.inl. Keep Phong out of the
// diffuse-only DOT3 replacement: Armada's standard MeshVB supplies an
// XYZ/NORMAL/TEX1 stream that D3D8 can light without rewriting CPU vertices.
namespace {

constexpr std::uintptr_t kPhongFactoryRva = 0x23e290;
constexpr std::uintptr_t kPhongVtableRva = 0x2bcbdc;
constexpr std::uintptr_t kPhongEnginePointerRva = 0x3ad508;
constexpr std::uintptr_t kPhongDirectionalVtableRva = 0x2bc8fc;
constexpr std::uintptr_t kPhongShaderInitRva = 0x227200;
constexpr std::size_t kPhongLightLimit = 8;

std::array<std::uintptr_t, 6> g_phong_vtable{};
UnmappedRender g_phong_native_render = nullptr;
bool g_phong_initialization_attempted = false;
bool g_phong_available = false;
bool g_phong_device_rejected = false;
IDirect3DDevice8* g_phong_last_device = nullptr; // Identity only, never dereferenced.
volatile LONG g_phong_prepared_logged = 0;
volatile LONG g_phong_visible_logged = 0;
volatile LONG g_phong_cloaked_logged = 0;
volatile LONG g_phong_fallback_logged = 0;

constexpr std::array<D3DRENDERSTATETYPE, 12> kPhongRenderStates{{
    D3DRS_LIGHTING, D3DRS_SPECULARENABLE, D3DRS_LOCALVIEWER,
    D3DRS_NORMALIZENORMALS, D3DRS_COLORVERTEX, D3DRS_AMBIENT,
    D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_SPECULARMATERIALSOURCE,
    D3DRS_AMBIENTMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE,
    D3DRS_SHADEMODE, D3DRS_CLIPPING
}};
constexpr std::array<D3DTEXTURESTAGESTATETYPE, 3> kPhongAlphaStates{{
    D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2
}};

struct PhongDrawContext {
    PhongDrawContext* previous = nullptr;
    IDirect3DDevice8* device = nullptr;
    void* wrapper = nullptr;
    const void* lighting = nullptr;
    const void* material = nullptr;
    std::array<D3DLIGHT8, kPhongLightLimit> lights{};
    std::array<D3DLIGHT8, kPhongLightLimit> saved_lights{};
    std::size_t light_count = 0;
    D3DMATERIAL8 saved_material{};
    std::array<DWORD, kPhongRenderStates.size()> saved_render_states{};
    std::array<DWORD, kPhongAlphaStates.size()> saved_alpha_states{};
    bool active = false;
};
thread_local PhongDrawContext* g_phong_draw = nullptr;

void* phong_engine() noexcept {
    return read_at<void*>(at(g_armada, kPhongEnginePointerRva), 0, nullptr);
}

bool phong_colour(const void* object, std::size_t offset,
                  D3DCOLORVALUE& colour) noexcept {
    if (!object || !readable_range(
            static_cast<const std::uint8_t*>(object) + offset,
            3 * sizeof(float))) return false;
    std::memcpy(&colour.r,
                static_cast<const std::uint8_t*>(object) + offset,
                3 * sizeof(float));
    colour.a = 1.0f;
    return std::isfinite(colour.r) && std::isfinite(colour.g) &&
        std::isfinite(colour.b) && colour.r >= 0.0f &&
        colour.g >= 0.0f && colour.b >= 0.0f;
}

bool phong_lighting_material(const void* material) noexcept {
    if (!readable_range(material, 0x46)) return false;
    const auto model = read_at<std::uint32_t>(material, 0x40, ~0u);
    const float power = read_at<float>(material, 0x3c, -1.0f);
    D3DCOLORVALUE colour{};
    return (model == 1 || model == 2) && std::isfinite(power) &&
        power >= 0.0f && power <= 128.0f &&
        phong_colour(material, 0x18, colour) &&
        phong_colour(material, 0x24, colour) &&
        phong_colour(material, 0x30, colour);
}

bool unmapped_mesh_uses_phong(void* mesh) noexcept {
    const auto count = read_at<std::uint32_t>(mesh, 0x100, 0);
    const auto* groups = read_at<const std::uint8_t*>(mesh, 0x104, nullptr);
    if (!count || count > 4096 ||
        !readable_range(groups, count * kUnmappedGroupBytes)) return false;
    for (std::uint32_t i = 0; i < count; ++i) {
        const void* material = read_at<const void*>(
            groups + i * kUnmappedGroupBytes, 0x14, nullptr);
        if (read_at<std::uint32_t>(material, 0x40, ~0u) == 2) return true;
    }
    return false;
}

bool phong_collect_lights(PhongDrawContext& context) noexcept {
    context.light_count = 0;
    void* engine = phong_engine();
    void* sentinel = read_at<void*>(engine, 0x60, nullptr);
    if (!readable_range(sentinel, 12)) return false;
    void* node = read_at<void*>(sentinel, 0, nullptr);
    // Only exact native directional lights are admitted. A wave affector may
    // advertise the same type number but implement different lighting math.
    while (node != sentinel) {
        if (context.light_count == kPhongLightLimit ||
            !readable_range(node, 12)) return false;
        const void* instance = read_at<const void*>(node, 8, nullptr);
        const void* light = read_at<const void*>(instance, 0, nullptr);
        if (read_at<void*>(light, 0, nullptr) !=
                at(g_armada, kPhongDirectionalVtableRva) ||
            !readable_range(instance, 0x40)) return false;
        D3DLIGHT8& output = context.lights[context.light_count++];
        output = D3DLIGHT8{};
        output.Type = D3DLIGHT_DIRECTIONAL;
        if (!phong_colour(instance, 4, output.Diffuse)) return false;
        output.Specular = output.Diffuse;
        // The native instance stores the direction towards the light; D3D
        // expects the direction travelled by the light instead.
        output.Direction.x = -read_at<float>(instance, 0x28, 0.0f);
        output.Direction.y = -read_at<float>(instance, 0x2c, 0.0f);
        output.Direction.z = -read_at<float>(instance, 0x30, 0.0f);
        const float length_squared =
            output.Direction.x * output.Direction.x +
            output.Direction.y * output.Direction.y +
            output.Direction.z * output.Direction.z;
        if (!std::isfinite(length_squared) || length_squared < 1.0e-12f)
            return false;
        node = read_at<void*>(node, 0, nullptr);
    }
    return true;
}

void phong_restore_draw(PhongDrawContext& context) noexcept {
    if (!context.active || !context.device) return;
    context.active = false;
    context.device->SetMaterial(&context.saved_material);
    for (std::size_t i = 0; i < kPhongRenderStates.size(); ++i)
        context.device->SetRenderState(
            kPhongRenderStates[i], context.saved_render_states[i]);
    for (std::size_t i = 0; i < kPhongAlphaStates.size(); ++i)
        context.device->SetTextureStageState(
            0, kPhongAlphaStates[i], context.saved_alpha_states[i]);
    for (std::size_t i = 0; i < context.light_count; ++i)
        context.device->SetLight(static_cast<DWORD>(i),
                                 &context.saved_lights[i]);
}

void phong_reject_draw() noexcept {
    g_phong_device_rejected = true;
    if (g_phong_draw) phong_restore_draw(*g_phong_draw);
    log_line("Phong MeshVB state setup failed; subsequent meshes use native CPU lighting");
}

int __fastcall phong_meshvb_can_render(
    void* meshvb, void*, void* wrapper) noexcept {
    if (!g_hooks_ready || !g_dxvk_backend_active || g_phong_draw ||
        g_craft_render_overflow != 0 || !current_render_craft() ||
        InterlockedCompareExchange(&g_runtime_enabled, 0, 0) == 0) return 0;
    void* mesh = read_at<void*>(meshvb, 0x0c, nullptr);
    void* diffuse = nullptr;
    if (!mesh || !unmapped_diffuse_texture(
            static_cast<std::uint8_t*>(mesh) + 0x114, &diffuse)) return 0;
    void* capabilities = read_at<void*>(wrapper, 0x78, nullptr);
    if ((read_at<std::uint32_t>(capabilities, 0x20, 0) & 8u) == 0) return 0;
    auto* device = read_at<IDirect3DDevice8*>(wrapper, 0x90, nullptr);
    if (!device || active_storm_device_wrapper(device) != wrapper) return 0;
    if (device != g_phong_last_device) {
        g_phong_last_device = device;
        g_phong_device_rejected = false;
    }
    if (g_phong_device_rejected) return 0;
    D3DCAPS8 caps{};
    PhongDrawContext context{};
    if (FAILED(device->GetDeviceCaps(&caps)) ||
        caps.MaxActiveLights < kPhongLightLimit ||
        !phong_collect_lights(context)) {
        if (InterlockedCompareExchange(&g_phong_fallback_logged, 1, 0) == 0)
            log_line("Phong MeshVB retained native lighting for an unsupported light setup");
        return 0;
    }
    adopt_live_device(device);
    return 1;
}

void __fastcall phong_meshvb_render(
    void* meshvb, void*, int group, const void* lighting,
    const void* material, const void* textures) noexcept {
    if (!g_phong_native_render) return;
    PhongDrawContext context{};
    context.previous = g_phong_draw;
    context.device = g_device;
    context.wrapper = active_storm_device_wrapper(context.device);
    context.lighting = lighting;
    context.material = material;
    g_phong_draw = &context;
    // Keep native ownership, buffer binding, indexed submission, diagnostics
    // and emissive hooks. Our pre/post hooks only replace the lighting state.
    g_phong_native_render(meshvb, group, lighting, material, textures);
    phong_restore_draw(context);
    g_phong_draw = context.previous;
    if (g_fast_nonbump_alpha_material == material)
        g_fast_nonbump_alpha_material = nullptr;
}

bool initialize_phong_meshvb() noexcept {
    if (g_phong_initialization_attempted) return g_phong_available;
    g_phong_initialization_attempted = true;
    auto* native_vtable = reinterpret_cast<std::uintptr_t*>(
        at(g_armada, kPhongVtableRva));
    constexpr std::array<std::uintptr_t, 5> expected{{
        0x23e5f0, 0x23e310, 0x23e340, 0x23e450, 0x23e5d0
    }};
    constexpr std::array<std::uint8_t, 5> factory_prefix{{
        0x55, 0x8b, 0xec, 0x6a, 0xff
    }};
    constexpr std::array<std::uint8_t, 6> init_prefix{{
        0x55, 0x8b, 0xec, 0x83, 0xec, 0x08
    }};
    if (!readable_range(native_vtable - 1, sizeof(g_phong_vtable)) ||
        !readable_range(at(g_armada, kPhongFactoryRva), factory_prefix.size()) ||
        !readable_range(at(g_armada, kPhongShaderInitRva), init_prefix.size()) ||
        std::memcmp(at(g_armada, kPhongFactoryRva), factory_prefix.data(),
                    factory_prefix.size()) != 0 ||
        std::memcmp(at(g_armada, kPhongShaderInitRva), init_prefix.data(),
                    init_prefix.size()) != 0) return false;
    for (std::size_t i = 0; i < expected.size(); ++i)
        if (native_vtable[i] != reinterpret_cast<std::uintptr_t>(
                at(g_armada, expected[i]))) return false;
    std::memcpy(g_phong_vtable.data(), native_vtable - 1,
                sizeof(g_phong_vtable));
    g_phong_native_render = reinterpret_cast<UnmappedRender>(native_vtable[3]);
    g_phong_vtable[2] = reinterpret_cast<std::uintptr_t>(&phong_meshvb_can_render);
    g_phong_vtable[4] = reinterpret_cast<std::uintptr_t>(&phong_meshvb_render);
    g_phong_available = true;
    log_line("Non-bump Phong MeshVB support enabled with GPU material lighting");
    return true;
}

} // namespace

extern "C" std::uint32_t __cdecl
a2fo_nebula_create_unmapped_or_phong_meshvb(void* mesh) noexcept {
    // Keep one 68-byte GPU geometry format for all non-bump light models.
    // This avoids the StandardMeshVB directional-light/capability restriction
    // and lets visible Phong objects use the same compositor as cloak.
    return a2fo_nebula_create_unmapped_meshvb(mesh);
}

extern "C" void __cdecl a2fo_nebula_phong_pre() noexcept {
    PhongDrawContext* context = g_phong_draw;
    if (!context || context->active) return;
    if (!context->device || !context->wrapper ||
        !phong_lighting_material(context->lighting) ||
        !phong_collect_lights(*context)) {
        phong_reject_draw();
        return;
    }
    auto* device = context->device;
    // The selector already decided whether this material may bypass sorting.
    // Reapply its native sorted-material state after Standard::Render's
    // ordinary material bind, without inventing blend factors for cloaking.
    if (g_fast_nonbump_alpha_material == context->material) {
        if (!writable_range(static_cast<std::uint8_t*>(context->wrapper) +
                            kStormCurrentMaterialOffset, sizeof(void*))) {
            phong_reject_draw();
            return;
        }
        void* previous = read_at<void*>(
            context->wrapper, kStormCurrentMaterialOffset, nullptr);
        void* empty = nullptr;
        std::memcpy(static_cast<std::uint8_t*>(context->wrapper) +
                        kStormCurrentMaterialOffset, &empty, sizeof(empty));
        a2fo_nebula_call_thiscall_0(
            at(g_armada, kTextureMaterialSetRenderStateZSortRva),
            const_cast<void*>(context->material));
        if (read_at<const void*>(context->wrapper,
                kStormCurrentMaterialOffset, nullptr) != context->material) {
            std::memcpy(static_cast<std::uint8_t*>(context->wrapper) +
                            kStormCurrentMaterialOffset,
                        &previous, sizeof(previous));
            phong_reject_draw();
            return;
        }
    }
    D3DMATERIAL8 material{};
    D3DCOLORVALUE ambient{};
    void* engine = phong_engine();
    float alpha = 1.0f;
    if (!phong_colour(engine, 0x28, ambient) ||
        !active_storm_material_alpha(device, &alpha) ||
        !phong_colour(context->lighting, 0x18, material.Emissive) ||
        !phong_colour(context->lighting, 0x24, material.Diffuse) ||
        !phong_colour(context->lighting, 0x30, material.Specular)) {
        phong_reject_draw();
        return;
    }
    if (read_at<std::uint8_t>(context->lighting, 0x45, 0) &&
        read_at<std::uint8_t>(engine, 0x40, 0)) {
        material.Emissive = D3DCOLORVALUE{};
        if (!phong_colour(engine, 0x34, material.Diffuse)) {
            phong_reject_draw();
            return;
        }
        material.Specular = material.Diffuse;
    }
    // CPU Phong adds scene ambient to the material's first colour triplet.
    // Express that as emission; a D3D ambient product would change the result.
    material.Emissive.r += ambient.r;
    material.Emissive.g += ambient.g;
    material.Emissive.b += ambient.b;
    material.Diffuse.a = alpha;
    material.Power = read_at<float>(context->lighting, 0x3c, 0.0f);
    const bool specular =
        read_at<std::uint32_t>(context->lighting, 0x40, 0) == 2;
    if (FAILED(device->GetMaterial(&context->saved_material))) {
        phong_reject_draw();
        return;
    }
    for (std::size_t i = 0; i < kPhongRenderStates.size(); ++i)
        if (FAILED(device->GetRenderState(
                kPhongRenderStates[i], &context->saved_render_states[i]))) {
            phong_reject_draw();
            return;
        }
    for (std::size_t i = 0; i < kPhongAlphaStates.size(); ++i)
        if (FAILED(device->GetTextureStageState(
                0, kPhongAlphaStates[i], &context->saved_alpha_states[i]))) {
            phong_reject_draw();
            return;
        }
    for (std::size_t i = 0; i < context->light_count; ++i)
        if (FAILED(device->GetLight(static_cast<DWORD>(i),
                                    &context->saved_lights[i]))) {
            phong_reject_draw();
            return;
        }
    // From here, all modified state has a captured counterpart, including
    // partial-failure paths. Native BeginRender already enabled these lights.
    context->active = true;
    const std::array<DWORD, kPhongRenderStates.size()> values{{
        TRUE, specular ? TRUE : FALSE, TRUE, TRUE, FALSE, 0,
        D3DMCS_MATERIAL, D3DMCS_MATERIAL, D3DMCS_MATERIAL,
        D3DMCS_MATERIAL, D3DSHADE_GOURAUD, TRUE
    }};
    if (FAILED(device->SetMaterial(&material))) {
        phong_reject_draw();
        return;
    }
    for (std::size_t i = 0; i < values.size(); ++i)
        if (FAILED(device->SetRenderState(kPhongRenderStates[i], values[i]))) {
            phong_reject_draw();
            return;
        }
    for (std::size_t i = 0; i < context->light_count; ++i)
        if (FAILED(device->SetLight(static_cast<DWORD>(i),
                                    &context->lights[i]))) {
            phong_reject_draw();
            return;
        }
    if (alpha < 0.99999f) {
        const std::array<DWORD, 3> alpha_values{{
            D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE
        }};
        for (std::size_t i = 0; i < alpha_values.size(); ++i)
            if (FAILED(device->SetTextureStageState(
                    0, kPhongAlphaStates[i], alpha_values[i]))) {
                phong_reject_draw();
                return;
            }
    }
    const auto cloak = current_craft_cloak_state(current_render_craft());
    volatile LONG* logged = cloak >= 1 && cloak <= 3
        ? &g_phong_cloaked_logged : &g_phong_visible_logged;
    if (InterlockedCompareExchange(logged, 1, 0) == 0)
        log_line(cloak >= 1 && cloak <= 3
            ? "Phong craft GPU MeshVB draw active while cloaked or transitioning"
            : "Phong craft GPU MeshVB draw active while visible");
}

extern "C" void __cdecl a2fo_nebula_phong_post() noexcept {
    if (g_phong_draw) phong_restore_draw(*g_phong_draw);
}
