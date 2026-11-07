// Exercise the actual core dispatch with inert native-layout records and COM
// slots. No device, HWND, game process or input is created by this fixture.
#include <windows.h>
#include <d3d8.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/renderer_cloak_policy.hpp"
#include "../renderer/cloak/refraction.hpp"

namespace {
unsigned native_calls = 0, backend_calls = 0, buffer_releases = 0;
a2fo::cloak::Outcome next_outcome = a2fo::cloak::Outcome::drawn;
a2fo::cloak::Parameters last_params;
a2fo::cloak::Draw last_draw;
void* current_craft = nullptr;
bool g_dxvk_backend_active = true, g_hooks_ready = true;
unsigned g_craft_render_overflow = 0;
void* g_m3d_active_render_mesh = nullptr;
void* g_general_render_mesh = nullptr;
constexpr unsigned kGameObjectCloakControllerOffset = 0x108;
constexpr unsigned kCraftClassOffset = 0x40;
template<class T> T read_at(const void* p, std::size_t offset, T fallback) noexcept {
    if (!p) return fallback;
    T value; std::memcpy(&value, static_cast<const BYTE*>(p) + offset, sizeof(value)); return value;
}
template<class T, std::size_t N> void put(std::array<BYTE, N>& p, unsigned offset, T value) {
    if (offset + sizeof(value) > N) std::abort();
    std::memcpy(p.data() + offset, &value, sizeof(value));
}
bool readable_range(const void* p, std::size_t bytes) { return p && bytes; }
void* current_render_craft() { return current_craft; }
unsigned current_craft_cloak_state(void* craft) {
    return read_at<unsigned>(read_at<void*>(craft, 0x108, nullptr), 0x3c, 0);
}
struct Composite {
    bool active = false;
    void* craft = nullptr;
    void* material = nullptr;
    IDirect3DDevice8* device = nullptr;
    std::array<std::array<float, 4>, 19> constants{};
} g_cloak_composite;
bool cloak_composite_selected(IDirect3DDevice8* d) { return g_cloak_composite.device == d; }
std::vector<std::string> diagnostic_lines;
void log_line(const char* line) { diagnostic_lines.emplace_back(line); }
void require(bool ok, const char* what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
struct FakeCom { void** table; };
void* buffer_table[3]{};
FakeCom buffer{buffer_table};
ULONG __stdcall release_buffer(void*) { ++buffer_releases; return 1; }
HRESULT __stdcall native_draw(void*, D3DPRIMITIVETYPE type, UINT minimum,
                              UINT vertices, UINT start, UINT primitives) {
    ++native_calls;
    require(type == D3DPT_TRIANGLELIST && minimum == 2 && vertices == 3 && start == 4 && primitives == 1,
            "native COM arguments changed");
    return E_ACCESSDENIED; // Deliberately preserve a non-success native HRESULT.
}
HRESULT __stdcall constants(void*, DWORD first, void* output, DWORD count) {
    require(first == 2 && count == 4, "native clip constant range");
    float values[16]{}; values[0] = values[5] = values[10] = values[15] = 1;
    std::memcpy(output, values, sizeof(values)); return S_OK;
}
HRESULT __stdcall indices(void*, IDirect3DIndexBuffer8** output, UINT* base) {
    *output = reinterpret_cast<IDirect3DIndexBuffer8*>(&buffer); *base = 7; return S_OK;
}
}
namespace a2fo::cloak {
Refraction::Refraction() : impl_(nullptr) {}
Refraction::~Refraction() = default;
void Refraction::release() noexcept {}
const char* Refraction::diagnostic() const noexcept { return "inert dispatch fixture"; }
Outcome Refraction::draw(void*, const Parameters& p, const Draw& d) noexcept {
    ++backend_calls; last_params = p; last_draw = d; return next_outcome;
}
}

#include "../core/renderer_cloak_refraction.inl"

int main() {
    // SDK-declared IDirect3DDevice8 slots: DIP 71, VS constants 80, indices 86.
    void* table[97]{};
    table[71] = reinterpret_cast<void*>(&native_draw);
    table[80] = reinterpret_cast<void*>(&constants);
    table[86] = reinterpret_cast<void*>(&indices);
    buffer_table[2] = reinterpret_cast<void*>(&release_buffer);
    FakeCom object{table}; auto* device = reinterpret_cast<IDirect3DDevice8*>(&object);
    auto draw = [&]() { return a2fo_nebula_refit_cloak_draw(device, D3DPT_TRIANGLELIST, 2, 3, 4, 1); };
    require(!g_refit_cloak_enabled, "prototype default must be off");
    require(draw() == E_ACCESSDENIED && native_calls == 1 && backend_calls == 0,
            "disabled pass-through/HRESULT");
    std::array<BYTE, 0x10c> craft{};
    std::array<BYTE, 0x50> controller{};
    std::array<BYTE, 0x30> material{};
    std::array<BYTE, 0x130> mesh{};
    float positions[9]{-2,-1,-0.5f, 2,1,0.5f, 0,0,0};
    put(craft, 0x108, controller.data()); put(controller, 0x3c, 1u); put(controller, 0x44, 0.6f);
    put(material, 0x20, DWORD(D3DBLEND_ZERO)); put(mesh, 0xc0, positions); put(mesh, 0xc8, DWORD(3));
    current_craft = craft.data(); g_general_render_mesh = mesh.data();
    g_cloak_composite.active = true; g_cloak_composite.craft = current_craft;
    g_cloak_composite.material = material.data(); g_cloak_composite.device = device;
    g_refit_cloak_enabled = true; native_calls = 0;
    put(craft, 0x40, material.data());
    register_nebula_cloak_class_name(material.data(), "kscout");
    require(draw() == S_OK && native_calls == 0 && backend_calls == 1, "replacement draw submitted once");
    require(std::any_of(diagnostic_lines.begin(), diagnostic_lines.end(), [](const std::string& s) {
        return s.find("odf=kscout state=1 status=active") != std::string::npos;
    }), "named per-ODF active status missing");
    const auto logged = diagnostic_lines.size();
    require(last_draw.base_vertex == 7 && last_draw.min_vertex == 2 && last_draw.num_vertices == 3 &&
            last_draw.start_index == 4 && last_draw.primitive_count == 1, "D3D9 indexed range");
    require(std::fabs(last_params.visibility - 0.5f) < 1e-6f && last_params.hull_floor == 0 &&
            last_params.bounds_extent[0] == 2 && last_params.bounds_extent[1] == 1,
            "engine progression, visibility floor and mesh bounds");
    const auto key = last_params.snapshot_key;
    require(key != 0 && draw() == S_OK && last_params.snapshot_key == key, "material groups share scene snapshot");
    require(diagnostic_lines.size() == logged, "duplicate per-frame status logging");
    reset_refit_cloak_scope();
    require(draw() == S_OK && last_params.snapshot_key != key, "new craft scope gets fresh snapshot");
    next_outcome = a2fo::cloak::Outcome::fatal;
    require(draw() == E_FAIL && native_calls == 0, "fatal submission must not replay native draw");
    const auto calls_after_fatal = backend_calls;
    require(draw() == E_ACCESSDENIED && native_calls == 1 && backend_calls == calls_after_fatal,
            "fatal pass disabled until reset");
    release_refit_cloak_pass(); next_outcome = a2fo::cloak::Outcome::fallback; native_calls = 0;
    require(draw() == E_ACCESSDENIED && native_calls == 1, "pre-submission failure native fallback/HRESULT");
    release_refit_cloak_pass(); next_outcome = a2fo::cloak::Outcome::drawn; native_calls = 0;
    put(material, 0x20, DWORD(D3DBLEND_ONE));
    require(draw() == E_ACCESSDENIED && native_calls == 1, "authored additive material retained");
    put(material, 0x20, DWORD(D3DBLEND_ZERO)); put(controller, 0x3c, 0u);
    require(draw() == E_ACCESSDENIED && native_calls == 2, "uncloaked hull retained");
    require(buffer_releases == backend_calls, "GetIndices ownership balanced");
    require(std::any_of(diagnostic_lines.begin(), diagnostic_lines.end(), [](const std::string& s) {
        return s.find("odf=kscout state=1 status=fallback") != std::string::npos;
    }), "named per-ODF fallback status missing");
    put(controller, 0x3c, 3u);
    const auto before_routes = diagnostic_lines.size();
    report_refit_cloak_mesh(current_craft, 0, false, false);
    report_refit_cloak_mesh(current_craft, 4, false, false);
    report_refit_cloak_mesh(current_craft, 4, true, true);
    require(diagnostic_lines.size() == before_routes + 3, "distinct route fallback categories were suppressed");
    report_refit_cloak_mesh(current_craft, 0, false, false);
    require(diagnostic_lines.size() == before_routes + 3, "route log duplicated per frame");
    g_refit_cloak_enabled = false;
    require(!refit_cloak_diagnostics_enabled(), "disabled route diagnostics gate");
    report_refit_cloak_mesh(current_craft, 0, false, false);
    require(diagnostic_lines.size() == before_routes + 3, "disabled path logged route diagnostics");
    std::puts("PASS: cloak native dispatch, stdcall arguments, HRESULT, fallback and consumed draw");
}
