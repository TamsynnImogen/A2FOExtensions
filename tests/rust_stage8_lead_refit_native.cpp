// Full frozen/current RefitYards module.cpp mock. No game or GUI entry point.
#include <windows.h>
#include <float.h>
#include <array>
#include <cmath>
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
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::array<std::uint8_t, 0x380> source{}, yard_one{}, yard_two{}, result{};
std::array<std::uint8_t, 0x40> source_class{}, yard_class{}, target_class{};
std::unordered_map<std::uint32_t, void*> entities;
std::unordered_map<std::string, void*> classes;
std::unordered_map<void*, Matrix34> transforms;
std::unordered_map<void*, BoundingSphere> bounds;
std::array<void*, 2> output_manager{};
std::array<std::uint8_t, 12> object_list{}, list_head{}, list_first{}, list_second{};
void* vtable[2]{};
bool delete_reenters = false;
std::uint32_t push_result = 0;
std::string init_mode;

void require(bool okay, const char* why) {
    if (!okay) throw std::runtime_error(why);
}
std::uint32_t bits(float value) {
    std::uint32_t result_bits = 0;
    std::memcpy(&result_bits, &value, sizeof(result_bits));
    return result_bits;
}
std::string name(void* object) {
    if (object == source.data()) return "source";
    if (object == yard_one.data()) return "yard1";
    if (object == yard_two.data()) return "yard2";
    if (object == result.data()) return "result";
    return object ? "other" : "null";
}
void A2FO_CALL logger(const char*, const char* message) {
    trace.emplace_back(std::string("log:") + message);
}
void* A2FO_CALL mock_armada_module() { trace.emplace_back("armada-module"); return image; }
void* A2FO_CALL mock_fleet_module() { trace.emplace_back("fleet-module"); return fleet; }
bool A2FO_CALL mock_hook(void*, void*, std::size_t,
                         const std::uint8_t*, A2FO_InlineHook* hook) {
    trace.emplace_back("install-hook");
    if (init_mode == "init-hook-throw") throw std::runtime_error("hook throw");
    if (init_mode == "init-hook-false") return false;
    hook->gateway = image + kDequeueCommandVectorRva;
    return true;
}
bool A2FO_CALL mock_class_registration(const char*, const char* const*,
        std::uint32_t, A2FO_GameObjectClassLoadedHandler, void*) {
    trace.emplace_back("register-class");
    if (init_mode == "init-class-throw") throw std::runtime_error("class throw");
    return init_mode != "init-class-false";
}
bool A2FO_CALL mock_craft_registration(const char*, std::uint32_t,
        A2FO_CraftEventHandler, void*) {
    trace.emplace_back("register-craft");
    if (init_mode == "init-craft-throw") throw std::runtime_error("craft throw");
    return init_mode != "init-craft-false";
}
void put_u32(void* object, std::size_t offset, std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void put_pointer(void* object, std::size_t offset, void* value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void position(void* object, float x, float y, float z) {
    Matrix34 matrix{};
    matrix.values[0] = matrix.values[4] = matrix.values[8] = 1.0f;
    matrix.values[9] = x;
    matrix.values[10] = y;
    matrix.values[11] = z;
    transforms[object] = matrix;
    bounds[object] = {{0.0f, 0.0f, 0.0f}, 20.0f};
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
extern "C" void mock_output_add() {}
extern "C" void mock_destroy() {}
void jump(std::uintptr_t rva, void* target) {
    const std::uint8_t code[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + rva, code, sizeof(code));
    std::memcpy(image + rva + 1, &target, sizeof(target));
}
std::uint32_t __cdecl push_refit(void* producer, void* target) {
    trace.emplace_back("push:" + name(producer) + ":" +
                       (target == target_class.data() ? "target" : "other") +
                       ":" + std::to_string(push_result));
    return push_result;
}
void seed() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x410000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x130000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    require(image && fleet, "synthetic images");
    g_armada = reinterpret_cast<HMODULE>(image);
    g_fleet_ops = reinterpret_cast<HMODULE>(fleet);
    static A2FO_ModuleApi api{};
    api.log = logger;
    g_api = &api;
    g_runtime_ready = true;
    g_push_refit = push_refit;
    jump(kEntityGetRva, reinterpret_cast<void*>(&mock_entity));
    jump(kGameObjectClassFindRva, reinterpret_cast<void*>(&mock_class));
    put_u32(source.data(), kObjectHandleOffset, 10);
    put_u32(yard_one.data(), kObjectHandleOffset, 20);
    put_u32(yard_two.data(), kObjectHandleOffset, 19);
    put_pointer(source.data(), kObjectClassOffset, source_class.data());
    put_pointer(yard_one.data(), kObjectClassOffset, yard_class.data());
    put_pointer(yard_two.data(), kObjectClassOffset, yard_class.data());
    put_u32(source.data(), kObjectTeamOffset, 2);
    put_u32(yard_one.data(), kObjectTeamOffset, 2);
    put_u32(yard_two.data(), kObjectTeamOffset, 2);
    source[kObjectAvoidMeOffset] = 1;
    entities[10] = source.data();
    entities[20] = yard_one.data();
    entities[19] = yard_two.data();
    put_pointer(image + kGameObjectListPointerRva, 0, object_list.data());
    put_pointer(object_list.data(), 4, list_head.data());
    put_u32(object_list.data(), 8, 2);
    put_pointer(list_head.data(), 0, list_first.data());
    put_pointer(list_first.data(), 0, list_second.data());
    put_pointer(list_first.data(), 8, yard_one.data());
    put_pointer(list_second.data(), 0, list_head.data());
    put_pointer(list_second.data(), 8, yard_two.data());
    classes["target"] = target_class.data();
    classes["source"] = source_class.data();
    position(source.data(), 0.0f, 0.0f, 0.0f);
    position(yard_one.data(), 0.0f, 0.0f, 100.0f);
    position(yard_two.data(), 0.0f, 0.0f, -100.0f);
    position(result.data(), 0.0f, 0.0f, 0.0f);
    SourceConfig source_config{};
    source_config.items[0] = "target";
    source_config.count = 1;
    g_source_classes[source_class.data()] = source_config;
    YardConfig yard_config{};
    yard_config.hardpoint = "root";
    yard_config.hardpoint_resolved = true;
    yard_config.hardpoint_node = yard_class.data();
    g_yard_classes[yard_class.data()] = yard_config;
    std::memcpy(image + kOutputQueueManagerVtableRva, vtable, sizeof(vtable));
    auto* queue_vtable = reinterpret_cast<void**>(image + kOutputQueueManagerVtableRva);
    queue_vtable[4] = reinterpret_cast<void*>(&mock_output_add);
    output_manager[0] = queue_vtable;
    put_pointer(yard_one.data(), kShipyardBuildOutputQueueOffset,
                output_manager.data());
    g_dequeue_command_vector_hook.gateway = image + kDequeueCommandVectorRva;
    void* source_vtable[2]{};
    source_vtable[1] = reinterpret_cast<void*>(&mock_destroy);
    static void* persistent_source_vtable[2]{};
    persistent_source_vtable[1] = source_vtable[1];
    put_pointer(source.data(), 0, persistent_source_vtable);
}
void seed_signatures() {
    std::memcpy(image + kGameObjectClassFindRva,
                kExpectedGameObjectClassFind, sizeof(kExpectedGameObjectClassFind));
    std::memcpy(image + kGameObjectClassGetHierarchyRootRva,
                kExpectedClassGetHierarchyRoot, sizeof(kExpectedClassGetHierarchyRoot));
    std::memcpy(image + kEntityGetTransformRva,
                kExpectedEntityGetTransform, sizeof(kExpectedEntityGetTransform));
    std::memcpy(image + kEntityGetBoundingSphereRva,
                kExpectedEntityGetBoundingSphere, sizeof(kExpectedEntityGetBoundingSphere));
    std::memcpy(image + kEntityGetWorldTransformRva,
                kExpectedEntityGetWorldTransform, sizeof(kExpectedEntityGetWorldTransform));
    std::memcpy(image + kEntityGetRva, kExpectedEntityGet, sizeof(kExpectedEntityGet));
    std::memcpy(image + kDequeueCommandVectorRva,
                kExpectedDequeueCommandVector, sizeof(kExpectedDequeueCommandVector));
    std::memcpy(image + kGameObjectSetTransformRva,
                kExpectedGameObjectSetTransform, sizeof(kExpectedGameObjectSetTransform));
    std::memcpy(image + kEvolverSwapObjectsRva,
                kExpectedEvolverSwapObjects, sizeof(kExpectedEvolverSwapObjects));
    std::memcpy(image + kCraftPreventCollisionsRva,
                kExpectedCraftPreventCollisions, sizeof(kExpectedCraftPreventCollisions));
    std::memcpy(image + kCraftPreventCollisionsRva + 0x2c,
                kExpectedCraftAvoidMeGate, sizeof(kExpectedCraftAvoidMeGate));
    std::memcpy(image + kSt3dNodeFindRecursiveRva,
                kExpectedNodeFindRecursive, sizeof(kExpectedNodeFindRecursive));
}
RefitJob job(RefitPhase phase, std::uint32_t yard = 20,
             std::uint32_t queue = 0) {
    RefitJob current{};
    current.source_handle = 10;
    current.yard_handle = yard;
    current.target_class = target_class.data();
    current.queue_id = queue;
    current.phase = phase;
    return current;
}
void state(const char* label) {
    const auto found = g_jobs.find(10);
    if (found == g_jobs.end()) {
        trace.emplace_back(std::string(label) + ":absent:avoid=" +
                           std::to_string(source[kObjectAvoidMeOffset]));
        return;
    }
    const auto& current = found->second;
    trace.emplace_back(std::string(label) + ":phase=" +
        std::to_string(static_cast<unsigned>(current.phase)) +
        ":yard=" + std::to_string(current.yard_handle) +
        ":queue=" + std::to_string(current.queue_id) +
        ":retry=" + std::to_string(current.retry_ticks) +
        ":move=" + std::to_string(current.move_ticks) +
        ":dock=" + std::to_string(bits(current.docking_elapsed_seconds)) +
        ":distance=" + std::to_string(bits(current.last_distance_squared)) +
        ":avoid=" + std::to_string(source[kObjectAvoidMeOffset]));
}
void run(const std::string& mode) {
    seed();
    if (mode == "travel") {
        g_jobs[10] = job(RefitPhase::travelling);
        g_jobs[10].move_ticks = kMoveRefreshTicks - 1;
        position(yard_one.data(), 0.0f, 0.0f, 1000.0f);
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("far");
        position(source.data(), 0.0f, 0.0f, -150.0f);
        position(yard_one.data(), 0.0f, 0.0f, 10.0f);
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("arrived");
    } else if (mode == "queue") {
        g_jobs[10] = job(RefitPhase::waiting_for_queue);
        g_jobs[10].retry_ticks = kRetryIntervalTicks - 1;
        push_result = 0;
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("rejected");
        g_jobs[10].retry_ticks = kRetryIntervalTicks - 1;
        push_result = 55;
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("queued");
        put_u32(yard_one.data(), kCurrentQueueIdOffset, 55);
        put_pointer(yard_one.data(), kCurrentBuildClassOffset,
                    target_class.data());
        update_refit_job(source.data(), &g_jobs.at(10), 2.5f);
        state("active");
        update_refit_job(source.data(), &g_jobs.at(10), 10.0f);
        state("docked");
        job_finished(yard_one.data(), 55, target_class.data(), result.data());
        state("finished");
    } else if (mode == "cancel") {
        g_jobs[10] = job(RefitPhase::active, 20, 55);
        g_jobs[10].avoidance_suppressed = true;
        g_jobs[10].saved_avoid_me = 1;
        source[kObjectAvoidMeOffset] = 0;
        delete_reenters = true;
        trace.emplace_back("cancel:" + std::to_string(cancel_source_job(source.data())));
        state("ejecting");
        put_u32(source.data(), kCraftQueueStageOffset, 0);
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("ejected");
    } else if (mode == "cleanup") {
        g_jobs[10] = job(RefitPhase::active, 20, 55);
        g_jobs[10].avoidance_suppressed = true;
        g_jobs[10].saved_avoid_me = 1;
        source[kObjectAvoidMeOffset] = 0;
        job_removed(yard_one.data(), 55, nullptr,
                    A2FO_REFIT_QUEUE_PRODUCER_DESTROYED);
        state("producer-destroyed");
    } else if (mode == "nearest") {
        Matrix34 selected{};
        void* yard = nearest_refit_yard(source.data(), &selected);
        trace.emplace_back("nearest:" + name(yard) + ":z=" +
                           std::to_string(bits(selected.values[11])));
        trace.emplace_back("request:" + std::to_string(
            request_refit(source.data(), target_class.data())));
        trace.emplace_back("consume:" + std::to_string(
            consume_synchronized_refit(source.data(), target_class.data())));
        state("accepted");
    } else if (mode == "numeric24" || mode == "numeric53" ||
               mode == "numeric64") {
        const unsigned int original = _controlfp(0, 0);
        const unsigned int precision = mode == "numeric24" ? _PC_24 :
                                       mode == "numeric53" ? _PC_53 : _PC_64;
        _controlfp(precision, _MCW_PC);
        trace.emplace_back("precision:" + mode);
        g_jobs[10] = job(RefitPhase::travelling);
        Matrix34 approach = refit_approach_transform(
            source.data(), yard_one.data(), transforms.at(yard_one.data()));
        const float inside = std::nextafter(96.0f, 0.0f);
        const float outside = std::nextafter(96.0f, 200.0f);
        position(source.data(), 0.0f, 0.0f, approach.values[11] + outside);
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("outside");
        position(source.data(), 0.0f, 0.0f, approach.values[11] + inside);
        update_refit_job(source.data(), &g_jobs.at(10), 0.1f);
        state("inside");
        _controlfp(original, _MCW_PC);
    } else if (mode.rfind("init-", 0) == 0) {
        init_mode = mode;
        g_runtime_ready = false;
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api);
        api.api_version = A2FO_MODULE_API_VERSION;
        api.log = logger;
        api.armada_module = mock_armada_module;
        api.fleetops_module = mock_fleet_module;
        api.install_inline_hook = mock_hook;
        api.capabilities = A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
                           A2FO_CAP_CRAFT_EVENTS;
        api.register_game_object_class_loaded_handler = mock_class_registration;
        api.register_craft_event_handler_masked = mock_craft_registration;
        if (mode != "init-signature") seed_signatures();
        try {
            trace.emplace_back(std::string("init-result:") +
                               std::to_string(A2FO_ModuleInit(&api)));
        } catch (const std::exception& error) {
            trace.emplace_back(std::string("init-throw:") + error.what());
        }
        trace.emplace_back(std::string("ready:") + std::to_string(g_runtime_ready));
    } else {
        throw std::runtime_error("unknown fixture mode");
    }
    for (const auto& line : trace) std::puts(line.c_str());
}
} // namespace

extern "C" std::uintptr_t a2fo_refit_call_thiscall_0(void* function, void* self) {
    if (function == image + kEntityGetTransformRva) {
        trace.emplace_back("transform:" + name(self));
        return reinterpret_cast<std::uintptr_t>(&transforms[self]);
    }
    if (function == image + kEntityGetBoundingSphereRva) {
        trace.emplace_back("bounds:" + name(self));
        return reinterpret_cast<std::uintptr_t>(&bounds[self]);
    }
    if (function == image + kGameObjectClassGetHierarchyRootRva) {
        trace.emplace_back("hierarchy-root");
        return reinterpret_cast<std::uintptr_t>(yard_class.data());
    }
    if (function == reinterpret_cast<void*>(&mock_destroy)) {
        trace.emplace_back("destroy:" + name(self));
        return 0;
    }
    throw std::runtime_error("unexpected native thiscall_0");
}
extern "C" std::uintptr_t a2fo_refit_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument) {
    if (function == image + kGameObjectSetTransformRva) {
        const auto* matrix = reinterpret_cast<const Matrix34*>(argument);
        trace.emplace_back("set-transform:" + name(self) + ":z=" +
                           std::to_string(bits(matrix->values[11])));
        transforms[self] = *matrix;
        return 0;
    }
    if (function == image + kEvolverSwapObjectsRva) {
        trace.emplace_back("swap:" + name(self) + ":" +
                           name(reinterpret_cast<void*>(argument)) +
                           ":job=" + std::to_string(g_jobs.count(10)));
        return 0;
    }
    if (function == fleet + kFoProducerActDeleteRva) {
        trace.emplace_back("act-delete:" + name(self) + ":" +
                           std::to_string(argument));
        if (delete_reenters) {
            delete_reenters = false;
            job_removed(self, static_cast<std::uint32_t>(argument), nullptr,
                        A2FO_REFIT_QUEUE_DELETED);
        }
        return 0;
    }
    if (function == reinterpret_cast<void*>(&mock_output_add)) {
        trace.emplace_back("output-add:" + name(reinterpret_cast<void*>(argument)));
        put_u32(source.data(), kCraftQueueStageOffset, 1);
        put_u32(source.data(), kCraftQueueOwnerOffset, 20);
        return 0;
    }
    if (function == image + kSt3dNodeFindRecursiveRva) {
        trace.emplace_back("node-find");
        return reinterpret_cast<std::uintptr_t>(yard_class.data());
    }
    throw std::runtime_error("unexpected native thiscall_1");
}
extern "C" std::uintptr_t a2fo_refit_call_thiscall_2(
    void* function, void* self, std::uintptr_t first, std::uintptr_t second) {
    if (function == image + kEntityGetWorldTransformRva) {
        trace.emplace_back("world-transform:" + name(self));
        *reinterpret_cast<Matrix34*>(first) = transforms[self];
        return first;
    }
    if (function == image + kGameObjectQueueClassCommandRva) {
        trace.emplace_back("class-command:" + name(self) + ":" +
                           std::to_string(first) + ":" +
                           (reinterpret_cast<void*>(second) == target_class.data()
                                ? "target" : "other"));
        return 0;
    }
    if (function == image + kDebriefingDestroyShipRva) {
        trace.emplace_back("debrief-destroy:" + name(reinterpret_cast<void*>(first)));
        return 0;
    }
    throw std::runtime_error("unexpected native thiscall_2");
}
extern "C" void a2fo_refit_call_thiscall_command_vector_int(
    void*, void* self, std::int32_t command, const void* vector,
    std::int32_t value) {
    const auto* destination = static_cast<const Vector3*>(vector);
    trace.emplace_back("command:" + name(self) + ":" +
                       std::to_string(command) + ":" +
                       std::to_string(bits(destination->x)) + ":" +
                       std::to_string(bits(destination->y)) + ":" +
                       std::to_string(bits(destination->z)) + ":" +
                       std::to_string(value));
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    try { run(argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "refit fixture: %s\n", error.what());
        return 1;
    }
}
