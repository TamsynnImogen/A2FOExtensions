#include "../modules/A2FOTextureVariants/texture_variants.hpp"

#include <cassert>
#include <cstdint>
#include <limits>
#include <string>

int main() {
    using a2fo::texture_variants::faction_texture_name;
    using a2fo::texture_variants::faction_node_flags;
    using a2fo::texture_variants::normalize_faction_node_name;
    using a2fo::texture_variants::normalize_faction_suffix;
    using a2fo::texture_variants::subsystem_condition;
    using a2fo::texture_variants::subsystem_damage_policy_active;
    using a2fo::texture_variants::subsystem_mesh_choice;
    using a2fo::texture_variants::subsystem_rebuild_scale;
    using a2fo::texture_variants::subsystem_repair_sample;
    using a2fo::texture_variants::SubsystemCondition;
    using a2fo::texture_variants::texture_asset_path;

    std::string suffix;
    assert(normalize_faction_suffix("  _k  ", &suffix));
    assert(suffix == "_k");
    assert(normalize_faction_suffix("ROM-2", &suffix));
    assert(suffix == "ROM-2");
    assert(normalize_faction_suffix("", &suffix));
    assert(suffix.empty());
    assert(!normalize_faction_suffix("../k", &suffix));
    assert(!normalize_faction_suffix("_k.dds", &suffix));
    assert(!normalize_faction_suffix(
        "_this_suffix_is_longer_than_thirty_two_characters", &suffix));

    std::string node_name;
    assert(normalize_faction_node_name("  KlingonEmpire  ", &node_name));
    assert(node_name == "klingonempire");
    assert(normalize_faction_node_name("species8472", &node_name));
    assert(node_name == "species8472");
    assert(normalize_faction_node_name("npc_breen-2", &node_name));
    assert(node_name == "npc_breen-2");
    assert(!normalize_faction_node_name("", &node_name));
    assert(!normalize_faction_node_name("Species 8472", &node_name));
    assert(!normalize_faction_node_name("../klingon", &node_name));

    assert(faction_node_flags(0x00000103u, true) == 0x00000100u);
    assert(faction_node_flags(0x00000100u, false) == 0x00000103u);
    assert(faction_node_flags(0xa5a5a5a5u, true) == 0xa5a5a5a4u);
    assert(faction_node_flags(0xa5a5a5a4u, false) == 0xa5a5a5a7u);

    assert(faction_texture_name("fed_hull", "_k") == "fed_hull_k");
    assert(faction_texture_name("fed_hull.tga", "_k") == "fed_hull_k");
    assert(faction_texture_name("fed_hull.DDS", "_k") == "fed_hull_k");
    assert(faction_texture_name("fed_hull_K", "_k") == "fed_hull_K");
    assert(faction_texture_name("Textures/fed_hull.dds", "_k") ==
           "Textures\\fed_hull_k");

    assert(texture_asset_path("fed_hull_k", ".dds") ==
           "Textures\\fed_hull_k.dds");
    assert(texture_asset_path("Textures/fed_hull_k", ".tga") ==
           "Textures\\fed_hull_k.tga");
    assert(texture_asset_path("textures\\fed_hull_k.DDS", ".dds") ==
           "textures\\fed_hull_k.DDS");

    assert(subsystem_condition(true, false, 100, 100.0, 0.0f) ==
           SubsystemCondition::operational);
    assert(subsystem_condition(false, true, 100, 50.0, 0.0f) ==
           SubsystemCondition::disabled);
    assert(subsystem_condition(false, false, 100, 50.0, 3.0f) ==
           SubsystemCondition::disabled);
    assert(subsystem_condition(false, false, 100, 50.0, 0.0f) ==
           SubsystemCondition::destroyed);
    assert(subsystem_condition(false, true, 100, 0.0, 9.0f) ==
           SubsystemCondition::destroyed);
    assert(subsystem_condition(false, false, 0, 0.0, 0.0f) ==
           SubsystemCondition::disabled);

    const std::size_t first = subsystem_mesh_choice(42, 1, 0, 7);
    assert(first < 7);
    assert(subsystem_mesh_choice(42, 1, 0, 7) == first);
    assert(subsystem_mesh_choice(42, 1, 0, 0) == 0);
    bool saw_another_choice = false;
    for (std::uint32_t generation = 1; generation < 16; ++generation) {
        const std::size_t choice =
            subsystem_mesh_choice(42, 1, generation, 7);
        assert(choice < 7);
        saw_another_choice = saw_another_choice || choice != first;
    }
    assert(saw_another_choice);

    assert(subsystem_rebuild_scale(100, 0.0) == 0.0f);
    assert(subsystem_rebuild_scale(0, 20.0) == 0.0f);
    const float quarter_scale = subsystem_rebuild_scale(100, 25.0);
    const float half_scale = subsystem_rebuild_scale(100, 50.0);
    const float nearly_complete_scale =
        subsystem_rebuild_scale(100, 90.0);
    assert(quarter_scale > 0.08f && quarter_scale < half_scale);
    assert(half_scale < nearly_complete_scale);
    assert(subsystem_rebuild_scale(100, 100.0) == 1.0f);
    assert(subsystem_rebuild_scale(100, 150.0) == 1.0f);

    const float repair_sample = subsystem_repair_sample(42, 1, 3, 0);
    assert(repair_sample >= 0.0f && repair_sample <= 1.0f);
    assert(subsystem_repair_sample(42, 1, 3, 0) == repair_sample);
    bool saw_different_sample = false;
    for (std::size_t component = 1; component < 5; ++component) {
        const float sample = subsystem_repair_sample(42, 1, 3, component);
        assert(sample >= 0.0f && sample <= 1.0f);
        saw_different_sample = saw_different_sample ||
            sample != repair_sample;
    }
    assert(saw_different_sample);

    assert(!subsystem_damage_policy_active(0, 0.0f, 0, 0));
    assert(!subsystem_damage_policy_active(0, 0.1f, 0, 4));
    assert(!subsystem_damage_policy_active(0, 0.1f, 2, 0));
    assert(!subsystem_damage_policy_active(0, 0.0f, 2, 4));
    assert(!subsystem_damage_policy_active(
        0, std::numeric_limits<float>::quiet_NaN(), 2, 4));
    assert(subsystem_damage_policy_active(0, 0.1f, 2, 4));
    assert(subsystem_damage_policy_active(1, 0.0f, 0, 0));
}
