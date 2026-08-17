#include "wreckage_policy.hpp"

#include <cassert>

int main() {
    using a2fo::wreckage::DecisionStatus;
    using a2fo::wreckage::decide_replacement;

    assert(decide_replacement("", false, "", 1).status ==
           DecisionStatus::none);

    const auto certain = decide_replacement(
        "  fed_sovereign_wreck  ", false, "", 42);
    assert(certain.status == DecisionStatus::replace);
    assert(certain.replacement_odf == "fed_sovereign_wreck");

    assert(decide_replacement("wreck", true, "0", 42).status ==
           DecisionStatus::none);
    assert(decide_replacement("wreck", true, "100", 42).status ==
           DecisionStatus::replace);
    assert(decide_replacement("wreck", true, "101", 42).status ==
           DecisionStatus::invalid_chance);
    assert(decide_replacement("wreck", true, "-1", 42).status ==
           DecisionStatus::invalid_chance);
    assert(decide_replacement("wreck", true, "not-a-number", 42).status ==
           DecisionStatus::invalid_chance);

    const auto first = decide_replacement("wreck", true, "37.5", 123456);
    const auto second = decide_replacement("wreck", true, "37.5", 123456);
    assert(first.status == second.status);
    return 0;
}
