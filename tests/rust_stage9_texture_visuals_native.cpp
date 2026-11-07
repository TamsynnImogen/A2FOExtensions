#include <windows.h>
#include <cstdlib>
#include <new>

int fixture_exception_destructors = 0;
bool fixture_fail_next_allocation = false;
void (*fixture_on_allocation)() = nullptr;
int fixture_fail_after_allocations = -1;
struct FixtureAllocationFailure final : std::bad_alloc {
    const char* what() const noexcept override { return "fixture allocation failure"; }
    ~FixtureAllocationFailure() noexcept override { ++fixture_exception_destructors; }
};
void* operator new(std::size_t size) {
    if (fixture_fail_next_allocation) {
        fixture_fail_next_allocation = false;
        throw FixtureAllocationFailure{};
    }
    if (void* memory = std::malloc(size ? size : 1)) {
        if (fixture_fail_after_allocations > 0 &&
            --fixture_fail_after_allocations == 0)
            fixture_fail_next_allocation = true;
        if (fixture_on_allocation) {
            auto* callback = fixture_on_allocation;
            fixture_on_allocation = nullptr;
            callback();
        }
        return memory;
    }
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

void* fixture_probe_node = nullptr;
bool fixture_mutate_on_read = false;
bool fixture_probe_mutated = false;
void* fixture_fail_on_query = nullptr;
void* fixture_partial_nodes[2]{};
void* fixture_partial_first = nullptr;
bool fixture_partial_injected = false;
bool fixture_partial_armed = false;
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
#include <map>

namespace {
std::vector<std::string> trace_rows;
std::string mode;
std::array<std::uint8_t, 0x180> mesh{};
std::array<std::uint8_t, 0x80> texture{};
std::array<std::uint8_t, 0x80> replacement{};
std::array<std::uint8_t, 0x100> variant{};
std::array<std::uint8_t, 0x100> node{};
std::array<std::uint8_t, 0x180> walk_root{}, walk_child{}, walk_sibling{}, walk_outside{};
std::array<std::uint8_t, 0x300> walk_class{};
std::array<std::uint8_t, 0x80> walk_geometry{};
std::array<void*, 2> walk_vtable{};
std::array<std::uint8_t, 0x300> craft{};
std::array<std::uint8_t, kSubsystemCount * kCraftSystemSize> systems{};
int instance_tag, class_tag, race_tag, parameter_tag;
std::map<std::string, std::string> odf_values;
std::vector<void*>* fixture_append_subtree = nullptr;
std::vector<SubsystemMeshEntry>* fixture_append_entries = nullptr;
void* fixture_resolve_mutate_query = nullptr;
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
    if (ptr == walk_root.data()) return "walk-root";
    if (ptr == walk_child.data()) return "walk-child";
    if (ptr == walk_sibling.data()) return "walk-sibling";
    if (ptr == walk_outside.data()) return "walk-outside";
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
        const std::string class_identity = std::string("on CraftClass ") + [&] {
            char class_address[64]{};
            std::snprintf(class_address, sizeof(class_address), "%p", static_cast<void*>(&class_tag));
            return std::string(class_address);
        }();
        const auto class_found = text.find(class_identity);
        if (class_found != std::string::npos)
            text.replace(class_found, class_identity.size(), "on CraftClass <class-tag>");
        char walk_class_address[64]{};
        std::snprintf(walk_class_address, sizeof(walk_class_address), "%p",
                      static_cast<void*>(walk_class.data()));
        const std::string walk_identity =
            std::string("on CraftClass ") + walk_class_address;
        const auto walk_found = text.find(walk_identity);
        if (walk_found != std::string::npos)
            text.replace(walk_found, walk_identity.size(),
                         "on CraftClass <walk-class>");
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
    g_subsystem_visuals_enabled = true;
}

void print_result() {
    if (mode == "shutdown") {
        trace_rows.push_back("shutdown-flags:" +
            std::to_string(g_runtime_alive) + ":" +
            std::to_string(g_capture_enabled) + ":" +
            std::to_string(g_faction_variants_enabled) + ":" +
            std::to_string(g_subsystem_visuals_enabled));
        trace_rows.push_back("shutdown-maps:" +
            std::to_string(g_craft_races.size()) + ":" +
            std::to_string(g_race_suffixes.size()) + ":" +
            std::to_string(g_class_meshes.size()) + ":" +
            std::to_string(g_class_damage_visuals.size()));
        for (const auto& row : trace_rows) std::cout << row << '\n';
        return;
    }
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
    const auto policy = g_class_damage_visuals.find(&class_tag);
    trace_rows.push_back("policy:" + std::to_string(policy != g_class_damage_visuals.end()));
    if (policy != g_class_damage_visuals.end()) {
        trace_rows.push_back("threshold:" + std::to_string(policy->second.damage_threshold));
        trace_rows.push_back("scorch:" + std::to_string(policy->second.scorch_effects.size()));
        for (std::size_t index = 0; index < kSubsystemCount; ++index) {
            trace_rows.push_back("system:" + std::to_string(index) + ":" +
                std::to_string(policy->second.systems[index].size()) + ":" +
                std::to_string(policy->second.target_hardpoints[index].size()));
            for (const auto& entry : policy->second.systems[index])
                trace_rows.push_back("entry:" + entry.node_name + ":" +
                                     std::to_string(entry.explosion_project_id));
        }
    }
    const auto visual = g_craft_damage_visuals.find(&instance_tag);
    trace_rows.push_back("visual:" + std::to_string(visual != g_craft_damage_visuals.end()));
    if (visual != g_craft_damage_visuals.end()) {
        for (std::size_t index = 0; index < kSubsystemCount; ++index) {
            const auto& state = visual->second.systems[index];
            trace_rows.push_back("state:" + std::to_string(index) + ":" +
                std::to_string(state.initialized) + ":" +
                std::to_string(static_cast<unsigned>(state.previous_condition)) + ":" +
                std::to_string(state.previous_hitpoints) + ":" +
                std::to_string(state.destruction_count) + ":" +
                std::to_string(state.repair_particle_sequence) + ":" +
                std::to_string(state.selected_entry) + ":" +
                std::to_string(state.rebuild_scale));
        }
    }
    for (const auto& row : trace_rows) std::cout << row << '\n';
}
} // namespace

SIZE_T WINAPI fixture_virtual_query(
    LPCVOID address, PMEMORY_BASIC_INFORMATION information, SIZE_T size) {
    if (fixture_partial_armed && fixture_partial_nodes[0] && !fixture_partial_injected) {
        const auto query = reinterpret_cast<std::uintptr_t>(address);
        for (int index = 0; index != 2; ++index) {
            auto* current = static_cast<std::uint8_t*>(fixture_partial_nodes[index]);
            const auto base = reinterpret_cast<std::uintptr_t>(current);
            if (query < base || query >= base + (index == 0 ? node.size() : walk_child.size())) continue;
            auto* previous = static_cast<std::uint8_t*>(fixture_partial_nodes[1 - index]);
            std::uint32_t flags = 0;
            float scale = 0.0f;
            std::memcpy(&flags, previous + kNodeFlagsOffset, sizeof(flags));
            std::memcpy(&scale, previous + kNodeScaleOffset, sizeof(scale));
            if (flags != 0 && scale == 0.0f) {
                fixture_partial_first = previous;
                fixture_partial_injected = true;
                trace_rows.push_back("partial-before-flags:" + std::to_string(flags));
                trace_rows.push_back("partial-before-scale:" + std::to_string(scale));
                return 0; // native query failure; checked readers are noexcept
            }
        }
    }
    if (address == fixture_resolve_mutate_query) {
        fixture_resolve_mutate_query = nullptr;
        (*fixture_append_entries)[1].node_name = "fed";
        trace_rows.push_back("mutate:resolve-second=fed");
    }
    if (address == fixture_fail_on_query) {
        fixture_fail_on_query = nullptr;
        fixture_fail_next_allocation = true;
    }
    if (fixture_mutate_on_read && !fixture_probe_mutated &&
        address == fixture_probe_node) {
        fixture_probe_mutated = true;
        put(node.data(), kNodeFlagsOffset, std::uint32_t{0});
        trace_rows.push_back("mutate:flags=0");
    }
    return ::VirtualQuery(address, information, size);
}

extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_0(
    void*, void* self) {
    if (mode == "walk-mesh") {
        trace_rows.push_back("type:" + texture_label(self));
        return 1;
    }
    return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_1(
    void*, void* self, std::uintptr_t) {
    if (mode == "walk-mesh") {
        trace_rows.push_back("texture:" + texture_label(self));
        return reinterpret_cast<std::uintptr_t>(texture.data());
    }
    return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_2(
    void*, void* self, std::uintptr_t first, std::uintptr_t) {
    trace_rows.push_back("set:" + texture_label(self) + ":" +
                         texture_label(reinterpret_cast<void*>(first)));
    return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_3(
    void*, void*, std::uintptr_t command_arg, std::uintptr_t output_arg,
    std::uintptr_t) {
    const std::string command(reinterpret_cast<const char*>(command_arg));
    trace_rows.push_back("project:" + command);
    *reinterpret_cast<std::uint32_t*>(output_arg) = 77;
    return 1;
}
extern "C" std::uintptr_t __cdecl a2fo_texture_variants_call_thiscall_4(
    void*, void*, std::uintptr_t command_arg, std::uintptr_t buffer_arg,
    std::uintptr_t capacity, std::uintptr_t) {
    const std::string command(reinterpret_cast<const char*>(command_arg));
    trace_rows.push_back("read:" + command);
    if (mode == "odf-mutate" && command == "sensorMesh1" && !mutated) {
        odf_values["engineMesh1"] = "engine_mutated";
        mutated = true;
        trace_rows.push_back("mutate:engineMesh1");
    }
    const auto row = odf_values.find(command);
    if (row == odf_values.end()) return 0;
    std::strncpy(reinterpret_cast<char*>(buffer_arg), row->second.c_str(), capacity - 1);
    return 1;
}
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
    if (mode == "shutdown") {
        g_class_damage_visuals[&class_tag] = {};
        A2FO_ModuleShutdown();
    } else if (mode == "init-invalid") {
        A2FO_ModuleApi invalid{};
        trace_rows.push_back("init-invalid:" +
                             std::to_string(A2FO_ModuleInit(&invalid)));
    } else if (mode == "walk-faction" || mode == "walk-mesh" ||
        mode == "walk-subtree" || mode == "walk-resolve" ||
        mode == "walk-resolve-mutate" ||
        mode == "walk-admission" || mode == "walk-partial") {
        put(walk_class.data(), kCraftClassGeometryOffset,
            static_cast<void*>(walk_geometry.data()));
        put(walk_geometry.data(), kGeometryRootNodeOffset,
            static_cast<void*>(walk_root.data()));
        put(walk_root.data(), kNodeNameOffset, "fed");
        put(walk_child.data(), kNodeNameOffset, "borg");
        put(walk_sibling.data(), kNodeNameOffset, "fed");
        put(walk_outside.data(), kNodeNameOffset, "outside");
        put(walk_root.data(), kNodeChildOffset,
            static_cast<void*>(walk_child.data()));
        put(walk_child.data(), kNodeSiblingOffset,
            static_cast<void*>(walk_sibling.data()));
        put(walk_root.data(), kNodeSiblingOffset,
            static_cast<void*>(walk_outside.data()));
        if (mode == "walk-admission") {
            put(walk_root.data(), kNodeChildOffset,
                static_cast<void*>(walk_root.data()));
            put(walk_root.data(), kNodeSiblingOffset,
                static_cast<void*>(nullptr));
        }
        if (mode == "walk-mesh") {
            walk_vtable[1] = static_cast<void*>(&class_tag);
            for (void* item : {static_cast<void*>(walk_root.data()),
                               static_cast<void*>(walk_child.data()),
                               static_cast<void*>(walk_sibling.data()),
                               static_cast<void*>(walk_outside.data())})
                put(item, 0, static_cast<void*>(walk_vtable.data()));
            collect_class_meshes(walk_class.data());
            const auto& rows = g_class_meshes.at(walk_class.data());
            trace_rows.push_back("mesh-count:" + std::to_string(rows.size()));
            for (const auto& row : rows)
                trace_rows.push_back("mesh-node:" + texture_label(row.mesh));
        } else if (mode == "walk-subtree") {
            const auto rows = collect_node_subtree(walk_root.data());
            trace_rows.push_back("subtree-count:" + std::to_string(rows.size()));
            for (void* row : rows)
                trace_rows.push_back("subtree-node:" + texture_label(row));
        } else if (mode == "walk-resolve" || mode == "walk-resolve-mutate") {
            auto& policy = g_class_damage_visuals[walk_class.data()];
            policy.systems[0].push_back({"fed", nullptr, {}, 0, 1.0f});
            if (mode == "walk-resolve-mutate") {
                policy.systems[0].push_back({"pending", nullptr, {}, 0, 1.0f});
                fixture_append_entries = &policy.systems[0];
                fixture_resolve_mutate_query = walk_root.data() + kNodeScaleOffset;
            }
            policy.systems[1].push_back({"missing", nullptr, {}, 0, 1.0f});
            resolve_class_damage_nodes(walk_class.data());
            trace_rows.push_back("resolved:" + std::to_string(policy.nodes_resolved));
            trace_rows.push_back("resolved-node:" +
                texture_label(policy.systems[0][0].node));
            trace_rows.push_back("resolved-subtree:" +
                std::to_string(policy.systems[0][0].subtree_nodes.size()));
            if (mode == "walk-resolve-mutate")
                trace_rows.push_back("resolved-second:" +
                    texture_label(policy.systems[0][1].node));
        } else {
            if (mode == "walk-partial")
                fixture_fail_on_query = walk_root.data() + kNodeSiblingOffset;
            try {
                collect_class_faction_nodes(walk_class.data());
                trace_rows.push_back("walk-exception:none");
            } catch (const FixtureAllocationFailure&) {
                trace_rows.push_back("walk-exception:typed");
            }
            const auto found = g_class_faction_nodes.find(walk_class.data());
            trace_rows.push_back("faction-cache:" +
                std::to_string(found != g_class_faction_nodes.end()));
            if (found != g_class_faction_nodes.end()) {
                for (const auto& row : found->second)
                    trace_rows.push_back("faction-node:" + texture_label(row.node));
            }
            if (mode == "walk-partial") {
                trace_rows.push_back("walk-injection:" +
                    std::string(fixture_fail_next_allocation ? "unused" : "used"));
                trace_rows.push_back("walk-destructors:" +
                    std::to_string(fixture_exception_destructors));
            }
        }
    } else if (mode == "odf-mesh" || mode == "odf-scorch" ||
        mode == "odf-invalid" || mode == "odf-empty" ||
        mode == "odf-mutate" || mode == "odf-exception") {
        if (mode != "odf-empty" && mode != "odf-exception") {
            odf_values["sensorMesh1"] = mode == "odf-invalid" ? "bad!" : "sensor_main";
            odf_values["engineMesh1"] = "engine_main";
        }
        if (mode == "odf-scorch") {
            odf_values["damageThreshold"] = "0.25";
            odf_values["sensorTargetHardpoints"] = "main, aux\tbad!";
            odf_values["scorchTexture1"] = "burn";
        }
        if (mode == "odf-exception") fixture_fail_next_allocation = true;
        register_class_damage_visuals(&class_tag, &parameter_tag);
        if (mode == "odf-exception")
            trace_rows.push_back(std::string("injection:") +
                                 (fixture_fail_next_allocation ? "unused" : "used"));
        if (mode == "odf-exception")
            trace_rows.push_back("exception-destructors:" +
                                 std::to_string(fixture_exception_destructors));
    } else if (mode == "select-empty" || mode == "select-choice" ||
               mode == "select-scan-mutate" || mode == "selected-valid" ||
               mode == "selected-stale" || mode == "select-admission") {
        auto& policy = g_class_damage_visuals[&class_tag];
        auto& entries = policy.systems[0];
        entries.reserve(3);
        entries.push_back({"first", mode == "select-empty" ? nullptr : node.data(), {}, 0, 1.0f});
        if (mode == "select-choice")
            entries.push_back({"second", walk_child.data(), {}, 0, 1.0f});
        SubsystemDamageVisualState state;
        state.selected_entry = mode == "selected-stale" || mode == "select-empty" ? 4 : 0;
        state.destruction_count = 2;
        if (mode == "select-scan-mutate") {
            fixture_append_entries = &entries;
            fixture_on_allocation = [] {
                fixture_append_entries->push_back(
                    {"second", walk_child.data(), {}, 0, 1.0f});
            };
        }
        SubsystemMeshEntry* chosen = nullptr;
        if (mode == "selected-valid" || mode == "selected-stale")
            chosen = selected_damage_mesh(&policy, 0, state);
        else chosen = select_damage_mesh(
            mode == "select-admission" ? nullptr : &policy, 0, 9, &state);
        trace_rows.push_back("choice-index:" + std::to_string(state.selected_entry));
        trace_rows.push_back("choice-count:" + std::to_string(state.destruction_count));
        trace_rows.push_back("choice-entries:" + std::to_string(entries.size()));
        trace_rows.push_back("choice-node:" +
            texture_label(chosen ? chosen->node : nullptr));
    } else if (mode == "race-field-exception") {
        constexpr char long_value[] =
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        A2FO_OdfFieldView field{{"name", 4},
                                {long_value, sizeof(long_value) - 1}};
        std::string value;
        fixture_fail_next_allocation = true;
        try {
            race_event_field(&field, 1, "name", &value);
            trace_rows.push_back("field-exception:none");
        } catch (const FixtureAllocationFailure&) {
            trace_rows.push_back("field-exception:typed");
        }
        trace_rows.push_back("field-destructors:" +
                             std::to_string(fixture_exception_destructors));
        trace_rows.push_back("field-injection:" +
                             std::string(fixture_fail_next_allocation ? "unused" : "used"));
    } else if (mode == "cleanup-multi" || mode == "cleanup-reuse") {
        g_craft_races.clear();
        g_craft_races[&instance_tag] = CraftRaceState{&class_tag, &race_tag};
        g_craft_races[&class_tag] = CraftRaceState{&class_tag, &race_tag};
        g_craft_races[&race_tag] = CraftRaceState{&parameter_tag, &race_tag};
        g_craft_damage_visuals[&instance_tag].object = &class_tag;
        g_craft_damage_visuals[&class_tag].object = &class_tag;
        g_craft_damage_visuals[&race_tag].object = &parameter_tag;
        A2FO_CraftEvent event{sizeof(A2FO_CraftEvent),
            A2FO_CRAFT_EVENT_CLEANUP, &class_tag, 0.0f};
        craft_cleanup_event_handler(&event, nullptr);
        trace_rows.push_back("cleanup-count:" +
            std::to_string(g_craft_races.size()) + ":" +
            std::to_string(g_craft_damage_visuals.size()));
        if (mode == "cleanup-reuse") {
            g_craft_races[&instance_tag] = CraftRaceState{&parameter_tag, &race_tag};
            g_craft_damage_visuals[&instance_tag].object = &parameter_tag;
            craft_cleanup_event_handler(&event, nullptr);
            trace_rows.push_back("reuse-count:" +
                std::to_string(g_craft_races.size()) + ":" +
                std::to_string(g_craft_damage_visuals.size()));
        }
    } else if (mode == "update-init" || mode == "update-destroy" ||
               mode == "update-repair" || mode == "update-operational" ||
               mode == "update-no-policy" || mode == "update-no-systems" ||
               mode == "update-reset") {
        put(craft.data(), kObjectClassOffset, static_cast<void*>(&class_tag));
        put(craft.data(), kCraftSystemsOffset, static_cast<void*>(systems.data()));
        put(craft.data(), kObjectHandleOffset, std::uint32_t{9});
        auto& policy = g_class_damage_visuals[&class_tag];
        policy.nodes_resolved = true;
        policy.systems[0].push_back({"sensor_main", node.data(), {}, 0, 1.0f});
        for (std::size_t index = 0; index < kSubsystemCount; ++index) {
            auto* system = systems.data() + index * kCraftSystemSize;
            put(system, kCraftSystemOperationalOffset, std::uint8_t{1});
            put(system, kCraftSystemMaximumHitpointsOffset, std::int32_t{100});
            put(system, kCraftSystemCurrentHitpointsOffset, double{100});
        }
        if (mode == "update-no-policy") g_class_damage_visuals.erase(&class_tag);
        if (mode == "update-no-systems")
            put(craft.data(), kCraftSystemsOffset, static_cast<void*>(nullptr));
        if (mode == "update-reset") {
            auto& state = g_craft_damage_visuals[&instance_tag];
            state.object = &race_tag;
            state.handle = 77;
            state.systems[0].initialized = true;
            state.systems[0].destruction_count = 4;
        }
        if (mode == "update-destroy" || mode == "update-repair" ||
            mode == "update-operational") {
            update_subsystem_damage_visuals(&instance_tag, craft.data());
            put(systems.data(), kCraftSystemCurrentHitpointsOffset, double{-5});
            put(systems.data(), kCraftSystemOperationalOffset, std::uint8_t{0});
            update_subsystem_damage_visuals(&instance_tag, craft.data());
            if (mode == "update-repair" || mode == "update-operational") {
                put(systems.data(), kCraftSystemCurrentHitpointsOffset, double{45});
                update_subsystem_damage_visuals(&instance_tag, craft.data());
            }
            if (mode == "update-operational") {
                put(systems.data(), kCraftSystemCurrentHitpointsOffset, double{100});
                put(systems.data(), kCraftSystemOperationalOffset, std::uint8_t{1});
                update_subsystem_damage_visuals(&instance_tag, craft.data());
            }
        } else update_subsystem_damage_visuals(&instance_tag, craft.data());
    } else if (mode == "render-hidden" || mode == "render-half" ||
               mode == "render-mutate" || mode == "render-none" ||
               mode == "render-grow" || mode == "render-plan-partial") {
        auto& policy = g_class_damage_visuals[&class_tag];
        policy.nodes_resolved = true;
        policy.systems[0].push_back({"sensor_main", node.data(), {node.data()}, 0, 2.0f});
        if (mode == "render-grow" || mode == "render-plan-partial") {
            auto& subtree = policy.systems[0][0].subtree_nodes;
            subtree.reserve(3);
            subtree.push_back(walk_child.data());
        }
        auto& visual = g_craft_damage_visuals[&instance_tag];
        visual.object_class = &class_tag;
        visual.systems[0].selected_entry = mode == "render-none" ? -1 : 0;
        visual.systems[0].rebuild_scale = mode == "render-hidden" ? 0.0f : 0.5f;
        put(node.data(), kNodeFlagsOffset, std::uint32_t{3});
        put(node.data(), kNodeScaleOffset, 2.0f);
        if (mode == "render-mutate") fixture_mutate_on_read = true;
        if (mode == "render-grow") {
            fixture_append_subtree = &policy.systems[0][0].subtree_nodes;
            fixture_on_allocation = [] {
                fixture_append_subtree->push_back(walk_sibling.data());
            };
        }
        if (mode == "render-plan-partial")
        {
            policy.systems[1].push_back({"sensor_child", walk_child.data(),
                                         {walk_child.data()}, 0, 2.0f});
            visual.systems[0].rebuild_scale = 0.0f;
            visual.systems[1].selected_entry = 0;
            visual.systems[1].rebuild_scale = 0.0f;
            put(node.data(), kNodeFlagsOffset, std::uint32_t{0});
            put(walk_child.data(), kNodeFlagsOffset, std::uint32_t{0});
            put(walk_child.data(), kNodeScaleOffset, 2.0f);
            fixture_partial_armed = true;
            fixture_partial_nodes[0] = node.data();
            fixture_partial_nodes[1] = walk_child.data();
        }
        DamageRenderOverride render_override;
        begin_subsystem_mesh_render_override(&instance_tag, &render_override);
        fixture_partial_armed = false;
        if (mode == "render-plan-partial")
            trace_rows.push_back(std::string("render-injection:") +
                                 (fixture_partial_injected ? "used" : "unused"));
        trace_rows.push_back("render-count:" + std::to_string(render_override.nodes.size()));
        trace_rows.push_back("render-flags:" + std::to_string(
            read_at<std::uint32_t>(node.data(), kNodeFlagsOffset, 0)));
        trace_rows.push_back("render-scale:" + std::to_string(
            read_at<float>(node.data(), kNodeScaleOffset, 0.0f)));
        restore_subsystem_mesh_render_override(&render_override);
        if (mode == "render-plan-partial") {
            trace_rows.push_back("partial-after-flags:" + std::to_string(
                read_at<std::uint32_t>(fixture_partial_first, kNodeFlagsOffset, 0)));
            trace_rows.push_back("partial-after-scale:" + std::to_string(
                read_at<float>(fixture_partial_first, kNodeScaleOffset, 0.0f)));
            fixture_partial_nodes[0] = fixture_partial_nodes[1] = nullptr;
        }
        trace_rows.push_back("restored-flags:" + std::to_string(
            read_at<std::uint32_t>(node.data(), kNodeFlagsOffset, 0)));
        trace_rows.push_back("restored-scale:" + std::to_string(
            read_at<float>(node.data(), kNodeScaleOffset, 0.0f)));
    } else if (mode == "race-valid") {
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
