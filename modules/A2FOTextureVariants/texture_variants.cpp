#include "texture_variants.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <utility>

namespace a2fo::texture_variants {
namespace {

char lower_ascii(char value) noexcept {
    return static_cast<char>(
        std::tolower(static_cast<unsigned char>(value)));
}

std::uint32_t mixed_subsystem_value(std::uint32_t craft_handle,
                                    std::size_t subsystem_index,
                                    std::uint32_t sequence,
                                    std::uint32_t salt) noexcept {
    std::uint32_t value = craft_handle ^ 0x9e3779b9u;
    value ^= static_cast<std::uint32_t>(subsystem_index + 1u) *
        0x85ebca6bu;
    value ^= (sequence + 1u) * 0xc2b2ae35u;
    value ^= salt * 0x27d4eb2du;
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

std::string trim(std::string_view input) {
    std::size_t first = 0;
    while (first < input.size() && std::isspace(
               static_cast<unsigned char>(input[first]))) {
        ++first;
    }
    std::size_t last = input.size();
    while (last > first && std::isspace(
               static_cast<unsigned char>(input[last - 1]))) {
        --last;
    }
    return std::string(input.substr(first, last - first));
}

bool ends_with_case_insensitive(std::string_view value,
                                std::string_view ending) noexcept {
    if (ending.size() > value.size()) return false;
    const std::size_t offset = value.size() - ending.size();
    for (std::size_t index = 0; index < ending.size(); ++index) {
        if (lower_ascii(value[offset + index]) !=
            lower_ascii(ending[index])) {
            return false;
        }
    }
    return true;
}

bool starts_with_textures(std::string_view value) noexcept {
    constexpr std::string_view prefix = "textures\\";
    if (value.size() < prefix.size()) return false;
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (lower_ascii(value[index]) != prefix[index]) return false;
    }
    return true;
}

std::string normalized_path(std::string_view value) {
    std::string result(value);
    std::replace(result.begin(), result.end(), '/', '\\');
    return result;
}

}  // namespace

bool normalize_faction_suffix(std::string_view input,
                              std::string* normalized) noexcept {
    if (!normalized) return false;
    try {
        std::string candidate = trim(input);
        if (candidate.size() > kMaximumFactionSuffixLength) return false;
        for (char character : candidate) {
            const unsigned char value =
                static_cast<unsigned char>(character);
            if (!std::isalnum(value) && character != '_' &&
                character != '-') {
                return false;
            }
        }
        *normalized = std::move(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

bool normalize_faction_node_name(std::string_view input,
                                 std::string* normalized) noexcept {
    if (!normalized) return false;
    try {
        std::string candidate = trim(input);
        if (candidate.empty() ||
            candidate.size() > kMaximumFactionNodeNameLength) {
            return false;
        }
        for (char& character : candidate) {
            const unsigned char value =
                static_cast<unsigned char>(character);
            if (!std::isalnum(value) && character != '_' &&
                character != '-') {
                return false;
            }
            character = lower_ascii(character);
        }
        *normalized = std::move(candidate);
        return true;
    } catch (...) {
        return false;
    }
}

std::uint32_t faction_node_flags(std::uint32_t current,
                                 bool selected) noexcept {
    constexpr std::uint32_t hidden_and_disabled = 0x00000003u;
    return selected
        ? current & ~hidden_and_disabled
        : current | hidden_and_disabled;
}

std::string faction_texture_name(std::string_view diffuse_name,
                                 std::string_view suffix) {
    std::string result = normalized_path(trim(diffuse_name));
    if (ends_with_case_insensitive(result, ".tga") ||
        ends_with_case_insensitive(result, ".dds")) {
        result.resize(result.size() - 4);
    }
    if (!suffix.empty() && !ends_with_case_insensitive(result, suffix)) {
        result.append(suffix.data(), suffix.size());
    }
    return result;
}

std::string texture_asset_path(std::string_view texture_name,
                               std::string_view extension) {
    std::string result = normalized_path(trim(texture_name));
    if (!starts_with_textures(result)) result.insert(0, "Textures\\");
    if (!extension.empty() &&
        !ends_with_case_insensitive(result, extension)) {
        result.append(extension.data(), extension.size());
    }
    return result;
}

SubsystemCondition subsystem_condition(bool operational,
                                       bool forced_disabled,
                                       std::int32_t maximum_hitpoints,
                                       double current_hitpoints,
                                       float disable_time) noexcept {
    if (maximum_hitpoints <= 0 || !std::isfinite(current_hitpoints) ||
        !std::isfinite(disable_time)) {
        return operational ? SubsystemCondition::operational
                           : SubsystemCondition::disabled;
    }
    if (operational) return SubsystemCondition::operational;

    constexpr double epsilon = 0.0001;
    const bool destroyed = current_hitpoints <= 0.0 ||
        (!forced_disabled && disable_time <= 0.0f &&
         current_hitpoints + epsilon <
             static_cast<double>(maximum_hitpoints));
    return destroyed ? SubsystemCondition::destroyed
                     : SubsystemCondition::disabled;
}

std::size_t subsystem_mesh_choice(std::uint32_t craft_handle,
                                  std::size_t subsystem_index,
                                  std::uint32_t destruction_count,
                                  std::size_t choice_count) noexcept {
    if (choice_count == 0) return 0;
    return static_cast<std::size_t>(mixed_subsystem_value(
        craft_handle, subsystem_index, destruction_count, 0u)) %
        choice_count;
}

float subsystem_rebuild_scale(std::int32_t maximum_hitpoints,
                              double current_hitpoints) noexcept {
    if (maximum_hitpoints <= 0 || !std::isfinite(current_hitpoints) ||
        current_hitpoints <= 0.0) {
        return 0.0f;
    }
    const double progress = std::clamp(
        current_hitpoints / static_cast<double>(maximum_hitpoints),
        0.0, 1.0);
    const double eased = progress * progress * (3.0 - 2.0 * progress);
    return static_cast<float>(0.08 + eased * 0.92);
}

float subsystem_repair_sample(std::uint32_t craft_handle,
                              std::size_t subsystem_index,
                              std::uint32_t sequence,
                              std::size_t component) noexcept {
    const std::uint32_t value = mixed_subsystem_value(
        craft_handle, subsystem_index, sequence,
        static_cast<std::uint32_t>(component + 1u));
    constexpr double denominator = 16777215.0;
    return static_cast<float>(
        static_cast<double>(value & 0x00ffffffu) / denominator);
}

bool subsystem_damage_policy_active(
    std::size_t mesh_count, float damage_threshold,
    std::size_t scorch_effect_count,
    std::size_t target_hardpoint_count) noexcept {
    if (mesh_count != 0) return true;
    return std::isfinite(damage_threshold) && damage_threshold > 0.0f &&
        scorch_effect_count != 0 && target_hardpoint_count != 0;
}

}  // namespace a2fo::texture_variants
