#include "race_menu_policy.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

using a1compat::LegacyRaceMenuInput;
using a1compat::LegacyRaceMenuPlan;
using a1compat::build_legacy_race_menu_plan;

LegacyRaceMenuInput legacy_race(
    const char* display_name, bool playable) {
    LegacyRaceMenuInput race;
    race.has_interface_configuration = playable;
    race.has_display_name = display_name != nullptr;
    if (display_name) race.display_name = display_name;
    return race;
}

void expect_slot(const LegacyRaceMenuPlan& plan,
                 std::size_t index, int slot) {
    assert(index < plan.entries.size());
    assert(plan.entries[index].instant_action_slot == slot);
}

void stock_a1_order_and_labels() {
    const std::vector<LegacyRaceMenuInput> races = {
        legacy_race("Borg", true),
        legacy_race("Dominion", false),
        legacy_race("Federation", true),
        legacy_race("Klingon", true),
        legacy_race("Romulan", true),
        legacy_race("Cardassian", false),
        legacy_race("Ferengi", false),
        legacy_race("Alien", false),
        legacy_race("Son'a", false),
        legacy_race("Breen", false),
    };
    const LegacyRaceMenuPlan plan =
        build_legacy_race_menu_plan(races, races.size());

    assert(plan.fallback_active);
    assert(plan.playable_count == 4);
    expect_slot(plan, 0, 0);
    expect_slot(plan, 1, -1);
    expect_slot(plan, 2, 1);
    expect_slot(plan, 3, 2);
    expect_slot(plan, 4, 3);
    for (std::size_t index = 5; index < races.size(); ++index) {
        expect_slot(plan, index, -1);
    }
    assert(plan.entries[0].replace_display_key);
    assert(plan.entries[0].display_key == "Borg");
    assert(plan.entries[2].replace_display_key);
    assert(plan.entries[2].display_key == "Federation");
    assert(!plan.entries[1].replace_display_key);
}

void valid_fo_list_is_untouched() {
    std::vector<LegacyRaceMenuInput> races(3);
    for (std::size_t index = 0; index < races.size(); ++index) {
        races[index].has_instant_action_slot = true;
        races[index].instant_action_slot = static_cast<int>(index);
        races[index].has_interface_configuration = true;
        races[index].has_display_key = true;
        races[index].has_display_name = true;
        races[index].display_name = "Legacy label must not win";
    }
    const LegacyRaceMenuPlan plan =
        build_legacy_race_menu_plan(races, races.size());
    assert(!plan.fallback_active);
    assert(plan.playable_count == 0);
    for (const auto& entry : plan.entries) {
        assert(!entry.replace_display_key);
    }
}

void mixed_and_invalid_slots_activate_fallback() {
    std::vector<LegacyRaceMenuInput> races = {
        legacy_race("First", true),
        legacy_race("Second", false),
        legacy_race("Third", true),
    };
    races[0].has_instant_action_slot = true;
    races[0].instant_action_slot = 2;
    races[2].has_instant_action_slot = true;
    races[2].instant_action_slot = 0;

    const LegacyRaceMenuPlan mixed =
        build_legacy_race_menu_plan(races, races.size());
    assert(mixed.fallback_active);
    assert(mixed.playable_count == 2);
    expect_slot(mixed, 0, 0);
    expect_slot(mixed, 1, -1);
    expect_slot(mixed, 2, 1);

    races[1].has_instant_action_slot = true;
    races[1].instant_action_slot = 2;
    const LegacyRaceMenuPlan duplicate =
        build_legacy_race_menu_plan(races, races.size());
    assert(duplicate.fallback_active);

    races[1].instant_action_slot = 3;
    const LegacyRaceMenuPlan out_of_range =
        build_legacy_race_menu_plan(races, races.size());
    assert(out_of_range.fallback_active);
}

void sparse_missing_and_inherited_records() {
    std::vector<LegacyRaceMenuInput> races = {
        legacy_race("Inherited playable", true),
        LegacyRaceMenuInput{},  // Sparse raceX or missing Race ODF.
        legacy_race("Child override", true),
    };
    races[2].has_display_key = true;

    const LegacyRaceMenuPlan plan =
        build_legacy_race_menu_plan(races, races.size());
    assert(plan.fallback_active);
    assert(plan.playable_count == 2);
    expect_slot(plan, 0, 0);
    expect_slot(plan, 1, -1);
    expect_slot(plan, 2, 1);
    assert(plan.entries[0].replace_display_key);
    assert(!plan.entries[2].replace_display_key);
}

void duplicate_names_are_distinct_declarations() {
    const std::vector<LegacyRaceMenuInput> races = {
        legacy_race("Duplicate", true),
        legacy_race("Duplicate", true),
    };
    const LegacyRaceMenuPlan plan =
        build_legacy_race_menu_plan(races, races.size());
    assert(plan.fallback_active);
    assert(plan.playable_count == 2);
    expect_slot(plan, 0, 0);
    expect_slot(plan, 1, 1);
}

void repeated_planning_is_stable() {
    const std::vector<LegacyRaceMenuInput> races = {
        legacy_race("One", true), LegacyRaceMenuInput{},
        legacy_race("Two", true),
    };
    const LegacyRaceMenuPlan first =
        build_legacy_race_menu_plan(races, races.size());
    const LegacyRaceMenuPlan second =
        build_legacy_race_menu_plan(races, races.size());
    assert(first.fallback_active == second.fallback_active);
    assert(first.playable_count == second.playable_count);
    assert(first.entries.size() == second.entries.size());
    for (std::size_t index = 0; index < first.entries.size(); ++index) {
        assert(first.entries[index].instant_action_slot ==
               second.entries[index].instant_action_slot);
        assert(first.entries[index].replace_display_key ==
               second.entries[index].replace_display_key);
        assert(first.entries[index].display_key ==
               second.entries[index].display_key);
    }
}

}  // namespace

int main() {
    stock_a1_order_and_labels();
    valid_fo_list_is_untouched();
    mixed_and_invalid_slots_activate_fallback();
    sparse_missing_and_inherited_records();
    duplicate_names_are_distinct_declarations();
    repeated_planning_is_stable();
    return 0;
}
