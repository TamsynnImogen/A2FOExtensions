#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace a1compat {

struct LegacyRaceMenuInput {
    bool has_instant_action_slot = false;
    int instant_action_slot = -1;
    bool has_interface_configuration = false;
    bool has_display_key = false;
    bool has_display_name = false;
    std::string display_name;
};

struct LegacyRaceMenuEntry {
    int instant_action_slot = -1;
    bool replace_display_key = false;
    std::string display_key;
};

struct LegacyRaceMenuPlan {
    bool fallback_active = false;
    int playable_count = 0;
    std::vector<LegacyRaceMenuEntry> entries;
};

// Builds the A1-only fallback view for the Race records already loaded from
// race0, race1, ... in declaration order. A native FO list is valid only when
// every referenced record has a distinct in-range instantActionSlot. Until a
// legacy/invalid record is observed, the returned plan requests no mutation.
LegacyRaceMenuPlan build_legacy_race_menu_plan(
    const std::vector<LegacyRaceMenuInput>& races,
    std::size_t declared_race_count);

}  // namespace a1compat
