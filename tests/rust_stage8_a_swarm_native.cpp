// Full frozen/current SwarmSystem module comparison with synthetic Armada
// image, ParameterDB, linked list and ST3D callbacks. No game or GUI access.
#include MODULE_SOURCE

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <float.h>
#include <stdexcept>

namespace {
std::string mode;
std::uint8_t* image = nullptr;
std::array<std::uint8_t, 0x300> object{}, class_bytes{}, hierarchy{};
std::array<std::uint8_t, 0x80> launch_node{}, interaction_node{};
std::array<std::uint8_t, 12> object_list{}, list_head{}, list_node{};
Matrix34 host_matrix{};
BoundingSphere host_sphere{};
#ifdef A2FO_BUILTIN_FEATURE
std::uint64_t activation_token = 0;
#endif
std::string odf_name = "swarm_host";
unsigned install_count = 0, render_count = 0;
unsigned gateway_simulations = 0, gateway_renders = 0;
bool model_failure = false, visible_host = true, rendered_instance = true;

void require(bool okay, const char* why) {
    if (!okay) {
        std::fprintf(stderr, "fixture:%s\n", why);
        std::exit(1);
    }
}
void trace(const std::string& line) { std::puts(line.c_str()); }
std::string bits(float value) { return std::to_string(float_bits(value)); }
template<typename T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

void A2FO_CALL logger(const char*, const char* message) {
    trace(std::string("log:") + message);
}
void* A2FO_CALL armada() { trace("armada"); return image; }
void __cdecl mock_delete(void* value) { std::free(value); }
void patch_jump(void* address, void* target) {
    auto* bytes = static_cast<std::uint8_t*>(address);
    bytes[0] = 0xe9;
    const std::int32_t relative = static_cast<std::int32_t>(
        reinterpret_cast<std::uintptr_t>(target) -
        reinterpret_cast<std::uintptr_t>(address) - 5);
    std::memcpy(bytes + 1, &relative, sizeof(relative));
}

template<std::size_t N>
void signature(std::uintptr_t rva, const std::uint8_t (&expected)[N]) {
    std::memcpy(image + rva, expected, N);
}
void setup_image() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x410000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    require(image != nullptr, "synthetic image");
    signature(kBeeColonySimulateRva, kExpectedBeeColonySimulate);
    signature(kBeeColonyRenderRva, kExpectedBeeColonyRender);
    signature(kBeeColonyShouldDrawBeeRva, kExpectedBeeColonyShouldDrawBee);
    signature(kParameterDbConstructorRva, kExpectedParameterDbConstructor);
    signature(kParameterDbDestructorRva, kExpectedParameterDbDestructor);
    for (auto rva : {kParameterDbGetIntRva, kParameterDbGetFloatRva,
                     kParameterDbGetBoolRva})
        signature(rva, kExpectedParameterDbGetScalar);
    signature(kParameterDbGetStringRva, kExpectedParameterDbGetString);
    signature(kParameterDbGetStringVectorRva, kExpectedParameterDbGetStringVector);
    signature(kSt3dInstanceConstructorRva, kExpectedSt3dInstanceConstructor);
    signature(kSt3dInstanceDestructorRva, kExpectedSt3dInstanceDestructor);
    signature(kSt3dInstanceSetAnimationFlagsRva,
              kExpectedSt3dInstanceSetAnimationFlags);
    signature(kSt3dInstanceScaleGeometryRva, kExpectedSt3dInstanceScaleGeometry);
    signature(kSt3dInstanceSetTransformRva, kExpectedSt3dInstanceSetTransform);
    signature(kSt3dInstanceSetDatabaseRva, kExpectedSt3dInstanceSetDatabase);
    signature(kSt3dInstanceRenderRva, kExpectedSt3dInstanceRender);
    signature(kSt3dInstanceTriggerAnimationRva,
              kExpectedSt3dInstanceTriggerAnimation);
    signature(kEntityGetTransformRva, kExpectedEntityGetTransform);
    signature(kEntityGetBoundingSphereRva, kExpectedEntityGetBoundingSphere);
    signature(kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    signature(kGameObjectClassGetHierarchyRootRva, kExpectedClassGetHierarchyRoot);
    signature(kGameObjectClassGetOdfNameRva, kExpectedClassGetOdfName);
    signature(kSt3dNodeFindRecursiveRva, kExpectedNodeFindRecursive);
    signature(kEngineOperatorDeleteRva, kExpectedEngineOperatorDelete);
    patch_jump(image + kEngineOperatorDeleteRva,
               reinterpret_cast<void*>(&mock_delete));
    if (mode == "init-signature") image[kBeeColonyRenderRva] ^= 1;
    if (mode == "init-core-jump") image[kParameterDbGetIntRva] = 0xe9;
}

void setup_host() {
    put(object.data(), kObjectHandleOffset, std::uint32_t{77});
    put(object.data(), kObjectClassOffset, static_cast<void*>(class_bytes.data()));
    put(object.data(), kObjectExpiredOffset, std::uint8_t{0});
    put(class_bytes.data(), 0x1d8, static_cast<void*>(hierarchy.data()));
    put(hierarchy.data(), 0x3c, static_cast<void*>(launch_node.data()));
    host_matrix.values[0] = 1.0f;
    host_matrix.values[4] = 1.0f;
    host_matrix.values[8] = 1.0f;
    host_matrix.values[9] = 20.0f;
    host_sphere.centre = {0.0f, 0.0f, 0.0f};
    host_sphere.radius = mode == "host-no-bounds" ? 0.0f : 8.0f;
    put(object_list.data(), 4, static_cast<void*>(list_head.data()));
    put(object_list.data(), 8, std::uint32_t{1});
    put(list_head.data(), 0, static_cast<void*>(list_node.data()));
    put(list_node.data(), 0, static_cast<void*>(list_head.data()));
    put(list_node.data(), 8, static_cast<void*>(object.data()));
    put(image + kGameObjectListPointerRva, 0,
        static_cast<void*>(object_list.data()));
}

bool A2FO_CALL install(void* target, void*, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    require(std::memcmp(target, expected, size) == 0, "install signature");
    ++install_count;
    trace("install:" + std::to_string(install_count));
    if (mode == "init-partial" && install_count == 2) return false;
    *hook = {target, image + (install_count == 1 ? 0x100 : 0x110),
             static_cast<std::uint32_t>(size)};
    return true;
}

bool get_int(const char* key, int* value) {
    if (std::strcmp(key, "swarm0Count") == 0) {
        *value = mode == "parse-zero" ? 0 : 3;
        return true;
    }
    if (std::strcmp(key, "swarm0HardpointCapacity") == 0) {
        *value = 1;
        return true;
    }
    if (std::strcmp(key, "swarm0InteractionCapacity") == 0) {
        *value = 1;
        return true;
    }
    return false;
}
bool get_float(const char* key, float* value) {
    if (std::strcmp(key, "swarm0Scale") == 0) *value = 1.5f;
    else if (std::strcmp(key, "swarm0Radius") == 0) *value = 13.0f;
    else if (std::strcmp(key, "swarm0MinRadius") == 0) *value = 2.0f;
    else if (std::strcmp(key, "swarm0MinSpeed") == 0) *value = 1.25f;
    else if (std::strcmp(key, "swarm0MaxSpeed") == 0) *value = 4.0f;
    else if (std::strcmp(key, "swarm0TurnRate") == 0) *value = 2.5f;
    else if (std::strcmp(key, "swarm0InteractionChance") == 0) *value = 1.0f;
    else if (std::strcmp(key, "swarm0InteractionTime") == 0) *value = 0.5f;
    else if (std::strcmp(key, "swarm0InteractionRadius") == 0) *value = 1.75f;
    else if (std::strcmp(key, "swarm0HostClearance") == 0) *value = 0.75f;
    else if (std::strcmp(key, "swarm0Separation") == 0) *value = 0.5f;
    else return false;
    return true;
}
bool get_bool(const char* key, std::uint8_t* value) {
    if (std::strcmp(key, "swarm0ReturnToHardpoint") == 0) *value = 1;
    else if (std::strcmp(key, "swarm0AvoidHost") == 0) *value = 1;
    else return false;
    return true;
}

void set_list(NativeStringVector* out, const char* name) {
    auto** values = static_cast<char**>(std::malloc(sizeof(char*)));
    require(values != nullptr, "vector allocation");
    const std::size_t length = std::strlen(name);
    values[0] = static_cast<char*>(std::malloc(length + 1));
    require(values[0] != nullptr, "string allocation");
    std::memcpy(values[0], name, length + 1);
    out->begin = values;
    out->end = values + 1;
    out->capacity = values + 1;
}
} // namespace

#ifdef A2FO_BUILTIN_FEATURE
namespace a2fo {
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept {
    return activation_token;
}
}
#endif

extern "C" std::uintptr_t __cdecl a2fo_swarm_call_thiscall_0(
    void* function, void* self) {
    if (function == image + kParameterDbDestructorRva) {
        trace("db:destroy"); return 1;
    }
    if (function == image + kGameObjectClassGetOdfNameRva) {
        if (mode == "class-throw") throw std::bad_alloc{};
        return reinterpret_cast<std::uintptr_t>(odf_name.c_str());
    }
    if (function == image + kGameObjectClassGetHierarchyRootRva)
        return reinterpret_cast<std::uintptr_t>(hierarchy.data());
    if (function == image + kSt3dInstanceConstructorRva) {
        std::memset(self, 0, kSt3dInstanceSize);
        trace("instance:construct"); return 1;
    }
    if (function == image + kSt3dInstanceDestructorRva) {
        trace("instance:destroy"); return 1;
    }
    if (function == image + kSt3dInstanceTriggerAnimationRva) {
        trace("instance:animate"); return 1;
    }
    if (function == image + kEntityGetTransformRva)
        return reinterpret_cast<std::uintptr_t>(&host_matrix);
    if (function == image + kEntityGetBoundingSphereRva)
        return reinterpret_cast<std::uintptr_t>(&host_sphere);
    require(false, "unknown thiscall0"); return 0;
}

extern "C" std::uintptr_t __cdecl a2fo_swarm_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1) {
    if (function == image + kParameterDbConstructorRva) return 1;
    if (function == image + 0x100) {
        ++gateway_simulations;
        trace("gateway:simulate:" + std::to_string(argument1));
#ifdef A2FO_BUILTIN_FEATURE
        if (mode == "hook-gateway-toggle") activation_token = 0x100000001ULL;
#endif
        return 1;
    }
    if (function == image + kBeeColonyShouldDrawBeeRva) {
        const std::uint32_t handle = read_at<std::uint32_t>(
            reinterpret_cast<void*>(argument1), 0xbc, 0);
        trace("visibility:" + std::to_string(handle));
        return visible_host ? 1 : 0;
    }
    if (function == image + kSt3dNodeFindRecursiveRva) {
        const char* name = reinterpret_cast<const char*>(argument1);
        trace(std::string("node:") + name);
        if (std::strcmp(name, "launch") == 0)
            return reinterpret_cast<std::uintptr_t>(launch_node.data());
        if (std::strcmp(name, "interaction") == 0)
            return reinterpret_cast<std::uintptr_t>(interaction_node.data());
        return 0;
    }
    if (function == image + kSt3dInstanceSetDatabaseRva) {
        const char* model = reinterpret_cast<const char*>(argument1);
        trace(std::string("instance:model:") + model);
        if (!model_failure) {
            put(self, kSt3dLogicalDatabaseOffset,
                static_cast<void*>(launch_node.data()));
            put(self, kSt3dBoundingSphereOffset + sizeof(Vec3), 2.0f);
        }
        return 1;
    }
    if (function == image + kSt3dInstanceScaleGeometryRva) {
        trace("instance:scale:" + std::to_string(argument1)); return 1;
    }
    if (function == image + kSt3dInstanceSetAnimationFlagsRva) {
        trace("instance:flags:" + std::to_string(argument1)); return 1;
    }
    if (function == image + kSt3dInstanceSetTransformRva) {
        const auto* transform = reinterpret_cast<const Matrix34*>(argument1);
        trace("instance:transform:" + bits(transform->values[9]) + ":" +
              bits(transform->values[10]) + ":" +
              bits(transform->values[11]));
        return 1;
    }
    if (function == image + kSt3dInstanceRenderRva) {
        ++render_count;
        trace("instance:render:" + std::to_string(render_count));
        return rendered_instance ? 1 : 0;
    }
    require(false, "unknown thiscall1"); return 0;
}

extern "C" std::uintptr_t __cdecl a2fo_swarm_call_thiscall_2(
    void* function, void*, std::uintptr_t argument1,
    std::uintptr_t argument2) {
    if (function == image + 0x110) {
        ++gateway_renders;
        trace("gateway:render:" + std::to_string(argument2));
        return 1;
    }
    if (function == image + kEntityGetWorldTransformRva) {
        auto* output = reinterpret_cast<Matrix34*>(argument1);
        *output = host_matrix;
        output->values[9] = argument2 ==
            reinterpret_cast<std::uintptr_t>(launch_node.data()) ? 2.0f : 0.0f;
        output->values[10] = argument2 ==
            reinterpret_cast<std::uintptr_t>(interaction_node.data()) ? 2.0f : 0.0f;
        return argument1;
    }
    require(false, "unknown thiscall2"); return 0;
}

extern "C" std::uintptr_t __cdecl a2fo_swarm_call_thiscall_3(
    void* function, void*, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t) {
    const char* key = reinterpret_cast<const char*>(argument1);
    trace(std::string("field:") + key);
    if (function == image + kParameterDbGetIntRva)
        return get_int(key, reinterpret_cast<int*>(argument2));
    if (function == image + kParameterDbGetFloatRva)
        return get_float(key, reinterpret_cast<float*>(argument2));
    if (function == image + kParameterDbGetBoolRva)
        return get_bool(key, reinterpret_cast<std::uint8_t*>(argument2));
    if (function == image + kParameterDbGetStringVectorRva) {
        auto* out = reinterpret_cast<NativeStringVector*>(argument2);
        if (std::strcmp(key, "swarm0Hardpoint") == 0) {
            set_list(out, " launch "); return 1;
        }
        if (std::strcmp(key, "swarm0Interaction") == 0) {
            set_list(out, "interaction"); return 1;
        }
        return 0;
    }
    require(false, "unknown thiscall3"); return 0;
}

extern "C" std::uintptr_t __cdecl a2fo_swarm_call_thiscall_4(
    void* function, void*, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t) {
    require(function == image + kParameterDbGetStringRva,
            "unknown thiscall4");
    const char* key = reinterpret_cast<const char*>(argument1);
    trace(std::string("field:") + key);
    if (std::strcmp(key, "swarm0") != 0) return 0;
    const char* model = mode == "parse-empty" ? "  " : " bee.sod ";
    auto* output = reinterpret_cast<char*>(argument2);
    require(argument3 > std::strlen(model), "string buffer size");
    std::strcpy(output, model);
    return 1;
}

namespace {
void set_precision() {
    if (mode.find("pc24") != std::string::npos) _controlfp(_PC_24, _MCW_PC);
    else if (mode.find("pc53") != std::string::npos) _controlfp(_PC_53, _MCW_PC);
    else if (mode.find("pc64") != std::string::npos) _controlfp(_PC_64, _MCW_PC);
}
void trace_policy(const ClassPolicy& policy) {
    trace("policy:" + std::to_string(policy.definitions.size()));
    for (const auto& d : policy.definitions) {
        trace("definition:" + std::to_string(d.index) + ":" + d.model +
              ":" + std::to_string(d.count) + ":" + bits(d.scale) +
              ":" + bits(d.minimum_radius) + ":" +
              bits(d.maximum_radius) + ":" +
              std::to_string(d.launch_hardpoints.size()) + ":" +
              std::to_string(d.interaction_hardpoints.size()));
    }
}
void trace_hosts(const char* label) {
    trace(std::string(label) + ":" + std::to_string(g_hosts.size()) +
          ":epoch=" + std::to_string(g_epoch) +
          ":countdown=" + std::to_string(g_host_discovery_countdown));
    for (const auto& entry : g_hosts) {
        const HostRuntime& host = entry.second;
        trace("host:" + std::to_string(entry.first) + ":" +
              std::to_string(host.groups.size()) + ":seen=" +
              std::to_string(host.last_seen_epoch));
        for (const auto& group : host.groups) {
            trace("group:" + std::to_string(group.definition) + ":" +
                  std::to_string(group.agents.size()));
            for (const auto& agent : group.agents) {
                if (!agent) continue;
                trace("agent:" + std::to_string(agent->identity) + ":" +
                      std::to_string(static_cast<int>(agent->phase)) + ":" +
                      bits(agent->position.x) + ":" + bits(agent->position.y) +
                      ":" + bits(agent->position.z) + ":" +
                      bits(agent->target.x) + ":" +
                      bits(agent->direction.z) + ":" + bits(agent->speed));
            }
        }
    }
}
void run() {
    setup_image();
    setup_host();
    set_precision();
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger;
    api.armada_module = armada;
    api.install_inline_hook = install;
    if (mode == "init-invalid") api.struct_size = 0;
    const bool accepted = A2FO_ModuleInit(&api);
    trace("init:" + std::to_string(accepted) + ":ready=" +
          std::to_string(g_runtime_ready) + ":hooks=" +
          std::to_string(install_count));
    if (mode.find("init-") == 0) {
        A2FO_ModuleShutdown();
        return;
    }
    require(accepted && g_runtime_ready, "runtime admission");
    if (mode == "parse-empty") odf_name = "";
    if (mode == "parse-no-hierarchy") {
        put(class_bytes.data(), 0x1d8, static_cast<void*>(nullptr));
    }
    if (mode == "class-throw") {
        trace("class:" + std::to_string(
            ensure_class_policy(class_bytes.data()) != nullptr));
    } else if (mode.find("parse-") == 0) {
        const ClassPolicy policy = parse_class_policy(class_bytes.data());
        trace_policy(policy);
    } else if (mode == "host-model-fail") {
        model_failure = true;
        const ClassPolicy policy = parse_class_policy(class_bytes.data());
        auto host = create_host_runtime(object.data(), policy);
        trace("created:" + std::to_string(host.groups.size()));
    } else if (mode == "host-create" || mode == "host-no-bounds") {
        const ClassPolicy policy = parse_class_policy(class_bytes.data());
        auto host = create_host_runtime(object.data(), policy);
        trace("created:" + std::to_string(host.groups.size()));
        for (const auto& group : host.groups)
            for (const auto& agent : group.agents)
                trace("created-agent:" + std::to_string(agent->identity) +
                      ":" + bits(agent->position.x) + ":" +
                      bits(agent->target.x) + ":" + bits(agent->speed));
    } else if (mode.find("simulate-") == 0) {
        simulate_swarms(0.125f);
        trace_hosts("first");
        if (mode != "simulate-expire") {
            require(g_hosts.count(77) != 0, "discovered host");
            auto& agents = g_hosts.at(77).groups.front().agents;
            require(agents.size() == 3, "three visual agents");
            agents[0]->phase = AgentPhase::approaching_interaction;
            agents[0]->target_hardpoint = 0;
            agents[0]->target_offset = {};
            agents[1]->phase = AgentPhase::dwelling_interaction;
            agents[1]->dwell_remaining = 0.01f;
            agents[2]->position = agents[0]->position;
        }
        simulate_swarms(0.25f);
        trace_hosts("second");
        if (mode == "simulate-expire") {
            put(object.data(), kObjectExpiredOffset, std::uint8_t{1});
            simulate_swarms(0.125f);
            trace_hosts("expired");
        }
    } else if (mode.find("render-") == 0) {
        visible_host = mode != "render-invisible";
        rendered_instance = mode != "render-frustum";
        const ClassPolicy policy = parse_class_policy(class_bytes.data());
        g_hosts.emplace(77, create_host_runtime(object.data(), policy));
        render_swarms(image + 0x300);
        trace("renders:" + std::to_string(render_count));
    } else if (mode.find("hook-") == 0) {
        void* worker = image + kWorkerBeeColonyRva;
        bee_colony_simulate_hook(image + 0x400, nullptr, 0.125f);
        bee_colony_render_hook(worker, nullptr, image + 0x300, 0x50607080);
#ifdef A2FO_BUILTIN_FEATURE
        activation_token = 0x100000001ULL;
        bee_colony_render_hook(worker, nullptr, image + 0x300, 0x50607080);
#endif
        bee_colony_simulate_hook(worker, nullptr, 0.125f);
        bee_colony_render_hook(worker, nullptr, image + 0x300, 0x50607080);
        trace_hosts("hook");
        trace("gateways:" + std::to_string(gateway_simulations) + ":" +
              std::to_string(gateway_renders));
    } else require(false, "unknown mode");
    A2FO_ModuleShutdown();
    trace("shutdown:" + std::to_string(g_hosts.size()) + ":" +
          std::to_string(g_class_policies.size()) + ":" +
          std::to_string(g_runtime_ready));
}
} // namespace

int main(int argc, char** argv) {
    require(argc == 2, "mode argument");
    mode = argv[1];
    run();
    return 0;
}
