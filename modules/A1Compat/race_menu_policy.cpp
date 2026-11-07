#include "race_menu_policy.hpp"
#include "../../rust/config_ffi.h"
#include <vector>
namespace a1compat {
LegacyRaceMenuPlan build_legacy_race_menu_plan(const std::vector<LegacyRaceMenuInput> &races,
                                               std::size_t declared_race_count) {
    std::vector<A2foRsRaceInput> inputs;
    inputs.reserve(races.size());
    for (const auto &race : races)
        inputs.push_back({race.instant_action_slot,
                          static_cast<uint8_t>(race.has_instant_action_slot),
                          static_cast<uint8_t>(race.has_interface_configuration),
                          static_cast<uint8_t>(race.has_display_key),
                          static_cast<uint8_t>(race.has_display_name)});
    std::vector<A2foRsRaceEntry> entries(races.size());
    uint8_t fallback = 0;
    int32_t playable = 0;
    a2fo_rs_config_race(inputs.data(), inputs.size(), declared_race_count, entries.data(),
                        &fallback, &playable);
    LegacyRaceMenuPlan plan;
    plan.entries.resize(races.size());
    plan.fallback_active = fallback != 0;
    plan.playable_count = playable;
    for (std::size_t i = 0; i < races.size(); ++i) {
        plan.entries[i].instant_action_slot = entries[i].slot;
        plan.entries[i].replace_display_key = entries[i].replace_key != 0;
        if (entries[i].replace_key)
            plan.entries[i].display_key = races[i].display_name;
    }
    return plan;
}
} // namespace a1compat
