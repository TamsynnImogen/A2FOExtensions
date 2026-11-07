// Full frozen/current Breeders module source with synthetic native SDK objects.
#include <windows.h>
#include <float.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include MODULE_SOURCE

namespace {
std::vector<std::string> trace;
std::uint8_t* engine = nullptr;
std::uint8_t* fleet = nullptr;
std::array<std::uint8_t, 0x300> breeder{}, mother_one{}, mother_two{}, seed{}, replacement{};
std::array<std::uint8_t, 0x100> virgin_class{}, mother_class{}, target_class{},
    result_class{}, seed_class{};
std::array<std::uint8_t, 12> object_list{}, list_head{}, node_one{}, node_two{};
std::array<std::uint8_t, 4> mission{};
std::array<void*, 7> mission_table{};
std::unordered_map<std::uint32_t, void*> entities;
std::unordered_map<std::string, void*> classes;
std::unordered_map<void*, Matrix34> transforms;
bool reenter_cleanup = false;
bool push_succeeds = false;
std::string init_mode;

void require(bool okay, const char* why) { if (!okay) throw std::runtime_error(why); }
void A2FO_CALL logger(const char*, const char* line) {
    trace.emplace_back(std::string("log:") + line);
}
void* A2FO_CALL mock_armada_module() {
    trace.emplace_back("armada-module");
    return engine;
}
void* A2FO_CALL mock_fleet_module() {
    trace.emplace_back("fleet-module");
    return fleet;
}
bool A2FO_CALL mock_alias(const char*, const char*, const char*) {
    trace.emplace_back("register-alias");
    if (init_mode == "init-alias-throw") throw std::runtime_error("alias throw");
    return init_mode != "init-alias-false";
}
bool A2FO_CALL mock_defaults(const char*, const char*,
        const A2FO_ClasslabelOdfDefault*, std::uint32_t) {
    trace.emplace_back("register-defaults");
    return true;
}
bool A2FO_CALL mock_class_registration(const char*, const char* const*,
        std::uint32_t, A2FO_GameObjectClassLoadedHandler, void*) {
    trace.emplace_back("register-class");
    if (init_mode == "init-class-throw") throw std::runtime_error("class throw");
    return true;
}
bool A2FO_CALL mock_producer_registration(const char*,
        A2FO_ProducerEventHandler, void*) {
    trace.emplace_back("register-producer");
    return true;
}
bool A2FO_CALL mock_craft_registration(const char*, std::uint32_t,
        A2FO_CraftEventHandler, void*) {
    trace.emplace_back("register-craft");
    return init_mode != "init-craft-false";
}
bool A2FO_CALL mock_craft_legacy_registration(const char*,
        A2FO_CraftEventHandler, void*) {
    trace.emplace_back("register-craft-legacy");
    return true;
}
void put_u32(void* object, std::size_t offset, std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void put_pointer(void* object, std::size_t offset, void* value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void position(void* object, float x, float y, float z) {
    const float values[] = {x, y, z};
    std::memcpy(static_cast<std::uint8_t*>(object) + kObjectPositionOffset,
                values, sizeof(values));
    Matrix34 transform{};
    transform.values[0] = transform.values[4] = transform.values[8] = 1.0f;
    transform.values[9] = x;
    transform.values[10] = y;
    transform.values[11] = z;
    transforms[object] = transform;
}
std::string name(void* object) {
    if (object == breeder.data()) return "breeder";
    if (object == mother_one.data()) return "mother1";
    if (object == mother_two.data()) return "mother2";
    if (object == seed.data()) return "seed";
    if (object == replacement.data()) return "replacement";
    return object ? "other" : "null";
}
extern "C" void* __cdecl mock_entity(std::uint32_t handle) {
    trace.emplace_back("entity:" + std::to_string(handle));
    const auto found = entities.find(handle);
    return found == entities.end() ? nullptr : found->second;
}
extern "C" void* __cdecl mock_class(const char* key) {
    const std::string wanted = key ? key : "";
    trace.emplace_back("class:" + wanted);
    const auto found = classes.find(wanted);
    return found == classes.end() ? nullptr : found->second;
}
extern "C" void* __cdecl mock_mission() { trace.emplace_back("mission"); return mission.data(); }
extern "C" void mock_add() {}
void jump(std::uintptr_t rva, void* target) {
    const std::uint8_t code[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(engine + rva, code, sizeof(code));
    std::memcpy(engine + rva + 1, &target, sizeof(target));
}
void seed_fixture() {
    engine = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x130000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    require(engine && fleet, "synthetic images");
    g_armada = reinterpret_cast<HMODULE>(engine);
    g_fleet_ops = reinterpret_cast<HMODULE>(fleet);
    static A2FO_ModuleApi api{};
    api.log = logger;
    g_api = &api;
    g_runtime_ready = true;
    jump(kEntityGetRva, reinterpret_cast<void*>(&mock_entity));
    jump(kGameObjectClassFindRva, reinterpret_cast<void*>(&mock_class));
    jump(kAiMissionGetCurrentRva, reinterpret_cast<void*>(&mock_mission));
    mission_table[6] = reinterpret_cast<void*>(&mock_add);
    put_pointer(mission.data(), 0, mission_table.data());
    put_u32(breeder.data(), kObjectHandleOffset, 10);
    put_u32(mother_one.data(), kObjectHandleOffset, 20);
    put_u32(mother_two.data(), kObjectHandleOffset, 19);
    put_u32(seed.data(), kObjectHandleOffset, 30);
    put_u32(replacement.data(), kObjectHandleOffset, 40);
    put_u32(breeder.data(), kObjectTeamOffset, 2);
    put_u32(mother_one.data(), kObjectTeamOffset, 2);
    put_u32(mother_two.data(), kObjectTeamOffset, 2);
    put_pointer(breeder.data(), kObjectClassOffset, virgin_class.data());
    put_pointer(mother_one.data(), kObjectClassOffset, mother_class.data());
    put_pointer(mother_two.data(), kObjectClassOffset, mother_class.data());
    put_pointer(seed.data(), kObjectClassOffset, seed_class.data());
    put_pointer(replacement.data(), kObjectClassOffset, result_class.data());
    entities[10] = breeder.data();
    entities[20] = mother_one.data();
    entities[19] = mother_two.data();
    entities[30] = seed.data();
    classes["seed"] = seed_class.data();
    classes["result"] = result_class.data();
    classes["target"] = target_class.data();
    position(breeder.data(), 0.0f, 0.0f, 0.0f);
    position(mother_one.data(), 0.0f, 0.0f, 100.0f);
    position(mother_two.data(), 0.0f, 0.0f, -100.0f);
    position(seed.data(), 0.0f, 0.0f, 100.0f);
    put_pointer(engine + kGameObjectListPointerRva, 0, object_list.data());
    put_pointer(object_list.data(), 4, list_head.data());
    put_u32(object_list.data(), 8, 2);
    put_pointer(list_head.data(), 0, node_one.data());
    put_pointer(node_one.data(), 0, node_two.data());
    put_pointer(node_one.data(), 8, mother_one.data());
    put_pointer(node_two.data(), 0, list_head.data());
    put_pointer(node_two.data(), 8, mother_two.data());
    ClassPolicy mother_policy{};
    mother_policy.mother = true;
    g_class_policies[mother_class.data()] = mother_policy;
    ClassPolicy virgin_policy{};
    virgin_policy.breeder = true;
    g_class_policies[virgin_class.data()] = virgin_policy;
    ClassPolicy target_policy{};
    target_policy.seed_odf = "seed";
    target_policy.result_odf = "result";
    g_class_policies[target_class.data()] = target_policy;
    ClassPolicy result_policy{};
    result_policy.breeder = true;
    result_policy.produces_odf = "target";
    g_class_policies[result_class.data()] = result_policy;
}
template<std::size_t N>
void signature(std::uint8_t* image, std::uintptr_t rva,
               const std::uint8_t (&expected)[N]) {
    std::memcpy(image + rva, expected, N);
}
void seed_signatures() {
    signature(engine, kAiMissionGetCurrentRva, kExpectedAiMissionGetCurrent);
    signature(engine, kGameObjectClassFindRva, kExpectedGameObjectClassFind);
    signature(engine, kGameObjectClassConstructRva, kExpectedGameObjectClassConstruct);
    signature(engine, kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(engine, kEntityGetRva, kExpectedEntityGet);
    signature(engine, kDequeueCommandVectorRva, kExpectedDequeueCommandVector);
    signature(engine, kEvolverSwapObjectsRva, kExpectedEvolverSwapObjects);
    signature(fleet, kFoProducerPushCheckedRva, kExpectedFoPushChecked);
}
void waiting_state(const char* label) {
    const auto found = g_waiting.find(10);
    if (found == g_waiting.end()) {
        trace.emplace_back(std::string(label) + ":none");
    } else {
        trace.emplace_back(std::string(label) + ":seed=" +
                           std::to_string(found->second.seed_handle) +
                           ":refresh=" + std::to_string(found->second.move_refresh));
    }
}
void run(const std::string& mode) {
    seed_fixture();
    if (mode == "metadata") {
        const char key1[] = " breederCLASS ", key2[] = "breederClass";
        const char value1[] = " yes ", value2[] = "1";
        const char key3[] = "breederRadius", value3[] = "'120.5'";
        const char key4[] = "breederSeed", value4[] = "Data/Ships/SEED.ODF";
        const A2FO_OdfFieldView fields[]{
            {{key1, sizeof(key1)-1}, {value1, sizeof(value1)-1}},
            {{key2, sizeof(key2)-1}, {value2, sizeof(value2)-1}},
            {{key3, sizeof(key3)-1}, {value3, sizeof(value3)-1}},
            {{key4, sizeof(key4)-1}, {value4, sizeof(value4)-1}},
        };
        A2FO_GameObjectClassLoadedEvent event{sizeof(event), virgin_class.data(),
                                               {}, fields, 4};
        class_loaded_handler(&event, nullptr);
        const auto& policy = g_class_policies.at(virgin_class.data());
        trace.emplace_back("policy:" + std::to_string(policy.breeder) + ":" +
                           std::to_string(policy.mother_radius) + ":" + policy.seed_odf);
    } else if (mode == "metadata-null") {
        A2FO_GameObjectClassLoadedEvent event{sizeof(event), virgin_class.data(),
                                               {}, nullptr, UINT32_MAX};
        class_loaded_handler(&event, nullptr);
        trace.emplace_back("policy-count:" +
                           std::to_string(g_class_policies.count(virgin_class.data())));
    } else if (mode == "metadata-invalid") {
        const char key1[] = "breederRadius", value1[] = "NaN";
        const char key2[] = "breederConsumeRadius", value2[] = "-1";
        const char key3[] = "breederSeed", value3[] = "'Data\\SEED.odf'";
        const A2FO_OdfFieldView fields[]{
            {{key1, sizeof(key1)-1}, {value1, sizeof(value1)-1}},
            {{key2, sizeof(key2)-1}, {value2, sizeof(value2)-1}},
            {{key3, sizeof(key3)-1}, {value3, sizeof(value3)-1}},
        };
        A2FO_GameObjectClassLoadedEvent event{sizeof(event), virgin_class.data(),
                                               {}, fields, 3};
        class_loaded_handler(&event, nullptr);
        const auto& policy = g_class_policies.at(virgin_class.data());
        trace.emplace_back("invalid:" + std::to_string(policy.mother_radius) +
                           ":" + std::to_string(policy.consume_radius) +
                           ":" + policy.seed_odf);
    } else if (mode == "nearest") {
        trace.emplace_back("nearest:" + name(nearest_mother(breeder.data())));
        put_u32(mother_two.data(), kObjectTeamOffset, 3);
        trace.emplace_back("other-team:" + name(nearest_mother(breeder.data())));
    } else if (mode == "numeric24" || mode == "numeric53" ||
               mode == "numeric64") {
        const unsigned int original = _controlfp(0, 0);
        _controlfp(mode == "numeric24" ? _PC_24 :
                   mode == "numeric53" ? _PC_53 : _PC_64, _MCW_PC);
        mother_two[kObjectExpiredOffset] = 1;
        position(mother_one.data(), std::nextafter(600.0f, 700.0f), 0.0f, 0.0f);
        trace.emplace_back("outside:" + name(nearest_mother(breeder.data())));
        position(mother_one.data(), std::nextafter(600.0f, 0.0f), 0.0f, 0.0f);
        trace.emplace_back("inside:" + name(nearest_mother(breeder.data())));
        _controlfp(original, _MCW_PC);
    } else if (mode == "request" || mode == "no-mother") {
        if (mode == "no-mother") put_u32(mother_one.data(), kObjectTeamOffset, 3),
                                  put_u32(mother_two.data(), kObjectTeamOffset, 3);
        A2FO_ProducerEvent event{sizeof(event), A2FO_PRODUCER_EVENT_ADMIT,
                                 breeder.data(), target_class.data()};
        trace.emplace_back("admit:" + std::to_string(producer_event_handler(&event, nullptr)));
        waiting_state("waiting");
    } else if (mode == "waiting") {
        WaitingSeed waiting{};
        waiting.breeder_handle = 10;
        waiting.seed_handle = 30;
        waiting.result_odf = "result";
        waiting.target_odf = "target";
        g_waiting[10] = waiting;
        A2FO_CraftEvent event{sizeof(event), A2FO_CRAFT_EVENT_SIMULATE_POST,
                              breeder.data(), 0.6f};
        craft_event_handler(&event, nullptr);
        waiting_state("first");
        craft_event_handler(&event, nullptr);
        waiting_state("move");
        position(seed.data(), 0.0f, 0.0f, 10.0f);
        reenter_cleanup = true;
        craft_event_handler(&event, nullptr);
        waiting_state("committed");
    } else if (mode == "active") {
        put_pointer(breeder.data(), kObjectClassOffset, result_class.data());
        A2FO_CraftEvent event{sizeof(event), A2FO_CRAFT_EVENT_SIMULATE_POST,
                              breeder.data(), 0.2f};
        craft_event_handler(&event, nullptr);
        trace.emplace_back("retry1:" + std::to_string(g_production_retry[10]));
        push_succeeds = true;
        event.elapsed_seconds = 0.3f;
        craft_event_handler(&event, nullptr);
        trace.emplace_back("retry2:" + std::to_string(g_production_retry[10]));
        event.elapsed_seconds = 0.3f;
        craft_event_handler(&event, nullptr);
        trace.emplace_back("retry3:" + std::to_string(g_production_retry[10]));
    } else if (mode == "disabled") {
        g_runtime_ready = false;
        trace.emplace_back("admit:" + std::to_string(producer_event_handler(
            reinterpret_cast<const A2FO_ProducerEvent*>(1), nullptr)));
    } else if (mode == "short-producer") {
        A2FO_ProducerEvent event{};
        event.struct_size = 0;
        event.producer = reinterpret_cast<void*>(1);
        trace.emplace_back("admit:" +
                           std::to_string(producer_event_handler(&event, nullptr)));
    } else if (mode == "short-craft") {
        A2FO_CraftEvent event{};
        event.struct_size = 0;
        event.craft = reinterpret_cast<void*>(1);
        craft_event_handler(&event, nullptr);
        trace.emplace_back("short-craft:done");
    } else if (mode == "cleanup") {
        WaitingSeed waiting{};
        waiting.seed_handle = 30;
        g_waiting[10] = waiting;
        g_production_retry[10] = 1.0f;
        A2FO_CraftEvent event{sizeof(event), A2FO_CRAFT_EVENT_CLEANUP,
                              breeder.data(), 0.0f};
        craft_event_handler(&event, nullptr);
        trace.emplace_back("cleanup:" + std::to_string(g_waiting.size()) +
                           ":" + std::to_string(g_production_retry.size()));
    } else if (mode == "shutdown") {
        g_waiting[10] = WaitingSeed{};
        g_production_retry[10] = 1.0f;
        A2FO_ModuleShutdown();
        trace.emplace_back("state:" + std::to_string(g_runtime_ready) + ":" +
                           std::to_string(g_waiting.size()) + ":" +
                           std::to_string(g_production_retry.size()) + ":" +
                           std::to_string(g_class_policies.size()) + ":" +
                           std::to_string(g_armada != nullptr));
    } else if (mode.rfind("init-", 0) == 0) {
        init_mode = mode;
        g_runtime_ready = false;
        if (mode != "init-signature") seed_signatures();
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api);
        api.api_version = A2FO_MODULE_API_VERSION;
        api.capabilities = A2FO_CAP_CLASSLABEL_ODF_DEFAULTS |
            A2FO_CAP_GAME_OBJECT_CLASS_LOADED | A2FO_CAP_PRODUCER_EVENTS |
            A2FO_CAP_CRAFT_EVENTS;
        api.log = logger;
        api.armada_module = mock_armada_module;
        api.fleetops_module = mock_fleet_module;
        api.register_classlabel_alias = mock_alias;
        api.register_classlabel_odf_defaults = mock_defaults;
        api.register_game_object_class_loaded_handler = mock_class_registration;
        api.register_producer_event_handler = mock_producer_registration;
        api.register_craft_event_handler = mock_craft_legacy_registration;
        api.register_craft_event_handler_masked = mock_craft_registration;
        try {
            trace.emplace_back("init:" + std::to_string(A2FO_ModuleInit(&api)));
        } catch (const std::exception& error) {
            trace.emplace_back(std::string("init-throw:") + error.what());
        }
        trace.emplace_back("ready:" + std::to_string(g_runtime_ready));
    } else {
        throw std::runtime_error("unknown mode");
    }
    for (const auto& line : trace) std::puts(line.c_str());
}
} // namespace

extern "C" std::uintptr_t a2fo_breeders_call_thiscall_0(void* function, void* self) {
    if (function == engine + kEntityGetTransformRva) {
        trace.emplace_back("transform:" + name(self));
        return reinterpret_cast<std::uintptr_t>(&transforms[self]);
    }
    if (function == engine + kCraftDoExpireRva) {
        trace.emplace_back("expire:" + name(self));
        static_cast<std::uint8_t*>(self)[kObjectExpiredOffset] = 1;
        return 0;
    }
    throw std::runtime_error("unexpected thiscall_0");
}
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument) {
    if (function == reinterpret_cast<void*>(&mock_add)) {
        trace.emplace_back("mission-add:" + name(reinterpret_cast<void*>(argument)));
        return 0;
    }
    if (function == engine + kEvolverSwapObjectsRva) {
        trace.emplace_back("swap:" + name(self) + ":" +
                           name(reinterpret_cast<void*>(argument)) +
                           ":waiting=" + std::to_string(g_waiting.count(10)));
        if (reenter_cleanup) {
            reenter_cleanup = false;
            A2FO_CraftEvent cleanup{sizeof(cleanup), A2FO_CRAFT_EVENT_CLEANUP,
                                    self, 0.0f};
            craft_event_handler(&cleanup, nullptr);
        }
        return 0;
    }
    if (function == fleet + kFoProducerPushCheckedRva) {
        trace.emplace_back("push:" + name(self));
        if (push_succeeds) put_u32(self, kProducerQueueCountOffset, 1);
        return 0;
    }
    throw std::runtime_error("unexpected thiscall_1");
}
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t a2fo_breeders_call_thiscall_4(
    void* function, void* self, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t label) {
    require(function == engine + kGameObjectClassConstructRva,
            "construct address");
    trace.emplace_back("construct:" +
                       std::string(reinterpret_cast<const char*>(label)));
    return reinterpret_cast<std::uintptr_t>(
        self == seed_class.data() ? seed.data() : replacement.data());
}
extern "C" void a2fo_breeders_call_thiscall_command_vector_int(
    void*, void* self, std::int32_t command, const void*, std::int32_t) {
    trace.emplace_back("move:" + name(self) + ":" + std::to_string(command));
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "breeders fixture: %s\n", error.what());
        return 1;
    }
}
