// Frozen/current full-source native Squadron workflow comparison. All images,
// entities and UI objects are synthetic; no game, GUI or input is opened.
#include MODULE_SOURCE

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
std::string mode;
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::map<std::uint32_t, void*> entities;
std::array<std::uint8_t, 0x500> station{}, peer{}, yard{};
std::array<std::uint8_t, 0x500> replacement{};
std::array<std::uint8_t, 0x100> station_geometry{}, peer_geometry{}, yard_geometry{};
std::array<std::uint8_t, 0x100> station_class{}, peer_class{}, yard_class{};
std::array<std::uint8_t, 0x500> selection{};
unsigned gateway_calls = 0;
unsigned mutation_calls = 0;
unsigned hooks = 0;
unsigned registrations = 0;
bool reentrant_class_loaded = false;
unsigned render_calls = 0;
a2fo::squadrons::SquadId active_squad_id = 0;
unsigned constructions = 0;
unsigned queue_adds = 0;
unsigned expirations = 0;

void emit_class(void* klass, const char* source_name,
                const A2FO_OdfFieldView* fields,
                std::uint32_t field_count);
void set_precision();
std::string bits(float value);

void check(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}
void trace(const std::string& text) { std::puts(text.c_str()); }
void A2FO_CALL logger(const char*, const char* message) {
    trace(std::string("log:") + message);
}
void* A2FO_CALL armada() {
    trace("api:armada");
    if (mode == "init-armada-throw")
        throw std::runtime_error("armada injected");
    return mode == "init-no-armada" ? nullptr : image;
}
void* A2FO_CALL fleetops() {
    trace("api:fleetops");
    return mode == "init-no-fleet" ? nullptr : fleet;
}
void put(void* target, std::size_t offset, const void* data, std::size_t size) {
    std::memcpy(static_cast<std::uint8_t*>(target) + offset, data, size);
}
template <typename T> void put(void* target, std::size_t offset, T value) {
    put(target, offset, &value, sizeof(value));
}
void jump_to(std::uint8_t* address, void* target) {
    const auto delta = reinterpret_cast<std::intptr_t>(target) -
        reinterpret_cast<std::intptr_t>(address + 5);
    const auto relative = static_cast<std::int32_t>(delta);
    address[0] = 0xe9;
    std::memcpy(address + 1, &relative, sizeof(relative));
}
void* __cdecl entity_get(std::uint32_t handle) {
    trace("entity:" + std::to_string(handle));
    const auto found = entities.find(handle);
    return found == entities.end() ? nullptr : found->second;
}
void* __cdecl class_find(const char* name) {
    trace(std::string("class-find:") + (name ? name : "<null>"));
    if (name && std::strcmp(name, "fighter2") == 0 &&
        mode == "econ-reentrant" && !reentrant_class_loaded) {
        reentrant_class_loaded = true;
        std::array<A2FO_OdfFieldView, 2> fields{{
            {{"classLabel", 10}, {"craft", 5}},
            {{"tritaniumCost", 13}, {"5", 1}},
        }};
        put(yard_class.data(), kClassBuildTimeOffset, 1.0f);
        emit_class(yard_class.data(), "fighter2.odf", fields.data(), fields.size());
        trace("mutation:class-reloaded");
        return yard_class.data();
    }
    return name && std::strcmp(name, "fighter") == 0 ? peer_class.data() : nullptr;
}
const char* __attribute__((fastcall)) class_name(void*, void*) {
    trace("class-name");
    return "fighter";
}
std::uint32_t __cdecl push_repair(void* producer, void* target) {
    trace("push-repair:" +
          std::to_string(object_handle(producer)) + ":" +
          std::to_string(target == peer_class.data()));
    return 77;
}
bool __cdecl cancel_repair(void* producer, std::uint32_t job) {
    trace("cancel-repair:" +
          std::to_string(object_handle(producer)) + ":" +
          std::to_string(job));
    if (mode == "repair-cancel-reenter") {
        trace("cancel-reenter-absent:" +
              std::to_string(g_repair_visits.count(active_squad_id) == 0));
        RepairVisit renewed;
        renewed.yard = 21;
        g_repair_visits.emplace(active_squad_id, renewed);
    }
    return true;
}
void* __cdecl current_mission() {
    trace("mission-current");
    return nullptr;
}
std::uintptr_t __attribute__((fastcall)) expire_member(void* craft, void*) {
    ++expirations;
    trace("expire:" + std::to_string(object_handle(craft)));
    return 1;
}
std::uintptr_t __attribute__((fastcall)) ordinary_finish(void* producer, void*) {
    trace("finish-original:" + std::to_string(object_handle(producer)));
    return reinterpret_cast<std::uintptr_t>(station.data());
}
std::uintptr_t __attribute__((fastcall)) pop_build_item(void* producer, void*) {
    trace("pop-build:" + std::to_string(object_handle(producer)));
    return 1;
}
std::uintptr_t __attribute__((fastcall)) stop_effect(void* producer, void*) {
    trace("stop-effect:" + std::to_string(object_handle(producer)));
    return 1;
}
std::uintptr_t __attribute__((fastcall)) transform_provider(
    void* producer, void*, Matrix34* output) {
    trace("transform:" + std::to_string(object_handle(producer)));
    output->values[9] = 12.0f;
    return reinterpret_cast<std::uintptr_t>(output);
}
std::uintptr_t __attribute__((fastcall)) queue_add(
    void* queue, void*, void* craft) {
    (void)queue;
    ++queue_adds;
    trace("queue-add:" + std::to_string(object_handle(craft)));
    put(craft, kCraftQueueStageOffset, std::uint32_t{1});
    put(craft, kCraftQueueOwnerOffset, std::uint32_t{21});
    return 1;
}
void* __attribute__((fastcall)) construct_member_native(
    void* klass, void*, const Matrix34* transform, std::uintptr_t team,
    std::uintptr_t, const char* label) {
    ++constructions;
    trace("construct:" + std::to_string(constructions) + ":" +
          std::to_string(klass == peer_class.data()) + ":" +
          std::to_string(team) + ":" + label + ":" +
          bits(transform->values[9]));
    if (((mode == "launch-first-fail" || mode == "finish-spawn-fail") &&
         constructions == 1) ||
        (mode == "launch-second-fail" && constructions == 2)) return nullptr;
    return constructions == 1 ? station.data() : peer.data();
}
void setup_image() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x400000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x400000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    check(image && fleet, "synthetic images");
    g_armada = reinterpret_cast<HMODULE>(image);
    g_fleet_ops = reinterpret_cast<HMODULE>(fleet);
    jump_to(image + kEntityGetRva, reinterpret_cast<void*>(&entity_get));
    jump_to(image + kGameObjectClassFindRva, reinterpret_cast<void*>(&class_find));
    jump_to(image + kGameObjectClassGetOdfNameRva,
            reinterpret_cast<void*>(&class_name));
    jump_to(image + kAiMissionGetCurrentRva,
            reinterpret_cast<void*>(&current_mission));
    jump_to(image + kCraftDoExpireRva,
            reinterpret_cast<void*>(&expire_member));
    jump_to(image + kGameObjectClassConstructRva,
            reinterpret_cast<void*>(&construct_member_native));
    jump_to(image + kProducerPopBuildQueueItemRva,
            reinterpret_cast<void*>(&pop_build_item));
}
void setup_init_image() {
    const std::uintptr_t getters[]{0xce170, 0xce190, 0xce180,
                                    0xce1b0, 0xce1a0, 0xce1c0};
    for (std::size_t index = 0; index < 6; ++index) {
        const std::uint8_t expected[]{0x8b, 0x81,
            static_cast<std::uint8_t>(0x80 + index * 4), 0, 0, 0, 0xc3};
        std::memcpy(image + getters[index], expected, sizeof(expected));
    }
    const std::uint8_t build_time[]{0x8b, 0x47, 0x68};
    std::memcpy(image + 0xce29f, build_time, sizeof(build_time));
    std::memcpy(image + kAiMissionGetCurrentRva,
                kExpectedAiMissionGetCurrent, sizeof(kExpectedAiMissionGetCurrent));
    std::memcpy(image + kGameObjectClassFindRva,
                kExpectedGameObjectClassFind, sizeof(kExpectedGameObjectClassFind));
    std::memcpy(image + kGameObjectClassConstructRva,
                kExpectedGameObjectClassConstruct, sizeof(kExpectedGameObjectClassConstruct));
    std::memcpy(image + kGameObjectClassGetOdfNameRva,
                kExpectedGameObjectClassGetOdfName, sizeof(kExpectedGameObjectClassGetOdfName));
    std::memcpy(image + kStarbaseFinishBuildRva,
                kExpectedStarbaseFinishBuild, sizeof(kExpectedStarbaseFinishBuild));
    std::memcpy(image + kEntityGetRva,
                kExpectedEntityGet, sizeof(kExpectedEntityGet));
    std::memcpy(image + kProducerPopBuildQueueItemRva,
                kExpectedProducerPopBuildQueueItem, sizeof(kExpectedProducerPopBuildQueueItem));
    put(image + kStarbaseVtableRva, kStarbaseFinishBuildVtableOffset,
        static_cast<void*>(image + kStarbaseFinishBuildRva));
    put(image + kShipyardVtableRva, kStarbaseFinishBuildVtableOffset,
        static_cast<void*>(image + kStarbaseFinishBuildRva));
    if (mode == "init-finish-first") {
        put(image + kStarbaseVtableRva, kStarbaseFinishBuildVtableOffset,
            static_cast<void*>(nullptr));
    }
    if (mode == "init-finish-second") {
        put(image + kShipyardVtableRva, kStarbaseFinishBuildVtableOffset,
            static_cast<void*>(nullptr));
    }
    const std::uint8_t pod_initialize[]{0x89,0x86,0xac,0x02,0,0};
    const std::uint8_t pod_list[]{0x89,0x86,0xb0,0x02,0,0};
    const std::uint8_t pod_child[]{0x8b,0x8e,0xb0,0x02,0,0,0x89,0x04,0x99};
    const std::uint8_t pod_remove[]{0x8b,0x8e,0xb0,0x02,0,0,0xc7,0x04,0x81,0,0,0,0};
    std::memcpy(image + 0xb9a5a, pod_initialize, sizeof(pod_initialize));
    std::memcpy(image + 0xb9a6c, pod_list, sizeof(pod_list));
    std::memcpy(image + 0xba68f, pod_child, sizeof(pod_child));
    std::memcpy(image + 0xba7ea, pod_remove, sizeof(pod_remove));
    put(image + kRepairQueueVtableRva, 0x2c,
        static_cast<void*>(image + kRepairActivateRva));
    const std::uint8_t repair_activate[]{0x55,0x8b,0xec,0x51,0x53};
    std::memcpy(image + kRepairActivateRva,
                repair_activate, sizeof(repair_activate));
    const std::uint8_t draw_text[]{0x55,0x8b,0xec,0x83,0xec,0x10};
    std::memcpy(image + kDrawTextRva, draw_text, sizeof(draw_text));
    image[0x1000] = 0xc3;
    put(image + kShipDisplayVtableRva, kShipDisplayRenderSlot,
        static_cast<void*>(image + 0x1000));
    std::memcpy(fleet + kFoSelectionAddRva,
                kExpectedFoSelectionAdd, sizeof(kExpectedFoSelectionAdd));
    const std::uint8_t repair_needs[]{0x55,0x8b,0xec,0x83,0xc4,0xcc};
    std::memcpy(fleet + kFoNeedsRepairRva,
                repair_needs, sizeof(repair_needs));
    if (mode == "init-signature") image[0xce170] = 0;
}
void seed_object(std::array<std::uint8_t, 0x500>& object,
                 std::uint32_t handle, void* klass, std::int32_t team,
                 std::array<std::uint8_t, 0x100>& geometry) {
    put(object.data(), kObjectHandleOffset, handle);
    put(object.data(), kObjectClassOffset, klass);
    put(object.data(), kObjectTeamOffset, team);
    put(object.data(), kObjectExpiredOffset, std::uint8_t{0});
    put(object.data(), 4, static_cast<void*>(geometry.data()));
    entities[handle] = object.data();
}
void seed_objects() {
    seed_object(station, 11, station_class.data(), 2, station_geometry);
    seed_object(peer, 12, peer_class.data(), 2, peer_geometry);
    seed_object(yard, 21, yard_class.data(), 2, yard_geometry);
    seed_object(replacement, 13, peer_class.data(), 2, peer_geometry);
    put(station_geometry.data(), 0x68, 0.0f);
    put(peer_geometry.data(), 0x68, 32.0f);
    put(yard_geometry.data(), 0x68, 8.0f);
    put(yard_geometry.data(), 0x40, 64.0f);
    put(station.data(), 0x1cc, 16777216.0f);
    put(peer.data(), 0x1cc, 3.0f);
}
a2fo::squadrons::SquadId seed_squad(bool missing_peer = false) {
    a2fo::squadrons::Definition definition;
    definition.odf = "squad";
    definition.members.push_back({0, "fighter", 2});
    const std::vector<a2fo::squadrons::Craft> members{
        {11, 2, "fighter"}, {12, 2, "fighter"}};
    const auto result = g_registry.create(definition, 2, members,
        [](const std::string&) { return MemberClass::mobile_craft; });
    check(static_cast<bool>(result), "seed squad");
    if (missing_peer) g_registry.remove(12);
    return *result.value;
}
void __stdcall selection_gateway(void* selected, void* craft,
    std::int32_t operation, std::int32_t clear, std::int32_t sound,
    std::int32_t compatibility) {
    const auto handle = object_handle(craft);
    trace("gateway:" + std::to_string(handle) + ":" +
          std::to_string(operation) + ":" + std::to_string(clear) +
          ":" + std::to_string(sound) + ":" + std::to_string(compatibility));
    ++gateway_calls;
    if (clear) put(selected, kNativeSelectionCountOffset, std::uint32_t{0});
    auto count = read_at<std::uint32_t>(selected, kNativeSelectionCountOffset, 0);
    if (operation == 0 && count < kNativeSelectionLimit) {
        put(selected, kNativeSelectionHandlesOffset + count * 4, handle);
        put(selected, kNativeSelectionCountOffset, count + 1);
        put(craft, kNativeSelectedFlagOffset, std::uint8_t{1});
    }
    if (mode == "selection-mutation" && gateway_calls == 1) {
        ++mutation_calls;
        g_registry.remove(12);
        trace("mutation:registry-peer-removed");
    }
}
void __stdcall poison_gateway(void*, void*, std::int32_t mode_value,
                             std::int32_t, std::int32_t, std::int32_t) {
    trace("poison-gateway:" + std::to_string(mode_value));
}
void setup_api() {
    static A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &logger;
    api.armada_module = &armada;
    api.fleetops_module = &fleetops;
    api.capabilities = A2FO_CAP_CLASSLABEL_ODF_DEFAULTS |
        A2FO_CAP_GAME_OBJECT_CLASS_LOADED |
        A2FO_CAP_PRODUCER_EVENTS | A2FO_CAP_CRAFT_EVENTS;
    api.register_classlabel_alias = [](auto...)->bool {
        trace("api:alias"); return true;
    };
    api.register_classlabel_odf_defaults = [](auto...)->bool {
        trace("api:defaults"); return true;
    };
    api.register_game_object_class_loaded_handler = [](auto...)->bool {
        ++registrations;
        trace("api:class-handler:" + std::to_string(registrations));
        if (mode == "init-class-throw" && registrations == 1)
            throw std::runtime_error("class registration injected");
        if (mode == "init-register-badalloc" && registrations == 1)
            throw std::bad_alloc();
        return !((mode == "init-register-fail" && registrations == 1) ||
                 (mode == "init-yard-first-fail" && registrations == 2) ||
                 (mode == "init-yard-second-fail" && registrations == 3));
    };
    api.register_producer_event_handler = [](auto...)->bool {
        trace("api:producer-handler"); return true;
    };
    api.register_craft_event_handler = [](auto...)->bool {
        trace("api:craft-handler"); return true;
    };
    api.register_craft_event_handler_masked = [](auto...)->bool {
        trace("api:craft-masked"); return true;
    };
    api.install_inline_hook = [](void* target, void*, std::size_t length,
                                 const std::uint8_t* expected,
                                 A2FO_InlineHook* hook)->bool {
        ++hooks;
        trace("api:hook:" + std::to_string(hooks));
        if (mode == "init-hook-throw")
            throw std::runtime_error("hook injected");
        check(std::memcmp(target, expected, length) == 0, "hook signature");
        if ((mode == "init-hook-repair-fail" && hooks == 1) ||
            (mode == "init-hook-selection-fail" && hooks == 2)) return false;
        hook->gateway = reinterpret_cast<void*>(&selection_gateway);
        return true;
    };
    g_api = &api;
}
void fields_case() {
    const char* names[]{"classLabel", "squadMember0", "ignored", "empty"};
    const char* values[]{"  \"Research\"  ", " 'fighter' ", "   ", ""};
    std::array<A2FO_OdfFieldView, 4> source{};
    for (std::size_t i = 0; i < source.size(); ++i) {
        source[i].name = {names[i], std::strlen(names[i])};
        source[i].value = {values[i], std::strlen(values[i])};
    }
    A2FO_GameObjectClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.odf_fields = source.data();
    event.odf_field_count = source.size();
    const auto fields = copy_fields(&event);
    trace("fields:" + std::to_string(fields.size()) + ":" +
          field_value(fields, "classLabel") + ":" +
          field_value(fields, "squadMember0") + ":" +
          field_value(fields, "ignored") + ":" +
          field_value(fields, "absent"));
    trace("lower:" + lower_ascii("ReSeArCh"));
    check(field_value(fields, "classLabel") == "Research", "trim quote");
    check(field_value(fields, "ignored").empty(), "whitespace field");
}
void emit_class(void* klass, const char* source_name,
                const A2FO_OdfFieldView* fields,
                std::uint32_t field_count) {
    A2FO_GameObjectClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.object_class = klass;
    event.source_odf = {source_name, std::strlen(source_name)};
    event.odf_fields = fields;
    event.odf_field_count = field_count;
    class_loaded_handler(&event, nullptr);
}
void class_case() {
    if (mode == "econ-pc24" || mode == "econ-pc53" ||
        mode == "econ-pc64") set_precision();
    g_runtime_ready = true;
    std::array<A2FO_OdfFieldView, 4> fighter{};
    fighter[0] = {{"classLabel", 10}, {"craft", 5}};
    fighter[1] = {{"tritaniumCost", 13}, {"3", 1}};
    put(peer_class.data(), kClassBuildTimeOffset,
        mode.rfind("econ-", 0) == 0 ? 16777216.0f : 1.25f);
    put(peer_class.data(), kClassNativeCostsOffset, std::int32_t{7});
    emit_class(peer_class.data(), "fighter.odf", fighter.data(), 2);
    std::array<A2FO_OdfFieldView, 6> squad{};
    squad[0] = {{"classLabel", 10}, {"squadron", 8}};
    squad[1] = {{"squadMember0", 12}, {"fighter", 7}};
    squad[2] = {{"squadMemberCount0", 17},
                {mode.rfind("econ-", 0) == 0 ? "1" : "2", 1}};
    squad[3] = {{"squadReinforceAtYard", 20}, {"1", 1}};
    squad[4] = {{"squadMember1", 12}, {"fighter2", 8}};
    squad[5] = {{"squadMemberCount1", 17}, {"1", 1}};
    if (mode == "class-malformed") squad[2].value = {"oops", 4};
    const bool two_rows = mode.rfind("econ-", 0) == 0;
    if (two_rows && mode != "econ-reentrant") {
        std::array<A2FO_OdfFieldView, 2> second{{
            {{"classLabel", 10}, {"craft", 5}},
            {{"tritaniumCost", 13}, {"5", 1}},
        }};
        put(yard_class.data(), kClassBuildTimeOffset, 1.0f);
        emit_class(yard_class.data(), "fighter2.odf",
                   second.data(), second.size());
    }
    emit_class(station_class.data(), "squad.odf", squad.data(),
               two_rows ? 6 : 4);
    trace("class:" + std::to_string(g_definitions.size()) + ":" +
          std::to_string(g_squad_economics.size()) + ":" +
          std::to_string(read_at<std::int32_t>(
              station_class.data(), kClassNativeCostsOffset, -1)) + ":" +
          bits(read_at<float>(station_class.data(),
              kClassBuildTimeOffset, 0.0f)));
    A2FO_GameObjectClassLoadedEvent short_event{};
    short_event.struct_size = 1;
    class_loaded_handler(&short_event, nullptr);
    class_loaded_handler(nullptr, nullptr);
}
void producer_case() {
    g_runtime_ready = true;
    A2FO_ProducerEvent event{};
    event.struct_size = sizeof(event);
    event.producer = yard.data();
    event.target_class = station_class.data();
    for (const auto kind : {A2FO_PRODUCER_EVENT_ADMIT,
                            A2FO_PRODUCER_EVENT_STARTING_EFFECT,
                            A2FO_PRODUCER_EVENT_FINISHING}) {
        event.kind = kind;
        trace("producer:" + std::to_string(kind) + ":" +
              std::to_string(producer_event_handler(&event, nullptr)));
    }
    event.struct_size = 1;
    trace("producer-short:" +
          std::to_string(producer_event_handler(&event, nullptr)));
}
void craft_case() {
    const auto id = seed_squad();
    g_runtime_ready = true;
    A2FO_CraftEvent event{};
    event.struct_size = sizeof(event);
    event.craft = station.data();
    event.kind = A2FO_CRAFT_EVENT_POST_LOAD;
    craft_event_handler(&event, nullptr);
    trace("craft-post:" + std::to_string(g_registry.find(id) != nullptr));
    event.kind = A2FO_CRAFT_EVENT_CLEANUP;
    craft_event_handler(&event, nullptr);
    trace("craft-clean:" +
          std::to_string(g_registry.containing(11) != nullptr) + ":" +
          std::to_string(g_registry.containing(12) != nullptr));
    event.struct_size = 1;
    craft_event_handler(&event, nullptr);
    craft_event_handler(nullptr, nullptr);
}
void member_case() {
    g_armada = nullptr;
    g_classes_by_odf["fighter"] = peer_class.data();
    g_classlabels_by_odf["fighter"] = "cRaFt";
    g_classlabels_by_odf["turret"] = "turret";
    g_classes_by_odf["turret"] = station_class.data();
    for (const char* name : {"fighter", "turret", "missing", "invalid/path"}) {
        const auto result = resolve_member_class(name);
        trace(std::string("member:") + name + ":" +
              std::to_string(static_cast<int>(result)));
    }
    check(resolve_member_class("turret") == MemberClass::unsupported,
          "unsupported label");
}
void selection_case() {
    seed_squad();
    g_runtime_ready = true;
    g_selection_add_hook.gateway = reinterpret_cast<void*>(&selection_gateway);
    if (mode == "selection-capacity") {
        put(selection.data(), kNativeSelectionCountOffset, std::uint32_t{30});
    }
    selection_add_hook(selection.data(), station.data(), 0, 0, 1, 1);
    trace("selection:" + std::to_string(read_at<std::uint32_t>(
        selection.data(), kNativeSelectionCountOffset, 0)) +
        ":calls=" + std::to_string(gateway_calls) +
        ":mutations=" + std::to_string(mutation_calls));
}
void pod_case() {
    g_pod_group_layout_ready = true;
    g_pod_group_classes.insert(station_class.data());
    g_pod_yard_classes.insert(peer_class.data());
    put(station.data(), 0, at(kResearchStationVtableRva));
    void* children[]{peer.data(), peer.data()};
    put(station.data(), kStationPodCountOffset,
        std::uint32_t{mode == "pod-duplicate" ? 2u : 1u});
    put(station.data(), kStationPodsOffset, static_cast<void*>(children));
    g_pod_group_roots.insert(11);
    const auto peers = attached_yard_peers(station.data(), &find_entity);
    trace("pod:" + std::to_string(peers.size()) +
          (peers.empty() ? "" : ":" + std::to_string(peers[0])));
    check(peers.size() == 1 && peers[0] == 12, "attached pod peer");
}
void repair_case() {
    const bool vacant = mode == "repair-update" ||
        mode == "repair-queue" || mode == "repair-queue-drop" ||
        mode == "finish-repair" || mode == "finish-repair-cancelled";
    const auto id = seed_squad(vacant);
    active_squad_id = id;
    g_repair_ready = true;
    void* queue_vtable = at(kRepairQueueVtableRva);
    std::array<std::uint8_t, 0x10> queue{};
    put(queue.data(), 0, queue_vtable);
    put(yard.data(), 0x2ac, static_cast<void*>(queue.data()));
    g_yard_build_items[yard_class.data()].insert("squad");
    std::array<std::uint8_t, 0x80> command{};
    put(station.data(), 0x44, static_cast<void*>(command.data()));
    put(command.data(), 0x3c, std::uint32_t{0});
    RepairVisit visit;
    visit.yard = 21;
    visit.serviced.insert(11);
    if (!vacant) {
        put(peer.data(), 0x44, static_cast<void*>(command.data()));
        visit.serviced.insert(12);
    }
    g_repair_visits.emplace(id, visit);
    if (mode == "repair-cancel-reenter") {
        std::array<std::uint8_t, 0x20> node{};
        put(node.data(), 0x0c, std::uint32_t{77});
        put(yard.data(), 0x270, static_cast<void*>(node.data()));
        g_repair_visits.at(id).queue_id = 77;
        g_cancel_repair_job = &cancel_repair;
        end_repair_visit(id, true);
        trace("cancel-result:" + std::to_string(g_repair_visits.size()) +
              ":" + std::to_string(g_repair_visits.at(id).queue_id));
        return;
    }
    a2fo::squadrons::RepairContext context;
    bool waiting = false;
    const bool observed = observe_repair(id, g_repair_visits.at(id),
                                         &context, &waiting);
    trace("repair:" + std::to_string(observed) + ":" +
          std::to_string(waiting) + ":" +
          std::to_string(context.serviced_members.size()));
    if (mode == "repair-update" || mode == "repair-queue" ||
        mode == "repair-queue-drop") {
        if (mode != "repair-update") {
            g_push_repair_job = &push_repair;
            g_cancel_repair_job = &cancel_repair;
        }
        update_reinforcement(station.data(), A2FO_CRAFT_EVENT_SIMULATE_POST, 0.5f);
        const auto found = g_repair_visits.find(id);
        trace("repair-visits:" + std::to_string(g_repair_visits.size()) +
              ":queue=" + std::to_string(found == g_repair_visits.end()
                  ? 0 : found->second.queue_id));
        if (mode == "repair-queue-drop") {
            update_reinforcement(station.data(),
                A2FO_CRAFT_EVENT_SIMULATE_POST, 0.5f);
            trace("repair-dropped:" + std::to_string(g_repair_visits.size()));
        }
    }
    if (mode == "finish-repair" || mode == "finish-repair-cancelled") {
        a2fo::squadrons::RepairContext request;
        request.yard = 21;
        request.team = 2;
        request.compatible = true;
        request.accepted_repair_cycle = true;
        request.serviced_members.push_back(11);
        const auto reserved = g_registry.reserve_replacement(id, request);
        check(static_cast<bool>(reserved), "reserve replacement");
        auto& pending = g_repair_visits.at(id);
        pending.ticket = reserved.value->ticket;
        pending.queue_id = 77;
        if (mode == "finish-repair-cancelled") g_registry.remove(11);
        finish_reinforcement(yard.data(), 77, replacement.data());
        trace("repair-finished:" +
              std::to_string(g_registry.containing(13) != nullptr) + ":" +
              std::to_string(g_repair_visits.count(id)));
    }
}
void set_precision() {
    unsigned short control = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    control = static_cast<unsigned short>(control & ~0x0300u);
    if (mode == "bar-pc53") control |= 0x0200u;
    if (mode == "bar-pc64") control |= 0x0300u;
    __asm__ __volatile__("fldcw %0" : : "m"(control));
}
void* __attribute__((fastcall)) values_provider(void* bar, void*, void* output) {
    void* craft = read_at<void*>(bar, 0x428, nullptr);
    const auto handle = object_handle(craft);
    const std::array<float, 2> values = handle == 11
        ? std::array<float, 2>{{16777216.0f, 16777218.0f}}
        : std::array<float, 2>{{1.0f, 2.0f}};
    std::memcpy(output, values.data(), sizeof(values));
    trace("values-provider:" + std::to_string(handle));
    return output;
}
void* __attribute__((fastcall)) colour_provider(void* bar, void*, void* output) {
    void* craft = read_at<void*>(bar, 0x428, nullptr);
    const auto handle = object_handle(craft);
    const std::array<float, 3> colour = handle == 11
        ? std::array<float, 3>{{0.1f, 0.2f, 0.3f}}
        : std::array<float, 3>{{0.7f, 0.8f, 0.9f}};
    std::memcpy(output, colour.data(), sizeof(colour));
    trace("colour-provider:" + std::to_string(handle));
    return output;
}
std::string bits(float value) {
    std::uint32_t word = 0;
    std::memcpy(&word, &value, sizeof(word));
    char text[16]{};
    std::snprintf(text, sizeof(text), "%08lx", static_cast<unsigned long>(word));
    return text;
}
void bar_case() {
    set_precision();
    seed_squad();
    std::array<std::uint8_t, 0x500> bar{};
    put(bar.data(), 0x428, static_cast<void*>(station.data()));
    std::array<void*, 11> vtable{};
    vtable[8] = reinterpret_cast<void*>(&colour_provider);
    vtable[9] = reinterpret_cast<void*>(&values_provider);
    SquadBarOverride binding;
    binding.bar = bar.data();
    binding.original_vtable = vtable.data();
    g_active_bars = &binding;
    g_active_bar_count = 1;
    std::array<float, 2> values{{123.0f, 456.0f}};
    void* returned = squad_bar_values_hook(bar.data(), nullptr, values.data());
    trace("bar:" + std::to_string(returned == values.data()) + ":" +
          bits(values[0]) + ":" + bits(values[1]));
    std::array<float, 3> colour{};
    returned = squad_bar_colour_hook(bar.data(), nullptr, colour.data());
    trace("colour:" + std::to_string(returned == colour.data()) + ":" +
          bits(colour[0]) + ":" + bits(colour[1]) + ":" + bits(colour[2]));
    g_active_bars = nullptr;
    g_active_bar_count = 0;
}
std::uintptr_t __attribute__((fastcall)) ui_count(void* selected, void*) {
    trace("ui-count");
    return read_at<std::uint32_t>(selected, kNativeSelectionCountOffset, 0);
}
std::uintptr_t __attribute__((fastcall)) ui_handles(void* selected, void*) {
    trace("ui-handles");
    return reinterpret_cast<std::uintptr_t>(
        static_cast<std::uint8_t*>(selected) + kNativeSelectionHandlesOffset);
}
void ui_case() {
    seed_squad();
    g_ui_get_count_original = reinterpret_cast<void*>(&ui_count);
    g_ui_get_handles_original = reinterpret_cast<void*>(&ui_handles);
    put(selection.data(), kNativeSelectionCountOffset, std::uint32_t{3});
    put(selection.data(), kNativeSelectionHandlesOffset, std::uint32_t{11});
    put(selection.data(), kNativeSelectionHandlesOffset + 4, std::uint32_t{12});
    put(selection.data(), kNativeSelectionHandlesOffset + 8, std::uint32_t{21});
    const auto count = logical_ui_selection(selection.data());
    trace("ui-logical:" + std::to_string(count) + ":" +
          std::to_string(g_logical_ui_handles[0]) + ":" +
          std::to_string(g_logical_ui_handles[1]));
    check(count == 2 && g_logical_ui_handles[0] == 11 &&
          g_logical_ui_handles[1] == 21, "logical dedup");
}
std::uintptr_t __attribute__((fastcall)) render_original(void* panel, void*) {
    ++render_calls;
    trace("render:" + std::to_string(render_calls));
    if (mode == "panel-nested" && render_calls == 1)
        squad_panel_render_hook(panel, nullptr);
    return 1;
}
void panel_case() {
    std::array<std::uint8_t, 0x300> panel{};
    g_ship_display_render_original = reinterpret_cast<void*>(&render_original);
    g_runtime_ready = mode != "panel-off";
    squad_panel_render_hook(panel.data(), nullptr);
    trace("panel:" + std::to_string(render_calls) + ":" +
          std::to_string(g_active_bars == nullptr) + ":" +
          std::to_string(g_active_bar_count));
}
void poison_case() {
    g_runtime_ready = false;
    g_selection_add_hook.gateway = reinterpret_cast<void*>(&poison_gateway);
    void* poison = reinterpret_cast<void*>(1);
    selection_add_hook(poison, poison, 2, 0, 0, 0);
    trace("poison-producer:" + std::to_string(
        producer_event_handler(
            static_cast<const A2FO_ProducerEvent*>(poison), nullptr)));
    craft_event_handler(static_cast<const A2FO_CraftEvent*>(poison), nullptr);
    std::int32_t costs[4]{2, 3, 4, 5};
    trace("poison-costs:" + std::to_string(
        A2FOSquadrons_GetAdditionalCosts(poison, costs, 4)) + ":" +
        std::to_string(costs[0]));
    g_runtime_ready = true;
    A2FO_ProducerEvent short_producer{};
    short_producer.struct_size = 1;
    short_producer.producer = poison;
    trace("short-producer:" + std::to_string(
        producer_event_handler(&short_producer, nullptr)));
    A2FO_CraftEvent short_craft{};
    short_craft.struct_size = 1;
    short_craft.craft = poison;
    craft_event_handler(&short_craft, nullptr);
    trace("short-craft:done");
}
void launch_case() {
    g_runtime_ready = true;
    g_classes_by_odf["fighter"] = peer_class.data();
    g_classlabels_by_odf["fighter"] = "craft";
    put(yard.data(), 0, static_cast<void*>(at(kShipyardVtableRva)));
    put(at(kShipyardVtableRva), kProducerBuildTransformVtableOffset,
        static_cast<void*>(reinterpret_cast<void*>(&transform_provider)));
    std::array<std::uint8_t, 0x100> queue{};
    put(queue.data(), 0, static_cast<void*>(at(kOutputQueueManagerVtableRva)));
    put(at(kOutputQueueManagerVtableRva), 4 * sizeof(void*),
        static_cast<void*>(reinterpret_cast<void*>(&queue_add)));
    put(yard.data(), kShipyardBuildOutputQueueOffset,
        static_cast<void*>(queue.data()));
    put(station.data(), kObjectClassOffset, static_cast<void*>(peer_class.data()));
    a2fo::squadrons::Definition definition;
    definition.odf = "squad";
    definition.members.push_back({0, "fighter", 2});
    const bool spawned = spawn_squadron(yard.data(), definition);
    trace("spawn:" + std::to_string(spawned) + ":" +
          std::to_string(g_pending_launches.size()) + ":" +
          std::to_string(g_registry.size()));
    if (!spawned) return;
    if (mode == "launch-yard-cleanup") {
        update_pending_launches(yard.data(), A2FO_CRAFT_EVENT_CLEANUP);
    } else {
        put(station.data(), kCraftQueueStageOffset, std::uint32_t{0});
        update_pending_launches(station.data(),
                                A2FO_CRAFT_EVENT_SIMULATE_POST);
        if (mode == "launch-sequence" && !g_pending_launches.empty()) {
            put(peer.data(), kCraftQueueStageOffset, std::uint32_t{0});
            update_pending_launches(peer.data(),
                                    A2FO_CRAFT_EVENT_SIMULATE_POST);
        }
    }
    trace("launch:" + std::to_string(g_pending_launches.size()) + ":" +
          std::to_string(g_registry.size()) + ":" +
          std::to_string(queue_adds) + ":" +
          std::to_string(expirations));
}
void finish_case() {
    g_runtime_ready = true;
    g_starbase_finish_original = reinterpret_cast<void*>(&ordinary_finish);
    put(yard.data(), 0, static_cast<void*>(at(kShipyardVtableRva)));
    put(yard.data(), kProducerCurrentBuildClassOffset,
        static_cast<void*>(station_class.data()));
    put(yard.data(), kProducerCurrentQueueIdOffset, std::uint32_t{19});
    put(yard.data(), kProducerLastBuiltHandleOffset, std::uint32_t{81});
    put(at(kShipyardVtableRva), kProducerStopConstructionEffectVtableOffset,
        static_cast<void*>(reinterpret_cast<void*>(&stop_effect)));
    if (mode != "finish-ordinary") {
        g_classes_by_odf["fighter"] = peer_class.data();
        g_classlabels_by_odf["fighter"] = "craft";
        put(at(kShipyardVtableRva), kProducerBuildTransformVtableOffset,
            static_cast<void*>(reinterpret_cast<void*>(&transform_provider)));
        static std::array<std::uint8_t, 0x100> queue{};
        put(queue.data(), 0, static_cast<void*>(at(kOutputQueueManagerVtableRva)));
        put(at(kOutputQueueManagerVtableRva), 4 * sizeof(void*),
            static_cast<void*>(reinterpret_cast<void*>(&queue_add)));
        put(yard.data(), kShipyardBuildOutputQueueOffset,
            static_cast<void*>(queue.data()));
        put(station.data(), kObjectClassOffset,
            static_cast<void*>(peer_class.data()));
        a2fo::squadrons::Definition definition;
        definition.odf = "squad";
        definition.members.push_back({0, "fighter", 2});
        g_definitions.emplace(station_class.data(), definition);
    }
    const auto result = starbase_finish_build_hook(yard.data(), nullptr);
    trace("finish:" + std::to_string(result ==
          reinterpret_cast<std::uintptr_t>(station.data())) + ":" +
          std::to_string(read_at<void*>(yard.data(),
              kProducerCurrentBuildClassOffset, nullptr) == nullptr) + ":" +
          std::to_string(read_at<std::uint32_t>(yard.data(),
              kProducerCurrentQueueIdOffset, 99)) + ":" +
          std::to_string(g_pending_launches.size()) + ":" +
          std::to_string(queue_adds));
}
void init_case() {
    static A2FO_ModuleApi api{};
    if (mode == "init-short") {
        trace(std::string("init:") + std::to_string(A2FO_ModuleInit(&api)));
        return;
    }
    setup_init_image();
    api = *g_api;
    bool result = false;
    try {
        result = A2FO_ModuleInit(&api);
    } catch (const std::exception& error) {
        trace(std::string("init-throw:") + error.what());
        return;
    }
    trace(std::string("init:") + std::to_string(result) +
          ":ready=" + std::to_string(g_runtime_ready) +
          ":hooks=" + std::to_string(hooks) +
          ":classes=" + std::to_string(registrations) +
          ":finish=" + std::to_string(g_starbase_finish_slot != nullptr) +
          ":repair=" + std::to_string(g_repair_ready));
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "mode argument");
        mode = argv[1];
        if (mode == "init-hook-throw") {
            std::set_terminate([] {
                try {
                    if (auto active = std::current_exception())
                        std::rethrow_exception(active);
                } catch (const std::exception& error) {
                    std::fprintf(stderr, "terminate-active:%s\n", error.what());
                    std::fflush(stderr);
                    std::_Exit(86);
                }
                std::fprintf(stderr, "terminate-no-active\n");
                std::fflush(stderr);
                std::_Exit(87);
            });
        }
        setup_image();
        setup_api();
        seed_objects();
        if (mode == "fields") fields_case();
        else if (mode == "class-events" || mode == "class-malformed" ||
                 mode == "econ-pc24" || mode == "econ-pc53" ||
                 mode == "econ-pc64" || mode == "econ-reentrant")
            class_case();
        else if (mode == "producer-events") producer_case();
        else if (mode == "craft-events") craft_case();
        else if (mode == "member-resolver") member_case();
        else if (mode == "selection-basic" || mode == "selection-capacity" ||
                 mode == "selection-mutation") selection_case();
        else if (mode == "pod-selection" || mode == "pod-duplicate") pod_case();
        else if (mode == "repair-observe" || mode == "repair-update" ||
                 mode == "repair-queue" || mode == "repair-queue-drop" ||
                 mode == "repair-cancel-reenter" ||
                 mode == "finish-repair" ||
                 mode == "finish-repair-cancelled") repair_case();
        else if (mode == "bar-pc24" || mode == "bar-pc53" ||
                 mode == "bar-pc64") bar_case();
        else if (mode == "ui-dedup") ui_case();
        else if (mode == "poison-off") poison_case();
        else if (mode == "launch-sequence" ||
                 mode == "launch-first-fail" ||
                 mode == "launch-second-fail" ||
                 mode == "launch-yard-cleanup") launch_case();
        else if (mode == "finish-ordinary" || mode == "finish-squad" ||
                 mode == "finish-spawn-fail") finish_case();
        else if (mode == "panel-off" || mode == "panel-active" ||
                 mode == "panel-nested") panel_case();
        else if (mode == "init-short" || mode == "init-no-armada" ||
                 mode == "init-no-fleet" || mode == "init-signature" ||
                 mode == "init-finish-first" ||
                 mode == "init-finish-second" || mode == "init-success" ||
                 mode == "init-armada-throw" ||
                 mode == "init-class-throw" ||
                 mode == "init-register-badalloc" ||
                 mode == "init-yard-first-fail" ||
                 mode == "init-yard-second-fail" ||
                 mode == "init-hook-throw" ||
                 mode == "init-hook-repair-fail" ||
                 mode == "init-hook-selection-fail" ||
                 mode == "init-register-fail") init_case();
        else throw std::runtime_error("unknown fixture mode");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fixture:%s\n", error.what());
        return 1;
    }
}
