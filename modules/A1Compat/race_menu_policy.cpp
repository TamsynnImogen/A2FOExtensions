#include "race_menu_policy.hpp"

#include <vector>

namespace a1compat {

LegacyRaceMenuPlan build_legacy_race_menu_plan(
    const std::vector<LegacyRaceMenuInput>& races,
    std::size_t declared_race_count) {
    LegacyRaceMenuPlan plan;
    plan.entries.resize(races.size());

    std::vector<bool> occupied_slots(declared_race_count, false);
    for (const LegacyRaceMenuInput& race : races) {
        if (!race.has_instant_action_slot || race.instant_action_slot < 0 ||
            static_cast<std::size_t>(race.instant_action_slot) >=
                declared_race_count ||
            occupied_slots[static_cast<std::size_t>(
                race.instant_action_slot)]) {
            plan.fallback_active = true;
            break;
        }
        occupied_slots[static_cast<std::size_t>(
            race.instant_action_slot)] = true;
    }

    if (!plan.fallback_active) return plan;

    int next_slot = 0;
    for (std::size_t index = 0; index < races.size(); ++index) {
        const LegacyRaceMenuInput& race = races[index];
        LegacyRaceMenuEntry& entry = plan.entries[index];
        if (!race.has_interface_configuration) {
            entry.instant_action_slot = -1;
            continue;
        }

        entry.instant_action_slot = next_slot++;
        if (!race.has_display_key && race.has_display_name) {
            entry.replace_display_key = true;
            entry.display_key = race.display_name;
        }
    }
    plan.playable_count = next_slot;
    return plan;
}

}  // namespace a1compat
