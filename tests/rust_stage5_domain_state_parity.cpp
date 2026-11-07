// One transcript linked against frozen C++ and then Rust-backed C++.
#include "../modules/A2FOCollectiveXP/collective_xp.hpp"
#include "../modules/A2FOSquadrons/squadron_state.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
using namespace a2fo;
static void d(double x) {
    std::uint64_t bits;
    std::memcpy(&bits, &x, 8);
    std::printf(" %016llx", static_cast<unsigned long long>(bits));
}
static void bo(bool x) { std::printf(" %d", x ? 1 : 0); }
static void status(collective_xp::Status s) { std::printf(" %d", static_cast<int>(s)); }
static void state(const collective_xp::PlayerState &s) {
    bo(s.enabled);
    std::printf(" %u", s.level);
    d(s.pool);
    d(s.multiplier);
}
static void resolution(const collective_xp::Resolution &r) {
    status(r.status);
    std::printf(" [%s] %u", r.class_id.c_str(), r.level);
    bo(r.has_next);
    d(r.next_xp);
}
static void progress(const collective_xp::Progress &p) {
    status(p.status);
    bo(p.consumed);
    std::printf(" %u", p.levels_gained);
    state(p.state);
    d(p.threshold);
}
static void hex(const std::vector<std::uint8_t> &bytes) {
    std::printf(" %zu", bytes.size());
    for (auto c : bytes)
        std::printf(" %02x", c);
}
static void xp_run() {
    using namespace collective_xp;
    Catalog catalog;
    const ClassRank ranks[] = {{"alpha", 1, 10, "beta"},   {"beta", 2, 20, "gamma"},
                               {"gamma", 3, 0, ""},        {"orphan", 1, 0, ""},
                               {"cycle1", 1, 1, "cycle2"}, {"cycle2", 2, 1, "cycle1"},
                               {"bad", 2, 0, ""}};
    for (const auto &rank : ranks) {
        std::printf("add [%s]", rank.id.c_str());
        bo(catalog.add(rank));
        std::putchar('\n');
    }
    for (const ClassRank &rank : {ClassRank{"", 1, 0, ""}, ClassRank{"alpha", 1, 0, ""},
                                  ClassRank{"self", 1, 1, "self"}}) {
        std::printf("add_bad [%s]", rank.id.c_str());
        bo(catalog.add(rank));
        std::putchar('\n');
    }
    for (const auto &base : {"alpha", "orphan", "cycle1", "bad", "missing", ""})
        for (unsigned level : {0u, 1u, 2u, 3u, 64u}) {
            std::printf("resolve [%s] %u", base, level);
            resolution(catalog.resolve(base, level));
            std::putchar('\n');
        }
    for (const auto &current : {"alpha", "beta", "gamma", "orphan", "missing"}) {
        std::printf("resolve_current [%s]", current);
        resolution(catalog.resolve("alpha", 2, current));
        std::putchar('\n');
    }
    for (const auto &id : {"alpha", "beta", "gamma", "missing"})
        std::printf("native [%s] %u\n", id, catalog.native_level(id));
    Controller ctl(catalog);
    for (auto [player, mult] :
         {std::pair<unsigned, double>{1, 1.5}, {2, 2.0}, {3, -1.0}}) {
        std::printf("configure %u", player);
        bo(ctl.configure(player, true, mult));
        std::putchar('\n');
    }
    const Unit units[] = {{11, 1, "alpha", "alpha", true, true, true},
                          {12, 1, "alpha", "beta", true, true, true},
                          {13, 1, "alpha", "gamma", true, true, true},
                          {14, 2, "alpha", "alpha", true, false, true},
                          {15, 2, "alpha", "alpha", true, true, false},
                          {16, 2, "alpha", "alpha", false, true, true},
                          {17, 2, "alpha", "orphan", true, true, true},
                          {0, 1, "alpha", "alpha", true, true, true}};
    for (const auto &u : units) {
        std::printf("upsert %llu", static_cast<unsigned long long>(u.id));
        status(ctl.upsert(u));
        std::putchar('\n');
    }
    for (auto id : {11ull, 12ull, 13ull, 14ull, 99ull}) {
        std::printf("target %llu", id);
        resolution(ctl.target(id));
        std::putchar('\n');
    }
    for (unsigned player : {1u, 2u, 3u, 4u}) {
        Status s;
        const double threshold = ctl.threshold(player, &s);
        std::printf("threshold %u", player);
        d(threshold);
        status(s);
        std::putchar('\n');
    }
    for (double xp :
         {0.0, 5.0, 10.0, 15.0, 100.0, -1.0, std::numeric_limits<double>::infinity()}) {
        std::printf("award");
        d(xp);
        progress(ctl.award(1, xp));
        std::putchar('\n');
    }
    std::printf("reconcile");
    progress(ctl.reconcile(1));
    std::putchar('\n');
    for (unsigned player : {1u, 2u, 4u}) {
        std::printf("state %u", player);
        const auto *p = ctl.state(player);
        bo(p != nullptr);
        if (p)
            state(*p);
        std::putchar('\n');
    }
    const auto saved = ctl.snapshot();
    std::printf("snapshot");
    hex(saved);
    std::putchar('\n');
    const auto one = ctl.snapshot_player(1);
    std::printf("snapshot_one");
    hex(one);
    std::putchar('\n');
    for (auto mutated :
         {saved, std::vector<std::uint8_t>(saved.begin(), saved.end() - 1),
          std::vector<std::uint8_t>{'C', 'X', 'P', '1', 0, 0, 0, 0}}) {
        Controller restored(catalog);
        std::printf("restore");
        bo(restored.restore(mutated));
        hex(restored.snapshot());
        std::putchar('\n');
    }
    std::printf("restore_player");
    bo(ctl.restore_player(1, one));
    bo(ctl.restore_player(2, one));
    std::putchar('\n');
    std::printf("transfer");
    status(ctl.transfer(11, 2));
    status(ctl.transfer(99, 2));
    bo(ctl.remove(12));
    bo(ctl.remove(99));
    std::putchar('\n');
    ctl.reset_player(2);
    std::printf("reset");
    hex(ctl.snapshot());
    resolution(ctl.target(11));
    std::putchar('\n');
}
static const squadrons::ClassResolver ships = [](const std::string &odf) {
    if (odf == "fighter" || odf == "bomber")
        return squadrons::MemberClass::mobile_craft;
    return squadrons::MemberClass::missing;
};
static void sq_snapshot(const squadrons::Registry &registry) {
    const auto saved = registry.snapshot();
    std::printf(" snap %u %llu %llu %zu", saved.version,
                static_cast<unsigned long long>(saved.next_squad),
                static_cast<unsigned long long>(saved.next_ticket),
                saved.squads.size());
    for (const auto &sq : saved.squads) {
        std::printf(" { %llu %u %s", static_cast<unsigned long long>(sq.id), sq.team,
                    sq.badge().c_str());
        for (const auto &slot : sq.slots)
            std::printf(" %u:%u:%s:%llu", slot.row, slot.ordinal, slot.odf.c_str(),
                        static_cast<unsigned long long>(slot.member));
        if (sq.replacement)
            std::printf(" R:%llu:%llu:%u:%s",
                        static_cast<unsigned long long>(sq.replacement->ticket),
                        static_cast<unsigned long long>(sq.replacement->yard),
                        sq.replacement->slot, sq.replacement->odf.c_str());
        std::printf(" }");
    }
}
static void sq_result(const squadrons::Result<squadrons::SquadId> &result) {
    bo(bool(result));
    if (result)
        std::printf(" %llu", static_cast<unsigned long long>(*result.value));
    std::printf(" [%s]", result.error.c_str());
}
static void sq_replacement(const squadrons::Result<squadrons::Replacement> &result) {
    bo(bool(result));
    if (result)
        std::printf(" %llu:%llu:%llu:%u:%s",
                    static_cast<unsigned long long>(result.value->ticket),
                    static_cast<unsigned long long>(result.value->squad),
                    static_cast<unsigned long long>(result.value->yard),
                    result.value->slot, result.value->odf.c_str());
    std::printf(" [%s]", result.error.c_str());
}
static void sq_removal(const squadrons::Removal &result) {
    bo(result.removed);
    bo(result.squad_retired);
    bo(result.cancelled.has_value());
    if (result.cancelled)
        std::printf(" %llu", static_cast<unsigned long long>(result.cancelled->ticket));
}
static void sq_selection(const squadrons::Result<squadrons::Selection> &result) {
    bo(bool(result));
    std::printf(" [%s]", result.error.c_str());
    if (!result)
        return;
    for (const auto &tile : result.value->tiles)
        std::printf(" T:%llu:%llu:%u:%u", static_cast<unsigned long long>(tile.squad),
                    static_cast<unsigned long long>(tile.representative), tile.live,
                    tile.maximum);
    for (auto member : result.value->members)
        std::printf(" M:%llu", static_cast<unsigned long long>(member));
}
static squadrons::RepairContext sq_repair(const squadrons::Registry &registry,
                                          squadrons::SquadId id,
                                          squadrons::ObjectId yard = 9000) {
    const auto *squad = registry.find(id);
    squadrons::RepairContext repair{yard, squad->team, true, true, {}};
    for (const auto &slot : squad->slots)
        if (slot.member)
            repair.serviced_members.push_back(slot.member);
    return repair;
}
static void sq_run() {
    using namespace squadrons;
    Definition def{"patrol", {{0, "fighter", 2}, {1, "bomber", 1}}, true};
    std::printf("sq_def [%s] %u\n", validate_definition(def, ships).c_str(),
                def.size());
    Registry registry;
    auto good =
        std::vector<Craft>{{10, 1, "fighter"}, {11, 1, "fighter"}, {12, 1, "bomber"}};
    std::printf("sq_bad_count");
    sq_result(registry.create(def, 1, {good[0]}, ships));
    sq_snapshot(registry);
    std::putchar('\n');
    auto wrong = good;
    wrong[1].odf = "bomber";
    std::printf("sq_bad_odf");
    sq_result(registry.create(def, 1, wrong, ships));
    sq_snapshot(registry);
    std::putchar('\n');
    wrong = good;
    wrong[1].id = wrong[0].id;
    std::printf("sq_duplicate");
    sq_result(registry.create(def, 1, wrong, ships));
    sq_snapshot(registry);
    std::putchar('\n');
    const auto created = registry.create(def, 1, good, ships);
    std::printf("sq_create");
    sq_result(created);
    sq_snapshot(registry);
    std::putchar('\n');
    const auto id = *created.value;
    const auto *stable = registry.find(id);
    std::printf("sq_lookup");
    bo(registry.find(id) != nullptr);
    bo(registry.containing(11) == stable);
    bo(registry.containing(99) != nullptr);
    std::printf(" %zu %u %llu [%s]", registry.size(), stable->live_count(),
                static_cast<unsigned long long>(stable->representative()),
                stable->badge().c_str());
    std::putchar('\n');
    std::printf("sq_select");
    sq_selection(registry.select(
        {good[2], {40, 1, "fighter"}, good[0], good[0], {41, 2, "fighter"}}, 1, 5));
    std::putchar('\n');
    std::printf("sq_select_limit");
    sq_selection(registry.select({good[0], {40, 1, "fighter"}}, 1, 3));
    std::putchar('\n');
    std::printf("sq_remove");
    sq_removal(registry.remove(12));
    sq_removal(registry.remove(99));
    sq_snapshot(registry);
    std::putchar('\n');
    auto repair = sq_repair(registry, id);
    auto bad_repair = repair;
    bad_repair.accepted_repair_cycle = false;
    std::printf("sq_reserve_bad");
    sq_replacement(registry.reserve_replacement(id, bad_repair));
    sq_snapshot(registry);
    std::putchar('\n');
    const auto reserved = registry.reserve_replacement(id, repair);
    std::printf("sq_reserve");
    sq_replacement(reserved);
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_reserve_again");
    sq_replacement(registry.reserve_replacement(id, repair));
    std::putchar('\n');
    std::printf("sq_complete_bad");
    bo(registry.complete_replacement(reserved.value->ticket, {12, 2, "bomber"},
                                     repair));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_complete");
    bo(registry.complete_replacement(reserved.value->ticket, {12, 1, "bomber"},
                                     repair));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_owner_same");
    sq_removal(registry.change_owner(11, 1));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_owner_change");
    sq_removal(registry.change_owner(11, 2));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_stable");
    bo(registry.find(id) == stable);
    std::putchar('\n');
    auto launch = registry.create_launching(def, 2, {50, 2, "fighter"}, ships);
    std::printf("sq_launch");
    sq_result(launch);
    sq_snapshot(registry);
    std::putchar('\n');
    const auto launch_id = *launch.value;
    std::printf("sq_launch_add_bad");
    bo(registry.add_launching(launch_id, 2, {51, 2, "fighter"}));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_launch_add");
    bo(registry.add_launching(launch_id, 1, {51, 2, "fighter"}));
    bo(registry.add_launching(launch_id, 2, {52, 2, "bomber"}));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_launch_finish");
    bo(registry.finish_launching(launch_id));
    bo(registry.finish_launching(launch_id));
    sq_snapshot(registry);
    std::putchar('\n');
    std::printf("sq_cancel_missing");
    bo(registry.cancel_replacement(999).has_value());
    std::printf(" %zu", registry.cancel_yard(9000).size());
    std::putchar('\n');
    const auto saved = registry.snapshot();
    Registry restored;
    std::string error = "old";
    std::printf("sq_restore");
    bo(restored.restore(saved, ships, error));
    std::printf(" [%s]", error.c_str());
    sq_snapshot(restored);
    std::putchar('\n');
    auto invalid = saved;
    invalid.version = 2;
    std::printf("sq_restore_version");
    bo(restored.restore(invalid, ships, error));
    std::printf(" [%s]", error.c_str());
    sq_snapshot(restored);
    std::putchar('\n');
    invalid = saved;
    invalid.squads[0].slots[0].member = 0;
    invalid.squads[0].slots[1].member = 0;
    invalid.squads[0].slots[2].member = 0;
    std::printf("sq_restore_empty");
    bo(restored.restore(invalid, ships, error));
    std::printf(" [%s]", error.c_str());
    sq_snapshot(restored);
    std::putchar('\n');
    invalid = saved;
    invalid.squads[0].slots[0].odf = "wrong";
    std::printf("sq_restore_slot");
    bo(restored.restore(invalid, ships, error));
    std::printf(" [%s]", error.c_str());
    sq_snapshot(restored);
    std::putchar('\n');
    invalid = saved;
    invalid.squads[1].slots[0].member = invalid.squads[0].slots[0].member;
    std::printf("sq_restore_duplicate");
    bo(restored.restore(invalid, ships, error));
    std::printf(" [%s]", error.c_str());
    sq_snapshot(restored);
    std::putchar('\n');
    std::printf("sq_retire");
    sq_removal(registry.remove(10));
    sq_removal(registry.remove(12));
    sq_snapshot(registry);
    std::putchar('\n');
}
static std::uint32_t sweep_seed = 0x5381a773u;
static std::uint32_t sweep_next() {
    sweep_seed = sweep_seed * 1664525u + 1013904223u;
    return sweep_seed;
}
static void xp_sweep() {
    using namespace collective_xp;
    const double denorm = std::numeric_limits<double>::denorm_min();
    const double min = std::numeric_limits<double>::min();
    const double max = std::numeric_limits<double>::max();
    const double thresholds[]{denorm, min,   1e-300, 0.1,    1.0 / 3.0,
                              1.0,    17.25, 1e300,  max / 4};
    const double multipliers[]{denorm, min,    0.125, 1.0,    1.5,
                               2.0,    1024.0, 1e300, max / 4};
    for (int i = 0; i < 144; ++i) {
        const double next_xp = thresholds[(i + sweep_next()) % 9];
        const double multiplier = multipliers[(i / 2 + sweep_next()) % 9];
        Catalog catalog;
        catalog.add({"a", 1, next_xp, "b"});
        catalog.add({"b", 2, 0, ""});
        Controller ctrl(catalog);
        const bool configured = ctrl.configure(1, true, multiplier);
        const auto admitted = ctrl.upsert({1, 1, "a", "a", true, true, true});
        Status status_value;
        const double threshold = ctrl.threshold(1, &status_value);
        double xp = 0;
        switch (i % 6) {
        case 0:
            xp = 0;
            break;
        case 1:
            xp = std::nextafter(threshold, 0.0);
            break;
        case 2:
            xp = threshold;
            break;
        case 3:
            xp = std::nextafter(threshold, max);
            break;
        case 4:
            xp = max;
            break;
        default:
            xp = denorm;
            break;
        }
        std::printf("xp_sweep %d", i);
        d(next_xp);
        d(multiplier);
        bo(configured);
        status(admitted);
        d(threshold);
        status(status_value);
        d(xp);
        progress(ctrl.award(1, xp));
        const auto snapshot = ctrl.snapshot();
        hex(snapshot);
        Controller restored(catalog);
        bo(restored.restore(snapshot));
        hex(restored.snapshot());
        std::putchar('\n');
    }
    Catalog catalog;
    Controller source(catalog);
    source.configure(1, true, 1.5);
    source.configure(2, false, 2.5);
    const auto valid = source.snapshot();
    for (std::size_t i = 0; i < valid.size(); ++i) {
        auto corrupted = valid;
        corrupted[i] ^= static_cast<std::uint8_t>(1u << (i % 8));
        Controller target(catalog);
        target.configure(99, true, 3);
        const auto before = target.snapshot();
        const bool ok = target.restore(corrupted);
        std::printf("xp_restore_mutation %zu %02x", i, corrupted[i]);
        bo(ok);
        hex(before);
        hex(target.snapshot());
        std::putchar('\n');
    }
}
static void sq_sweep() {
    using namespace squadrons;
    for (int i = 0; i < 128; ++i) {
        Definition def{"swarm",
                       {{0, "fighter", static_cast<std::uint32_t>(1 + i % 3)},
                        {1, "bomber", static_cast<std::uint32_t>(1 + (i / 3) % 2)}},
                       true};
        std::vector<Craft> members;
        std::uint64_t object = 1000 + i * 100;
        for (const auto &row : def.members)
            for (std::uint32_t j = 0; j < row.count; ++j)
                members.push_back({object++, static_cast<TeamId>(i % 4 + 1), row.odf});
        Registry registry;
        const auto team = static_cast<TeamId>(i % 4 + 1);
        const auto result = registry.create(def, team, members, ships);
        std::printf("sq_sweep %d %u %zu", i, team, members.size());
        sq_result(result);
        if (result) {
            const auto id = *result.value;
            const auto *pointer = registry.find(id);
            bo(pointer == registry.containing(members[0].id));
            const auto selection = registry.select({members.back(),
                                                    members.front(),
                                                    members.front(),
                                                    {object + 1, team, "fighter"}},
                                                   team, members.size() + 1);
            sq_selection(selection);
            const auto removed = registry.remove(members[i % members.size()].id);
            sq_removal(removed);
            if (!removed.squad_retired) {
                auto repair = sq_repair(registry, id, 9000 + i);
                if (i % 5 == 0)
                    repair.serviced_members.push_back(repair.serviced_members.front());
                const auto reservation = registry.reserve_replacement(id, repair);
                sq_replacement(reservation);
                if (reservation) {
                    if (i % 2 == 0) {
                        const auto cancellation = registry.cancel_yard(repair.yard);
                        std::printf(" C:%zu", cancellation.size());
                    } else {
                        auto replacement = members[i % members.size()];
                        bo(registry.complete_replacement(reservation.value->ticket,
                                                         replacement, repair));
                    }
                }
            }
            const auto saved = registry.snapshot();
            Registry restored;
            std::string error = "prior";
            bo(restored.restore(saved, ships, error));
            std::printf(" [%s]", error.c_str());
            sq_snapshot(restored);
            if (!saved.squads.empty()) {
                auto corrupted = saved;
                switch (i % 5) {
                case 0:
                    corrupted.next_squad = 0;
                    break;
                case 1:
                    corrupted.squads[0].id = 0;
                    break;
                case 2:
                    corrupted.squads[0].slots[0].odf = "wrong";
                    break;
                case 3:
                    corrupted.squads[0].slots[0].member = 0;
                    break;
                default:
                    corrupted.squads[0].definition.members[0].odf = "missing";
                    break;
                }
                bo(restored.restore(corrupted, ships, error));
                std::printf(" [%s]", error.c_str());
                sq_snapshot(restored);
            }
        }
        std::putchar('\n');
    }
}
static void run() {
    xp_run();
    sq_run();
    xp_sweep();
    sq_sweep();
}
int main() {
#if defined(_WIN32) && defined(__i386__)
    unsigned short saved;
    __asm__ __volatile__("fnstcw %0" : "=m"(saved));
    for (unsigned short precision :
         {(unsigned short)0x0000, (unsigned short)0x0200, (unsigned short)0x0300}) {
        unsigned short cw = (saved & ~0x0300) | precision;
        __asm__ __volatile__("fldcw %0" ::"m"(cw));
        std::printf("precision %04x\n", precision);
        run();
    }
    __asm__ __volatile__("fldcw %0" ::"m"(saved));
#else
    run();
#endif
}
