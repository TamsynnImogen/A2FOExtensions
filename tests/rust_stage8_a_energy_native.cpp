// Full frozen/current EnergySystems source, synthetic native image and callbacks.
#include <windows.h>
#include <float.h>
#include <stdexcept>
#include <string>
#include MODULE_SOURCE

#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>

std::vector<std::string> ordered_trace;
void* expected_weapon_class = nullptr;
void trace_line(const std::string& value) { ordered_trace.push_back(value); }
std::uint32_t float_bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

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
    trace_line(std::string("class:") + source + ":" +
        std::to_string(g_craft_policies.size()) + ":" +
        std::to_string(g_resupply_providers.size()));
}

void event(void* craft, std::uint32_t kind, float seconds = 1.0f) {
    const A2FO_CraftEvent value{sizeof(A2FO_CraftEvent), kind, craft, seconds};
    craft_event_handler(&value, nullptr);
    trace_line(std::string("event:") + std::to_string(kind) + ":" +
        std::to_string(float_bits(seconds)) + ":" +
        std::to_string(g_resupply_activity.size()) + ":" +
        std::to_string(g_resupply_weapon_targets.size()));
}

void empty_stores(void* craft) {
    set_amount(craft, Ammunition::photon, 0.0f);
    set_amount(craft, Ammunition::quantum, 0.0f);
    set_amount(craft, Ammunition::shuttle_craft, 0.0f);
}

void expect_amounts(void* craft, float photon, float quantum, float shuttle) {
    trace_line(std::string("amounts:") +
        std::to_string(float_bits(current_amount(craft, Ammunition::photon))) + ":" +
        std::to_string(float_bits(current_amount(craft, Ammunition::quantum))) + ":" +
        std::to_string(float_bits(current_amount(craft, Ammunition::shuttle_craft))));
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
    expected_weapon_class = weapon_class;
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
    trace_line(std::string("trigger:") + std::to_string(g_triggered.size()));
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

int run_runtime() {
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
    return 0;
}

namespace {
std::string fixture_mode;
std::uint8_t* synthetic_armada = nullptr;
std::uint8_t* synthetic_fleet = nullptr;
int hook_attempts = 0;
A2FO_GameObjectClassLoadedHandler registered_class = nullptr;
A2FO_WeaponClassLoadedHandler registered_weapon = nullptr;
A2FO_CraftEventHandler registered_craft = nullptr;
A2FO_WeaponTriggerHandler registered_trigger = nullptr;
void* mock_owner = nullptr;
std::uintptr_t mock_selector_result = 7;
std::uintptr_t mock_gateway_result = 1;
std::array<std::uint8_t, sizeof(SavedStores)> saved_bytes{};

void A2FO_CALL mock_log(const char*, const char* message) {
    std::string stable(message);
    const std::string marker = "WeaponClass ";
    const auto at = stable.find(marker);
    if (at != std::string::npos) {
        const auto start = at + marker.size();
        const auto end = stable.find_first_of(" \t\r\n", start);
        const std::string token = stable.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        char* parsed_end = nullptr;
        const auto observed = std::strtoull(token.c_str(), &parsed_end, 16);
        if (!expected_weapon_class || !parsed_end || *parsed_end ||
            observed != reinterpret_cast<std::uintptr_t>(
                expected_weapon_class)) {
            throw std::runtime_error("WeaponClass log identity mismatch");
        }
        stable.replace(start, end == std::string::npos
            ? std::string::npos : end - start, "<class>");
    }
    trace_line("log:" + stable);
}
void* A2FO_CALL mock_armada() {
    trace_line("module:armada");
    if (fixture_mode == "init-module-throw")
        throw std::runtime_error("armada callback");
    return fixture_mode == "init-no-armada" ? nullptr : synthetic_armada;
}
void* A2FO_CALL mock_fleet() {
    trace_line("module:fleet");
    return synthetic_fleet;
}
bool A2FO_CALL mock_class(const char*, const char* const*, std::uint32_t,
                          A2FO_GameObjectClassLoadedHandler handler, void*) {
    trace_line("register:craft-class");
    registered_class = handler;
    if (fixture_mode == "init-class-throw")
        throw std::runtime_error("class callback");
    return fixture_mode != "init-class-false";
}
bool A2FO_CALL mock_weapon(const char*, const char* const*, std::uint32_t,
                           A2FO_WeaponClassLoadedHandler handler, void*) {
    trace_line("register:weapon-class");
    registered_weapon = handler;
    if (fixture_mode == "init-weapon-throw")
        throw std::runtime_error("weapon callback");
    return fixture_mode != "init-weapon-false";
}
bool A2FO_CALL mock_craft(const char*, A2FO_CraftEventHandler handler,
                          void*) {
    trace_line("register:craft-event");
    registered_craft = handler;
    if (fixture_mode == "init-craft-throw")
        throw std::runtime_error("craft callback");
    return fixture_mode != "init-craft-false";
}
bool A2FO_CALL mock_trigger(const char*, A2FO_WeaponTriggerHandler handler,
                            void*) {
    trace_line("register:trigger-legacy");
    registered_trigger = handler;
    return fixture_mode != "init-trigger-false";
}
bool A2FO_CALL mock_trigger_masked(const char*, std::uint32_t mask,
                                   A2FO_WeaponTriggerHandler handler, void*) {
    trace_line("register:trigger-masked:" + std::to_string(mask));
    registered_trigger = handler;
    return fixture_mode != "init-trigger-false";
}
bool A2FO_CALL mock_alias(const char*, const char*, const char*) {
    trace_line("register:alias");
    return fixture_mode != "init-alias-false";
}
bool A2FO_CALL mock_hook(void* site, void*, std::size_t,
                         const std::uint8_t*, A2FO_InlineHook* output) {
    ++hook_attempts;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(site);
    const std::uintptr_t armada = reinterpret_cast<std::uintptr_t>(
        synthetic_armada);
    const std::uintptr_t base = address >= armada && address < armada + 0x400000
        ? armada : reinterpret_cast<std::uintptr_t>(synthetic_fleet);
    trace_line("hook:" + std::to_string(hook_attempts) + ":" +
               std::to_string(address - base));
    if (fixture_mode == "init-hook-throw" && hook_attempts == 2)
        throw std::runtime_error("hook callback");
    if (fixture_mode == "init-hook-false" && hook_attempts == 3)
        return false;
    if (fixture_mode == "init-persistence-false" && hook_attempts == 7)
        return false;
    output->gateway = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(0x11000 + hook_attempts));
    return true;
}
template<std::size_t N>
void seed(std::uint8_t* base, std::uintptr_t rva,
          const std::uint8_t (&signature)[N]) {
    std::memcpy(base + rva, signature, N);
}
void init_images() {
    synthetic_armada = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, 0x400000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_EXECUTE_READWRITE));
    synthetic_fleet = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, 0x200000, MEM_RESERVE | MEM_COMMIT,
                     PAGE_EXECUTE_READWRITE));
    if (!synthetic_armada || !synthetic_fleet)
        throw std::runtime_error("VirtualAlloc");
    seed(synthetic_armada, kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
    seed(synthetic_armada, kWeaponSelectTargetOrdnanceRva,
         kExpectedWeaponSelectTargetOrdnance);
    seed(synthetic_armada, kWeaponSelectPositionOrdnanceRva,
         kExpectedWeaponSelectPositionOrdnance);
    seed(synthetic_armada, kWeaponCommitShotRva, kExpectedWeaponCommitShot);
    seed(synthetic_armada, kWeaponLaunchPositionOrdnanceRva,
         kExpectedWeaponLaunchPositionOrdnance);
    seed(synthetic_fleet, kCannonImpLaunchTargetOrdnanceRva,
         kExpectedCannonImpLaunchTargetOrdnance);
    seed(synthetic_fleet, kCannonImpSelectTargetOrdnanceRva,
         kExpectedCannonImpSelectTargetOrdnance);
    seed(synthetic_armada, kCraftLoadRva, kExpectedCraftLoad);
    seed(synthetic_armada, kCraftSaveRva, kExpectedCraftSave);
    seed(synthetic_armada, kParameterDbGetRectangleRva,
         kExpectedParameterDbGetRectangle);
    seed(synthetic_armada, kParameterDbGetColorRva,
         kExpectedParameterDbGetColor);
    seed(synthetic_armada, kDisplayInterfaceDrawTextInRectangleRva,
         kExpectedDisplayInterfaceDrawTextInRectangle);
    if (fixture_mode == "init-signature")
        synthetic_armada[kWeaponGetOwnerRva] ^= 1;
    if (fixture_mode == "init-shot-signature")
        synthetic_armada[kWeaponCommitShotRva] ^= 1;
    if (fixture_mode == "init-persistence-signature")
        synthetic_armada[kCraftSaveRva] ^= 1;
}
A2FO_ModuleApi mock_api() {
    A2FO_ModuleApi api{};
    api.struct_size = fixture_mode == "init-short" ? 0 : sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
        A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS |
        A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.log = mock_log;
    api.armada_module = mock_armada;
    api.fleetops_module = mock_fleet;
    api.install_inline_hook = mock_hook;
    api.register_game_object_class_loaded_handler = mock_class;
    api.register_weapon_class_loaded_handler = mock_weapon;
    api.register_craft_event_handler = mock_craft;
    api.register_weapon_trigger_handler = mock_trigger;
    api.register_weapon_trigger_handler_masked = mock_trigger_masked;
    api.register_classlabel_alias = mock_alias;
    if (fixture_mode == "init-trigger-legacy")
        api.struct_size = offsetof(
            A2FO_ModuleApi, register_weapon_trigger_handler_masked);
    return api;
}
void run_init() {
    init_images();
    A2FO_ModuleApi api = mock_api();
    try {
        trace_line("result:" + std::to_string(A2FO_ModuleInit(&api)));
    } catch (const std::exception& error) {
        trace_line(std::string("exception:") + error.what());
    }
    trace_line("state:" + std::to_string(g_runtime_ready) + ":" +
        std::to_string(g_persistence_ready) + ":" +
        std::to_string(g_ui_ready) + ":" + std::to_string(hook_attempts) +
        ":" + std::to_string(registered_class != nullptr) +
        ":" + std::to_string(registered_weapon != nullptr) +
        ":" + std::to_string(registered_craft != nullptr) +
        ":" + std::to_string(registered_trigger != nullptr));
}
}

extern "C" std::uintptr_t __cdecl a2fo_energy_call_thiscall_0(
    void* function, void*) {
    trace_line("native:0:" + std::to_string(
        reinterpret_cast<std::uintptr_t>(function) < 0x20000));
    if (g_armada && function == at(kWeaponGetOwnerRva))
        return reinterpret_cast<std::uintptr_t>(mock_owner);
    return 1;
}
extern "C" std::uintptr_t __cdecl a2fo_energy_call_thiscall_1(
    void* function, void*, std::uintptr_t arg1) {
    trace_line("native:1:" + std::to_string(
        reinterpret_cast<std::uintptr_t>(function) < 0x20000) + ":" +
        std::to_string(arg1 != 0));
    if (function == g_weapon_select_target_ordnance_hook.gateway ||
        function == g_weapon_select_position_ordnance_hook.gateway ||
        function == g_cannon_imp_select_target_ordnance_hook.gateway)
        return mock_selector_result;
    if (function == g_craft_save_hook.gateway ||
        function == g_craft_load_hook.gateway)
        return mock_gateway_result;
    return 1;
}
extern "C" std::uintptr_t __cdecl a2fo_energy_call_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t) {
    trace_line("native:2");
    return 1;
}
extern "C" std::uintptr_t __cdecl a2fo_energy_call_thiscall_3(
    void* function, void*, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t) {
    if (g_armada && function == at(kParameterDbGetRectangleRva)) {
        const char* key = reinterpret_cast<const char*>(argument1);
        trace_line(std::string("rectangle:") + key);
        if (fixture_mode == "ui-auto") return 0;
        auto* output = reinterpret_cast<RawRectangle*>(argument2);
        *output = RawRectangle{0, 0, 100, 20};
        if (std::strcmp(key, "infoSingleCaptainTextArea") == 0) return 1;
        if (std::strcmp(key, "infoSinglePhotonTorpedoesTextArea") == 0) {
            output->y = 60;
            return 1;
        }
        if (std::strcmp(key, "infoSingleQuantumTorpedoesTextArea") == 0) {
            output->y = 90;
            return 1;
        }
        return 0;
    }
    if (g_armada && function == at(kParameterDbGetColorRva)) {
        const char* key = reinterpret_cast<const char*>(argument1);
        trace_line(std::string("colour:") + key);
        auto* output = reinterpret_cast<Colour*>(argument2);
        *output = Colour{0.25f, 0.5f, 0.75f};
        return fixture_mode != "ui-auto";
    }
    trace_line("native:3");
    return 1;
}
extern "C" std::uintptr_t __cdecl a2fo_energy_call_thiscall_7(
    void*, void*, std::uintptr_t text, std::uintptr_t rectangle, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) {
    const auto* box = reinterpret_cast<const NativeRectangle*>(rectangle);
    trace_line(std::string("draw:") + reinterpret_cast<const char*>(text) +
        ":" + std::to_string(box->top));
    return 1;
}


void run_shot() {
    init_images();
    g_armada = reinterpret_cast<HMODULE>(synthetic_armada);
    g_fleet_ops = reinterpret_cast<HMODULE>(synthetic_fleet);
    g_runtime_ready = fixture_mode != "shot-disabled";
    int craft_class = 0, weapon_class = 0;
    CraftFixture owner(&craft_class, 1, {{0, 0, 0}});
    CraftFixture target(&craft_class, 1, {{1, 0, 0}});
    mock_owner = owner.get();
    g_craft_stores[owner.get()] = Stores{5.0f, 0.0f, 0.0f};
    g_weapon_policies[&weapon_class] =
        WeaponPolicy{Ammunition::photon, 2.0f};
    WeaponFixture weapon(&weapon_class, 13);
    g_weapon_select_target_ordnance_hook.gateway =
        reinterpret_cast<void*>(0x11001);
    g_weapon_select_position_ordnance_hook.gateway =
        reinterpret_cast<void*>(0x11002);
    g_weapon_commit_shot_hook.gateway =
        reinterpret_cast<void*>(0x11003);
    g_weapon_launch_position_ordnance_hook.gateway =
        reinterpret_cast<void*>(0x11004);
    g_cannon_imp_select_target_ordnance_hook.gateway =
        reinterpret_cast<void*>(0x11005);
    g_cannon_imp_launch_target_ordnance_hook.gateway =
        reinterpret_cast<void*>(0x11006);
    if (fixture_mode == "shot-empty")
        g_craft_stores[owner.get()].photon = 0.0f;
    if (fixture_mode == "shot-native-zero" ||
        fixture_mode == "shot-disabled")
        mock_selector_result = 0;
    if (fixture_mode == "shot-resupply-allow" ||
        fixture_mode == "shot-resupply-deny" ||
        fixture_mode == "shot-resupply-position") {
        g_resupply_weapon_policies[&weapon_class] = {};
        g_resupply_weapon_targets[weapon.get()] =
            {owner.get(), target.get()};
        if (fixture_mode == "shot-resupply-allow")
            g_resupply_activity[owner.get()].push_back(
                {target.get(), 0.5f});
    }
    const bool cannon = fixture_mode == "shot-cannon" ||
                        fixture_mode == "shot-cannon-position";
    if (fixture_mode == "shot-commit-only") {
        weapon_commit_shot_hook(weapon.get(), nullptr);
    } else if (fixture_mode == "shot-cannon-position") {
        g_pending_cannon_imp_weapon = weapon.get();
        weapon_launch_position_ordnance_hook(weapon.get(), nullptr, 3);
    } else if (fixture_mode == "shot-resupply-position") {
        const auto result = weapon_select_position_ordnance_hook(
            weapon.get(), nullptr, 1, 2, 3);
        trace_line("result:" + std::to_string(result));
    } else if (cannon) {
        const auto result = cannon_imp_select_target_ordnance_hook(
            weapon.get(), nullptr,
            reinterpret_cast<std::uintptr_t>(target.get()));
        trace_line("result:" + std::to_string(result));
        cannon_imp_launch_target_ordnance_hook(
            weapon.get(), nullptr, result);
    } else {
        const auto result = weapon_select_target_ordnance_hook(
            weapon.get(), nullptr,
            reinterpret_cast<std::uintptr_t>(target.get()));
        trace_line("result:" + std::to_string(result));
        if (fixture_mode == "shot-allow" ||
            fixture_mode == "shot-resupply-allow" ||
            fixture_mode == "shot-native-zero")
            weapon_commit_shot_hook(weapon.get(), nullptr);
    }
    trace_line("shot-state:" +
        std::to_string(float_bits(g_craft_stores[owner.get()].photon)) + ":" +
        std::to_string(g_pending_ammunition_weapon == weapon.get()) + ":" +
        std::to_string(g_pending_cannon_imp_weapon == weapon.get()));
}


extern "C" bool __cdecl mock_file_out(
    void*, void* data, std::uint32_t size, const char* label) {
    trace_line(std::string("file-out:") + std::to_string(size) + ":" + label);
    std::memcpy(saved_bytes.data(), data, sizeof(SavedStores));
    return fixture_mode != "save-false";
}
extern "C" bool __cdecl mock_file_in(
    void*, void* data, std::uint32_t size) {
    trace_line("file-in:" + std::to_string(size));
    if (fixture_mode == "load-missing") return false;
    std::memcpy(data, saved_bytes.data(), sizeof(SavedStores));
    return true;
}
void install_jmp(std::uint8_t* from, void* to) {
    from[0] = 0xe9;
    const auto displacement = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(to) -
        reinterpret_cast<std::uintptr_t>(from + 5));
    std::memcpy(from + 1, &displacement, sizeof(displacement));
    FlushInstructionCache(GetCurrentProcess(), from, 5);
}
void run_persistence() {
    init_images();
    g_armada = reinterpret_cast<HMODULE>(synthetic_armada);
    A2FO_ModuleApi api = mock_api();
    g_api = &api;
    g_persistence_ready = fixture_mode != "save-disabled";
    g_craft_save_hook.gateway = reinterpret_cast<void*>(0x12001);
    g_craft_load_hook.gateway = reinterpret_cast<void*>(0x12002);
    install_jmp(synthetic_armada + kFileOutBytesRva,
                reinterpret_cast<void*>(&mock_file_out));
    install_jmp(synthetic_armada + kFileInBytesRva,
                reinterpret_cast<void*>(&mock_file_in));
    int craft_class = 0;
    CraftFixture craft(&craft_class, 1, {{0, 0, 0}});
    CraftPolicy policy{};
    policy.photon.maximum = 10.0f;
    policy.quantum.maximum = 10.0f;
    policy.shuttle_craft.maximum = 10.0f;
    g_craft_policies[&craft_class] = policy;
    g_craft_stores[craft.get()] = Stores{2.0f, 3.0f, 4.0f};
    if (fixture_mode.rfind("save-", 0) == 0) {
        const auto result = craft_save_hook(
            craft.get(), nullptr, reinterpret_cast<void*>(0x4400));
        trace_line("result:" + std::to_string(result));
    } else {
        if (fixture_mode == "load-native-false") mock_gateway_result = 0;
        if (fixture_mode == "load-legacy") {
            LegacySavedStores legacy{
                kLegacySaveMagic, kLegacySaveVersion, 7.0f, 8.0f};
            std::memcpy(saved_bytes.data(), &legacy, sizeof(legacy));
        } else {
            SavedStores saved{};
            saved.photon = 7.0f;
            saved.quantum = 8.0f;
            saved.shuttle_craft = 9.0f;
            if (fixture_mode == "load-invalid") saved.magic ^= 1;
            std::memcpy(saved_bytes.data(), &saved, sizeof(saved));
        }
        const auto result = craft_load_hook(
            craft.get(), nullptr, reinterpret_cast<void*>(0x5500));
        trace_line("result:" + std::to_string(result));
    }
    const Stores& stored = g_craft_stores.at(craft.get());
    trace_line("stores:" + std::to_string(float_bits(stored.photon)) + ":" +
        std::to_string(float_bits(stored.quantum)) + ":" +
        std::to_string(float_bits(stored.shuttle_craft)));
}


void run_ui() {
    init_images();
    g_armada = reinterpret_cast<HMODULE>(synthetic_armada);
    A2FO_ModuleApi api = mock_api();
    g_api = &api;
    g_runtime_ready = fixture_mode != "ui-disabled";
    g_ui_ready = true;
    int craft_class = 0;
    CraftFixture craft(&craft_class, 1, {{0, 0, 0}});
    CraftPolicy policy{};
    policy.photon.maximum = 10.0f;
    policy.photon.mode = RechargeMode::automatic;
    policy.quantum.maximum = 8.0f;
    policy.quantum.mode = RechargeMode::resupply_only;
    policy.shuttle_craft.maximum = 4.0f;
    policy.shuttle_craft.mode = RechargeMode::none;
    g_craft_policies[&craft_class] = policy;
    g_craft_stores[craft.get()] = Stores{4.0f, 3.0f, 2.0f};
    std::array<std::uint8_t, 0x200> info{};
    std::array<std::uint8_t, 0x100> text_component{};
    std::array<std::uint8_t, 0x20> parameter_db{};
    void* text_pointer = text_component.data();
    put(info.data(), kInfoDisplayCaptainTextOffset, text_pointer);
    NativeRectangle anchor{10, 20, 110, 40};
    put(text_component.data(), kTextComponentLiveRectangleOffset, anchor);
    void* display = reinterpret_cast<void*>(0x5000);
    put(text_component.data(), 0x04, display);
    Colour colour{1.0f, 1.0f, 1.0f};
    put(text_component.data(), 0x70, colour);
    void* db = fixture_mode == "ui-null-db" ? nullptr : parameter_db.data();
    put(synthetic_armada, kGuiParameterDbPointerRva, db);
    selected_info_render_handler(info.data(), craft.get(), nullptr);
    trace_line("ui-state:" + std::to_string(g_ui_configuration.loaded) + ":" +
        std::to_string(g_ui_configuration.anchor_found) + ":" +
        std::to_string(g_ui_configuration.photon_rectangle_found) + ":" +
        std::to_string(g_ui_configuration.quantum_rectangle_found) + ":" +
        std::to_string(g_ui_configuration.shuttle_craft_rectangle_found));
}


void run_parser() {
    g_runtime_ready = true;
    A2FO_ModuleApi api = mock_api();
    g_api = &api;
    int craft_class = 0, weapon_class = 0;
    std::vector<A2FO_OdfFieldView> fields;
    const auto add = [&](const char* name, const char* value) {
        fields.push_back({{name, static_cast<std::uint32_t>(std::strlen(name))},
            {value, static_cast<std::uint32_t>(std::strlen(value))}});
    };
    if (fixture_mode == "parse-duplicate") {
        add("MAXPHOTONTORPEDOES", "10f");
        add("maxPhotonTorpedoes", "20");
    } else if (fixture_mode == "parse-null-empty") {
        fields.push_back({{"maxPhotonTorpedoes", static_cast<std::uint32_t>(
            std::strlen("maxPhotonTorpedoes"))}, {nullptr, 0}});
    } else if (fixture_mode == "parse-invalid-view") {
        fields.push_back({{"maxPhotonTorpedoes", static_cast<std::uint32_t>(
            std::strlen("maxPhotonTorpedoes"))}, {nullptr, 1}});
        add("maxPhotonTorpedoes", "9");
    } else if (fixture_mode == "parse-suffix") {
        add("maxPhotonTorpedoes", " 1.25f \t");
        add("photonTorpedoRate", "2.5F");
    } else if (fixture_mode == "parse-mode-fraction") {
        add("maxPhotonTorpedoes", "10");
        add("photonTorpedoRechargeMode", "1.5");
    } else if (fixture_mode == "parse-bool-string") {
        add("torpedoResupply", "yes");
        add("canResupplySelf", "off");
    } else if (fixture_mode == "parse-auto-provider") {
        add("classLabel", "\"RePaIrShIp\"");
    } else {
        add("maxPhotonTorpedoes", "nan");
    }
    A2FO_GameObjectClassLoadedEvent craft_event{
        sizeof(A2FO_GameObjectClassLoadedEvent), &craft_class,
        {"Ships/Foo.odf", 13}, fields.data(),
        static_cast<std::uint32_t>(fields.size())};
    craft_class_loaded_handler(&craft_event, nullptr);
    const auto craft = g_craft_policies.find(&craft_class);
    trace_line("craft-policy:" +
        std::to_string(craft != g_craft_policies.end()));
    if (craft != g_craft_policies.end()) {
        const auto& policy = craft->second;
        trace_line("craft:" +
            std::to_string(float_bits(policy.photon.maximum)) + ":" +
            std::to_string(float_bits(policy.photon.recharge_per_second)) +
            ":" + std::to_string(static_cast<unsigned>(policy.photon.mode)) +
            ":" + std::to_string(policy.provider) + ":" +
            std::to_string(policy.can_resupply_self));
    }
    std::vector<A2FO_OdfFieldView> weapon_fields;
    weapon_fields.push_back({{"classLabel", static_cast<std::uint32_t>(
        std::strlen("classLabel"))}, {"ResupplyWeapon", static_cast<std::uint32_t>(
        std::strlen("ResupplyWeapon"))}});
    weapon_fields.push_back({{"photonTorpedoCost", static_cast<std::uint32_t>(
        std::strlen("photonTorpedoCost"))}, {"1.5f", 4}});
    A2FO_WeaponClassLoadedEvent weapon_event{
        sizeof(A2FO_WeaponClassLoadedEvent), &weapon_class,
        nullptr, nullptr, weapon_fields.data(),
        static_cast<std::uint32_t>(weapon_fields.size())};
    expected_weapon_class = &weapon_class;
    weapon_class_loaded_handler(&weapon_event, nullptr);
    trace_line("weapon:" +
        std::to_string(g_weapon_policies.count(&weapon_class)) + ":" +
        std::to_string(g_resupply_weapon_policies.count(&weapon_class)));
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    fixture_mode = argv[1];
    if (fixture_mode == "runtime" ||
        fixture_mode == "runtime-pc24" ||
        fixture_mode == "runtime-pc53" ||
        fixture_mode == "runtime-pc64") {
        if (fixture_mode == "runtime-pc24") _controlfp(_PC_24, _MCW_PC);
        if (fixture_mode == "runtime-pc53") _controlfp(_PC_53, _MCW_PC);
        if (fixture_mode == "runtime-pc64") _controlfp(_PC_64, _MCW_PC);
        run_runtime();
    } else if (fixture_mode.rfind("shot-", 0) == 0) {
        run_shot();
    } else if (fixture_mode.rfind("save-", 0) == 0 ||
               fixture_mode.rfind("load-", 0) == 0) {
        run_persistence();
    } else if (fixture_mode.rfind("ui-", 0) == 0) {
        run_ui();
    } else if (fixture_mode.rfind("parse-", 0) == 0) {
        run_parser();
    } else {
        run_init();
    }
    for (const auto& line : ordered_trace)
        std::printf("%s\n", line.c_str());
    return 0;
}
