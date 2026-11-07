#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace a2fo::energy_systems {

struct ResupplyTargetFilter {
    bool valid = true;
    bool default_allow = true;
    std::unordered_map<std::string, bool> types;
    std::unordered_map<std::string, bool> odfs;
};

// Normalize once when caching target identity, outside the simulation tick.
std::string normalize_resupply_type(std::string_view raw);
std::string normalize_resupply_odf(std::string_view raw);

// A call represents an explicit scalar field. A default-constructed filter
// represents an omitted field and allows other Craft by default.
ResupplyTargetFilter parse_resupply_targets(std::string_view raw) noexcept;

// Inputs are already normalized and cached; this function does not allocate.
bool resupply_target_allowed(
    const ResupplyTargetFilter& filter,
    const std::string& normalized_type,
    const std::string& normalized_odf,
    bool same_object,
    bool can_resupply_self) noexcept;

}  // namespace a2fo::energy_systems
