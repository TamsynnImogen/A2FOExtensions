#include "../modules/A2FOEnergySystems/resupply_policy.hpp"

#include <cassert>
#include <cstdio>
#include <string>

#ifdef NDEBUG
#error "This regression requires active assertions"
#endif

using a2fo::energy_systems::ResupplyTargetFilter;
using a2fo::energy_systems::normalize_resupply_odf;
using a2fo::energy_systems::normalize_resupply_type;
using a2fo::energy_systems::parse_resupply_targets;
using a2fo::energy_systems::resupply_target_allowed;

namespace {
bool allows(const ResupplyTargetFilter& filter,
            const std::string& type, const std::string& odf) {
    return resupply_target_allowed(filter, type, odf, false, false);
}

void check_invalid(const std::string& input) {
    const auto filter = parse_resupply_targets(input);
    assert(!filter.valid && !filter.default_allow);
    assert(filter.types.empty() && filter.odfs.empty());
    assert(!allows(filter, "fighter", "fed_fighter"));
}
}

int main() {
    assert(normalize_resupply_type(" \t\"FiGhTeR\" \n") == "fighter");
    assert(normalize_resupply_type(" Frigate_2-A.B ") == "frigate_2-a.b");
    assert(normalize_resupply_type("fighter/craft").empty());
    assert(normalize_resupply_type("\"fighter").empty());
    assert(normalize_resupply_odf(" \"C:\\Fleet\\Ships\\Fed_Scout.ODF\" ") ==
           "fed_scout");
    assert(normalize_resupply_odf("mods/ships/Fed_Fighter.odf") == "fed_fighter");
    assert(normalize_resupply_odf("./").empty());
    assert(normalize_resupply_odf("bad name.odf").empty());

    const ResupplyTargetFilter omitted;
    assert(omitted.valid && omitted.default_allow);
    assert(allows(omitted, "", ""));
    assert(!resupply_target_allowed(omitted, "fighter", "fed_fighter", true, false));
    assert(resupply_target_allowed(omitted, "fighter", "fed_fighter", true, true));

    const auto empty = parse_resupply_targets(" \"   \" ");
    assert(empty.valid && !empty.default_allow && empty.types.empty());
    assert(!allows(empty, "fighter", "fed_fighter"));
    assert(resupply_target_allowed(empty, "", "", true, true));

    const auto fighters = parse_resupply_targets("\"type:FiGhTeR odf:fed_scout.odf\"");
    assert(fighters.valid && !fighters.default_allow);
    assert(allows(fighters, "fighter", "unknown"));
    assert(allows(fighters, "", "fed_scout"));
    assert(!allows(fighters, "bomber", "unknown"));
    assert(!allows(fighters, "", ""));

    const auto wildcard = parse_resupply_targets(
        "* !type:bomber odf:fed_bomber !odf:fed_scout.odf");
    assert(wildcard.valid && wildcard.default_allow);
    assert(allows(wildcard, "frigate", ""));
    assert(!allows(wildcard, "bomber", "other"));
    assert(allows(wildcard, "bomber", "fed_bomber"));
    assert(!allows(wildcard, "fighter", "fed_scout"));

    const auto precedence = parse_resupply_targets(
        "type:fighter !odf:fed_fighter !type:bomber odf:fed_bomber");
    assert(precedence.valid);
    assert(allows(precedence, "fighter", "other"));
    assert(!allows(precedence, "fighter", "fed_fighter"));
    assert(allows(precedence, "bomber", "fed_bomber"));
    assert(!allows(precedence, "bomber", "other"));
    assert(!allows(precedence, "", ""));

    const auto duplicates = parse_resupply_targets(
        "TYPE:Fighter type:fighter ODF:Fed_Scout.odf odf:fed_scout");
    assert(duplicates.valid && duplicates.types.size() == 1 &&
           duplicates.odfs.size() == 1);

    for (const char* input : {
             "type:fighter !type:FIGHTER", "odf:fed_scout !ODF:fed_scout.odf",
             "!", "!*", "fighter", "type:", "odf:bad/name.odf",
             "type:bad/name", "class:fighter", "type:foo:bar",
             "\"type:fighter", "type:fighter\"", "type:\"fighter\"",
             "type:fighter \"odf:fed_scout\""}) {
        check_invalid(input);
    }
    std::string boundary = "type:fighter";
    boundary.append(4094 - boundary.size(), ' ');
    assert(parse_resupply_targets(boundary).valid);
    boundary.push_back(' ');
    check_invalid(boundary);
    std::string excessive;
    for (int index = 0; index < 257; ++index) excessive += "type:fighter ";
    check_invalid(excessive);

    const auto invalid = parse_resupply_targets("type:fighter !type:fighter");
    assert(!resupply_target_allowed(invalid, "fighter", "", true, false));
    assert(resupply_target_allowed(invalid, "fighter", "", true, true));
    std::puts("resupply policy: normalization, precedence, fail-closed parsing and self override PASS");
}
