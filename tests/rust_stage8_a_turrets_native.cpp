#include MODULE_SOURCE

#include <algorithm>
#include <array>
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string mode;
std::vector<std::string> records;
std::map<std::string, std::string> parameters;
std::map<std::uint32_t, void*> entities;
std::string original_label = "hull";
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::array<std::uint8_t, 0x200> parent_object{}, child_object{},
    target_object{}, parent_class{}, turret_class{}, hierarchy{}, node{};
std::array<std::uint8_t, 0x80> parent_command{}, child_command{};
Matrix34 parent_matrix{}, target_matrix{};
void* weapon_owner_value = nullptr;
std::uintptr_t get_command_count = 0;
std::uintptr_t gateway_count = 0;
std::uintptr_t construction_count = 0;
std::uintptr_t hook_count = 0;
std::array<std::uint8_t, 0x200> duplicate_child{};

void trace(const std::string& value) { records.push_back(value); }
void check(bool condition, const char* label) {
    if (!condition) {
        std::cerr << "fixture:" << label << '\n';
        std::exit(1);
    }
}
template <typename T> void put(void* object, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value,
                sizeof(value));
}
std::string bits(float value) {
    std::uint32_t word = 0;
    std::memcpy(&word, &value, sizeof(word));
    return std::to_string(word);
}
void jump_to(std::uint8_t* site, void* destination) {
    const auto delta = reinterpret_cast<std::intptr_t>(destination) -
        reinterpret_cast<std::intptr_t>(site + 5);
    const auto relative = static_cast<std::int32_t>(delta);
    site[0] = 0xe9;
    std::memcpy(site + 1, &relative, sizeof(relative));
}
void* __cdecl entity_get(std::uint32_t handle) {
    trace("entity:" + std::to_string(handle));
    const auto found = entities.find(handle);
    return found == entities.end() ? nullptr : found->second;
}
void* __cdecl class_find(const char* name) {
    trace(std::string("class-find:") + name);
    return mode == "spawn-missing-class" ? nullptr : turret_class.data();
}
void* __cdecl current_mission() { trace("mission:get"); return image + 0x300; }
void* __cdecl class_name(void* klass) {
    return klass == parent_class.data()
        ? const_cast<char*>("parent.odf") : const_cast<char*>("turret.odf");
}
void* __cdecl hierarchy_root(void*) { trace("hierarchy:get"); return hierarchy.data(); }
void __cdecl mission_add(void*, void* object) {
    trace(std::string("mission:add:") +
        (object == child_object.data() ? "child" : "other"));
}
void setup_image() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x400000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x200000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    check(image && fleet, "images");
    g_armada = reinterpret_cast<HMODULE>(image);
    g_fleet_ops = reinterpret_cast<HMODULE>(fleet);
    std::memcpy(image + kGameObjectClassConstructorRva,
        kExpectedGameObjectClassConstructor,
        sizeof(kExpectedGameObjectClassConstructor));
    std::memcpy(image + kWeaponSetTargetRva,
        kExpectedWeaponSetTarget, sizeof(kExpectedWeaponSetTarget));
    std::memcpy(image + kCraftGetAlertStatusRva,
        kExpectedCraftGetAlertStatus,
        sizeof(kExpectedCraftGetAlertStatus));
    std::memcpy(image + kCraftSetAlertStatusRva,
        kExpectedCraftSetAlertStatus,
        sizeof(kExpectedCraftSetAlertStatus));
    std::memcpy(image + kCraftGetSpecialWeaponAutonomyRva,
        kExpectedCraftGetSpecialWeaponAutonomy,
        sizeof(kExpectedCraftGetSpecialWeaponAutonomy));
    std::memcpy(image + kCraftSetSpecialWeaponAutonomyRva,
        kExpectedCraftSetSpecialWeaponAutonomy,
        sizeof(kExpectedCraftSetSpecialWeaponAutonomy));
    std::memcpy(image + kGameObjectGetCurrentCommandRva,
        kExpectedGameObjectGetCurrentCommand,
        sizeof(kExpectedGameObjectGetCurrentCommand));
    std::memcpy(image + kGameObjectSetTargetlessCommandRva,
        kExpectedGameObjectSetTargetlessCommand,
        sizeof(kExpectedGameObjectSetTargetlessCommand));
    std::memcpy(image + kGameObjectSetObjectCommandRva,
        kExpectedGameObjectSetObjectCommand,
        sizeof(kExpectedGameObjectSetObjectCommand));
    if (mode == "init-patch-false") {
        std::memcpy(fleet + kFoGameObjectClassConstructorHandlerRva,
            kExpectedFoGameObjectClassConstructorHandler,
            sizeof(kExpectedFoGameObjectClassConstructorHandler));
        image[kGameObjectClassConstructorRva] = 0x68;
        const auto address = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(fleet +
                kFoGameObjectClassConstructorHandlerRva));
        std::memcpy(image + kGameObjectClassConstructorRva + 1,
            &address, sizeof(address));
        image[kGameObjectClassConstructorRva + 5] = 0xc3;
    }
    jump_to(image + kEntityGetRva, reinterpret_cast<void*>(&entity_get));
    jump_to(image + kGameObjectClassFindRva,
            reinterpret_cast<void*>(&class_find));
    jump_to(image + kAiMissionGetCurrentRva,
            reinterpret_cast<void*>(&current_mission));
    void* add = reinterpret_cast<void*>(&mission_add);
    put(image + 0x380, 0x18, add);
    put(image + 0x300, 0, static_cast<void*>(image + 0x380));
    for (auto& matrix : {&parent_matrix, &target_matrix}) {
        matrix->values[0] = matrix->values[4] = matrix->values[8] = 1.0f;
    }
    parent_matrix.values[9] = 10.0f;
    target_matrix.values[9] = 14.0f;
    target_matrix.values[10] = 1.25f;
    put(parent_object.data(), kObjectHandleOffset, std::uint32_t{11});
    put(parent_object.data(), kObjectClassOffset,
        static_cast<void*>(parent_class.data()));
    put(parent_object.data(), kObjectTeamOffset, std::int32_t{2});
    put(child_object.data(), kObjectHandleOffset, std::uint32_t{12});
    put(child_object.data(), kObjectClassOffset,
        static_cast<void*>(turret_class.data()));
    put(child_object.data(), kObjectTeamOffset, std::int32_t{1});
    put(target_object.data(), kObjectHandleOffset, std::uint32_t{13});
    entities[11] = parent_object.data();
    entities[12] = child_object.data();
    entities[13] = target_object.data();
    weapon_owner_value = child_object.data();
    g_runtime_ready = true;
}
void setup_api() {
    static A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = [](const char*, const char* text) {
        trace(std::string("log:") + text);
    };
    api.armada_module = []()->void* { trace("api:armada"); return image; };
    api.fleetops_module = []()->void* { trace("api:fleet"); return fleet; };
    api.get_original_classlabel = [](void*, char* output,
                                     std::uint32_t size)->bool {
        trace("api:classlabel");
        std::snprintf(output, size, "%s", original_label.c_str());
        return mode != "classlabel-false";
    };
    api.register_classlabel_alias = [](auto...)->bool {
        trace("api:alias"); return mode != "init-alias-false";
    };
    api.register_classlabel_odf_defaults = [](auto...)->bool {
        trace("api:defaults"); return mode != "init-defaults-false";
    };
    api.register_weapon_trigger_handler = [](auto...)->bool {
        trace("api:weapon"); return mode != "init-weapon-false";
    };
    api.register_weapon_trigger_handler_masked = [](auto...)->bool {
        trace("api:weapon-masked"); return mode != "init-weapon-false";
    };
    api.register_craft_event_handler = [](auto...)->bool {
        trace("api:craft"); return mode != "init-craft-false";
    };
    api.install_inline_hook = [](void* site, void*, std::size_t length,
        const std::uint8_t* expected, A2FO_InlineHook* hook)->bool {
        ++hook_count;
        trace("api:hook:" + std::to_string(hook_count));
        check(std::memcmp(site, expected, length) == 0, "hook signature");
        if (mode == "init-hook-throw") throw std::bad_alloc{};
        if (mode == "init-hook-false") return false;
        hook->gateway = image + 0x100;
        return true;
    };
    api.patch_jump = [](auto...)->bool {
        trace("api:patch"); return mode != "init-patch-false";
    };
    api.capabilities = A2FO_CAP_ORIGINAL_CLASSLABEL |
        A2FO_CAP_CLASSLABEL_ODF_DEFAULTS |
        A2FO_CAP_WEAPON_TRIGGER_EVENTS | A2FO_CAP_CRAFT_EVENTS;
    g_api = &api;
}
void print_state() {
    trace("state:classes=" + std::to_string(g_turret_classes.size()) +
          ":parents=" + std::to_string(g_parent_classes.size()) +
          ":hosts=" + std::to_string(g_parents.size()) +
          ":children=" + std::to_string(g_children.size()) +
          ":targets=" + std::to_string(g_targets.size()));
    for (const auto& [handle, child] : g_children) {
        trace("child:" + std::to_string(handle) + ":" +
            std::to_string(child.parent_handle) + ":" +
            std::to_string(child.mount_index) + ":" +
            std::to_string(child.ordered_target_handle) + ":" +
            bits(child.current.yaw_degrees) + ":" +
            bits(child.current.pitch_degrees));
    }
    for (const auto& [handle, target] : g_targets) {
        trace("target:" + std::to_string(handle) + ":" +
            std::to_string(target));
    }
}
} // namespace

extern "C" std::uintptr_t __cdecl a2fo_turret_call_thiscall_0(
    void* function, void* self) {
    if (function == image + kGameObjectClassGetOdfNameRva)
        return reinterpret_cast<std::uintptr_t>(class_name(self));
    if (function == image + kGameObjectClassGetHierarchyRootRva)
        return reinterpret_cast<std::uintptr_t>(hierarchy_root(self));
    if (function == image + kWeaponGetOwnerRva)
        return reinterpret_cast<std::uintptr_t>(weapon_owner_value);
    if (function == image + kGameObjectGetCurrentCommandRva) {
        ++get_command_count;
        trace("command:get:" + std::to_string(get_command_count));
        return reinterpret_cast<std::uintptr_t>(
            self == parent_object.data() ? parent_command.data()
                                         : child_command.data());
    }
    if (function == image + kEntityGetTransformRva)
        return reinterpret_cast<std::uintptr_t>(&target_matrix);
    if (function == image + kCraftDoExpireRva) {
        trace("craft:expire"); return 1;
    }
    if (function == image + kCraftGetAlertStatusRva) return 3;
    if (function == image + kCraftGetSpecialWeaponAutonomyRva) return 4;
    check(false, "thiscall0"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_turret_call_thiscall_1(
    void* function, void* self, std::uintptr_t arg) {
    if (function == image + 0x100) {
        ++gateway_count;
        if (mode == "weapon-callback-mutation") g_targets[12] = 11;
        trace("gateway:" + std::to_string(gateway_count)); return 1;
    }
    if (function == image + kSt3dNodeFindRecursiveRva) {
        trace(std::string("node:") + reinterpret_cast<const char*>(arg));
        return mode == "spawn-missing-node" ? 0 :
            reinterpret_cast<std::uintptr_t>(node.data());
    }
    if (function == image + kGameObjectSetTransformRva) {
        const auto* matrix = reinterpret_cast<const Matrix34*>(arg);
        trace("transform:set:" + bits(matrix->values[9]) + ":" +
              bits(matrix->values[10]) + ":" +
              bits(matrix->values[11])); return 1;
    }
    if (function == image + kGameObjectSwapTeamRva ||
        function == image + kCraftSetAlertStatusRva ||
        function == image + kCraftSetSpecialWeaponAutonomyRva) {
        trace("policy:set:" + std::to_string(arg)); return 1;
    }
    if (function == reinterpret_cast<void*>(&mission_add)) {
        mission_add(self, reinterpret_cast<void*>(arg)); return 1;
    }
    check(false, "thiscall1"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_turret_call_thiscall_2(
    void* function, void* self, std::uintptr_t first,
    std::uintptr_t second) {
    if (function == image + kEntityGetWorldTransformRva) {
        trace("transform:get");
        *reinterpret_cast<Matrix34*>(first) = parent_matrix;
        return first;
    }
    if (function == image + kGameObjectSwapRaceAndTeamRva) {
        trace("race:swap:" + std::to_string(second)); return 1;
    }
    if (function == image + 0x100) {
        trace("class:constructor"); return reinterpret_cast<std::uintptr_t>(self);
    }
    check(false, "thiscall2"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_turret_call_thiscall_4(
    void* function, void*, std::uintptr_t first, std::uintptr_t second,
    std::uintptr_t third, std::uintptr_t fourth) {
    if (function == image + kParameterDbGetStringRva) {
        const char* key = reinterpret_cast<const char*>(first);
        trace(std::string("db:") + key);
        const auto found = parameters.find(key);
        if (found == parameters.end()) return 0;
        std::snprintf(reinterpret_cast<char*>(second), third, "%s",
                      found->second.c_str());
        return 1;
    }
    if (function == image + kGameObjectClassConstructRva) {
        ++construction_count;
        trace(std::string("construct:") + reinterpret_cast<const char*>(fourth) +
              ":team=" + std::to_string(second));
        return mode == "spawn-null" ? 0 :
            reinterpret_cast<std::uintptr_t>(child_object.data());
    }
    if (function == image + kGameObjectSetObjectCommandRva ||
        function == image + kGameObjectSetTargetlessCommandRva) {
        trace("command:set:" + std::to_string(first) + ":" +
              std::to_string(third));
        return 1;
    }
    check(false, "thiscall4"); return 0;
}

int main(int argc, char** argv) {
    check(argc == 2, "mode");
    mode = argv[1];
    if (mode.find("pc24") != std::string::npos) _controlfp(_PC_24, _MCW_PC);
    if (mode.find("pc53") != std::string::npos) _controlfp(_PC_53, _MCW_PC);
    if (mode.find("pc64") != std::string::npos) _controlfp(_PC_64, _MCW_PC);
    setup_image();
    setup_api();
    if (mode.find("parse") == 0 || mode == "classlabel-false") {
        original_label = mode == "parse-turret" ? "TuRrEt" : "hull";
        parameters["turret1"] = "  Child.ODF  ";
        parameters["turretHardpoint1"] = " mount ";
        parameters["turret2"] = "orphan.odf";
        parameters["turretYawMin"] = "-190.5f";
        parameters["turretYawMax"] = "45.0";
        parameters["turretReturnToRest"] = "YeS";
        register_class_policy(parent_class.data(), parent_object.data());
    } else if (mode.find("spawn") == 0 || mode.find("update") == 0) {
        ParentClassConfig parent{};
        parent.mounts.push_back(MountClassConfig{1,"child","mount"});
        g_parent_classes[parent_class.data()] = parent;
        TurretClassConfig config{};
        g_turret_classes[turret_class.data()] = config;
        spawn_pending_turrets(parent_object.data());
        if (mode.find("update") == 0) {
            g_targets[12] = 13;
            update_linked_turret(child_object.data(), 12, 0.125f);
        }
    } else if (mode.find("loaded") == 0) {
        g_turret_classes[turret_class.data()] = TurretClassConfig{};
        ParentClassConfig parent{};
        parent.mounts.push_back(MountClassConfig{1,"child","mount"});
        g_parent_classes[parent_class.data()] = parent;
        static const char label[] = "A2FOT:0000000B:1";
        put(child_object.data(), kObjectLabelOffset, label);
        if (mode == "loaded-absent-parent") entities.erase(11);
        if (mode == "loaded-missing-mount")
            g_parent_classes[parent_class.data()].mounts.clear();
        attach_loaded_turret(child_object.data());
        if (mode == "loaded-duplicate") {
            duplicate_child = child_object;
            put(duplicate_child.data(), kObjectHandleOffset, std::uint32_t{14});
            entities[14] = duplicate_child.data();
            attach_loaded_turret(duplicate_child.data());
        }
    } else if (mode.find("weapon") == 0) {
        g_weapon_set_target_hook.gateway = image + 0x100;
        g_turret_classes[turret_class.data()] = TurretClassConfig{};
        g_children[12] = ChildRuntime{11,1,13,{}};
        A2FO_WeaponTriggerEvent event{};
        event.struct_size = sizeof(event);
        event.kind = mode == "weapon-committed"
            ? A2FO_WEAPON_TRIGGER_COMMITTED : A2FO_WEAPON_TRIGGER_PRECHECK;
        event.weapon = image + 0x400;
        event.target = mode == "weapon-match" ? target_object.data()
            : parent_object.data();
        if (mode == "weapon-no-owner") weapon_owner_value = nullptr;
        if (mode == "weapon-expired-target")
            put(target_object.data(), kObjectExpiredOffset, std::uint8_t{1});
        trace("weapon:result:" + std::to_string(
            weapon_trigger_handler(&event, nullptr)));
        weapon_set_target_hook(event.weapon, nullptr, target_object.data());
    } else if (mode.find("cleanup") == 0) {
        ParentRuntime parent{};
        parent.mounts.push_back(MountRuntime{});
        parent.mounts.back().config.index = 1;
        parent.mounts.back().child_handle = 12;
        parent.mounts.back().status = MountStatus::active;
        g_parents[11] = parent;
        g_children[12] = ChildRuntime{11,1,13,{}};
        g_targets[12] = 13;
        if (mode == "cleanup-child-disappeared") entities.erase(12);
        A2FO_CraftEvent event{};
        event.struct_size = sizeof(event);
        event.kind = A2FO_CRAFT_EVENT_CLEANUP;
        event.craft = parent_object.data();
        craft_event_handler(&event, nullptr);
    } else if (mode == "event-short") {
        A2FO_CraftEvent event{};
        event.struct_size = sizeof(event) - 1;
        event.craft = reinterpret_cast<void*>(1);
        craft_event_handler(&event, nullptr);
        event.struct_size = sizeof(event);
        g_runtime_ready = false;
        craft_event_handler(&event, nullptr);
        trace("event:short-safe");
    } else if (mode.find("init") == 0) {
        g_runtime_ready = false;
        auto* api = const_cast<A2FO_ModuleApi*>(g_api);
        if (mode == "init-invalid") api->struct_size = 0;
        trace("init:" + std::to_string(A2FO_ModuleInit(api)) +
              ":ready=" + std::to_string(g_runtime_ready));
    } else {
        check(false, "unknown mode");
    }
    print_state();
    for (const auto& record : records) std::cout << record << '\n';
}
