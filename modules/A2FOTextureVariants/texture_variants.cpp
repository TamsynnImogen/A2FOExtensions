#include "texture_variants.hpp"
#include "../../rust/gameplay_ffi.h"

#include <utility>

namespace a2fo::texture_variants {

bool normalize_faction_suffix(std::string_view input, std::string *normalized) noexcept {
    if (!normalized)
        return false;
    try {
        std::string candidate(input.size(), '\0');
        std::size_t length = 0;
        if (!a2fo_rs_gameplay_faction_suffix(
                reinterpret_cast<const std::uint8_t *>(input.data()), input.size(),
                reinterpret_cast<std::uint8_t *>(candidate.data()), &length))
            return false;
        candidate.resize(length);
        *normalized = std::move(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool normalize_faction_node_name(std::string_view input, std::string *normalized) noexcept {
    if (!normalized)
        return false;
    try {
        std::string candidate(input.size(), '\0');
        std::size_t length = 0;
        if (!a2fo_rs_gameplay_node_name(
                reinterpret_cast<const std::uint8_t *>(input.data()), input.size(),
                reinterpret_cast<std::uint8_t *>(candidate.data()), &length))
            return false;
        candidate.resize(length);
        *normalized = std::move(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

std::uint32_t faction_node_flags(std::uint32_t current, bool selected) noexcept {
    return a2fo_rs_gameplay_node_flags(current, selected);
}

std::string faction_texture_name(std::string_view diffuse_name, std::string_view suffix) {
    std::string result(diffuse_name.size() + suffix.size(), '\0');
    const auto length = a2fo_rs_gameplay_texture_name(
        reinterpret_cast<const std::uint8_t *>(diffuse_name.data()), diffuse_name.size(),
        reinterpret_cast<const std::uint8_t *>(suffix.data()), suffix.size(),
        reinterpret_cast<std::uint8_t *>(result.data()));
    result.resize(length);
    return result;
}

std::string texture_asset_path(std::string_view texture_name, std::string_view extension) {
    std::string result(texture_name.size() + extension.size() + 9, '\0');
    const auto length = a2fo_rs_gameplay_texture_path(
        reinterpret_cast<const std::uint8_t *>(texture_name.data()), texture_name.size(),
        reinterpret_cast<const std::uint8_t *>(extension.data()), extension.size(),
        reinterpret_cast<std::uint8_t *>(result.data()));
    result.resize(length);
    return result;
}

SubsystemCondition subsystem_condition(bool operational, bool forced_disabled,
                                       std::int32_t maximum_hitpoints, double current_hitpoints,
                                       float disable_time) noexcept {
    return static_cast<SubsystemCondition>(a2fo_rs_gameplay_subsystem_condition(
        operational, forced_disabled, maximum_hitpoints, current_hitpoints, disable_time));
}

std::size_t subsystem_mesh_choice(std::uint32_t craft_handle, std::size_t subsystem_index,
                                  std::uint32_t destruction_count,
                                  std::size_t choice_count) noexcept {
    return a2fo_rs_gameplay_mesh_choice(craft_handle, subsystem_index, destruction_count,
                                        choice_count);
}

float subsystem_rebuild_scale(std::int32_t maximum_hitpoints, double current_hitpoints) noexcept {
    return a2fo_rs_gameplay_rebuild_scale(maximum_hitpoints, current_hitpoints);
}

float subsystem_repair_sample(std::uint32_t craft_handle, std::size_t subsystem_index,
                              std::uint32_t sequence, std::size_t component) noexcept {
    return a2fo_rs_gameplay_repair_sample(craft_handle, subsystem_index, sequence, component);
}

bool subsystem_damage_policy_active(std::size_t mesh_count, float damage_threshold,
                                    std::size_t scorch_effect_count,
                                    std::size_t target_hardpoint_count) noexcept {
    return a2fo_rs_gameplay_damage_policy_active(mesh_count, damage_threshold, scorch_effect_count,
                                                 target_hardpoint_count) != 0;
}

} // namespace a2fo::texture_variants
