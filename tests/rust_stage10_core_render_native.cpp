// Whole-source CPU fixture. The runner supplies an absolute CORE_SOURCE copy.
// Every selected render path exits before COM adoption or shader creation.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <string>
#include <vector>

static std::wstring fixture_command_line;
static bool fixture_command_line_null = false;
static LPWSTR WINAPI fixture_GetCommandLineW();
static LONG WINAPI fixture_InterlockedCompareExchange(volatile LONG*, LONG, LONG);
#undef GetCommandLineW
#define GetCommandLineW fixture_GetCommandLineW
#undef InterlockedCompareExchange
#define InterlockedCompareExchange fixture_InterlockedCompareExchange
#include CORE_SOURCE
#undef GetCommandLineW
#undef InterlockedCompareExchange

namespace a2fo::renderer::game {
std::string legacy_source_path(IDirect3DDevice8*, const char*) noexcept {
    return {};
}
}

static std::vector<const volatile LONG*> atomic_reads;
static LPWSTR WINAPI fixture_GetCommandLineW() {
    return fixture_command_line_null ? nullptr : fixture_command_line.data();
}
static LONG WINAPI fixture_InterlockedCompareExchange(
    volatile LONG* location, LONG exchange, LONG comparand) {
    atomic_reads.push_back(location);
    const LONG previous = *location;
    if (previous == comparand) *location = exchange;
    return previous;
}

namespace {

using namespace a2fo;

void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "FAIL:%s\n", label);
        std::abort();
    }
}

void test_command_line() {
    fixture_command_line_null = true;
    check(!command_line_requests_dx9(), "command null");
    fixture_command_line_null = false;
    struct Case { const wchar_t* input; bool expected; };
    const Case cases[]{
        {L"game.exe", false},
        {L"game.exe /d3d9", true},
        {L"game.exe -D3D9", true},
        {L"game.exe /d3d9extra", false},
        {L"game.exe \"/d3d9\"", true},
        {L"game.exe /d3\"d9", true},
        {L"game.exe \"/d3d9", true},
        {L"game.exe\t/d3d9", true},
        {L"game.exe \u03a9/d3d9", false},
        {L"game.exe \u03a9 /d3d9", true},
        {L"game.exe /d3d9\tother", true},
    };
    for (const auto& item : cases) {
        fixture_command_line = item.input;
        check(command_line_requests_dx9() == item.expected, "command token");
    }
    fixture_command_line = L"game.exe " + std::wstring(8192, L'x') + L" /d3d9";
    check(command_line_requests_dx9(), "command huge token then switch");
    fixture_command_line = L"game.exe " + std::wstring(8192, L'x');
    check(!command_line_requests_dx9(), "command huge non-switch");
    std::puts("render-command-line:PASS");
}

void test_join_path() {
    check(join_path(nullptr, nullptr).empty(), "path null pair");
    check(join_path("", "tail") == "tail", "path empty left");
    check(join_path("root", nullptr) == "root\\", "path null right");
    check(join_path("root", "tail") == "root\\tail", "path separator");
    check(join_path("root/", "tail") == "root/tail", "path slash");
    check(join_path("root\\", "tail") == "root\\tail", "path backslash");
    std::puts("render-path:PASS");
}

void test_suffix() {
    check(std::setlocale(LC_CTYPE, "C") != nullptr, "C locale available");
    g_faction_texture_suffixes.clear();
    check(!register_nebula_faction_texture_suffix(nullptr), "suffix null");
    check(!register_nebula_faction_texture_suffix(""), "suffix empty");
    check(register_nebula_faction_texture_suffix("Ab_9-Z"), "suffix accepted");
    check(g_faction_texture_suffixes.count("ab_9-z") == 1, "suffix lowered");
    check(register_nebula_faction_texture_suffix("aB_9-z"), "suffix duplicate success");
    check(g_faction_texture_suffixes.size() == 1, "suffix duplicate size");
    check(register_nebula_faction_texture_suffix(std::string(32, 'A').c_str()),
          "suffix max length");
    check(g_faction_texture_suffixes.count(std::string(32, 'a')) == 1,
          "suffix max stored");
    check(!register_nebula_faction_texture_suffix(std::string(33, 'A').c_str()),
          "suffix overlong");
    check(!register_nebula_faction_texture_suffix("a.b"), "suffix punctuation");
    const char high[]{'a', static_cast<char>(0xff), 0};
    check(!register_nebula_faction_texture_suffix(high), "suffix high byte C locale");
    check(g_faction_texture_suffixes.size() == 2, "suffix rejection unchanged");
    std::puts("render-suffix:PASS");
}

void reset_gate() {
    g_hooks_ready = true;
    g_dxvk_backend_active = true;
    g_runtime_enabled = 1;
    g_fast_nonbump_shader_rejected = 0;
    g_craft_render_overflow = 0;
    atomic_reads.clear();
}

void test_gate() {
    reset_gate();
    check(a2fo_nebula_general_mesh_enabled(), "gate enabled");
    check(atomic_reads.size() == 2 && atomic_reads[0] == &g_runtime_enabled &&
          atomic_reads[1] == &g_fast_nonbump_shader_rejected, "gate atomic order");
    reset_gate(); g_hooks_ready = false;
    check(!a2fo_nebula_general_mesh_enabled() && atomic_reads.empty(),
          "gate hooks short circuit");
    reset_gate(); g_dxvk_backend_active = false;
    check(!a2fo_nebula_general_mesh_enabled() && atomic_reads.empty(),
          "gate backend short circuit");
    reset_gate(); g_runtime_enabled = 0;
    check(!a2fo_nebula_general_mesh_enabled() && atomic_reads.size() == 1 &&
          atomic_reads[0] == &g_runtime_enabled, "gate runtime short circuit");
    reset_gate(); g_fast_nonbump_shader_rejected = 1;
    check(!a2fo_nebula_general_mesh_enabled() && atomic_reads.size() == 2,
          "gate rejection");
    reset_gate(); g_craft_render_overflow = 1;
    check(!a2fo_nebula_general_mesh_enabled() && atomic_reads.size() == 2,
          "gate overflow");
    std::puts("render-gate:PASS");
}

template <typename T>
void put(unsigned char* bytes, std::size_t offset, T value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}

void test_selector() {
    alignas(void*) unsigned char mesh[0x140]{};
    alignas(void*) unsigned char wrapper[0xa0]{};
    alignas(void*) unsigned char material[0x50]{};
    alignas(void*) unsigned char meshvb[0x20]{};
    void* texture_slots[2]{nullptr, material};
    g_armada = reinterpret_cast<HMODULE>(0x10000000u);
    put(wrapper, kStormCurrentMaterialOffset, static_cast<void*>(material));
    put(mesh, 0x128, static_cast<void*>(meshvb));
    g_renderer_route_diagnostics_enabled = false;
    g_log = [](const std::string&) {};
    reset_gate();
    g_general_render_mesh = material;
    g_m3d_active_render_mesh = material;
    a2fo_nebula_general_mesh_selector(9, mesh, wrapper);
    check(!g_general_render_mesh && !g_m3d_active_render_mesh,
          "selector unsupported clears meshes");

    g_cloak_composite.selected = true;
    g_cloak_composite.material = material;
    g_cloak_composite.alpha_sort = false;
    a2fo_nebula_general_mesh_selector(5, mesh, wrapper);
    check(g_general_render_mesh == mesh && g_cloak_composite.selected &&
          g_cloak_composite.alpha_sort, "selector same material sort retained");

    g_cloak_composite.selected = true;
    g_cloak_composite.material = material;
    a2fo_nebula_general_mesh_selector(4, mesh, wrapper);
    check(g_general_render_mesh == mesh && !g_cloak_composite.selected,
          "selector non-DOT3 clears selection");

    put(meshvb, 0, at(g_armada, 0x002bc7d4));
    put(mesh, 0x114, static_cast<void*>(texture_slots));
    put(mesh, 0x11c, 2);
    put(material, 0x20, static_cast<DWORD>(D3DBLEND_ZERO));
    g_fast_nonbump_enabled = 0;
    g_craft_render_depth = 0;
    a2fo_nebula_general_mesh_selector(4, mesh, wrapper);
    check(g_general_render_mesh == mesh && !g_cloak_composite.selected &&
          g_device == nullptr, "selector DOT3 bump native bypass");
    std::puts("render-selector:PASS");
}

void test_compositor() {
    alignas(void*) unsigned char material[0x50]{};
    g_device = nullptr;
    g_cloak_composite.selected = true;
    reset_gate(); g_hooks_ready = false;
    check(prepare_cloak_composite(material) && !g_cloak_composite.selected,
          "compositor inactive clears then succeeds");
    reset_gate();
    check(!prepare_cloak_composite(material), "compositor no device");
    g_device = reinterpret_cast<IDirect3DDevice8*>(0x1000u);
    g_cloak_composite_rejected = true;
    check(!prepare_cloak_composite(material), "compositor rejected");
    g_cloak_composite_rejected = false;
    check(!prepare_cloak_composite(nullptr), "compositor null material");
    // The production phong_engine() reads a pointer at this documented RVA.
    // The synthetic image exists only to make that read valid.
    static std::vector<unsigned char> image(kPhongEnginePointerRva + sizeof(void*));
    alignas(void*) unsigned char engine[0x80]{};
    g_armada = reinterpret_cast<HMODULE>(image.data());
    put(image.data(), kPhongEnginePointerRva, static_cast<void*>(engine));
    check(!prepare_cloak_composite(material), "compositor missing sentinel");
    alignas(void*) unsigned char sentinel[16]{};
    put(engine, 0x60, static_cast<void*>(sentinel));
    put(sentinel, 0, static_cast<void*>(sentinel));
    check(!prepare_cloak_composite(material), "compositor empty light list");
    // The empty-list prerequisite path can reject the fast shader globally.
    // Keep the next traversal cases enabled and independent.
    reset_gate();
    g_cloak_composite_rejected = false;
    alignas(void*) unsigned char nodes[9][16]{};
    for (unsigned i = 0; i < 9; ++i)
        put(nodes[i], 0, static_cast<void*>(i == 8 ? sentinel : nodes[i + 1]));
    put(sentinel, 0, static_cast<void*>(nodes[0]));
    check(!prepare_cloak_composite(material) && !g_cloak_composite.selected,
          "compositor ninth light fallback");
    put(nodes[0], 0, static_cast<void*>(nodes[0]));
    check(!prepare_cloak_composite(material), "compositor cycle fallback");
    g_device = nullptr;
    std::puts("render-compositor:PASS");
}

} // namespace

int main() {
    test_command_line();
    test_join_path();
    test_suffix();
    test_gate();
    test_selector();
    test_compositor();
    std::puts("stage10 core render native: PASS");
    return 0;
}
