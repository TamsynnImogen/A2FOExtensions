// Synthetic Craft events exercise production resupply policy and recharge.
// No game binary, native hook, GUI or input device is used.
#include "../modules/A2FOEnergySystems/module.cpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

#ifdef NDEBUG
#error "This regression requires active assertions"
#endif

namespace {
template <class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

struct CraftFixture {
    std::array<std::uint8_t, 0x140> bytes{};
    CraftFixture(void* object_class, int team, std::array<float, 3> position) {
        put(bytes.data(), kObjectClassOffset, object_class);
        put(bytes.data(), kObjectTeamOffset, team);
        put(bytes.data(), kObjectPositionOffset, position);
    }
    void* get() { return bytes.data(); }
    void position(std::array<float, 3> value) {
        put(bytes.data(), kObjectPositionOffset, value);
    }
    void team(int value) { put(bytes.data(), kObjectTeamOffset, value); }
    void expired(bool value) {
        put(bytes.data(), kObjectExpiredOffset, static_cast<std::uint8_t>(value));
    }
};

void load(void* object_class, const char* source,
          std::initializer_list<std::pair<const char*, const char*>> values) {
    std::vector<A2FO_OdfFieldView> fields;
    fields.reserve(values.size());
    for (const auto& [name, value] : values) {
        fields.push_back({{name, static_cast<std::uint32_t>(std::strlen(name))},
                          {value, static_cast<std::uint32_t>(std::strlen(value))}});
    }
    const A2FO_GameObjectClassLoadedEvent event{
        sizeof(A2FO_GameObjectClassLoadedEvent), object_class,
        {source, static_cast<std::uint32_t>(std::strlen(source))},
        fields.data(), static_cast<std::uint32_t>(fields.size())};
    craft_class_loaded_handler(&event, nullptr);
}

void event(void* craft, std::uint32_t kind, float seconds = 1.0f) {
    const A2FO_CraftEvent value{sizeof(A2FO_CraftEvent), kind, craft, seconds};
    craft_event_handler(&value, nullptr);
}

void empty_stores(void* craft) {
    set_amount(craft, Ammunition::photon, 0.0f);
    set_amount(craft, Ammunition::quantum, 0.0f);
    set_amount(craft, Ammunition::shuttle_craft, 0.0f);
}

void expect_amounts(void* craft, float photon, float quantum, float shuttle) {
    assert(std::fabs(current_amount(craft, Ammunition::photon) - photon) < 0.001f);
    assert(std::fabs(current_amount(craft, Ammunition::quantum) - quantum) < 0.001f);
    assert(std::fabs(current_amount(craft, Ammunition::shuttle_craft) - shuttle) < 0.001f);
}

void expect_waiting(void* craft) {
    assert(remaining_reload_seconds(craft, Ammunition::photon) == -1.0f);
    assert(remaining_reload_seconds(craft, Ammunition::quantum) == -1.0f);
    assert(remaining_reload_seconds(craft, Ammunition::shuttle_craft) == -1.0f);
}

void expect_reloading(void* craft) {
    assert(remaining_reload_seconds(craft, Ammunition::photon) > 0.0f);
    assert(remaining_reload_seconds(craft, Ammunition::quantum) > 0.0f);
    assert(remaining_reload_seconds(craft, Ammunition::shuttle_craft) > 0.0f);
}

void load_weapon(void* weapon_class,
                 std::initializer_list<std::pair<const char*, const char*>> values) {
    std::vector<A2FO_OdfFieldView> fields;
    for (const auto& [name, value] : values) {
        fields.push_back({{name, static_cast<std::uint32_t>(std::strlen(name))},
                          {value, static_cast<std::uint32_t>(std::strlen(value))}});
    }
    const A2FO_WeaponClassLoadedEvent event{
        sizeof(A2FO_WeaponClassLoadedEvent), weapon_class, nullptr, nullptr,
        fields.data(), static_cast<std::uint32_t>(fields.size())};
    weapon_class_loaded_handler(&event, nullptr);
}

struct WeaponFixture {
    std::array<std::uint8_t, 0x40> bytes{};
    WeaponFixture(void* weapon_class, std::uint32_t owner) {
        put(bytes.data(), kWeaponClassOffset, weapon_class);
        put(bytes.data(), kWeaponOwnerHandleOffset, owner);
    }
    void* get() { return bytes.data(); }
};

struct Triggered {
    void* weapon;
    void* target;
};
std::vector<Triggered> g_triggered;

// Mirrors native Trigger: records the request and sets the active byte. The
// production precheck must accept it because the module is issuing it.
void record_trigger(void* weapon, void* target) {
    const A2FO_WeaponTriggerEvent precheck{
        sizeof(A2FO_WeaponTriggerEvent), A2FO_WEAPON_TRIGGER_PRECHECK,
        weapon, target};
    assert(resupply_weapon_trigger_handler(&precheck, nullptr));
    g_triggered.push_back({weapon, target});
    put(weapon, kWeaponActiveOffset, std::uint8_t{1});
}

bool native_precheck(void* weapon) {
    const A2FO_WeaponTriggerEvent precheck{
        sizeof(A2FO_WeaponTriggerEvent), A2FO_WEAPON_TRIGGER_PRECHECK,
        weapon, nullptr};
    return resupply_weapon_trigger_handler(&precheck, nullptr);
}

void test_resupply_weapons() {
    g_trigger_weapon = &record_trigger;
    int tender_class, pilot_class, frigate_class, beam_class, plain_class;
    load_weapon(&beam_class, {{"classLabel", "\"ResupplyWeapon\""},
                              {"resupplyValidTargets", "type:fighter"}});
    load_weapon(&plain_class, {{"classLabel", "cannon"}});
    assert(g_resupply_weapon_policies.count(&beam_class) == 1);
    assert(g_resupply_weapon_policies.count(&plain_class) == 0);

    load(&tender_class, "fed_tender.odf", {
        {"torpedoResupply", "1"}, {"torpedoResupplyRange", "300"},
        {"canResupplySelf", "1"},
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "1"},
        {"photonTorpedoRechargeMode", "2"}});
    load(&pilot_class, "fed_pilot.odf", {
        {"resupplyTargetType", "fighter"},
        {"maxPhotonTorpedoes", "4"}, {"photonTorpedoRate", "1"},
        {"photonTorpedoRechargeMode", "2"},
        {"maxShuttleCraft", "4"}, {"shuttleCraftRate", "1"},
        {"shuttleCraftRechargeMode", "1"}});
    load(&frigate_class, "fed_frigate.odf", {
        {"resupplyTargetType", "cruiser"},
        {"maxPhotonTorpedoes", "4"}, {"photonTorpedoRate", "1"},
        {"photonTorpedoRechargeMode", "2"}});

    constexpr std::uint32_t kTenderHandle = 77;
    CraftFixture tender(&tender_class, 4, {{50000, 0, 0}});
    put(tender.get(), kObjectHandleOffset, kTenderHandle);
    WeaponFixture beam(&beam_class, kTenderHandle);
    WeaponFixture plain(&plain_class, kTenderHandle);
    WeaponFixture foreign(&beam_class, kTenderHandle + 1);
    std::array<void*, 3> slots{{beam.get(), plain.get(), foreign.get()}};
    std::array<std::uint8_t, 0x20> carrier{};
    put(carrier.data(), kCarrierWeaponsBeginOffset, slots.data());
    put(carrier.data(), kCarrierWeaponsEndOffset, slots.data() + slots.size());
    put(tender.get(), kCraftCarrierOffset, static_cast<void*>(carrier.data()));

    CraftFixture pilot(&pilot_class, 4, {{50100, 0, 0}});
    CraftFixture wingmate(&pilot_class, 4, {{50150, 0, 0}});
    CraftFixture frigate(&frigate_class, 4, {{50100, 0, 0}});
    for (void* craft : {tender.get(), pilot.get(), wingmate.get(), frigate.get()}) {
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_PRE, 0.1f);
    }

    // Native Craft AI or player orders cannot start the host cannon.
    assert(!native_precheck(beam.get()));
    assert(native_precheck(plain.get()));

    // Everyone nearby is fully loaded: no activity, no shot.
    for (void* craft : {pilot.get(), wingmate.get(), frigate.get()}) {
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    }
    assert(g_triggered.empty());
    assert(!resupply_weapon_allows_shot(beam.get(), pilot.get()));
    // Icon state: grey with no valid receiver, -1 for ordinary weapons.
    assert(A2FOEnergySystems_GetResupplyWeaponState(beam.get()) == 0);
    assert(A2FOEnergySystems_GetResupplyWeaponState(plain.get()) == -1);

    // Self-resupply never aims the weapon at its own ship.
    set_amount(tender.get(), Ammunition::photon, 0);
    event(tender.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(current_amount(tender.get(), Ammunition::photon) > 0);
    assert(g_triggered.empty());

    // Automatic recharge is not a resupply.
    set_amount(pilot.get(), Ammunition::shuttle_craft, 0);
    event(pilot.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(g_triggered.empty());

    // A replenished fighter triggers only the owner's resupplyweapon.
    set_amount(pilot.get(), Ammunition::photon, 0);
    event(pilot.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(g_triggered.size() == 1);
    assert(g_triggered[0].weapon == beam.get());
    assert(g_triggered[0].target == pilot.get());
    assert(resupply_weapon_allows_shot(beam.get(), pilot.get()));
    assert(A2FOEnergySystems_GetResupplyWeaponState(beam.get()) == 1);
    assert(!resupply_weapon_allows_shot(beam.get(), frigate.get()));
    assert(!resupply_weapon_allows_shot(beam.get(), nullptr));
    assert(!resupply_weapon_allows_shot(foreign.get(), pilot.get()));
    assert(resupply_weapon_allows_shot(plain.get(), frigate.get()));

    // The weapon's own resupplyValidTargets excludes the cruiser.
    set_amount(frigate.get(), Ammunition::photon, 0);
    event(frigate.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(current_amount(frigate.get(), Ammunition::photon) > 0);
    assert(g_triggered.size() == 1);
    assert(!resupply_weapon_allows_shot(beam.get(), frigate.get()));

    // While the first receiver is still reloading, the weapon stays on it.
    set_amount(wingmate.get(), Ammunition::photon, 0);
    event(pilot.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    event(wingmate.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(g_triggered.size() == 1);

    // Once the first is full and its activity expires, it moves on.
    set_amount(pilot.get(), Ammunition::photon, 4);
    event(tender.get(), A2FO_CRAFT_EVENT_SIMULATE_PRE, 1.0f);
    assert(!resupply_weapon_allows_shot(beam.get(), pilot.get()));
    assert(A2FOEnergySystems_GetResupplyWeaponState(beam.get()) == 0);
    event(pilot.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(g_triggered.size() == 1);
    event(wingmate.get(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.1f);
    assert(g_triggered.size() == 2);
    assert(g_triggered[1].target == wingmate.get());
    assert(resupply_weapon_allows_shot(beam.get(), wingmate.get()));

    // Receiver destruction and owner cleanup remove every sidecar.
    event(wingmate.get(), A2FO_CRAFT_EVENT_CLEANUP);
    assert(!resupply_weapon_allows_shot(beam.get(), wingmate.get()));
    event(tender.get(), A2FO_CRAFT_EVENT_CLEANUP);
    assert(g_resupply_activity.count(tender.get()) == 0);
    assert(g_resupply_weapon_targets.empty());
    assert(A2FOEnergySystems_GetResupplyWeaponState(beam.get()) == 0);

    // Reloading the class as an ordinary weapon drops the policy.
    load_weapon(&beam_class, {{"classLabel", "cannon"}});
    assert(g_resupply_weapon_policies.empty());
    assert(native_precheck(beam.get()));
    g_trigger_weapon = nullptr;
}
}

int main() {
    g_runtime_ready = true;
    int carrier_class, fighter_class, cruiser_class, second_provider_class;
    int untyped_class, automatic_class, disabled_class, omitted_self_class;
    int malformed_self_class;

    load(&carrier_class, "Mods\\Stations\\Fed_Carrier.ODF", {
        {"torpedoResupply", "1"}, {"torpedoResupplyRange", "300"},
        {"resupplyValidTargets", "\"type:fighter\""},
        {"canResupplySelf", "0"},
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "2"},
        {"photonTorpedoRechargeMode", "2"},
        {"maxQuantumTorpedoes", "10"}, {"quantumTorpedoRate", "4"},
        {"quantumTorpedoRechargeMode", "2"},
        {"maxShuttleCraft", "10"}, {"shuttleCraftRate", "1"},
        {"shuttleCraftRechargeMode", "2"}});
    load(&fighter_class, "Mods/Ships/Fed_Fighter.ODF", {
        {"resupplyTargetType", "\"FiGhTeR\""},
        {"maxPhotonTorpedoes", "20"}, {"photonTorpedoRate", "2"},
        {"photonTorpedoRechargeMode", "2"},
        {"maxQuantumTorpedoes", "20"}, {"quantumTorpedoRate", "4"},
        {"quantumTorpedoRechargeMode", "2"},
        {"maxShuttleCraft", "20"}, {"shuttleCraftRate", "1"},
        {"shuttleCraftRechargeMode", "2"}});
    load(&cruiser_class, "Mods/Ships/Fed_Cruiser.odf", {
        {"resupplyTargetType", "cruiser"},
        {"maxPhotonTorpedoes", "20"}, {"photonTorpedoRate", "2"},
        {"photonTorpedoRechargeMode", "2"},
        {"maxQuantumTorpedoes", "20"}, {"quantumTorpedoRate", "4"},
        {"quantumTorpedoRechargeMode", "2"},
        {"maxShuttleCraft", "20"}, {"shuttleCraftRate", "1"},
        {"shuttleCraftRechargeMode", "2"}});
    CraftFixture carrier(&carrier_class, 1, {{0, 0, 0}});
    CraftFixture fighter(&fighter_class, 1, {{100, 0, 0}});
    CraftFixture cruiser(&cruiser_class, 1, {{100, 0, 0}});
    for (void* craft : {carrier.get(), fighter.get(), cruiser.get()}) {
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_PRE);
        empty_stores(craft);
    }
    assert(g_resupply_providers.count(carrier.get()) == 1);
    expect_reloading(fighter.get());
    expect_waiting(cruiser.get());
    expect_waiting(carrier.get());
    for (void* craft : {carrier.get(), fighter.get(), cruiser.get()}) {
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_POST);
    }
    expect_amounts(carrier.get(), 0, 0, 0);
    expect_amounts(fighter.get(), 2, 4, 1);  // Receiver's rates.
    expect_amounts(cruiser.get(), 0, 0, 0);

    // A rejecting provider never vetoes a second eligible provider.
    load(&second_provider_class, "fed_supply.odf", {{"torpedoResupply", "1"}});
    CraftFixture second(&second_provider_class, 1, {{0, 0, 0}});
    event(second.get(), A2FO_CRAFT_EVENT_SIMULATE_PRE);
    expect_reloading(carrier.get());  // Explicit self=false, external supply=true.
    expect_reloading(cruiser.get());
    event(carrier.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    event(cruiser.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    expect_amounts(carrier.get(), 2, 4, 1);
    expect_amounts(cruiser.get(), 2, 4, 1);
    event(second.get(), A2FO_CRAFT_EVENT_CLEANUP);
    assert(g_resupply_providers.count(second.get()) == 0);
    expect_waiting(cruiser.get());

    // Exact ODF rules override the shared type, with path/case/.odf normalized.
    load(&carrier_class, "fed_carrier.odf", {
        {"torpedoResupply", "1"}, {"torpedoResupplyRange", "300"},
        {"canResupplySelf", "0"},
        {"resupplyValidTargets", "type:fighter !odf:FED_FIGHTER.ODF odf:fed_cruiser"}});
    empty_stores(fighter.get());
    empty_stores(cruiser.get());
    expect_waiting(fighter.get());
    expect_reloading(cruiser.get());
    event(fighter.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    event(cruiser.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    expect_amounts(fighter.get(), 0, 0, 0);
    expect_amounts(cruiser.get(), 2, 4, 1);

    // Boundary, team and expired checks still gate an otherwise valid target.
    load(&carrier_class, "fed_carrier.odf", {
        {"torpedoResupply", "1"}, {"torpedoResupplyRange", "300"},
        {"canResupplySelf", "0"}, {"resupplyValidTargets", "type:fighter"}});
    fighter.position({{300, 0, 0}});
    expect_reloading(fighter.get());
    fighter.position({{301, 0, 0}});
    expect_waiting(fighter.get());
    fighter.position({{100, 0, 0}});
    fighter.team(2);
    expect_waiting(fighter.get());
    fighter.team(1);
    carrier.expired(true);
    expect_waiting(fighter.get());
    carrier.expired(false);
    expect_reloading(fighter.get());

    load(&untyped_class, "fed_untagged.odf", {
        {"maxPhotonTorpedoes", "5"}, {"photonTorpedoRate", "1"},
        {"photonTorpedoRechargeMode", "2"}});
    CraftFixture untyped(&untyped_class, 1, {{100, 0, 0}});
    event(untyped.get(), A2FO_CRAFT_EVENT_SIMULATE_PRE);
    empty_stores(untyped.get());
    assert(remaining_reload_seconds(untyped.get(), Ammunition::photon) == -1.0f);

    for (const char* targets : {"\"\"", "type:fighter !type:fighter"}) {
        load(&carrier_class, "fed_carrier.odf", {
            {"torpedoResupply", "1"}, {"resupplyValidTargets", targets}});
        expect_waiting(fighter.get());
    }

    // Omitted self field remains true, regardless of a restrictive target rule.
    load(&omitted_self_class, "fed_self.odf", {
        {"torpedoResupply", "1"}, {"resupplyValidTargets", "type:fighter"},
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "3"},
        {"photonTorpedoRechargeMode", "2"}});
    CraftFixture self(&omitted_self_class, 1, {{2000, 0, 0}});
    event(self.get(), A2FO_CRAFT_EVENT_SIMULATE_PRE);
    set_amount(self.get(), Ammunition::photon, 0);
    assert(g_craft_policies[&omitted_self_class].can_resupply_self);
    assert(remaining_reload_seconds(self.get(), Ammunition::photon) > 0);
    event(self.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(std::fabs(current_amount(self.get(), Ammunition::photon) - 3) < 0.001f);

    load(&malformed_self_class, "fed_bad_self.odf", {
        {"torpedoResupply", "1"}, {"canResupplySelf", "perhaps"},
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "3"},
        {"photonTorpedoRechargeMode", "2"}});
    CraftFixture malformed_self(&malformed_self_class, 1, {{3000, 0, 0}});
    event(malformed_self.get(), A2FO_CRAFT_EVENT_SIMULATE_PRE);
    set_amount(malformed_self.get(), Ammunition::photon, 0);
    assert(!g_craft_policies[&malformed_self_class].can_resupply_self);
    assert(remaining_reload_seconds(malformed_self.get(), Ammunition::photon) == -1);
    event(malformed_self.get(), A2FO_CRAFT_EVENT_SIMULATE_POST);
    assert(current_amount(malformed_self.get(), Ammunition::photon) == 0);

    // Automatic recharge ignores providers; mode zero remains disabled.
    load(&automatic_class, "fed_auto.odf", {
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "5"},
        {"photonTorpedoRechargeMode", "1"}});
    load(&disabled_class, "fed_disabled.odf", {
        {"maxPhotonTorpedoes", "10"}, {"photonTorpedoRate", "5"},
        {"photonTorpedoRechargeMode", "0"}});
    CraftFixture automatic(&automatic_class, 3, {{9000, 0, 0}});
    CraftFixture disabled(&disabled_class, 3, {{9000, 0, 0}});
    for (void* craft : {automatic.get(), disabled.get()}) {
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_PRE);
        set_amount(craft, Ammunition::photon, 0);
        event(craft, A2FO_CRAFT_EVENT_SIMULATE_POST);
    }
    assert(std::fabs(current_amount(automatic.get(), Ammunition::photon) - 5) < 0.001f);
    assert(current_amount(disabled.get(), Ammunition::photon) == 0);

    load(&carrier_class, "fed_carrier.odf", {
        {"torpedoResupply", "1"}, {"resupplyValidTargets", "type:fighter"},
        {"canResupplySelf", "0"}});
    expect_reloading(fighter.get());
    event(carrier.get(), A2FO_CRAFT_EVENT_CLEANUP);
    assert(g_resupply_providers.count(carrier.get()) == 0);
    expect_waiting(fighter.get());
    test_resupply_weapons();
    std::puts("resupply runtime: resupplyweapon activation, filters, retargeting and cleanup PASS");
    std::puts("resupply runtime: selective providers, ODF precedence, self, limits, reload and cleanup PASS");
}
