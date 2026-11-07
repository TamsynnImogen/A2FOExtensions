// Optional cloak replacement at the existing, signature-checked FO DOT3 draw.
// This file shares the core's native device/material/craft lifetime. It adds
// no engine hooks, gameplay writes, object enumeration or renderer selection.
namespace {

SRWLOCK g_refit_cloak_lock = SRWLOCK_INIT;
struct RefitCloakLock {
    RefitCloakLock() noexcept { AcquireSRWLockExclusive(&g_refit_cloak_lock); }
    ~RefitCloakLock() { ReleaseSRWLockExclusive(&g_refit_cloak_lock); }
};
std::unique_ptr<a2fo::cloak::Refraction> g_refit_cloak_pass;
bool g_refit_cloak_enabled = false;
bool g_refit_cloak_blocked = false;
volatile LONG g_refit_cloak_logged = 0;
volatile LONG g_refit_cloak_fallback_logged = 0;

struct RefitCloakClassStatus {
    std::string odf;
    std::uint32_t reported = 0;
};
std::unordered_map<const void*, RefitCloakClassStatus> g_refit_cloak_classes;

// Caller holds g_refit_cloak_lock. At most one line per ODF/state/reason category;
// rendering a fleet cannot produce per-frame log traffic.
void report_refit_cloak_ship(void* craft, unsigned event, const char* status,
                            const char* reason) noexcept {
    if (!g_refit_cloak_enabled || !craft || event > 7) return;
    const unsigned state = current_craft_cloak_state(craft);
    if (state < 1 || state > 3) return;
    const void* object_class = read_at<const void*>(craft, kCraftClassOffset, nullptr);
    const auto found = g_refit_cloak_classes.find(object_class);
    if (found == g_refit_cloak_classes.end()) return;
    const auto bit = std::uint32_t(1) << (event * 4 + state - 1);
    if (found->second.reported & bit) return;
    found->second.reported |= bit;
    char line[768]{};
    std::snprintf(line, sizeof(line), "Refit cloak ship: odf=%.160s state=%u status=%s reason=%.460s",
        found->second.odf.c_str(), state, status, reason);
    log_line(line);
}

struct RefitCloakBounds {
    void* mesh = nullptr;
    std::array<float, 3> centre{};
    std::array<float, 3> extent{};
    unsigned long long snapshot = 0;
};
thread_local RefitCloakBounds g_refit_cloak_bounds{};
// Accessed under g_refit_cloak_lock. Keys cannot collide between render threads.
unsigned long long g_refit_cloak_snapshot = 0;

void reset_refit_cloak_scope() noexcept { g_refit_cloak_bounds = {}; }

bool refit_cloak_mesh_bounds(void* mesh) noexcept {
    if (g_refit_cloak_bounds.mesh == mesh && mesh) return true;
    reset_refit_cloak_scope();
    // Same CPU position/count layout checked by unmapped_geometry_compatible.
    const DWORD count = read_at<DWORD>(mesh, 0xc8, 0);
    const auto* positions = read_at<const unsigned char*>(mesh, 0xc0, nullptr);
    if (!count || count > 65535 || !readable_range(positions, count * 12u)) return false;
    std::array<float, 3> low{}, high{};
    for (DWORD i = 0; i < count; ++i) {
        for (unsigned c = 0; c < 3; ++c) {
            const float v = read_at<float>(positions, i * 12u + c * 4u, NAN);
            if (!std::isfinite(v) || std::fabs(v) > 1e6f) return false;
            if (i == 0) low[c] = high[c] = v;
            else { low[c] = std::min(low[c], v); high[c] = std::max(high[c], v); }
        }
    }
    for (unsigned c = 0; c < 3; ++c) {
        g_refit_cloak_bounds.centre[c] = (low[c] + high[c]) * 0.5f;
        g_refit_cloak_bounds.extent[c] = std::max(0.001f, (high[c] - low[c]) * 0.5f);
    }
    if (++g_refit_cloak_snapshot == 0) ++g_refit_cloak_snapshot;
    g_refit_cloak_bounds.snapshot = g_refit_cloak_snapshot;
    g_refit_cloak_bounds.mesh = mesh;
    return true;
}

} // namespace

void register_nebula_cloak_class_name(void* object_class, const char* odf) noexcept {
    if (!g_refit_cloak_enabled || !object_class || !odf || !*odf) return;
    const RefitCloakLock lock;
    try {
        const std::size_t length = std::strlen(odf);
        if (length > 160 || g_refit_cloak_classes.size() >= 4096) return;
        g_refit_cloak_classes[object_class] = {std::string(odf, length), 0};
    } catch (...) { /* Diagnostics must never affect class loading. */ }
}

extern "C" void report_refit_cloak_mesh(
    void* craft, unsigned reason, bool dot3, bool m3d) noexcept {
    if (!g_refit_cloak_enabled || !craft) return;
    const RefitCloakLock lock;
    if (m3d)
        report_refit_cloak_ship(craft, 4, "fallback", "M3D compatibility mesh");
    else if (reason != 4 && reason != 5)
        report_refit_cloak_ship(craft, 3, "fallback", "native CPU/sorted route; no admitted MeshVB draw");
    else if (!dot3)
        report_refit_cloak_ship(craft, 6, "fallback", "standard MeshVB path; no DOT3 stream");
}

extern "C" bool refit_cloak_diagnostics_enabled() noexcept {
    return g_refit_cloak_enabled;
}

extern "C" void release_refit_cloak_pass() noexcept {
    const RefitCloakLock lock;
    if (g_refit_cloak_pass) g_refit_cloak_pass->release();
    g_refit_cloak_blocked = false;
    reset_refit_cloak_scope();
}

// An ordinary stdcall replacement consumes exactly the native COM call's six
// arguments. The surrounding assembly still runs every existing post hook and
// preserves this HRESULT. A failed restore consumes the call with E_FAIL: it
// must never duplicate a submitted draw or replay against uncertain state.
extern "C" HRESULT __stdcall a2fo_nebula_refit_cloak_draw(
    IDirect3DDevice8* device, D3DPRIMITIVETYPE type, UINT minimum,
    UINT vertices, UINT start, UINT primitives) noexcept {
    capture_ingame_shadow_draw(device,type,minimum,vertices,start,primitives);
    auto native = [&]() noexcept {
        return device->DrawIndexedPrimitive(type, minimum, vertices, start, primitives);
    };
    if (!g_refit_cloak_enabled) return native();
    // Serialize the mutable D3D9 pass and its release/reset path. No assumption
    // about a single engine render thread is required for this cache's safety.
    const RefitCloakLock lock;
    if (g_refit_cloak_blocked) {
        report_refit_cloak_ship(current_render_craft(), 5, "fallback", "refraction pass unavailable until device reset");
        return native();
    }
    const auto& state = g_cloak_composite;
    const unsigned cloak_state = current_craft_cloak_state(state.craft);
    if (!a2fo::cloak::eligible(g_refit_cloak_enabled && !g_refit_cloak_blocked,
            g_dxvk_backend_active && g_hooks_ready,
            state.active && cloak_composite_selected(device),
            state.craft && state.craft == current_render_craft() && g_craft_render_overflow == 0,
            cloak_state, read_at<DWORD>(state.material, 0x20, 0)) ||
        type != D3DPT_TRIANGLELIST || !vertices || !primitives ||
        g_m3d_active_render_mesh || !refit_cloak_mesh_bounds(g_general_render_mesh)) {
        report_refit_cloak_ship(current_render_craft(), state.active ? 2 : 7, "fallback",
            state.active ? "authored material/mesh outside prototype" : "native cloak compositor not selected");
        return native();
    }

    a2fo::cloak::Parameters params;
    const void* controller = read_at<const void*>(state.craft, kGameObjectCloakControllerOffset, nullptr);
    const float alpha = read_at<float>(controller, 0x44, NAN);
    if (!a2fo::cloak::visibility(cloak_state, alpha, params.visibility) ||
        FAILED(device->GetVertexShaderConstant(2, params.clip, 4))) return native();
    std::memcpy(params.lights, state.constants.data(), sizeof(params.lights));
    std::copy(g_refit_cloak_bounds.centre.begin(), g_refit_cloak_bounds.centre.end(), params.bounds_center);
    std::copy(g_refit_cloak_bounds.extent.begin(), g_refit_cloak_bounds.extent.end(), params.bounds_extent);
    params.snapshot_key = g_refit_cloak_bounds.snapshot;
    params.time = static_cast<float>(GetTickCount() % 1000000u) * 0.001f;
    // Until FO owner/detection policies have a dedicated fixture, reveal no
    // additional hull trace on ANY submitted cloaked ship. Engine culling is
    // authoritative; no hidden object is made visible by this pass.
    params.hull_floor = 0.0f;
    IDirect3DIndexBuffer8* indices = nullptr;
    UINT base = 0;
    const HRESULT got_indices = device->GetIndices(&indices, &base);
    if (indices) indices->Release();
    if (FAILED(got_indices) || !indices || base > static_cast<UINT>(INT_MAX)) return native();
    const a2fo::cloak::Draw draw{static_cast<unsigned>(type), static_cast<int>(base), minimum, vertices, start, primitives};
    try {
        if (!g_refit_cloak_pass) g_refit_cloak_pass = std::make_unique<a2fo::cloak::Refraction>();
    } catch (...) { return native(); }
    const auto result = g_refit_cloak_pass->draw(device, params, draw);
    if (result == a2fo::cloak::Outcome::drawn) {
        report_refit_cloak_ship(state.craft, 1, "active", "D3D9 refraction DRAW executed");
        if (InterlockedCompareExchange(&g_refit_cloak_logged, 1, 0) == 0)
            log_line("Refit cloak prototype DRAW active: D3D9 scene refraction, native hull geometry and engine cloak progression");
        return D3D_OK;
    }
    // A provider/shader/copy failure is cached until reset, so unsupported
    // backends never compile or allocate again for every ship/material.
    g_refit_cloak_blocked = true;
    report_refit_cloak_ship(state.craft, 5, "fallback", g_refit_cloak_pass->diagnostic());
    if (InterlockedCompareExchange(&g_refit_cloak_fallback_logged, 1, 0) == 0) {
        char message[768]{};
        std::snprintf(message, sizeof(message), "Refit cloak prototype %s: %.600s",
            result == a2fo::cloak::Outcome::fatal ? "draw failed; call consumed and pass disabled" : "unavailable; current cloak retained",
            g_refit_cloak_pass->diagnostic());
        log_line(message);
    }
    return result == a2fo::cloak::Outcome::fatal ? E_FAIL : native();
}
