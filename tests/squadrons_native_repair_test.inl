// Included in the native adapter harness; no Armada process or GUI needed.
std::array<std::array<std::uint8_t, 0x80>, 4> repair_geometry{};
std::array<std::array<std::uint8_t, 0x50>, 3> repair_process{};
std::array<std::uint8_t, 0x30> repair_queue{};
std::array<std::uint8_t, 0x20> repair_job_node{};
unsigned repair_pushes = 0, repair_deletes = 0;
bool repair_admission = true;
std::uint32_t repair_result_handle = 2;
std::unordered_set<std::string> registered_yard_fields;

bool A2FO_CALL install_repair_test_hook(void* target, void*, std::uint32_t size,
    const std::uint8_t* expected, A2FO_InlineHook* hook) {
    if (std::memcmp(target, expected, size)) return false;
    hook->gateway = target;
    return true;
}

void repair_installation_tests() {
    // FeaturePack loads first in the real installation and replaces ActDelete
    // with JMP rel32. The on-disk signatures alone never exercise this case.
    g_fleet_ops = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x200000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    assert(g_fleet_ops);
    auto* fo = reinterpret_cast<std::uint8_t*>(g_fleet_ops);
    const std::uint8_t needs[]{0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
    const std::uint8_t activate[]{0x55, 0x8b, 0xec, 0x51, 0x53};
    std::memcpy(fo + kFoNeedsRepairRva, needs, sizeof(needs));
    std::memcpy(at(kRepairActivateRva), activate, sizeof(activate));
    put(at(kRepairQueueVtableRva + 0x2c), 0, at(kRepairActivateRva));
    fo[0x122c8c] = 0xe9;
    A2FO_ModuleApi api{};
    api.install_inline_hook = &install_repair_test_hook;
    const auto* previous_api = g_api;
    g_api = &api;
    assert(install_repair_hooks());
    assert(g_repair_ready);
    assert(read_at<void*>(at(kRepairQueueVtableRva + 0x2c), 0) ==
        reinterpret_cast<void*>(&squad_repair_activate));
    assert(fo[0x122c8c] == 0xe9); // Must not overwrite another module's hook.
    put(at(kRepairQueueVtableRva + 0x2c), 0, at(kRepairActivateRva));
    fo[kFoNeedsRepairRva] = 0xcc;
    g_repair_ready = false;
    assert(!install_repair_hooks());
    assert(!g_repair_ready);
    assert(read_at<void*>(at(kRepairQueueVtableRva + 0x2c), 0) == at(kRepairActivateRva));
    g_api = previous_api;
    g_needs_repair_hook = {};
    VirtualFree(g_fleet_ops, 0, MEM_RELEASE);
    g_fleet_ops = nullptr;
    std::cout << "PASS repair hook installation with FeaturePack detour; unknown signatures rejected\n";
}

bool A2FO_CALL register_yard_test(const char*, const char* const* fields,
    std::uint32_t count, A2FO_GameObjectClassLoadedHandler handler, void*) {
    assert(count <= 64 && handler == &yard_class_loaded_handler);
    for (unsigned i = 0; i < count; ++i) registered_yard_fields.insert(fields[i]);
    return true;
}

std::uintptr_t __attribute__((fastcall)) healthy_method(
    void*, void*, float* crew, std::uint32_t, std::uint32_t) {
    if (crew) *crew = 0;
    return 0;
}
std::uintptr_t __attribute__((fastcall)) repair_activate_method(
    void*, void*, void* craft, std::uint32_t) {
    float crew = -1;
    // Reinforcement eligibility must not keep the real health repair stuck.
    assert(squad_needs_repair(craft, nullptr, &crew, 1, 0) == 0 && crew == 0);
    put<std::uint32_t>(craft, kCraftQueueStageOffset, 1); // normal repair output
    return 1;
}
std::uint32_t __cdecl push_repair_method(void* producer, void* target) {
    ++repair_pushes;
    assert(producer == yard.data() && target == member_class.data());
    if (!repair_admission) return 0;
    put<std::uint32_t>(repair_job_node.data(), 0x0c, 77);
    put(producer, 0x270, repair_job_node.data());
    put(producer, kProducerCurrentBuildClassOffset, target);
    put<std::uint32_t>(producer, kProducerCurrentQueueIdOffset, 77);
    return 77;
}
bool __cdecl delete_repair_method(void* producer, std::uint32_t id) {
    assert(id == 77);
    ++repair_deletes;
    put<void*>(producer, 0x270, nullptr);
    return true;
}
std::uintptr_t __attribute__((fastcall)) finish_repair_method(void* producer, void*) {
    auto* result = entity(repair_result_handle);
    put<void*>(producer, 0x270, nullptr);
    put<std::uint32_t>(producer, kProducerCurrentQueueIdOffset, 0);
    put<std::uint32_t>(result, kCraftQueueStageOffset, 1);
    put<std::uint32_t>(result, kCraftQueueOwnerOffset, 900);
    put<std::uint32_t>(read_at<void*>(result, 0x44), 0x3c, 0x0a);
    return reinterpret_cast<std::uintptr_t>(result);
}

void repair_tests() {
    A2FO_ModuleApi metadata_api{};
    metadata_api.register_game_object_class_loaded_handler = &register_yard_test;
    assert(register_yard_fields(&metadata_api));
    assert(registered_yard_fields.size() == 100);
    assert(registered_yard_fields.count("buildItem0") && registered_yard_fields.count("buildItem99"));
    static_assert(kRequiredFieldCount <= 64);
    const A2FO_OdfFieldView metadata[] = {{{"buildItem99", 11}, {"\"patrol.odf\"", 12}}};
    A2FO_GameObjectClassLoadedEvent metadata_event{};
    metadata_event.struct_size = sizeof(metadata_event);
    metadata_event.object_class = member_class.data();
    metadata_event.odf_fields = metadata;
    metadata_event.odf_field_count = 1;
    g_runtime_ready = true;
    g_yard_build_items.clear();
    yard_class_loaded_handler(&metadata_event, nullptr);
    assert(g_yard_build_items.at(member_class.data()).count("patrol"));
    const Definition definition{"patrol", {{0, "fighter", 3}}, true};
    const auto reset = [&]() {
        g_registry = Registry{};
        g_pending_launches.clear();
        g_repair_visits.clear();
        repair_pushes = repair_deletes = 0;
        repair_admission = true;
        yard.fill(0);
        put<std::uint32_t>(yard.data(), kObjectHandleOffset, 900);
        put<std::int32_t>(yard.data(), kObjectTeamOffset, 1);
        put(yard.data(), kObjectClassOffset, member_class.data());
        put(yard.data(), 4, repair_geometry[3].data());
        put(yard.data(), 0x2ac, repair_queue.data());
        put(repair_queue.data(), 0, at(kRepairQueueVtableRva));
        put(repair_queue.data(), 0x1c, yard.data());
        g_yard_build_items[member_class.data()] = {"patrol"};
        for (std::size_t i = 0; i < ships.size(); ++i) {
            ships[i].fill(0);
            repair_geometry[i].fill(0);
            put<std::uint32_t>(ships[i].data(), kObjectHandleOffset, i + 1);
            put<std::int32_t>(ships[i].data(), kObjectTeamOffset, 1);
            put(ships[i].data(), kObjectClassOffset, member_class.data());
            put(ships[i].data(), 4, repair_geometry[i].data());
            put(ships[i].data(), 0x44, repair_process[i].data());
            put<std::uint32_t>(repair_process[i].data(), 0x3c, 0x11);
        }
        assert(g_registry.create(definition, 1,
            {{1, 1, "fighter"}, {2, 1, "fighter"}, {3, 1, "fighter"}},
            [](const std::string&) { return MemberClass::mobile_craft; }));
        g_registry.remove(3);
        g_runtime_ready = g_repair_ready = true;
        g_needs_repair_hook.gateway = reinterpret_cast<void*>(&healthy_method);
        g_repair_activate_original = reinterpret_cast<void*>(&repair_activate_method);
        g_push_repair_job = &push_repair_method;
        g_cancel_repair_job = &delete_repair_method;
        g_starbase_finish_original = reinterpret_cast<void*>(&finish_repair_method);
    };
    const auto admit = [](std::size_t index) {
        put<std::uint32_t>(ships[index].data(), kCraftQueueStageOffset, 3);
        put<std::uint32_t>(ships[index].data(), kCraftQueueOwnerOffset, 900);
        // Exercise the real two-argument thiscall ABI, with a float stack word.
        assert(a2fo_squadrons_call_thiscall_2(reinterpret_cast<void*>(&squad_repair_activate),
            repair_queue.data(), reinterpret_cast<std::uintptr_t>(ships[index].data()),
            0x3dcccccd) == 1);
    };
    const auto exit_yard = [](std::size_t index) {
        put<std::uint32_t>(ships[index].data(), kCraftQueueStageOffset, 0);
        put<std::uint32_t>(ships[index].data(), kCraftQueueOwnerOffset, 0);
        put<std::uint32_t>(repair_process[index].data(), 0x3c, 3);
    };
    const auto tick = []() {
        update_reinforcement(ships[0].data(), A2FO_CRAFT_EVENT_SIMULATE_POST, 1.0f);
    };
    reset();
    float crew = -1;
    assert(a2fo_squadrons_call_thiscall_3(reinterpret_cast<void*>(&squad_needs_repair),
        ships[0].data(), reinterpret_cast<std::uintptr_t>(&crew), 1, 0) == 1);
    assert(crew == 0);
    tick(); // Merely parked near the yard is not an accepted repair visit.
    assert(!repair_pushes && g_repair_visits.empty());
    admit(0);
    exit_yard(0);
    tick(); // The other live member must actually get serviced too.
    assert(!repair_pushes && !g_repair_visits.empty());
    admit(1);
    tick(); // Output must clear before production is admitted.
    assert(!repair_pushes);
    exit_yard(1);
    repair_admission = false;
    tick();
    assert(repair_pushes == 1 && !g_registry.containing(1)->replacement);
    repair_admission = true;
    tick();
    assert(repair_pushes == 2 && g_registry.containing(1)->replacement);
    tick();
    assert(repair_pushes == 2); // No duplicate payment/job.
    finish_reinforcement(yard.data(), 76, ships[2].data());
    assert(g_registry.containing(1)->live_count() == 2); // same-class unrelated job
    repair_result_handle = 3;
    starbase_finish_build_hook(yard.data(), nullptr);
    assert(g_registry.containing(1)->badge() == "3/3");
    assert(g_registry.containing(3)->id == g_registry.containing(1)->id);
    tick(); // Newly built command 0x0a/output is allowed; no extra replacement.
    assert(repair_pushes == 2);
    exit_yard(2);
    tick();
    assert(g_repair_visits.empty());
    assert(squad_needs_repair(ships[0].data(), nullptr, &crew, 1, 0) == 0);

    reset();
    g_registry.remove(2);
    admit(0);
    exit_yard(0);
    tick();
    repair_result_handle = 2;
    starbase_finish_build_hook(yard.data(), nullptr);
    assert(g_registry.containing(1)->badge() == "2/3");
    tick();
    assert(repair_pushes == 1); // Only one replacement may be launching.
    exit_yard(1);
    tick();
    assert(repair_pushes == 2);
    repair_result_handle = 3;
    starbase_finish_build_hook(yard.data(), nullptr);
    assert(g_registry.containing(1)->badge() == "3/3");

    for (int cancellation = 0; cancellation < 6; ++cancellation) {
        reset();
        g_registry.remove(2);
        admit(0);
        exit_yard(0);
        tick();
        assert(repair_pushes == 1);
        if (cancellation == 0) put<std::uint32_t>(repair_process[0].data(), 0x3c, 2); // new order
        if (cancellation == 1) put<std::int32_t>(yard.data(), kObjectTeamOffset, 2);
        if (cancellation == 2) put<float>(repair_geometry[0].data(), 0x68, 2000.0f);
        if (cancellation == 3) put<void*>(yard.data(), 0x270, nullptr); // native queue cancel
        if (cancellation == 4) g_registry.remove(1); // total loss
        if (cancellation == 5) put<std::uint8_t>(yard.data(), kObjectExpiredOffset, 1);
        tick();
        assert(g_repair_visits.empty());
        if (auto* squad = g_registry.containing(1)) assert(!squad->replacement);
        assert(repair_deletes == (cancellation == 3 || cancellation == 5 ? 0u : 1u));
        tick();
        assert(repair_pushes == 1); // Cancellation doesn't silently requeue.
    }
    reset();
    put<std::uint32_t>(repair_process[0].data(), 0x3c, 0x14); // recycle, not repair
    admit(0);
    assert(g_repair_visits.empty());
    reset();
    g_yard_build_items.clear();
    admit(0);
    assert(g_repair_visits.empty());
    reset();
    put<std::int32_t>(yard.data(), kObjectTeamOffset, 2);
    admit(0);
    assert(g_repair_visits.empty());
    g_repair_ready = false;
    std::cout << "PASS native repair ABI, healthy depletion, actual service/all-survivor gates, paid admission retry, sequential replacements, job identity and cancellation\n";
}
