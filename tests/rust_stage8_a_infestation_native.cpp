// Full frozen/current Infestation module source with synthetic native objects.
#include <windows.h>
#include <float.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include MODULE_SOURCE

std::atomic<bool> fail_next_native_new{false};
void (*volatile native_free_indirect)(void*) = &std::free;
void* operator new(std::size_t size) {
    if (fail_next_native_new.exchange(false)) throw std::bad_alloc();
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { native_free_indirect(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
    native_free_indirect(pointer);
}

namespace {
std::vector<std::string> trace;
std::uint8_t* image = nullptr;
std::array<std::uint8_t, 0x380> target{}, attacker{}, mother_one{}, mother_two{},
    yard{}, larva_one{}, larva_two{};
std::array<std::uint8_t, 0x80> weapon{}, weapon_class{}, mother_class{},
    larva_class{}, target_class{}, command_carrier{}, repair_queue{},
    explosion_class{}, mission{};
std::array<void*, 7> mission_vtable{};
std::array<void*, 3> explosion_vtable{};
std::array<std::uint8_t, 12> object_list{}, list_head{}, node_one{}, node_two{};
std::unordered_map<std::uint32_t, void*> entities;
std::unordered_map<std::string, void*> classes;
std::unordered_map<void*, Matrix34> transforms;
std::string init_mode;
bool reenter_cleanup = false;
unsigned constructed = 0;

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL logger(const char*, const char* line) {
    trace.emplace_back(std::string("log:") + line);
}
void put_u32(void* base, std::size_t offset, std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}
void put_ptr(void* base, std::size_t offset, void* value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}
std::uint32_t bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
void position(void* object, float x, float y, float z) {
    const float value[]{x, y, z};
    std::memcpy(static_cast<std::uint8_t*>(object) + kObjectPositionOffset,
                value, sizeof(value));
    Matrix34 transform{};
    transform.values[0] = transform.values[4] = transform.values[8] = 1.0f;
    transform.values[9] = x;
    transform.values[10] = y;
    transform.values[11] = z;
    transforms[object] = transform;
}
std::string name(void* object) {
    if (object == target.data()) return "target";
    if (object == attacker.data()) return "attacker";
    if (object == mother_one.data()) return "mother1";
    if (object == mother_two.data()) return "mother2";
    if (object == yard.data()) return "yard";
    if (object == larva_one.data()) return "larva1";
    if (object == larva_two.data()) return "larva2";
    return object ? "other" : "null";
}
extern "C" void* __cdecl mock_entity(std::uint32_t handle) {
    trace.emplace_back("entity:" + std::to_string(handle));
    const auto found = entities.find(handle);
    return found == entities.end() ? nullptr : found->second;
}
extern "C" void* __cdecl mock_craft(std::uint32_t handle) {
    trace.emplace_back("craft:" + std::to_string(handle));
    const auto found = entities.find(handle);
    return found == entities.end() ? nullptr : found->second;
}
extern "C" void* __cdecl mock_class(const char* key) {
    const std::string wanted = key ? key : "";
    trace.emplace_back("class:" + wanted);
    const auto found = classes.find(wanted);
    return found == classes.end() ? nullptr : found->second;
}
extern "C" void* __cdecl mock_mission() {
    trace.emplace_back("mission");
    return mission.data();
}
extern "C" void* __cdecl mock_explosion_class(const std::uint32_t* project) {
    trace.emplace_back("explosion-class:" + std::to_string(*project));
    return explosion_class.data();
}
extern "C" void mock_mission_add() {}
extern "C" void mock_explosion_build() {}
void jump(std::uintptr_t rva, void* target_address) {
    const std::uint8_t code[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + rva, code, sizeof(code));
    std::memcpy(image + rva + 1, &target_address, sizeof(target_address));
}
void seed() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    require(image != nullptr, "synthetic image");
    g_armada = reinterpret_cast<HMODULE>(image);
    static A2FO_ModuleApi api{};
    api.log = logger;
    g_api = &api;
    g_runtime_ready = true;
    g_explosions_ready = true;
    jump(kEntityGetRva, reinterpret_cast<void*>(&mock_entity));
    jump(kCraftGetRva, reinterpret_cast<void*>(&mock_craft));
    jump(kGameObjectClassFindRva, reinterpret_cast<void*>(&mock_class));
    jump(kAiMissionGetCurrentRva, reinterpret_cast<void*>(&mock_mission));
    jump(kExplosionClassFindRva, reinterpret_cast<void*>(&mock_explosion_class));
    const std::array<void*, 7> objects{
        target.data(), attacker.data(), mother_one.data(), mother_two.data(),
        yard.data(), larva_one.data(), larva_two.data()};
    const std::array<std::uint32_t, 7> handles{10, 20, 40, 39, 50, 30, 31};
    for (std::size_t index = 0; index < objects.size(); ++index) {
        put_u32(objects[index], kObjectHandleOffset, handles[index]);
        entities[handles[index]] = objects[index];
    }
    put_u32(target.data(), kObjectTeamOffset, 2);
    put_u32(attacker.data(), kObjectTeamOffset, 1);
    put_u32(mother_one.data(), kObjectTeamOffset, 1);
    put_u32(mother_two.data(), kObjectTeamOffset, 1);
    put_u32(yard.data(), kObjectTeamOffset, 2);
    put_u32(larva_one.data(), kObjectTeamOffset, 1);
    put_u32(larva_two.data(), kObjectTeamOffset, 1);
    put_ptr(target.data(), kObjectClassOffset, target_class.data());
    put_ptr(mother_one.data(), kObjectClassOffset, mother_class.data());
    put_ptr(mother_two.data(), kObjectClassOffset, mother_class.data());
    put_ptr(weapon.data(), kWeaponClassOffset, weapon_class.data());
    position(target.data(), 0.0f, 0.0f, 0.0f);
    position(attacker.data(), 0.0f, 0.0f, 100.0f);
    position(mother_one.data(), 0.0f, 0.0f, 100.0f);
    position(mother_two.data(), 0.0f, 0.0f, -100.0f);
    position(larva_one.data(), 0.0f, 0.0f, 200.0f);
    position(larva_two.data(), 0.0f, 0.0f, 200.0f);
    classes["larva"] = larva_class.data();
    g_mother_classes[mother_class.data()] = true;
    put_ptr(image + kGameObjectListPointerRva, 0, object_list.data());
    put_ptr(object_list.data(), 4, list_head.data());
    put_u32(object_list.data(), 8, 2);
    put_ptr(list_head.data(), 0, node_one.data());
    put_ptr(node_one.data(), 0, node_two.data());
    put_ptr(node_one.data(), 8, mother_one.data());
    put_ptr(node_two.data(), 0, list_head.data());
    put_ptr(node_two.data(), 8, mother_two.data());
    mission_vtable[6] = reinterpret_cast<void*>(&mock_mission_add);
    put_ptr(mission.data(), 0, mission_vtable.data());
    explosion_vtable[2] = reinterpret_cast<void*>(&mock_explosion_build);
    put_ptr(explosion_class.data(), 0, explosion_vtable.data());
    put_ptr(repair_queue.data(), 0, image + kRepairQueueVtableRva);
    put_ptr(yard.data(), kYardRepairQueueOffset, repair_queue.data());
    put_ptr(target.data(), kObjectCommandCarrierOffset, command_carrier.data());
}
WeaponPolicy policy() {
    WeaponPolicy value{};
    value.enabled = true;
    value.delay_seconds = 1.5f;
    value.damage = 3.0f;
    value.larva_odf = "larva";
    value.larva_count = 2;
    value.explosion_project_id = 7;
    value.bypass_shields = true;
    value.spawn_radius = 20.0f;
    return value;
}
void infection_state(const char* label) {
    const auto found = g_infections.find(10);
    trace.emplace_back(std::string(label) + ":" +
        (found == g_infections.end() ? "absent" :
         "remaining=" + std::to_string(bits(found->second.remaining_seconds))) +
        ":routes=" + std::to_string(g_larva_routes.size()));
}
template<std::size_t N>
void signature(std::uintptr_t rva, const std::uint8_t (&expected)[N]) {
    std::memcpy(image + rva, expected, N);
}
void seed_signatures() {
    signature(kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
    signature(kCraftGetRva, kExpectedCraftGet);
    signature(kExplosionClassFindRva, kExpectedExplosionClassFind);
    signature(kCraftDamageRva, kExpectedCraftDamage);
    signature(kGameObjectClassFindRva, kExpectedGameObjectClassFind);
    signature(kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
    signature(kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(kEntityGetRva, kExpectedEntityGet);
    signature(kParameterDbGetProjectIdRva, kExpectedParameterDbGetProjectId);
    signature(kDequeueCommandVectorRva, kExpectedDequeueCommandVector);
    signature(kWeaponGetOwnerRva, kExpectedWeaponGetOwner);
}
void* A2FO_CALL mock_armada() { trace.emplace_back("armada"); return image; }
bool A2FO_CALL mock_register_class(const char*, const char* const*,
        std::uint32_t, A2FO_GameObjectClassLoadedHandler, void*) {
    trace.emplace_back("register-class");
    return init_mode != "init-class-false";
}
bool A2FO_CALL mock_register_weapon(const char*, const char* const*,
        std::uint32_t, A2FO_WeaponClassLoadedHandler, void*) {
    trace.emplace_back("register-weapon");
    if (init_mode == "init-weapon-throw") throw std::runtime_error("weapon throw");
    return true;
}
bool A2FO_CALL mock_register_trigger(const char*, std::uint32_t,
        A2FO_WeaponTriggerHandler, void*) {
    trace.emplace_back("register-trigger");
    return true;
}
bool A2FO_CALL mock_register_craft(const char*, std::uint32_t,
        A2FO_CraftEventHandler, void*) {
    trace.emplace_back("register-craft");
    return true;
}
void run(const std::string& mode) {
    seed();
    if (mode == "native-badalloc") {
        std::set_terminate([]() noexcept {
            const char* kind = "terminate:none";
            try {
                if (const auto error = std::current_exception()) {
                    std::rethrow_exception(error);
                }
            } catch (const std::bad_alloc&) {
                kind = "terminate:bad_alloc";
            } catch (...) {
                kind = "terminate:other";
            }
            std::puts(kind);
            std::fflush(stdout);
            ExitProcess(0);
        });
        const char enabled_name[] = "noxterInfestation", yes[] = "yes";
        const char delay_name[] = "infestationDelay";
        const char long_delay[] = "12345678901234567890123456789012345678901234567890";
        const A2FO_OdfFieldView fields[]{
            {{enabled_name,sizeof(enabled_name)-1},{yes,sizeof(yes)-1}},
            {{delay_name,sizeof(delay_name)-1},{long_delay,sizeof(long_delay)-1}},
        };
        A2FO_WeaponClassLoadedEvent event{
            sizeof(event), weapon_class.data(), nullptr, nullptr, fields, 2};
        fail_next_native_new = true;
        weapon_class_loaded(&event, nullptr);
        throw std::runtime_error("native allocation fault was not reached");
    } else if (mode == "metadata") {
        const char a[] = "noxterInfestation", yes[] = "yes";
        const char b[] = "infestationDelay", delay[] = "2.5junk";
        const char c[] = "infestationDamage", damage[] = "-4";
        const char d[] = "infestationLarva", larva[] = "'Data/larva.ODF'";
        const char e[] = "infestationLarvaCount", count[] = "66junk";
        const char f[] = "infestationBypassShields", off[] = "off";
        const char g[] = "infestationSpawnRadius", radius[] = "NaN";
        const A2FO_OdfFieldView fields[]{
            {{a,sizeof(a)-1},{yes,sizeof(yes)-1}},
            {{b,sizeof(b)-1},{delay,sizeof(delay)-1}},
            {{c,sizeof(c)-1},{damage,sizeof(damage)-1}},
            {{d,sizeof(d)-1},{larva,sizeof(larva)-1}},
            {{e,sizeof(e)-1},{count,sizeof(count)-1}},
            {{f,sizeof(f)-1},{off,sizeof(off)-1}},
            {{g,sizeof(g)-1},{radius,sizeof(radius)-1}},
        };
        A2FO_WeaponClassLoadedEvent event{sizeof(event), weapon_class.data(),
                                          nullptr, target.data(), fields, 7};
        weapon_class_loaded(&event, nullptr);
        const auto& value = g_weapon_policies.at(weapon_class.data());
        trace.emplace_back("policy:" + std::to_string(bits(value.delay_seconds)) +
            ":" + std::to_string(bits(value.damage)) + ":" + value.larva_odf +
            ":" + std::to_string(value.larva_count) + ":" +
            std::to_string(value.bypass_shields) + ":" +
            std::to_string(bits(value.spawn_radius)) + ":" +
            std::to_string(value.explosion_project_id));
        const char mother_key[] = "breederMother", mother_yes[] = "On";
        const A2FO_OdfFieldView mother_field[]{
            {{mother_key,sizeof(mother_key)-1},{mother_yes,sizeof(mother_yes)-1}}};
        A2FO_GameObjectClassLoadedEvent mother_event{
            sizeof(mother_event), mother_class.data(), {}, mother_field, 1};
        object_class_loaded(&mother_event, nullptr);
        trace.emplace_back("mother:" +
            std::to_string(g_mother_classes.count(mother_class.data())));
    } else if (mode == "metadata-null") {
        A2FO_WeaponClassLoadedEvent event{sizeof(event), weapon_class.data(),
                                          nullptr, nullptr, nullptr, UINT32_MAX};
        g_weapon_policies[weapon_class.data()] = policy();
        weapon_class_loaded(&event, nullptr);
        trace.emplace_back("policy-count:" +
                           std::to_string(g_weapon_policies.count(weapon_class.data())));
    } else if (mode == "trigger") {
        g_weapon_policies[weapon_class.data()] = policy();
        A2FO_WeaponTriggerEvent pre{sizeof(pre), A2FO_WEAPON_TRIGGER_PRECHECK,
                                    weapon.data(), target.data()};
        put_u32(target.data(), kObjectTeamOffset, 1);
        trace.emplace_back("friendly:" + std::to_string(weapon_trigger(&pre, nullptr)));
        put_u32(target.data(), kObjectTeamOffset, 2);
        trace.emplace_back("enemy:" + std::to_string(weapon_trigger(&pre, nullptr)));
        pre.kind = A2FO_WEAPON_TRIGGER_COMMITTED;
        trace.emplace_back("committed:" + std::to_string(weapon_trigger(&pre, nullptr)));
        infection_state("infected");
        pre.kind = A2FO_WEAPON_TRIGGER_PRECHECK;
        trace.emplace_back("duplicate:" + std::to_string(weapon_trigger(&pre, nullptr)));
    } else if (mode == "hatch") {
        Infection active{};
        active.target_handle = 10;
        active.infector_handle = 20;
        active.infector_team = 1;
        active.remaining_seconds = 1.5f;
        active.policy = policy();
        g_infections[10] = active;
        A2FO_CraftEvent tick{sizeof(tick), A2FO_CRAFT_EVENT_SIMULATE_POST,
                             target.data(), 1.0f};
        craft_event(&tick, nullptr);
        infection_state("first");
        reenter_cleanup = true;
        craft_event(&tick, nullptr);
        infection_state("hatched");
    } else if (mode == "cure") {
        Infection active{};
        active.target_handle = 10;
        active.remaining_seconds = 1.5f;
        g_infections[10] = active;
        put_u32(command_carrier.data(), 0x3c, 0x11);
        put_u32(target.data(), kCraftQueueStageOffset, 1);
        put_u32(target.data(), kCraftQueueOwnerOffset, 50);
        A2FO_CraftEvent tick{sizeof(tick), A2FO_CRAFT_EVENT_SIMULATE_POST,
                             target.data(), 0.1f};
        craft_event(&tick, nullptr);
        infection_state("cured");
    } else if (mode == "route") {
        LarvaRoute route{};
        route.larva_handle = 30;
        route.mother_handle = 40;
        route.team = 1;
        g_larva_routes[30] = route;
        A2FO_CraftEvent tick{sizeof(tick), A2FO_CRAFT_EVENT_SIMULATE_POST,
                             larva_one.data(), 0.6f};
        craft_event(&tick, nullptr);
        tick.elapsed_seconds = 0.5f;
        craft_event(&tick, nullptr);
        position(larva_one.data(), 0.0f, 0.0f, 99.0f);
        tick.elapsed_seconds = 1.0f;
        craft_event(&tick, nullptr);
        trace.emplace_back("routes:" + std::to_string(g_larva_routes.size()));
    } else if (mode == "nearest") {
        const Vector3 origin{};
        trace.emplace_back("nearest:" + name(nearest_mother(origin, 1)));
        put_u32(mother_two.data(), kObjectTeamOffset, 2);
        trace.emplace_back("team:" + name(nearest_mother(origin, 1)));
    } else if (mode == "numeric24" || mode == "numeric53" ||
               mode == "numeric64") {
        const unsigned int original = _controlfp(0, 0);
        _controlfp(mode == "numeric24" ? _PC_24 :
                   mode == "numeric53" ? _PC_53 : _PC_64, _MCW_PC);
        LarvaRoute route{};
        route.larva_handle = 30;
        route.mother_handle = 40;
        route.team = 1;
        g_larva_routes[30] = route;
        A2FO_CraftEvent tick{sizeof(tick), A2FO_CRAFT_EVENT_SIMULATE_POST,
                             larva_one.data(), 1.0f};
        position(larva_one.data(), 0.0f, 0.0f,
                 100.0f + std::nextafter(40.0f, 50.0f));
        craft_event(&tick, nullptr);
        trace.emplace_back("outside:" + std::to_string(g_larva_routes.size()));
        position(larva_one.data(), 0.0f, 0.0f,
                 100.0f + std::nextafter(40.0f, 0.0f));
        craft_event(&tick, nullptr);
        trace.emplace_back("inside:" + std::to_string(g_larva_routes.size()));
        _controlfp(original, _MCW_PC);
    } else if (mode == "disabled") {
        g_runtime_ready = false;
        trace.emplace_back("trigger:" + std::to_string(weapon_trigger(
            reinterpret_cast<const A2FO_WeaponTriggerEvent*>(1), nullptr)));
    } else if (mode == "short") {
        A2FO_WeaponTriggerEvent event{};
        event.struct_size = 0;
        event.weapon = reinterpret_cast<void*>(1);
        trace.emplace_back("trigger:" + std::to_string(weapon_trigger(&event, nullptr)));
        A2FO_CraftEvent craft{};
        craft.struct_size = 0;
        craft.craft = reinterpret_cast<void*>(1);
        craft_event(&craft, nullptr);
        trace.emplace_back("craft-short");
    } else if (mode.rfind("init-", 0) == 0) {
        init_mode = mode;
        g_runtime_ready = false;
        if (mode != "init-signature") seed_signatures();
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api);
        api.api_version = A2FO_MODULE_API_VERSION;
        api.capabilities = A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
            A2FO_CAP_WEAPON_CLASS_LOADED | A2FO_CAP_WEAPON_TRIGGER_EVENTS |
            A2FO_CAP_CRAFT_EVENTS;
        api.log = logger;
        api.armada_module = mock_armada;
        api.register_game_object_class_loaded_handler = mock_register_class;
        api.register_weapon_class_loaded_handler = mock_register_weapon;
        api.register_weapon_trigger_handler_masked = mock_register_trigger;
        api.register_craft_event_handler_masked = mock_register_craft;
        try { trace.emplace_back("init:" + std::to_string(A2FO_ModuleInit(&api))); }
        catch (const std::exception& error) {
            trace.emplace_back(std::string("init-throw:") + error.what());
        }
        trace.emplace_back("ready:" + std::to_string(g_runtime_ready));
    } else {
        throw std::runtime_error("unknown mode");
    }
    for (const auto& line : trace) std::puts(line.c_str());
}
} // namespace

extern "C" std::uintptr_t a2fo_infestation_call_thiscall_0(void* function, void* self) {
    if (function == image + kWeaponGetOwnerRva) {
        trace.emplace_back("owner");
        return reinterpret_cast<std::uintptr_t>(attacker.data());
    }
    if (function == image + kEntityGetTransformRva) {
        trace.emplace_back("transform:" + name(self));
        return reinterpret_cast<std::uintptr_t>(&transforms[self]);
    }
    throw std::runtime_error("unexpected thiscall_0");
}
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_1(
    void* function, void*, std::uintptr_t argument) {
    require(function == reinterpret_cast<void*>(&mock_mission_add), "mission add");
    trace.emplace_back("mission-add:" + name(reinterpret_cast<void*>(argument)));
    return 0;
}
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_2(
    void* function, void*, std::uintptr_t, std::uintptr_t scale) {
    require(function == reinterpret_cast<void*>(&mock_explosion_build), "explosion build");
    trace.emplace_back("explosion-build:" + std::to_string(scale));
    return 0;
}
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_3(
    void* function, void*, std::uintptr_t, std::uintptr_t output, std::uintptr_t) {
    require(function == image + kParameterDbGetProjectIdRva, "project id method");
    *reinterpret_cast<std::uint32_t*>(output) = 7;
    trace.emplace_back("project-id");
    return 1;
}
extern "C" std::uintptr_t a2fo_infestation_call_thiscall_4(
    void* function, void* self, std::uintptr_t first, std::uintptr_t,
    std::uintptr_t, std::uintptr_t) {
    if (function == image + kGameObjectClassConstructRva) {
        void* result = constructed++ == 0 ? larva_one.data() : larva_two.data();
        trace.emplace_back("construct:" + name(result));
        return reinterpret_cast<std::uintptr_t>(result);
    }
    if (function == image + kCraftDamageRva) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(first);
        float amount = 0.0f;
        std::uint32_t flags = 0, attacker_handle = 0;
        std::memcpy(&amount, bytes + kDamageInfoDamageOffset, sizeof(amount));
        std::memcpy(&flags, bytes + kDamageInfoFlagsOffset, sizeof(flags));
        std::memcpy(&attacker_handle, bytes + kDamageInfoAttackerHandleOffset,
                    sizeof(attacker_handle));
        trace.emplace_back("damage:" + name(self) + ":" +
            std::to_string(bits(amount)) + ":" + std::to_string(flags) +
            ":" + std::to_string(attacker_handle) +
            ":infection=" + std::to_string(g_infections.count(10)));
        if (reenter_cleanup) {
            reenter_cleanup = false;
            A2FO_CraftEvent cleanup{sizeof(cleanup), A2FO_CRAFT_EVENT_CLEANUP,
                                    self, 0.0f};
            craft_event(&cleanup, nullptr);
        }
        return 0;
    }
    throw std::runtime_error("unexpected thiscall_4");
}
extern "C" void a2fo_infestation_call_thiscall_command_vector_int(
    void*, void* self, std::int32_t command, const void*, std::int32_t) {
    trace.emplace_back("move:" + name(self) + ":" + std::to_string(command));
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "infestation fixture: %s\n", error.what());
        return 1;
    }
}
