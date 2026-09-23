#include "squadron_state.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <limits>

using namespace a2fo::squadrons;

namespace {
const ClassResolver ships = [](const std::string& odf) {
    if (odf == "fighter" || odf == "bomber") return MemberClass::mobile_craft;
    if (odf == "nested") return MemberClass::squadron;
    if (odf == "station") return MemberClass::unsupported;
    return MemberClass::missing;
};

OdfFields mixed_fields() {
    return {{"classLabel", "\"squadron\""}, {"unitName", "Patrol"},
            {"buildTime", "45"}, {"dilithiumCost", "450"},
            {"squadMember1", "bomber"}, {"squadMemberCount1", "2"},
            {"squadMember0", "fighter"}, {"squadMemberCount0", "3"}};
}

Definition mixed() {
    const auto parsed = parse_definition("patrol.odf", mixed_fields());
    assert(parsed);
    return *parsed.definition;
}

Definition triple() {
    return *parse_definition("triple", {{"classLabel", "squadron"},
            {"squadMember0", "fighter"}, {"squadMemberCount0", "3"}}).definition;
}

std::vector<Craft> members(const Definition& def, ObjectId start = 10, TeamId team = 1) {
    std::vector<Craft> result;
    for (const auto& row : def.members)
        for (std::uint32_t i = 0; i < row.count; ++i)
            result.push_back({start++, team, row.odf});
    return result;
}

SquadId create(Registry& registry, const Definition& def = mixed(), ObjectId start = 10, TeamId team = 1) {
    const auto result = registry.create(def, team, members(def, start, team), ships);
    assert(result);
    return *result.value;
}

RepairContext repair(const Registry& registry, SquadId id, ObjectId yard = 9000) {
    const auto& squad = *registry.find(id);
    RepairContext result{yard, squad.team, true, true, {}};
    for (const auto& slot : squad.slots) if (slot.member) result.serviced_members.push_back(slot.member);
    return result;
}

void config_accepts_authored_composition() {
    auto fields = mixed_fields();
    fields[0] = {"CLASSLABEL", "'SQUADRON'"};
    fields.push_back({"squadMember10", "  \"FIGHTER.ODF\" \r\n"});
    fields.push_back({"SQUADMEMBERCOUNT10", "1"});
    const auto parsed = parse_definition("PATROL.ODF", fields);
    assert(parsed && parsed.definition->odf == "patrol");
    assert(parsed.definition->size() == 6);
    assert(parsed.definition->members[0].row == 0);
    assert(parsed.definition->members[1].row == 1);
    assert(parsed.definition->members[2].row == 10);
    assert(parsed.definition->members[2].odf == "fighter");
    assert(parsed.definition->reinforce_at_yard);
    assert(validate_definition(*parsed.definition, ships).empty());
    fields.push_back({"squadReinforceAtYard", "0"});
    assert(!parse_definition("patrol", fields).definition->reinforce_at_yard);
}

void economics_sum_each_member_and_reject_overflow() {
    BuildEconomics fighter{{10, 2, 55, 0, 0, 0, 3, 4, 5, 6}, 6};
    BuildEconomics bomber{{20, 3, 100, 5, 7, 9, 11, 13, 17, 19}, 12.5f};
    const auto resolve = [&](const std::string& odf) -> std::optional<BuildEconomics> {
        if (odf == "fighter") return fighter;
        if (odf == "bomber") return bomber;
        return {};
    };
    BuildEconomics total;
    assert(sum_build_economics(mixed(), resolve, &total).empty());
    const std::array<std::int32_t, 10> expected{{70, 12, 365, 10, 14, 18, 31, 38, 49, 56}};
    assert(total.costs == expected && total.seconds == 43);
    // Sparse duplicate type rows each contribute their own count.
    Definition duplicate{"patrol", {{0, "fighter", 2}, {10, "fighter", 3}}, true};
    assert(sum_build_economics(duplicate, resolve, &total).empty());
    assert(total.costs[2] == 275 && total.seconds == 30);
    const auto before = total;
    for (int invalid = 0; invalid < 4; ++invalid) {
        auto bad = fighter;
        if (invalid == 0) bad.costs[2] = std::numeric_limits<std::int32_t>::max();
        if (invalid == 1) bad.costs[6] = -1;
        if (invalid == 2) bad.seconds = std::numeric_limits<float>::infinity();
        if (invalid == 3) bad.seconds = std::numeric_limits<float>::max();
        assert(!sum_build_economics(triple(), [&](const std::string&) { return bad; }, &total).empty());
        assert(total.costs == before.costs && total.seconds == before.seconds);
    }
    assert(!sum_build_economics(triple(), [](const std::string&) {
        return std::optional<BuildEconomics>{}; }, &total).empty());
    fighter = {};
    assert(sum_build_economics(triple(), resolve, &total).empty());
    assert(total.seconds == 0 && total.costs == fighter.costs);
}

void config_rejects_malformed_values() {
    for (const auto* count : {"0", "-1", "+3", "3.0", "1e1", "3 rubbish", "", "33", "999999999999999999999"}) {
        auto fields = mixed_fields();
        fields.back().second = count;
        assert(!parse_definition("patrol", fields));
    }
    for (const auto* key : {"squadMember16", "squadMember-1", "squadMember01", "squadMember", "squadSize", "squadMemberCount999999999999999"}) {
        auto fields = mixed_fields();
        fields.push_back({key, "1"});
        assert(!parse_definition("patrol", fields));
    }
    for (const auto* odf : {"../fighter", "ships/fighter", "ships\\fighter", "", "fighter.exe", "fight\ner", "fighter x"}) {
        auto fields = mixed_fields();
        fields[6].second = std::string("\"") + odf + "\"";
        assert(!parse_definition("patrol", fields));
    }
    auto fields = mixed_fields();
    fields.push_back({"SQUADMEMBERCOUNT0", "3"});
    assert(!parse_definition("patrol", fields));
    fields = mixed_fields();
    fields.pop_back();
    assert(!parse_definition("patrol", fields));
    fields = mixed_fields();
    fields.back().second = "32";
    assert(!parse_definition("patrol", fields)); // 32 fighters + two bombers
    fields = mixed_fields();
    fields.push_back({"squadReinforceAtYard", "true"});
    assert(!parse_definition("patrol", fields));
    assert(!parse_definition("fighter", mixed_fields())); // self-reference
    assert(!parse_definition("patrol", {{"classLabel", "squadron"}}));
    assert(!parse_definition("patrol", {{"classLabel", "craft"}}));
    assert(!parse_definition("../patrol", mixed_fields()));
}

void class_validation_rejects_non_ships_and_nesting() {
    for (const auto* odf : {"missing", "station", "nested"}) {
        auto fields = mixed_fields();
        fields[6].second = odf;
        const auto parsed = parse_definition("patrol", fields);
        assert(parsed);
        assert(!validate_definition(*parsed.definition, ships).empty());
    }
    assert(!validate_definition(mixed(), {}).empty());
    auto mutated = mixed();
    mutated.members[0].count = UINT32_MAX;
    assert(!validate_definition(mutated, ships).empty());
    mutated = mixed();
    std::swap(mutated.members[0], mutated.members[1]);
    assert(!validate_definition(mutated, ships).empty());
}

void initial_membership_is_atomic() {
    Registry registry;
    const auto def = mixed();
    auto craft = members(def);
    craft.pop_back();
    assert(!registry.create(def, 1, craft, ships));
    assert(registry.size() == 0 && registry.snapshot().next_squad == 1);
    craft = members(def);
    craft[4].id = craft[0].id;
    assert(!registry.create(def, 1, craft, ships));
    craft = members(def);
    craft[4].team = 2;
    assert(!registry.create(def, 1, craft, ships));
    craft = members(def);
    craft[4].odf = "fighter";
    assert(!registry.create(def, 1, craft, ships));
    craft = members(def);
    craft[4].id = 0;
    assert(!registry.create(def, 1, craft, ships));
    assert(!registry.containing(10));
    const auto id = create(registry);
    assert(id == 1 && registry.find(id)->badge() == "5/5");
    assert(!registry.create(def, 1, members(def), ships));
    assert(registry.size() == 1 && registry.snapshot().next_squad == 2);
}

void casualties_keep_slots_and_identity() {
    Registry registry;
    const auto id = create(registry);
    assert(registry.find(id)->representative() == 10);
    assert(registry.remove(10).removed);
    assert(registry.find(id)->representative() == 11);
    assert(registry.find(id)->badge() == "4/5");
    assert(registry.remove(13).removed);
    const auto& squad = *registry.find(id);
    assert(squad.id == id && squad.badge() == "3/5");
    assert(squad.slots[0].member == 0 && squad.slots[0].odf == "fighter");
    assert(squad.slots[3].member == 0 && squad.slots[3].odf == "bomber");
    assert(!registry.remove(13).removed);
    assert(!registry.containing(13));
}

void selection_groups_and_expands_without_truncation() {
    Registry registry;
    const auto first = create(registry);
    const auto second = create(registry, mixed(), 20);
    registry.remove(10);
    const std::vector<Craft> selected{{23, 1, "bomber"}, {11, 1, "fighter"},
        {12, 1, "fighter"}, {23, 1, "bomber"}, {100, 1, "fighter"}, {500, 2, "fighter"}};
    const auto result = registry.select(selected, 1, 10);
    assert(result && result.value->tiles.size() == 3);
    const auto& tiles = result.value->tiles;
    assert(tiles[0].squad == first && tiles[0].representative == 11);
    assert(tiles[0].live == 4 && tiles[0].maximum == 5);
    assert(tiles[1].squad == second && tiles[1].live == 5);
    assert(tiles[2].squad == 0 && tiles[2].representative == 100);
    const std::vector<ObjectId> expected{11, 12, 13, 14, 20, 21, 22, 23, 24, 100};
    assert(result.value->members == expected);
    auto reversed = selected;
    std::reverse(reversed.begin(), reversed.end());
    assert(registry.select(reversed, 1, 10).value->members == expected);
    assert(!registry.select(selected, 1, 9));
    assert(!registry.select(selected, 1, 0));
    assert(registry.select({}, 1, 0));
    assert(!registry.select({{11, 2, "fighter"}}, 2, 32)); // ownership callback overdue
}

void repair_requires_accepted_cycle_and_all_survivors() {
    Registry registry;
    const auto id = create(registry);
    assert(!registry.reserve_replacement(id, repair(registry, id)));
    registry.remove(13);
    auto context = repair(registry, id);
    context.accepted_repair_cycle = false;
    assert(!registry.reserve_replacement(id, context)); // proximity is insufficient
    context = repair(registry, id); context.compatible = false;
    assert(!registry.reserve_replacement(id, context));
    context = repair(registry, id); context.team = 2;
    assert(!registry.reserve_replacement(id, context));
    context = repair(registry, id); context.yard = 0;
    assert(!registry.reserve_replacement(id, context));
    context = repair(registry, id); context.serviced_members.pop_back();
    assert(!registry.reserve_replacement(id, context));
    context = repair(registry, id); context.serviced_members.push_back(10);
    assert(!registry.reserve_replacement(id, context));
    context = repair(registry, id, 10);
    assert(!registry.reserve_replacement(id, context));
    assert(registry.snapshot().next_ticket == 1);
    // No health requirement: full-health survivors still replenish when a
    // native bridge explicitly accepts this depleted squad's repair cycle.
    const auto request = registry.reserve_replacement(id, repair(registry, id));
    assert(request && request.value->odf == "bomber" && request.value->slot == 3);
    assert(!registry.reserve_replacement(id, repair(registry, id)));
    assert(registry.snapshot().next_ticket == 2);
}

void replacement_restores_exact_type_once() {
    Registry registry;
    const auto id = create(registry);
    registry.remove(13);
    const auto ticket = registry.reserve_replacement(id, repair(registry, id)).value->ticket;
    assert(!registry.complete_replacement(ticket, {100, 1, "fighter"}, repair(registry, id)));
    assert(!registry.complete_replacement(ticket, {100, 2, "bomber"}, repair(registry, id)));
    assert(!registry.complete_replacement(ticket, {10, 1, "bomber"}, repair(registry, id)));
    assert(!registry.complete_replacement(ticket, {9000, 1, "bomber"}, repair(registry, id)));
    assert(!registry.complete_replacement(ticket, {100, 1, "bomber"}, repair(registry, id, 9001)));
    auto departed = repair(registry, id); departed.serviced_members.pop_back();
    assert(!registry.complete_replacement(ticket, {100, 1, "bomber"}, departed));
    assert(!registry.complete_replacement(ticket + 1, {100, 1, "bomber"}, repair(registry, id)));
    assert(registry.find(id)->badge() == "4/5");
    assert(registry.complete_replacement(ticket, {100, 1, "BOMBER.ODF"}, repair(registry, id)));
    assert(!registry.complete_replacement(ticket, {101, 1, "bomber"}, repair(registry, id)));
    assert(registry.find(id)->badge() == "5/5" && registry.find(id)->slots[3].member == 100);
    assert(!registry.find(id)->replacement);
}

void reservations_cancel_without_resurrection() {
    Registry registry;
    const auto id = create(registry, triple());
    registry.remove(10);
    const auto old_context = repair(registry, id);
    const auto first = *registry.reserve_replacement(id, old_context).value;
    assert(registry.cancel_replacement(first.ticket)->squad == id);
    assert(!registry.cancel_replacement(first.ticket));
    const auto second = *registry.reserve_replacement(id, old_context).value;
    assert(second.ticket > first.ticket);
    assert(!registry.complete_replacement(first.ticket, {100, 1, "fighter"}, old_context));
    assert(!registry.remove(11).squad_retired);
    const auto loss = registry.remove(12);
    assert(loss.squad_retired && loss.cancelled && loss.cancelled->ticket == second.ticket);
    assert(!registry.find(id) && registry.size() == 0);
    assert(!registry.complete_replacement(second.ticket, {101, 1, "fighter"}, old_context));
    assert(!registry.reserve_replacement(id, old_context));
    const auto next = create(registry, triple(), 20);
    assert(next > id);
}

void capture_detaches_and_yard_cancellation_is_exact() {
    Registry registry;
    const auto first = create(registry);
    const auto second = create(registry, mixed(), 20);
    const auto third = create(registry, mixed(), 30);
    assert(!registry.change_owner(10, 1).removed);
    assert(registry.change_owner(10, 2).removed);
    assert(!registry.containing(10));
    auto captured = registry.select({{10, 2, "fighter"}}, 2, 1);
    assert(captured && captured.value->tiles[0].squad == 0);
    registry.remove(20); registry.remove(30);
    const auto a = *registry.reserve_replacement(first, repair(registry, first)).value;
    const auto b = *registry.reserve_replacement(second, repair(registry, second)).value;
    const auto c = *registry.reserve_replacement(third, repair(registry, third, 9001)).value;
    const auto cancelled = registry.cancel_yard(9000);
    assert(cancelled.size() == 2 && cancelled[0].ticket == a.ticket && cancelled[1].ticket == b.ticket);
    assert(registry.cancel_yard(9000).empty());
    assert(registry.find(third)->replacement->ticket == c.ticket);
}

void multiple_losses_replace_lowest_vacant_slot() {
    Registry registry;
    const auto id = create(registry);
    registry.remove(13); registry.remove(10);
    const auto fighter = *registry.reserve_replacement(id, repair(registry, id)).value;
    assert(fighter.slot == 0 && fighter.odf == "fighter");
    assert(registry.complete_replacement(fighter.ticket, {100, 1, "fighter"}, repair(registry, id)));
    const auto bomber = *registry.reserve_replacement(id, repair(registry, id)).value;
    assert(bomber.slot == 3 && bomber.odf == "bomber");
    assert(registry.complete_replacement(bomber.ticket, {101, 1, "bomber"}, repair(registry, id)));
    assert(registry.find(id)->representative() == 100 && registry.find(id)->badge() == "5/5");
}

void snapshots_preserve_pending_identity_and_vacancies() {
    Registry source;
    const auto id = create(source);
    source.remove(10); source.remove(13);
    const auto request = *source.reserve_replacement(id, repair(source, id)).value;
    const auto saved = source.snapshot();
    Registry restored;
    std::string error;
    assert(restored.restore(saved, ships, error) && error.empty());
    assert(restored.find(id)->badge() == "3/5");
    assert(restored.find(id)->representative() == 11);
    assert(restored.containing(14)->id == id && !restored.containing(10));
    assert(!restored.reserve_replacement(id, repair(restored, id)));
    assert(restored.complete_replacement(request.ticket, {100, 1, "fighter"}, repair(restored, id)));
    assert(restored.find(id)->badge() == "4/5");
    const auto next = *restored.reserve_replacement(id, repair(restored, id)).value;
    assert(next.ticket > request.ticket && next.odf == "bomber");
    assert(create(restored, triple(), 200) > id);
}

void malformed_snapshots_leave_live_state_unchanged() {
    Registry registry;
    const auto id = create(registry);
    registry.remove(10);
    registry.reserve_replacement(id, repair(registry, id));
    const auto original = registry.snapshot();
    const auto reject = [&](Snapshot bad) {
        std::string error;
        assert(!registry.restore(bad, ships, error) && !error.empty());
        assert(registry.size() == 1 && registry.find(id)->badge() == "4/5");
        assert(registry.containing(11)->id == id);
        assert(registry.find(id)->replacement->ticket == original.squads[0].replacement->ticket);
    };
    auto bad = original; bad.version = 2; reject(bad);
    bad = original; bad.next_squad = id; reject(bad);
    bad = original; bad.next_ticket = 1; reject(bad);
    bad = original; bad.squads.push_back(bad.squads[0]); reject(bad);
    bad = original; bad.squads[0].slots[2].member = 11; reject(bad);
    bad = original; bad.squads[0].slots[0].odf = "bomber"; reject(bad);
    bad = original; bad.squads[0].slots[0].ordinal = 99; reject(bad);
    bad = original; bad.squads[0].slots.clear(); reject(bad);
    bad = original; for (auto& slot : bad.squads[0].slots) slot.member = 0; reject(bad);
    bad = original; bad.squads[0].replacement->slot = 99; reject(bad);
    bad = original; bad.squads[0].replacement->slot = 1; reject(bad);
    bad = original; bad.squads[0].replacement->team = 2; reject(bad);
    bad = original; bad.squads[0].replacement->yard = 11; reject(bad);
    bad = original; bad.squads[0].replacement->odf = "bomber"; reject(bad);
    bad = original; bad.squads[0].definition.reinforce_at_yard = false; reject(bad);
    bad = original; bad.squads[0].definition.members[0].count = UINT32_MAX; reject(bad);
}

void bounds_and_disabled_reinforcement() {
    Registry registry;
    const auto def = triple();
    for (std::uint32_t i = 0; i < kMaxSquadsPerTeam; ++i) create(registry, def, 10 + i * 3);
    const auto next_id = registry.snapshot().next_squad;
    assert(!registry.create(def, 1, members(def, 8000), ships));
    assert(registry.snapshot().next_squad == next_id);
    assert(registry.create(def, 2, members(def, 8000, 2), ships));
    auto disabled = mixed(); disabled.reinforce_at_yard = false;
    const auto id = create(registry, disabled, 8100, 2);
    registry.remove(8100);
    assert(!registry.reserve_replacement(id, repair(registry, id)));
    const auto maxed = parse_definition("maxed", {{"classLabel", "squadron"},
        {"squadMember0", "fighter"}, {"squadMemberCount0", "32"}});
    assert(maxed && maxed.definition->size() == 32);
    Registry bounded;
    create(bounded, *maxed.definition);
    assert(bounded.select({{10, 1, "fighter"}}, 1, 32));
    assert(!bounded.select({{10, 1, "fighter"}}, 1, 31));
    auto exhausted = bounded.snapshot();
    exhausted.next_squad = std::numeric_limits<SquadId>::max();
    std::string error;
    assert(bounded.restore(exhausted, ships, error));
    assert(!bounded.create(def, 1, members(def, 100), ships));
    exhausted = bounded.snapshot();
    exhausted.next_ticket = std::numeric_limits<TicketId>::max();
    assert(bounded.restore(exhausted, ships, error));
    bounded.remove(10);
    assert(!bounded.reserve_replacement(1, repair(bounded, 1)));
}

void three_ship_scenario() {
    Registry registry;
    const auto id = create(registry, triple());
    assert(registry.select({{11, 1, "fighter"}}, 1, 3).value->tiles.size() == 1);
    std::cout << "  three-member build: " << registry.find(id)->badge();
    registry.remove(10);
    std::cout << " -> casualty: " << registry.find(id)->badge();
    const auto job = *registry.reserve_replacement(id, repair(registry, id)).value;
    assert(registry.complete_replacement(job.ticket, {100, 1, "fighter"}, repair(registry, id)));
    std::cout << " -> repair completion: " << registry.find(id)->badge() << '\n';
    assert(registry.find(id)->badge() == "3/3");
}

void sequential_launch_keeps_one_squad_identity() {
    Registry registry;
    const auto def = mixed();
    const auto first = registry.create_launching(
        def, 1, {10, 1, "fighter"}, ships);
    assert(first);
    const auto id = *first.value;
    assert(registry.find(id)->badge() == "1/5");
    assert(!registry.reserve_replacement(id, repair(registry, id)));
    assert(registry.select({{10, 1, "fighter"}}, 1, 30).value->tiles.size() == 1);
    const auto first_loss = registry.remove(10);
    assert(first_loss.removed && !first_loss.squad_retired);
    assert(registry.find(id)->live_count() == 0);
    assert(!registry.add_launching(id, 1, {11, 1, "bomber"}));
    assert(registry.add_launching(id, 1, {11, 1, "fighter"}));
    assert(registry.add_launching(id, 2, {12, 1, "fighter"}));
    assert(registry.add_launching(id, 3, {13, 1, "bomber"}));
    assert(registry.add_launching(id, 4, {14, 1, "bomber"}));
    assert(!registry.add_launching(id, 4, {15, 1, "bomber"}));
    assert(registry.find(id)->badge() == "4/5");
    assert(registry.finish_launching(id));
    assert(!registry.add_launching(id, 0, {16, 1, "fighter"}));
    registry.remove(11);
    registry.remove(12);
    registry.remove(13);
    assert(registry.remove(14).squad_retired);
    assert(!registry.find(id));
}
} // namespace

int main() {
    const std::pair<const char*, void(*)()> cases[] = {
        {"authored mixed composition", config_accepts_authored_composition},
        {"count-weighted member economics and overflow", economics_sum_each_member_and_reject_overflow},
        {"malformed configuration", config_rejects_malformed_values},
        {"class and nesting validation", class_validation_rejects_non_ships_and_nesting},
        {"atomic initial membership", initial_membership_is_atomic},
        {"casualty slots and leader survival", casualties_keep_slots_and_identity},
        {"logical selection and physical capacity", selection_groups_and_expands_without_truncation},
        {"accepted repair and all-survivor gate", repair_requires_accepted_cycle_and_all_survivors},
        {"exact-type single replacement", replacement_restores_exact_type_once},
        {"cancellation and total loss", reservations_cancel_without_resurrection},
        {"capture and yard cancellation", capture_detaches_and_yard_cancellation_is_exact},
        {"ordered mixed replacement", multiple_losses_replace_lowest_vacant_slot},
        {"pending snapshot round trip", snapshots_preserve_pending_identity_and_vacancies},
        {"transactional corrupt snapshot rejection", malformed_snapshots_leave_live_state_unchanged},
        {"limits and disabled reinforcement", bounds_and_disabled_reinforcement},
        {"three-member acceptance scenario", three_ship_scenario},
        {"sequential native launch membership", sequential_launch_keeps_one_squad_identity},
    };
    for (const auto& test : cases) {
        test.second();
        std::cout << "PASS " << test.first << '\n';
    }
    std::cout << "Squadron policy: " << sizeof(cases) / sizeof(cases[0]) << " scenarios passed\n";
}
