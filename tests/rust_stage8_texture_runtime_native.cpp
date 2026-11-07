#include <windows.h>
#include <cstdlib>
#include <new>

int fixture_exception_destructors = 0;
bool fixture_fail_next_allocation = false;
struct FixtureAllocationFailure final : std::bad_alloc {
    const char* what() const noexcept override { return "fixture allocation failure"; }
    ~FixtureAllocationFailure() noexcept override { ++fixture_exception_destructors; }
};
void* operator new(std::size_t size) {
    if (fixture_fail_next_allocation) {
        fixture_fail_next_allocation = false;
        throw FixtureAllocationFailure{};
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* fixture_probe_node = nullptr;
bool fixture_mutate_on_read = false;
bool fixture_probe_mutated = false;
SIZE_T WINAPI fixture_virtual_query(
    LPCVOID address, PMEMORY_BASIC_INFORMATION information, SIZE_T size);
#define VirtualQuery fixture_virtual_query
#include MODULE_SOURCE
#undef VirtualQuery

#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> trace_rows;
std::string mode;
std::array<std::uint8_t, 0x180> mesh{};
std::array<std::uint8_t, 0x80> texture{};
std::array<std::uint8_t, 0x80> replacement{};
std::array<std::uint8_t, 0x100> variant{};
std::array<std::uint8_t, 0x100> node{};
int instance_tag, class_tag, race_tag;
bool mutated = false;
const char* original_name = "hull.tga";
const char* replacement_name = "new.tga";

template <typename T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

void check(bool condition, const char* label) {
    if (!condition) { std::cerr << label << '\n'; std::exit(1); }
}

std::string texture_label(const void* ptr) {
    if (ptr == texture.data()) return "original";
    if (ptr == replacement.data()) return "replacement";
    if (ptr == variant.data()) return "variant";
    if (ptr == mesh.data()) return "mesh";
    return ptr ? "other" : "null";
}

void setup() {
    static A2FO_ModuleApi api{};
    api.log = [](const char*, const char* message) {
        // MinGW %p has no 0x prefix; only this known fixture identity may vary
        // with the frozen/current executable layout. Preserve every other byte.
        char address[64]{};
        std::snprintf(address, sizeof(address), "%p", static_cast<void*>(&race_tag));
        std::string text(message);
        const std::string identity = std::string("on Race ") + address;
        const auto found = text.find(identity);
        if (found != std::string::npos)
            text.replace(found, identity.size(), "on Race <race-tag>");
        trace_rows.push_back("log:" + text);
    };
    g_api = &api;
    g_runtime_alive = true;
    g_faction_variants_enabled = true;
    g_capture_enabled = true;
    g_race_suffixes[&race_tag] = "fed";
    g_race_node_names[&race_tag] = "fed";
    g_known_race_node_names.insert("fed");
    g_craft_races[&instance_tag] = CraftRaceState{nullptr, &race_tag};
    put(texture.data(), kTextureNameOffset, original_name);
    put(replacement.data(), kTextureNameOffset, replacement_name);
    put(texture.data(), kTextureFlagsOffset, std::uint32_t{7});
    put(replacement.data(), kTextureFlagsOffset, std::uint32_t{8});
    g_class_meshes[&class_tag].push_back({mesh.data(), texture.data()});
    g_class_faction_nodes[&class_tag].push_back({node.data(), "fed"});
    fixture_probe_node = node.data() + kNodeFlagsOffset;
}

void print_result() {
    const auto name = g_race_node_names.find(&race_tag);
    trace_rows.push_back("race-name:" +
        (name == g_race_node_names.end() ? "<none>" : name->second));
    const auto suffix = g_race_suffixes.find(&race_tag);
    trace_rows.push_back("race-suffix:" +
        (suffix == g_race_suffixes.end() ? "<none>" : suffix->second));
    trace_rows.push_back("known:" + std::to_string(g_known_race_node_names.size()));
    trace_rows.push_back("mesh-original:" + texture_label(
        g_class_meshes[&class_tag][0].original_texture));
    trace_rows.push_back("node-flags:" + std::to_string(
        read_at<std::uint32_t>(node.data(), kNodeFlagsOffset, 0)));
    for (const auto& row : trace_rows) std::cout << row << '\n';
}
} // namespace

SIZE_T WINAPI fixture_virtual_query(
    LPCVOID address, PMEMORY_BASIC_INFORMATION information, SIZE_T size) {
    if (fixture_mutate_on_read && !fixture_probe_mutated &&
        address == fixture_probe_node) {
        fixture_probe_mutated = true;
        put(node.data(), kNodeFlagsOffset, std::uint32_t{0});
        trace_rows.push_back("mutate:flags=0");
    }
    return ::VirtualQuery(address, information, size);
}

extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_0(
    void*, void*) { return 0; }
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_1(
    void*, void*, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_2(
    void*, void* self, std::uintptr_t first, std::uintptr_t) {
    trace_rows.push_back("set:" + texture_label(self) + ":" +
                         texture_label(reinterpret_cast<void*>(first)));
    return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_3(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_4(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t) { return 0; }
extern "C" void a2fo_texture_variants_render_swap_hook() {}

int main(int argc, char** argv) {
    check(argc == 2, "mode required");
    mode = argv[1];
    setup();
    g_set_mesh_texture = reinterpret_cast<void*>(1);
    g_texture_find = [](void*, const char* name, std::uint32_t flags) -> void* {
        trace_rows.push_back(std::string("find:") + name + ":" +
                             std::to_string(flags));
        return mode == "hit" || mode == "cache" ? variant.data() : nullptr;
    };
    g_texture_file_exists = [](const char* path) -> bool {
        trace_rows.push_back(std::string("exists:") + path);
        if (mode == "mutate-miss" && !mutated) {
            mutated = true;
            g_class_meshes[&class_tag][0].original_texture = replacement.data();
            trace_rows.push_back("mutate:original=replacement");
        }
        return mode == "hit" || mode == "cache";
    };
    if (mode == "race-valid") {
        apply_race_rendering_policy(&race_tag, true, "Federation", true, "Fed");
    } else if (mode == "race-invalid") {
        apply_race_rendering_policy(&race_tag, true, "bad!", true, "bad!");
    } else if (mode == "race-absent") {
        apply_race_rendering_policy(&race_tag, false, "", false, "");
    } else if (mode == "race-exception") {
        fixture_fail_next_allocation = true;
        try {
            apply_race_rendering_policy(&race_tag, true, "Federation", true, "Fed");
            trace_rows.push_back("exception:none");
        } catch (const FixtureAllocationFailure&) {
            trace_rows.push_back("exception:typed");
            trace_rows.push_back("lifetime:inside:" +
                                 std::to_string(fixture_exception_destructors));
        } catch (...) {
            trace_rows.push_back("exception:other");
        }
        trace_rows.push_back("lifetime:after:" +
                             std::to_string(fixture_exception_destructors));
        trace_rows.push_back(std::string("injection:") +
                             (fixture_fail_next_allocation ? "unused" : "used"));
    } else if (mode == "hit" || mode == "miss" || mode == "cache" ||
               mode == "mutate-miss" || mode == "borg" || mode == "base") {
        if (mode == "borg") put(mesh.data(), kMeshBorgTextureOffset, variant.data());
        if (mode == "base") g_race_suffixes.erase(&race_tag);
        apply_faction_textures(&instance_tag, &class_tag);
        if (mode == "cache") apply_faction_textures(&instance_tag, &class_tag);
    } else if (mode == "node-show" || mode == "node-hide" ||
               mode == "node-mutate" ||
               mode == "node-admission") {
        put(node.data(), kNodeFlagsOffset, std::uint32_t{3});
        if (mode == "node-hide") g_race_node_names[&race_tag] = "other";
        if (mode == "node-mutate") fixture_mutate_on_read = true;
        if (mode == "node-admission") g_faction_variants_enabled = false;
        apply_faction_node_visibility(&instance_tag, &class_tag);
    } else if (mode == "disabled" || mode == "null") {
        if (mode == "disabled") g_faction_variants_enabled = false;
        apply_faction_textures(mode == "null" ? nullptr : &instance_tag,
                               &class_tag);
        apply_faction_node_visibility(mode == "null" ? nullptr : &instance_tag,
                                      &class_tag);
    } else {
        check(false, "unknown mode");
    }
    print_result();
}
