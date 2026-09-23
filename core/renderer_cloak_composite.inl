// Fleet Ops' DOT3 lighting passes use ONE/ZERO and then ONE/ONE against the
// scene colour buffer. That accumulation cannot be exposed behind a later
// translucent material draw. For neutral-bump cloak draws, collect the native
// per-light inputs instead and light the textured hull in one GPU submission.
namespace {

constexpr std::size_t kCloakCompositeLights = 8;
constexpr UINT kCloakCompositeFirstConstant = 8;
constexpr UINT kCloakCompositeConstantCount = 19;
constexpr std::array<D3DTEXTURESTAGESTATETYPE, 6> kCloakCompositeStageStates{{
    D3DTSS_COLOROP, D3DTSS_COLORARG1, D3DTSS_COLORARG2,
    D3DTSS_ALPHAOP, D3DTSS_ALPHAARG1, D3DTSS_ALPHAARG2
}};

struct CloakCompositeState {
    void* material = nullptr;
    void* craft = nullptr;
    IDirect3DDevice8* device = nullptr;
    DWORD shader = 0;
    std::size_t capacity = 0;
    std::size_t count = 0;
    std::array<std::array<float, 4>, kCloakCompositeConstantCount> constants{};
    std::array<std::array<float, 4>, kCloakCompositeConstantCount> saved_constants{};
    std::array<std::array<DWORD, 6>, 2> saved_stages{};
    DWORD saved_shader = 0;
    bool alpha_sort = true;
    bool selected = false;
    bool active = false;
    bool failed = false;
};
thread_local CloakCompositeState g_cloak_composite{};
IDirect3DDevice8* g_cloak_shader_device = nullptr; // Owned by the core renderer.
std::array<DWORD, kCloakCompositeLights + 1> g_cloak_composite_shaders{};
bool g_cloak_composite_rejected = false;
volatile LONG g_cloak_composite_logged = 0;
volatile LONG g_cloak_composite_fallback_logged = 0;
volatile LONG g_cloak_composite_alpha_logged[2]{};

void log_cloak_shader_failure(const char* phase, HRESULT result,
                              std::size_t lights,
                              const char* detail = "") noexcept {
    char message[768]{};
    std::snprintf(message, sizeof(message),
        "Cloak shader %s failed: HRESULT=0x%08lx, lights=%lu; %.560s",
        phase, static_cast<unsigned long>(result),
        static_cast<unsigned long>(lights), detail);
    log_line(message);
}

DWORD create_cloak_composite_shader(
    IDirect3DDevice8* device, std::size_t lights) noexcept {
    if (!device || lights > kCloakCompositeLights ||
        !ensure_fast_nonbump_vertex_shader(device)) {
        log_cloak_shader_failure("prerequisite", E_FAIL, lights);
        return 0;
    }
    if (g_cloak_shader_device != device) {
        // adopt_live_device releases the previous owner's shaders first.
        if (g_cloak_shader_device) {
            log_cloak_shader_failure("device ownership", E_FAIL, lights);
            return 0;
        }
        g_cloak_shader_device = device;
    }
    if (g_cloak_composite_shaders[lights])
        return g_cloak_composite_shaders[lights];
    // Armada's native DOT3 stream is 68 bytes: position, normal, UV, and
    // three tangent-basis vectors. Do not depend on reading a different
    // shader's declaration back through the D3D8 compatibility wrapper.
    constexpr std::array<DWORD, 8> declaration{{
        D3DVSD_STREAM(0),
        D3DVSD_REG(0, D3DVSDT_FLOAT3),
        D3DVSD_REG(1, D3DVSDT_FLOAT3),
        D3DVSD_REG(2, D3DVSDT_FLOAT2),
        D3DVSD_REG(3, D3DVSDT_FLOAT3),
        D3DVSD_REG(4, D3DVSDT_FLOAT3),
        D3DVSD_REG(5, D3DVSDT_FLOAT3),
        D3DVSD_END()
    }};
    using BufferPointer = decltype(g_compiled_fast_nonbump_vertex_shader);
    using AssembleShader = HRESULT (WINAPI*)(
        const void*, UINT, DWORD, BufferPointer*, BufferPointer*, BufferPointer*);
    // ensure_fast_nonbump_vertex_shader above already uses the core's
    // D3DX81ab.dll instance. Resolve the memory assembler from that same
    // module instead of guessing a different DirectX utility DLL name.
    const auto assemble = imported_function<AssembleShader>(
        g_d3dx, "D3DXAssembleShader");
    if (!assemble) {
        log_cloak_shader_failure("assembler lookup", E_NOINTERFACE, lights);
        return 0;
    }

    // Use the same DX8 assembler family as the working geometric-lighting
    // shader, with validation enabled. Fully initialize outputs and temporaries
    // rather than relying on undefined components in hand-encoded bytecode.
    std::array<char, 8192> source{};
    std::size_t used = 0;
    auto append = [&](const char* format, auto... arguments) {
        const int length = std::snprintf(source.data() + used,
            source.size() - used, format, arguments...);
        if (length < 0 || static_cast<std::size_t>(length) >= source.size() - used)
            return false;
        used += static_cast<std::size_t>(length);
        return true;
    };
    if (!append("vs.1.1\n"
                "m4x4 oPos, v0, c2\n"
                "mov oT0, v2\n"
                "mov oT1, v2\n"
                "mov oT2, v2\n"
                "mov r0, c24.x\n"
                "mov r1, c26\n"
                "mov r2, c24.x\n")) return 0;
    for (unsigned light = 0; light < lights; ++light) {
        const unsigned direction = 8 + light * 2;
        if (!append("dp3 r0.x, v3, c%u\n"
                    "dp3 r0.y, v4, c%u\n"
                    "dp3 r0.z, v5, c%u\n"
                    "dp3 r2.x, r0, r0\n"
                    "max r2.x, r2.x, c24.w\n"
                    "rsq r2.x, r2.x\n"
                    "mul r2.x, r0.z, r2.x\n"
                    "max r2.x, r2.x, c24.x\n"
                    "mad r1.xyz, r2.x, c%u, r1\n",
                    direction, direction, direction, direction + 1)) return 0;
    }
    // Clamp the accumulated light before fading, matching the colour output
    // clamp that ordinary vertex lighting receives before framebuffer blend.
    if (!append("min r1.xyz, r1, c24.y\n"
                "mul r1.xyz, r1, c25.x\n"
                "mov r1.w, c24.z\n"
                "mov oD0, r1\n"
                "mov oD1, c24.x\n")) return 0;
    BufferPointer compiled = nullptr;
    BufferPointer errors = nullptr;
    const HRESULT assembled = assemble(source.data(), static_cast<UINT>(used),
                                       0, nullptr, &compiled, &errors);
    if (FAILED(assembled) || !compiled) {
        char detail[561]{};
        if (errors && errors->GetBufferPointer()) {
            const auto length = std::min<std::size_t>(
                sizeof(detail) - 1, errors->GetBufferSize());
            std::memcpy(detail, errors->GetBufferPointer(), length);
        }
        log_cloak_shader_failure("assembly", assembled, lights, detail);
        if (errors) errors->Release();
        if (compiled) compiled->Release();
        return 0;
    }
    if (errors) errors->Release();
    DWORD shader = 0;
    const HRESULT created = device->CreateVertexShader(declaration.data(),
        static_cast<const DWORD*>(compiled->GetBufferPointer()), &shader, 0);
    compiled->Release();
    if (FAILED(created) || shader == 0) {
        log_cloak_shader_failure("device creation", created, lights);
        return 0;
    }
    g_cloak_composite_shaders[lights] = shader;
    char message[128]{};
    std::snprintf(message, sizeof(message),
        "Cloak compositor GPU shader created: lights=%lu, handle=0x%08lx",
        static_cast<unsigned long>(lights), static_cast<unsigned long>(shader));
    log_line(message);
    return shader;
}

void restore_cloak_composite() noexcept {
    auto& state = g_cloak_composite;
    if (!state.active || !state.device) return;
    state.active = false;
    state.device->SetVertexShader(state.saved_shader);
    state.device->SetVertexShaderConstant(kCloakCompositeFirstConstant,
        state.saved_constants.data(), kCloakCompositeConstantCount);
    for (DWORD stage = 0; stage < 2; ++stage)
        for (std::size_t i = 0; i < kCloakCompositeStageStates.size(); ++i)
            state.device->SetTextureStageState(stage,
                kCloakCompositeStageStates[i], state.saved_stages[stage][i]);
}

extern "C" void clear_cloak_composite_selection() noexcept {
    restore_cloak_composite();
    g_cloak_composite = CloakCompositeState{};
}

extern "C" void release_cloak_composite_shaders(IDirect3DDevice8* device) noexcept {
    release_transparent_mesh_indices(device);
    clear_cloak_composite_selection();
    if (device && g_cloak_shader_device == device &&
        device_method_is_callable(device, kDeleteVertexShaderVtableIndex)) {
        for (DWORD shader : g_cloak_composite_shaders)
            if (shader) device->DeleteVertexShader(shader);
    }
    g_cloak_composite_shaders.fill(0);
    g_cloak_shader_device = nullptr;
    g_cloak_composite_rejected = false;
}

extern "C" bool prepare_cloak_composite(void* material) noexcept {
    clear_cloak_composite_selection();
    if (!a2fo_nebula_general_mesh_enabled()) return true;
    if (!g_device || g_cloak_composite_rejected || !material) return false;
    const void* sentinel = read_at<const void*>(phong_engine(), 0x60, nullptr);
    if (!readable_range(sentinel, 12)) return false;
    const void* node = read_at<const void*>(sentinel, 0, nullptr);
    std::size_t lights = 0;
    while (node != sentinel) {
        if (!readable_range(node, 12) || lights == kCloakCompositeLights) {
            if (InterlockedCompareExchange(&g_cloak_composite_fallback_logged, 1, 0) == 0)
                log_line("General compositor unavailable: invalid light list or more than eight scene lights; native route retained");
            return false;
        }
        ++lights;
        node = read_at<const void*>(node, 0, nullptr);
    }
    const DWORD shader = create_cloak_composite_shader(g_device, lights);
    if (!shader) {
        g_cloak_composite_rejected = true;
        if (InterlockedCompareExchange(&g_cloak_composite_fallback_logged, 1, 0) == 0)
            log_line("General compositor shader unavailable; native route retained");
        return false;
    }
    g_cloak_composite.material = material;
    g_cloak_composite.craft = current_render_craft();
    g_cloak_composite.device = g_device;
    g_cloak_composite.shader = shader;
    g_cloak_composite.capacity = lights;
    g_cloak_composite.selected = true;
    return true;
}

bool cloak_composite_selected(IDirect3DDevice8* device) noexcept {
    const auto& state = g_cloak_composite;
    return state.selected && state.device == device &&
        state.craft == current_render_craft();
}

void reject_cloak_composite_draw() noexcept {
    restore_cloak_composite();
    g_cloak_composite.failed = true;
    g_cloak_composite_rejected = true;
    log_line("Cloak compositor state failure; subsequent cloak meshes use native sorting");
}

} // namespace

extern "C" int __cdecl a2fo_nebula_cloak_collect_light(
    IDirect3DDevice8* device) noexcept {
    if (!cloak_composite_selected(device)) return 0;
    auto& state = g_cloak_composite;
    // A rejected partial draw must not resume the opaque accumulation that
    // caused the white flash. The next selector will use the native fallback.
    if (state.failed) return 1;
    if (state.count >= state.capacity) {
        reject_cloak_composite_draw();
        return 1;
    }
    std::array<float, 4> direction{};
    DWORD factor = 0;
    if (FAILED(device->GetVertexShaderConstant(6, direction.data(), 1)) ||
        FAILED(device->GetRenderState(D3DRS_TEXTUREFACTOR, &factor)) ||
        !std::isfinite(direction[0]) || !std::isfinite(direction[1]) ||
        !std::isfinite(direction[2])) {
        reject_cloak_composite_draw();
        return 1;
    }
    state.constants[state.count * 2] = direction;
    state.constants[state.count * 2 + 1] = {{
        static_cast<float>((factor >> 16) & 255) / 255.0f,
        static_cast<float>((factor >> 8) & 255) / 255.0f,
        static_cast<float>(factor & 255) / 255.0f, 0.0f
    }};
    ++state.count;
    return 1;
}

extern "C" void __cdecl a2fo_nebula_cloak_begin_final(
    IDirect3DDevice8* device) noexcept {
    if (cloak_composite_selected(device)) {
        g_fast_nonbump_alpha_material = g_cloak_composite.alpha_sort
            ? g_cloak_composite.material : nullptr;
    }
}

extern "C" void __cdecl a2fo_nebula_cloak_final_pre(
    IDirect3DDevice8* device) noexcept {
    if (!cloak_composite_selected(device)) return;
    auto& state = g_cloak_composite;
    float alpha = 1.0f;
    DWORD alpha_blending = FALSE;
    DWORD source_blend = D3DBLEND_SRCALPHA;
    DWORD destination_blend = D3DBLEND_ZERO;
    if (!active_storm_material_alpha(device, &alpha) ||
        FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &alpha_blending)) ||
        FAILED(device->GetRenderState(D3DRS_SRCBLEND, &source_blend)) ||
        FAILED(device->GetRenderState(D3DRS_DESTBLEND, &destination_blend)) ||
        FAILED(device->GetVertexShader(&state.saved_shader)) ||
        FAILED(device->GetVertexShaderConstant(kCloakCompositeFirstConstant,
            state.saved_constants.data(), kCloakCompositeConstantCount))) {
        reject_cloak_composite_draw();
        return;
    }
    for (DWORD stage = 0; stage < 2; ++stage)
        for (std::size_t i = 0; i < kCloakCompositeStageStates.size(); ++i)
            if (FAILED(device->GetTextureStageState(stage,
                    kCloakCompositeStageStates[i], &state.saved_stages[stage][i]))) {
                reject_cloak_composite_draw();
                return;
            }
    state.constants[16] = {{0.0f, 1.0f, alpha, 1.0e-12f}};
    // ONE-source additive/premultiplied blending does not multiply RGB by
    // source alpha. Fade it here exactly once; SRCALPHA modes already do
    // that in the blend unit and must retain unscaled lighting RGB.
    const float rgb_fade = alpha_blending && source_blend == D3DBLEND_ONE
        ? alpha : 1.0f;
    state.constants[17] = {{rgb_fade, 0.0f, 0.0f, 0.0f}};
    state.active = true;
    // Native code has already bound the diffuse hull texture and published
    // the cloak blend/depth policy. Keep those and the mapped-material hooks.
    const std::array<std::array<DWORD, 6>, 2> stages{{
        {{D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE,
          D3DTOP_MODULATE, D3DTA_TEXTURE, D3DTA_DIFFUSE}},
        {{D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT,
          D3DTOP_SELECTARG1, D3DTA_CURRENT, D3DTA_CURRENT}}
    }};
    if (FAILED(device->SetVertexShader(state.shader)) ||
        FAILED(device->SetVertexShaderConstant(kCloakCompositeFirstConstant,
            state.constants.data(), kCloakCompositeConstantCount))) {
        reject_cloak_composite_draw();
        return;
    }
    for (DWORD stage = 0; stage < 2; ++stage)
        for (std::size_t i = 0; i < kCloakCompositeStageStates.size(); ++i)
            if (FAILED(device->SetTextureStageState(
                    stage, kCloakCompositeStageStates[i], stages[stage][i]))) {
                reject_cloak_composite_draw();
                return;
            }
    if (InterlockedCompareExchange(&g_cloak_composite_logged, 1, 0) == 0) {
        log_line("Single-pass textured cloak lighting active; opaque DOT3 light draws suppressed");
        char message[192]{};
        std::snprintf(message, sizeof(message),
            "Cloak GPU blend: source=%lu destination=%lu alpha=%.3f RGB fade=%.3f",
            static_cast<unsigned long>(source_blend),
            static_cast<unsigned long>(destination_blend),
            static_cast<double>(alpha), static_cast<double>(rgb_fade));
        log_line(message);
    }
    if (state.alpha_sort) {
        const unsigned category = destination_blend == D3DBLEND_ONE ? 1u : 0u;
        if (InterlockedCompareExchange(
                &g_cloak_composite_alpha_logged[category], 1, 0) == 0) {
            char message[240]{};
            std::snprintf(message, sizeof(message),
                "Alpha compositor DRAW active: shader=0x%08lx lights=%lu src=%lu dst=%lu alpha=%.3f craft-context=%s; native opaque light draws suppressed",
                static_cast<unsigned long>(state.shader),
                static_cast<unsigned long>(state.count),
                static_cast<unsigned long>(source_blend),
                static_cast<unsigned long>(destination_blend),
                static_cast<double>(alpha), state.craft ? "yes" : "no");
            log_line(message);
        }
    }
}

extern "C" void __cdecl a2fo_nebula_cloak_final_post() noexcept {
    restore_cloak_composite();
    if (g_cloak_composite.selected && g_cloak_composite.device) {
        void* wrapper = active_storm_device_wrapper(g_cloak_composite.device);
        if (wrapper && writable_range(static_cast<unsigned char*>(wrapper) +
                kStormCurrentMaterialOffset, sizeof(void*))) {
            void* empty = nullptr;
            std::memcpy(static_cast<unsigned char*>(wrapper) +
                kStormCurrentMaterialOffset, &empty, sizeof(empty));
        }
    }
    // Keep the selector decision for the remaining groups of this mesh.
    g_cloak_composite.count = 0;
    g_cloak_composite.constants = {};
}
