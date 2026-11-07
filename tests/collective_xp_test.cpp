#include "../modules/A2FOCollectiveXP/collective_xp.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace a2fo::collective_xp;

namespace {
void check(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "collective_xp_test: %s\n", message);
        std::exit(1);
    }
}

Catalog ranks() {
    Catalog catalog;
    check(catalog.add({"base", 1, 10, "veteran"}), "add base");
    check(catalog.add({"veteran", 2, 20, "elite"}), "add veteran");
    check(catalog.add({"elite", 3, 0, ""}), "add elite");
    return catalog;
}

void catalog_contract() {
    auto catalog = ranks();
    check(!catalog.add({"base", 1, 10, "veteran"}), "reject duplicate");
    check(!catalog.add({"", 1, 0, ""}), "reject empty id");
    check(!catalog.add({"zero", 0, 0, ""}), "reject zero level");
    check(!catalog.add({"bad", 1, 0, "later"}), "reject missing next XP");
    check(!catalog.add({"self", 1, 1, "self"}), "reject self link");
    const auto resolved = catalog.resolve("base", 2, "elite");
    check(resolved.status == Status::ok && resolved.class_id == "veteran" &&
              resolved.level == 2 && resolved.has_next && resolved.next_xp == 20,
          "resolve explicit family and target level");
    check(catalog.resolve("base", 64).class_id == "elite", "clamp beyond terminal rank");
    check(catalog.resolve("base", 2, "outsider").status == Status::current_not_in_family,
          "reject current outside family");
    check(catalog.resolve("missing", 1).status == Status::missing_class,
          "report missing base");
    check(catalog.resolve("base", 0).status == Status::invalid_input,
          "reject invalid target level");
    check(catalog.native_level("veteran") == 2 && catalog.native_level("missing") == 0,
          "native level lookup");

    Catalog broken;
    check(broken.add({"start", 1, 10, "absent"}), "add broken chain start");
    check(broken.resolve("start", 1).status == Status::missing_class,
          "validate whole chain before resolving");
    check(broken.add({"absent", 3, 0, ""}), "add wrong-level link");
    check(broken.resolve("start", 1).status == Status::malformed_chain,
          "reject skipped native rank");
    Catalog cyclic;
    check(cyclic.add({"first", 1, 10, "second"}) &&
              cyclic.add({"second", 2, 10, "first"}), "add cross-link cycle");
    check(cyclic.resolve("first", 1).status == Status::cycle, "reject cycle");
}

void controller_contract() {
    const auto catalog = ranks();
    Controller controller(catalog);
    check(controller.configure(1, true, 2), "configure first owner");
    check(controller.configure(2, true, 1), "configure second owner");
    check(!controller.configure(1, true, 0) &&
              !controller.configure(1, true, std::numeric_limits<double>::infinity()),
          "reject invalid modifiers");
    check(controller.state(1)->multiplier == 2, "failed configuration leaves state intact");
    check(controller.upsert({11, 1, "base", "base", true, true, true}) == Status::ok &&
              controller.upsert({12, 1, "base", "base", true, true, true}) == Status::ok,
          "admit live craft");
    check(controller.upsert({0, 1, "base", "base", true, true, true}) == Status::invalid_input,
          "reject invalid identity");
    check(controller.upsert({11, 1, "base", "outsider", true, true, true}) ==
              Status::current_not_in_family, "reject invalid replacement atomically");
    check(controller.threshold(1) == 40 && controller.target(11).class_id == "base",
          "two craft and modifier set owner threshold");
    const auto almost = controller.award(1, 39);
    check(almost.status == Status::ok && almost.consumed && almost.levels_gained == 0 &&
              almost.state.pool == 39 && almost.threshold == 40, "award remains unscaled");
    const auto promoted = controller.award(1, 1);
    check(promoted.status == Status::ok && promoted.levels_gained == 1 &&
              promoted.state.level == 2 && promoted.state.pool == 0 &&
              promoted.threshold == 80, "promote at summed threshold");
    check(controller.target(11).class_id == "veteran", "target follows collective level");
    check(controller.transfer(12, 2) == Status::ok, "transfer craft");
    check(controller.threshold(1) == 40 && controller.threshold(2) == 10,
          "transfer invalidates both owner thresholds");
    check(controller.target(12).class_id == "base", "new owner target is independent");
    check(controller.upsert({12, 2, "base", "elite", true, true, true}) == Status::ok,
          "observe captured high-rank craft");
    check(controller.target(12).class_id == "elite" && controller.threshold(2) == 0,
          "capture never demotes and terminal rank adds no threshold");
    check(controller.remove(11) && !controller.remove(11), "remove membership once");
    check(controller.threshold(1) == 0, "fleet loss invalidates threshold");
    const auto held = controller.award(1, 7);
    check(held.status == Status::ok && held.state.level == 2 && held.state.pool == 7 &&
              held.threshold == 0, "pool retained without eligible next rank");
    check(controller.transfer(999, 2) == Status::unknown_unit &&
              controller.target(999).status == Status::unknown_unit, "unknown unit reporting");
    check(controller.configure(2, false, 1), "disable owner");
    const auto stock = controller.award(2, 50);
    check(stock.status == Status::ok && !stock.consumed && stock.state.pool == 0,
          "disabled owner stays on stock path");
    const auto invalid = controller.award(1, -1);
    check(invalid.status == Status::invalid_input && controller.state(1)->pool == 7,
          "invalid award leaves pool untouched");
    controller.reset_player(2);
    check(controller.state(2) == nullptr && controller.target(12).status == Status::unknown_unit,
          "reset removes owner and its craft");
}

void snapshot_contract() {
    const auto catalog = ranks();
    Controller source(catalog);
    check(source.configure(1, true, 1) && source.configure(2, true, 2),
          "configure snapshot owners");
    check(source.upsert({21, 1, "base", "base", true, true, true}) == Status::ok,
          "admit snapshot craft");
    check(source.award(1, 5).status == Status::ok, "set persisted pool");
    const auto saved = source.snapshot();
    const auto one = source.snapshot_player(1);
    check(saved.size() == 58 && one.size() == 33, "documented v1 record sizes");

    Controller restored(catalog);
    check(restored.configure(3, true, 1), "configure existing unrelated owner");
    check(restored.restore(saved), "restore valid full snapshot");
    check(restored.state(1) && restored.state(1)->pool == 5 &&
              restored.state(2) && restored.state(2)->multiplier == 2 &&
              restored.state(3) == nullptr, "full restore replaces player states");
    check(restored.target(21).status == Status::unknown_unit,
          "snapshot does not recreate live craft");
    const auto baseline = restored.snapshot();
    auto bad_magic = saved;
    bad_magic[0] = 'X';
    auto bad_enabled = saved;
    bad_enabled[12] = 2; // First record's enabled byte in documented v1 layout.
    auto duplicate = saved;
    for (unsigned i = 0; i < 4; ++i) duplicate[33 + i] = duplicate[8 + i];
    const std::vector<std::vector<std::uint8_t>> bad = {
        {}, std::vector<std::uint8_t>(saved.begin(), saved.end() - 1),
        bad_magic, bad_enabled, duplicate};
    for (const auto& bytes : bad) {
        check(!restored.restore(bytes), "reject invalid or truncated full snapshot");
        check(restored.snapshot() == baseline, "failed full restore is transactional");
    }
    check(!restored.restore_player(2, one) && restored.snapshot() == baseline,
          "reject mismatched per-player record without mutation");
    check(!restored.restore_player(1, std::vector<std::uint8_t>(one.begin(), one.end() - 1)) &&
              restored.snapshot() == baseline, "reject truncated per-player record");

    check(restored.configure(1, true, 3), "change one owner");
    check(restored.restore_player(1, one), "restore matching owner");
    check(restored.state(1)->pool == 5 && restored.state(1)->multiplier == 1 &&
              restored.state(2)->multiplier == 2, "per-player restore leaves other owners alone");
}
} // namespace

int main() {
    catalog_contract();
    controller_contract();
    snapshot_contract();
    std::puts("collective_xp_test: ok");
}
