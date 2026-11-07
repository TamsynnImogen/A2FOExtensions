// Full frozen/current directional-shield module fixture; no game process.
#include <windows.h>
#include <float.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include MODULE_SOURCE

namespace {
std::vector<std::string> trace;
std::uint8_t* image = nullptr;
std::array<std::uint8_t, 0x300> cls{}, craft{};
std::array<std::uint8_t, 0x100> attacker{}, weapon{}, ordnance{};
Matrix34 craft_transform{}, attacker_transform{};
std::string mode;
unsigned hook_count = 0;
unsigned hit_count = 0;

std::uint32_t bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
void A2FO_CALL logger(const char*, const char* line) {
    trace.emplace_back(std::string("log:") + line);
}
void* A2FO_CALL armada() { trace.emplace_back("armada"); return image; }
bool A2FO_CALL hook(void* target, void*, std::size_t,
                    const std::uint8_t*, A2FO_InlineHook* output) {
    ++hook_count;
    trace.emplace_back("hook:" + std::to_string(
        static_cast<std::uint8_t*>(target) - image));
    if (mode == "init-hook-throw" && hook_count == 1) {
        throw std::runtime_error("hook throw");
    }
    if (mode == "init-create-false" && hook_count == 1) return false;
    if (mode == "init-stop-false" && hook_count == 2) return false;
    if (mode == "init-update-false" && hook_count == 3) return false;
    output->gateway = mode == "init-stop-nogateway" && hook_count == 2
        ? nullptr : reinterpret_cast<void*>(1);
    return true;
}
bool A2FO_CALL register_class(const char*, const char* const*, std::uint32_t,
        A2FO_GameObjectClassLoadedHandler, void*) {
    trace.emplace_back("register-class");
    if (mode == "init-class-throw") {
        throw std::runtime_error("class throw");
    }
    return mode != "init-class-false";
}
bool A2FO_CALL register_craft(const char*, A2FO_CraftEventHandler, void*) {
    trace.emplace_back("register-craft");
    return mode != "init-craft-false";
}
template <std::size_t N>
void signature(std::uintptr_t rva, const std::array<std::uint8_t, N>& expected) {
    std::memcpy(image + rva, expected.data(), N);
}
void seed_signatures() {
    signature(kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
    signature(kCreateShieldHitRva, kExpectedCreateShieldHit);
    signature(kStopShieldEffectRva, kExpectedShieldEffectLookup);
    signature(kUpdateShieldEffectRva, kExpectedShieldEffectLookup);
}
void setup() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x360000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!image) throw std::runtime_error("VirtualAlloc");
    g_armada = reinterpret_cast<HMODULE>(image);
    static A2FO_ModuleApi api{};
    api.log = logger;
    g_api = &api;
    g_runtime_ready = true;
    g_damage_bridge_connected = true;
}
void dump_policy() {
    const auto found = g_class_policies.find(cls.data());
    trace.emplace_back("policy:" + std::to_string(found != g_class_policies.end()));
    if (found != g_class_policies.end()) {
        trace.emplace_back("mode:" + std::to_string(
            static_cast<std::uint32_t>(found->second.mode)));
        for (float value : found->second.maximum) {
            trace.emplace_back("max:" + std::to_string(bits(value)));
        }
    }
    float native = 0.0f;
    std::memcpy(&native, cls.data() + kMaximumShieldsOnCraftClassOffset,
                sizeof(native));
    trace.emplace_back("native:" + std::to_string(bits(native)));
}
std::int32_t __cdecl mock_hit(void* object, const void*, std::int32_t type,
                              float, std::int32_t) {
    ++hit_count;
    if (mode == "hit-gate-mutation") g_damage_bridge_connected = false;
    float presented = 0.0f;
    if (object == craft.data()) {
        std::memcpy(&presented, craft.data() + kCurrentShieldsOffset,
                    sizeof(presented));
    }
    trace.emplace_back("gateway:" + std::to_string(type) + ":" +
                       std::to_string(bits(presented)));
    return 7;
}
void __cdecl mock_update(std::int32_t id, const void*) {
    trace.emplace_back("update:" + std::to_string(id));
}
void __cdecl mock_stop(std::int32_t id) {
    trace.emplace_back("stop:" + std::to_string(id));
}
void hit_case(const std::string& which) {
    g_create_shield_hit_hook.gateway = reinterpret_cast<void*>(&mock_hit);
    ShieldPolicy policy{};
    policy.mode = Mode::quad;
    policy.maximum = {10.0f, 10.0f, 10.0f, 10.0f};
    g_class_policies[cls.data()] = policy;
    void* class_pointer = cls.data();
    std::memcpy(craft.data() + kObjectClassOffset, &class_pointer,
                sizeof(class_pointer));
    auto& state = g_craft_states[craft.data()];
    state.stores.current = {8.0f, 7.0f, 0.0f, 5.0f};
    state.pending_effect = true;
    state.pending_effect_facing = Facing::port;
    float current = 20.0f, maximum = 40.0f;
    std::memcpy(craft.data() + kCurrentShieldsOffset, &current, sizeof(current));
    std::memcpy(craft.data() + kMaximumShieldsOffset, &maximum, sizeof(maximum));
    if (which == "hit-present" || which == "hit-gate-mutation") {
        state.pending_effect_facing = Facing::forward;
    }
    if (which == "hit-pass") g_runtime_ready = false;
    const std::int32_t type = which == "hit-collapse" ? 1 : 0;
    const auto effect = create_shield_hit_hook(craft.data(), nullptr, type, 1.0f, 0);
    std::memcpy(&current, craft.data() + kCurrentShieldsOffset, sizeof(current));
    trace.emplace_back("effect:" + std::to_string(effect));
    trace.emplace_back("current:" + std::to_string(bits(current)));
    trace.emplace_back("pending:" + std::to_string(state.pending_effect));
    trace.emplace_back("hits:" + std::to_string(hit_count));
    trace.emplace_back("tracked:" + std::to_string(g_impact_effects.size()));
    trace.emplace_back("bridge:" + std::to_string(g_damage_bridge_connected));
}
void update_case(const std::string& which) {
    g_update_shield_effect_hook.gateway = reinterpret_cast<void*>(&mock_update);
    g_stop_shield_effect_hook.gateway = reinterpret_cast<void*>(&mock_stop);
    void* class_pointer = cls.data();
    std::memcpy(craft.data() + kObjectClassOffset, &class_pointer,
                sizeof(class_pointer));
    ShieldPolicy policy{};
    policy.mode = Mode::quad;
    policy.maximum = {10.0f, 10.0f, 10.0f, 10.0f};
    if (which != "update-empty") g_class_policies[cls.data()] = policy;
    auto& state = g_craft_states[craft.data()];
    state.stores.current = {8.0f, 7.0f, 0.0f, 5.0f};
    if (which == "update-active") state.stores.current[2] = 4.0f;
    g_impact_effects[7] = {craft.data(), Facing::port};
    if (which == "update-pass") g_runtime_ready = false;
    if (which == "stop") stop_shield_effect_hook(7);
    else update_shield_effect_hook(7, nullptr);
    trace.emplace_back("tracked:" + std::to_string(g_impact_effects.size()));
}
void damage_case(const std::string& which) {
    A2FO_DirectionalShieldDamageScope scope{};
    scope.struct_size = which == "begin-short" ? 0 : sizeof(scope);
    if (which == "begin-short") {
        trace.emplace_back("begin:" + std::to_string(begin_damage(
            reinterpret_cast<void*>(1), reinterpret_cast<void*>(1), &scope)));
        return;
    }
    void* class_pointer = cls.data();
    std::memcpy(craft.data() + kObjectClassOffset, &class_pointer,
                sizeof(class_pointer));
    ShieldPolicy policy{};
    policy.mode = Mode::quad;
    policy.maximum = {10.0f, 10.0f, 10.0f, 10.0f};
    g_class_policies[cls.data()] = policy;
    auto& state = g_craft_states[craft.data()];
    state.stores.current = {8.0f, 7.0f, 6.0f, 5.0f};
    float current = 26.0f;
    std::memcpy(craft.data() + kCurrentShieldsOffset, &current, sizeof(current));
    void* weapon_pointer = weapon.data();
    std::memcpy(ordnance.data() + kWeaponOnOrdnanceOffset, &weapon_pointer,
                sizeof(weapon_pointer));
    craft_transform.values[0] = 1.0f;
    craft_transform.values[8] = 1.0f;
    attacker_transform.values[11] = 10.0f;
    if (which == "end-invalid") {
        end_damage(&scope);
        trace.emplace_back("invalid-active:" + std::to_string(scope.active));
        return;
    }
    if (which == "begin-collapse") {
        const std::int32_t no_effect = -1;
        std::memcpy(craft.data() + kNativeShieldEffectIdOffset,
                    &no_effect, sizeof(no_effect));
        g_stop_shield_effect_hook.gateway = reinterpret_cast<void*>(&mock_stop);
        g_impact_effects[7] = {craft.data(), Facing::forward};
    }
    trace.emplace_back("begin:" + std::to_string(begin_damage(
        craft.data(), ordnance.data() + kDamageInfoOnOrdnanceOffset, &scope)));
    std::memcpy(&current, craft.data() + kCurrentShieldsOffset, sizeof(current));
    trace.emplace_back("presented:" + std::to_string(bits(current)));
    trace.emplace_back("active:" + std::to_string(scope.active));
    if (scope.active) {
        current = which == "begin-collapse" ? 0.0f : 3.0f;
        std::memcpy(craft.data() + kCurrentShieldsOffset, &current, sizeof(current));
        end_damage(&scope);
        std::memcpy(&current, craft.data() + kCurrentShieldsOffset, sizeof(current));
        trace.emplace_back("remaining:" + std::to_string(bits(
            state.stores.current[scope.facing])));
        trace.emplace_back("aggregate:" + std::to_string(bits(current)));
        trace.emplace_back("pending:" + std::to_string(state.pending_effect));
        trace.emplace_back("scopes:" + std::to_string(
            g_active_damage_effect_scope_count));
        trace.emplace_back("tracked:" + std::to_string(g_impact_effects.size()));
    }
}
void state_case(const std::string& which) {
    void* class_pointer = cls.data();
    std::memcpy(craft.data() + kObjectClassOffset, &class_pointer,
                sizeof(class_pointer));
    ShieldPolicy policy{};
    policy.mode = which == "state-inactive" ? Mode::tri_forward : Mode::quad;
    policy.maximum = {10.0f, 10.0f, 10.0f, 10.0f};
    g_class_policies[cls.data()] = policy;
    A2FO_DirectionalShieldState values{};
    values.struct_size = sizeof(values);
    values.maximum[0] = 10.0f;
    values.maximum[1] = 10.0f;
    values.maximum[2] = 10.0f;
    values.maximum[3] = 10.0f;
    values.current[0] = 5.0f;
    values.current[1] = 6.0f;
    values.current[2] = 7.0f;
    values.current[3] = 8.0f;
    if (which == "state-short") values.struct_size = 0;
    trace.emplace_back("set:" + std::to_string(set_editor_state(craft.data(), &values)));
    trace.emplace_back("overrides:" +
                       std::to_string(g_craft_policy_overrides.size()));
    float maximum = 0.0f, current = 0.0f;
    std::memcpy(&maximum, craft.data() + kMaximumShieldsOffset, sizeof(maximum));
    std::memcpy(&current, craft.data() + kCurrentShieldsOffset, sizeof(current));
    trace.emplace_back("max:" + std::to_string(bits(maximum)));
    trace.emplace_back("current:" + std::to_string(bits(current)));
    trace.emplace_back("enabled:" + std::to_string(
        A2FODirectionalShields_IsEnabled(craft.data())));
    trace.emplace_back("mode:" + std::to_string(
        A2FODirectionalShields_GetMode(craft.data())));
    trace.emplace_back("query-current:" + std::to_string(bits(
        A2FODirectionalShields_GetCurrent(craft.data(), 0))));
    trace.emplace_back("query-max:" + std::to_string(bits(
        A2FODirectionalShields_GetMaximum(craft.data(), 0))));
    A2FO_DirectionalShieldState output{};
    output.struct_size = sizeof(output);
    trace.emplace_back("get:" + std::to_string(
        A2FODirectionalShields_GetState(craft.data(), &output)));
    trace.emplace_back("get-current:" + std::to_string(bits(output.current[0])));
}
void class_case(const std::string& which) {
    const char source[] = "case.odf";
    const char a[] = "directionalShields";
    const char b[] = "forwardShieldStrength";
    const char c[] = "aftShieldStrength";
    const char d[] = "portShieldStrength";
    const char e[] = "starboardShieldStrength";
    const char f[] = "maxShields";
    const char one[] = "1", two[] = "2", yes[] = "YeS";
    const char forward[] = "1.25", aft[] = "1.0";
    const char port[] = "0", starboard[] = "2.5";
    const char native[] = "10", bad[] = "garbage";
    const char near_value[] = "1.0001", tiny[] = "0.00005";
    const char share_total[] = "1.02", quarter[] = "0.25";
    const char compare_total[] = "1.01000001";
    const A2FO_OdfFieldView valid[]{
        {{a,sizeof(a)-1},{two,sizeof(two)-1}},
        {{b,sizeof(b)-1},{forward,sizeof(forward)-1}},
        {{d,sizeof(d)-1},{port,sizeof(port)-1}},
        {{e,sizeof(e)-1},{starboard,sizeof(starboard)-1}},
        {{f,sizeof(f)-1},{native,sizeof(native)-1}},
    };
    const A2FO_OdfFieldView derived[]{
        {{a,sizeof(a)-1},{yes,sizeof(yes)-1}},
        {{b,sizeof(b)-1},{forward,sizeof(forward)-1}},
        {{c,sizeof(c)-1},{aft,sizeof(aft)-1}},
        {{d,sizeof(d)-1},{forward,sizeof(forward)-1}},
        {{e,sizeof(e)-1},{starboard,sizeof(starboard)-1}},
    };
    const A2FO_OdfFieldView invalid[]{
        {{a,sizeof(a)-1},{one,sizeof(one)-1}},
        {{b,sizeof(b)-1},{bad,sizeof(bad)-1}},
    };
    const A2FO_OdfFieldView boundary[]{
        {{a,sizeof(a)-1},{two,sizeof(two)-1}},
        {{b,sizeof(b)-1},{one,sizeof(one)-1}},
        {{e,sizeof(e)-1},{tiny,sizeof(tiny)-1}},
        {{f,sizeof(f)-1},{near_value,sizeof(near_value)-1}},
    };
    const A2FO_OdfFieldView share[]{
        {{a,sizeof(a)-1},{two,sizeof(two)-1}},
        {{b,sizeof(b)-1},{one,sizeof(one)-1}},
        {{f,sizeof(f)-1},{share_total,sizeof(share_total)-1}},
    };
    const A2FO_OdfFieldView compare[]{
        {{a,sizeof(a)-1},{one,sizeof(one)-1}},
        {{b,sizeof(b)-1},{quarter,sizeof(quarter)-1}},
        {{c,sizeof(c)-1},{quarter,sizeof(quarter)-1}},
        {{d,sizeof(d)-1},{quarter,sizeof(quarter)-1}},
        {{e,sizeof(e)-1},{quarter,sizeof(quarter)-1}},
        {{f,sizeof(f)-1},{compare_total,sizeof(compare_total)-1}},
    };
    const A2FO_OdfFieldView* fields = valid;
    std::uint32_t count = static_cast<std::uint32_t>(std::size(valid));
    if (which == "class-derived") {
        fields = derived; count = static_cast<std::uint32_t>(std::size(derived));
    } else if (which == "class-invalid") {
        fields = invalid; count = static_cast<std::uint32_t>(std::size(invalid));
    } else if (which.rfind("numeric-share", 0) == 0) {
        fields = share; count = static_cast<std::uint32_t>(std::size(share));
    } else if (which.rfind("numeric-compare", 0) == 0) {
        fields = compare; count = static_cast<std::uint32_t>(std::size(compare));
    } else if (which.rfind("numeric", 0) == 0) {
        fields = boundary; count = static_cast<std::uint32_t>(std::size(boundary));
    }
    A2FO_GameObjectClassLoadedEvent event{
        sizeof(event), cls.data(), {source, sizeof(source)-1}, fields, count};
    craft_class_loaded_handler(&event, nullptr);
    dump_policy();
}
void run(const std::string& which) {
    mode = which;
    setup();
    if (which.rfind("class-", 0) == 0 || which.rfind("numeric", 0) == 0) {
        const unsigned int original = _controlfp(0, 0);
        if (which.size() >= 2 && which.substr(which.size()-2) == "24") {
            _controlfp(_PC_24, _MCW_PC);
        }
        if (which.size() >= 2 && which.substr(which.size()-2) == "53") {
            _controlfp(_PC_53, _MCW_PC);
        }
        if (which.size() >= 2 && which.substr(which.size()-2) == "64") {
            _controlfp(_PC_64, _MCW_PC);
        }
        class_case(which);
        _controlfp(original, _MCW_PC);
    } else if (which == "disabled") {
        g_runtime_ready = false;
        craft_class_loaded_handler(
            reinterpret_cast<const A2FO_GameObjectClassLoadedEvent*>(1), nullptr);
        trace.emplace_back("disabled-done");
    } else if (which == "short") {
        A2FO_GameObjectClassLoadedEvent event{};
        event.struct_size = 0;
        event.object_class = reinterpret_cast<void*>(1);
        craft_class_loaded_handler(&event, nullptr);
        trace.emplace_back("short-done");
    } else if (which.rfind("init-", 0) == 0) {
        g_runtime_ready = false;
        seed_signatures();
        if (which == "init-signature") image[kWeaponGetOwnerRva] = 0;
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api);
        api.api_version = A2FO_MODULE_API_VERSION;
        api.log = logger;
        api.armada_module = armada;
        api.install_inline_hook = hook;
        api.register_game_object_class_loaded_handler = register_class;
        api.register_craft_event_handler = register_craft;
        api.capabilities = A2FO_CAP_GAME_OBJECT_CLASS_LOADED | A2FO_CAP_CRAFT_EVENTS;
        try {
            trace.emplace_back("init:" + std::to_string(A2FO_ModuleInit(&api)));
        } catch (const std::exception& error) {
            trace.emplace_back(std::string("init-throw:") + error.what());
        }
        trace.emplace_back("ready:" + std::to_string(g_runtime_ready));
    } else if (which.rfind("hit-", 0) == 0) {
        hit_case(which);
    } else if (which.rfind("update-", 0) == 0 || which == "stop") {
        update_case(which);
    } else if (which.rfind("begin-", 0) == 0 || which == "end-invalid") {
        damage_case(which);
    } else if (which.rfind("state-", 0) == 0) {
        state_case(which);
    } else if (which == "bridge") {
        g_damage_bridge_connected = false;
        trace.emplace_back("connect1:" + std::to_string(
            A2FODirectionalShields_ConnectDamageBridge()));
        trace.emplace_back("connect2:" + std::to_string(
            A2FODirectionalShields_ConnectDamageBridge()));
        g_runtime_ready = false;
        trace.emplace_back("disabled:" + std::to_string(
            A2FODirectionalShields_ConnectDamageBridge()));
    } else if (which == "craft-cleanup" || which == "craft-postload" ||
               which == "craft-simulate") {
        void* class_pointer = cls.data();
        std::memcpy(craft.data() + kObjectClassOffset, &class_pointer,
                    sizeof(class_pointer));
        ShieldPolicy policy{};
        policy.mode = Mode::quad;
        policy.maximum = {10.0f, 10.0f, 10.0f, 10.0f};
        g_class_policies[cls.data()] = policy;
        g_craft_states[craft.data()] = {};
        if (which != "craft-simulate") {
            g_craft_policy_overrides[craft.data()] = {};
        }
        A2FO_DirectionalShieldDamageScope target_scope{}, other_scope{};
        if (which == "craft-cleanup") {
            g_impact_effects[7] = {craft.data(), Facing::forward};
            g_impact_effects[8] = {attacker.data(), Facing::aft};
            push_active_damage_effect_scope(
                &target_scope, craft.data(), Facing::forward);
            push_active_damage_effect_scope(
                &other_scope, attacker.data(), Facing::aft);
        }
        float current = 20.0f;
        std::memcpy(craft.data() + kCurrentShieldsOffset, &current,
                    sizeof(current));
        const std::uint32_t kind = which == "craft-cleanup"
            ? A2FO_CRAFT_EVENT_CLEANUP :
            which == "craft-postload" ? A2FO_CRAFT_EVENT_POST_LOAD :
            A2FO_CRAFT_EVENT_SIMULATE_PRE;
        A2FO_CraftEvent event{sizeof(event), kind,
                              craft.data(), 0.0f};
        craft_event_handler(&event, nullptr);
        trace.emplace_back("states:" + std::to_string(g_craft_states.size()));
        trace.emplace_back("overrides:" + std::to_string(g_craft_policy_overrides.size()));
        trace.emplace_back("tracked:" + std::to_string(g_impact_effects.size()));
        trace.emplace_back("scopes:" + std::to_string(
            g_active_damage_effect_scope_count));
        std::memcpy(&current, craft.data() + kCurrentShieldsOffset,
                    sizeof(current));
        trace.emplace_back("current:" + std::to_string(bits(current)));
    } else {
        throw std::runtime_error("unknown mode");
    }
    for (const auto& item : trace) std::puts(item.c_str());
}
} // namespace

extern "C" std::uintptr_t __cdecl a2fo_directional_shields_call_thiscall_0(
    void* function, void* self) {
    if (function == image + kWeaponGetOwnerRva && self == weapon.data()) {
        trace.emplace_back("owner");
        return reinterpret_cast<std::uintptr_t>(attacker.data());
    }
    if (function == image + kEntityGetTransformRva) {
        trace.emplace_back(self == craft.data() ? "target-transform" :
                           "attacker-transform");
        return reinterpret_cast<std::uintptr_t>(
            self == craft.data() ? &craft_transform : &attacker_transform);
    }
    return 0;
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "directional fixture: %s\n", error.what());
        return 1;
    }
}
