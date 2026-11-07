struct SolCompletionLogCapture {
    const char* expected_prefix;
    const char* auxiliary_prefix = nullptr;
    std::uint32_t matching = 0;
    std::uint32_t auxiliary_matching = 0;
    std::uint32_t unexpected = 0;
};

SolCompletionLogCapture* g_sol_completion_log_capture = nullptr;

void A2FO_CALL sol_completion_capture_log(const char*, const char* message) {
    if (!g_sol_completion_log_capture) return;
    auto& capture = *g_sol_completion_log_capture;
    if (message && capture.expected_prefix &&
        std::strncmp(message, capture.expected_prefix,
                     std::strlen(capture.expected_prefix)) == 0) {
        ++capture.matching;
    } else if (message && capture.auxiliary_prefix &&
               std::strcmp(message, capture.auxiliary_prefix) == 0) {
        ++capture.auxiliary_matching;
    } else {
        ++capture.unexpected;
    }
}

void* __cdecl sol_completion_current_mission() {
    return g_fixture_mission_ptr;
}

void sol_completion_scenarios() {
    assert(g_fixture_armada && g_armada && g_api);
    static_assert(sizeof(LONG) == sizeof(std::int32_t));
    static_assert(sizeof(std::uintptr_t) == sizeof(std::uint32_t));

    auto* image = static_cast<std::uint8_t*>(g_fixture_armada);
    std::array<std::uint8_t, 7> old_probe_stub{};
    std::array<std::uint8_t, 7> old_mission_factory_stub{};
    assert(memory_range_accessible(image + kLoadGameVersionProbeRva,
                                   old_probe_stub.size(), true));
    assert(memory_range_accessible(image + kEmptyMissionFactoryRva,
                                   old_mission_factory_stub.size(), true));
    std::memcpy(old_probe_stub.data(), image + kLoadGameVersionProbeRva,
                old_probe_stub.size());
    std::memcpy(old_mission_factory_stub.data(), image + kEmptyMissionFactoryRva,
                old_mission_factory_stub.size());

    void* saved_mission_slot = nullptr;
    void* saved_list_slot = nullptr;
    void* saved_ai_slot = nullptr;
    std::memcpy(&saved_mission_slot, image + kAiMissionCurrentRva,
                sizeof(saved_mission_slot));
    std::memcpy(&saved_list_slot, image + kGameObjectListPointerRva,
                sizeof(saved_list_slot));
    std::memcpy(&saved_ai_slot, image + kAiGlobalPointerRva,
                sizeof(saved_ai_slot));

    const bool saved_selected_valid = g_selected_valid;
    const auto saved_selected_map = g_selected_map;
    const std::string saved_selected_path = g_selected_path;
    const auto saved_sources = g_sources;
    void* const saved_fixture_mission = g_fixture_mission_ptr;
    const bool saved_fail_allocate = fail_allocate;
    const A2FO_ModuleApi* const saved_api = g_api;
    const auto saved_nebula_hook = g_sol_nebula_simulate_hook;
    const LONG saved_version_count = InterlockedCompareExchange(
        &g_runtime_version_bypass_count, 0, 0);
    const LONG saved_ai_count = InterlockedCompareExchange(
        &g_sol_ai_null_guard_count, 0, 0);
    const LONG saved_nebula_count = InterlockedCompareExchange(
        &g_sol_nebula_null_guard_count, 0, 0);
    const LONG saved_discovery_count = InterlockedCompareExchange(
        &g_discovery_dispatch_count, 0, 0);
    const LONG saved_map_details_count = InterlockedCompareExchange(
        &g_map_details_hook_count, 0, 0);
    const LONG saved_table_count = InterlockedCompareExchange(
        &g_runtime_table_skip_count, 0, 0);
    const LONG saved_post_count = InterlockedCompareExchange(
        &g_runtime_post_mission_skip_count, 0, 0);

    A2FO_ModuleApi capture_api = *g_api;
    capture_api.log = &sol_completion_capture_log;
    g_api = &capture_api;
    g_selected_valid = true;
    g_selected_map = {};
    g_selected_map.version = 3021;
    g_selected_path = "completion.sol";
    fixture_stub(kLoadGameVersionProbeRva,
                 reinterpret_cast<void*>(&fixture_native_probe));
    fixture_stub(kEmptyMissionFactoryRva,
                 reinterpret_cast<void*>(&sol_completion_current_mission));

    // Fail every attempted native allocation so missing context, MapDetails,
    // mission, or object-list fields remain genuinely absent in this matrix.
    fail_allocate = true;
    InterlockedExchange(&g_runtime_version_bypass_count, 16);
    std::array<std::uint8_t, 32> mission_token{};
    for (std::uint32_t bits = 0; bits < 32; ++bits) {
        const bool reader = (bits & 1u) != 0;
        const bool context = (bits & 2u) != 0;
        const bool details = (bits & 4u) != 0;
        const bool mission = (bits & 8u) != 0;
        const bool list = (bits & 16u) != 0;
        const std::uintptr_t reader_value = reader ? 1u : 0u;
        const std::uintptr_t context_value = context ? 2u : 0u;
        const std::uintptr_t details_value = details ? 3u : 0u;
        void* const mission_value = mission ? mission_token.data() : nullptr;
        void* const list_value = list ? g_fixture_start.data() : nullptr;
        g_fixture_mission_ptr = mission_value;
        std::memcpy(image + kAiMissionCurrentRva, &mission_value,
                    sizeof(mission_value));
        std::memcpy(image + kGameObjectListPointerRva, &list_value,
                    sizeof(list_value));

        std::array<std::uint8_t, 24> header{};
        const std::uint32_t native_version = 3021;
        std::memcpy(header.data() + 0x04, &native_version, sizeof(native_version));
        std::memcpy(header.data() + 0x0c, &reader_value, sizeof(reader_value));
        std::memcpy(header.data() + 0x10, &context_value, sizeof(context_value));
        std::memcpy(header.data() + 0x14, &details_value, sizeof(details_value));

        SolCompletionLogCapture capture{
            "SOL phase2j probe synthesis incomplete:"};
        if (reader && context && details && mission && list) {
            capture.expected_prefix = nullptr;
        }
        g_sol_completion_log_capture = &capture;
        void* const returned = A2FOSOLMaps_LoadGameVersionProbe(
            header.data(), 0, 0);
        g_sol_completion_log_capture = nullptr;

        std::uint32_t output_version = 0;
        std::uintptr_t output_reader = 0;
        std::uintptr_t output_context = 0;
        std::uintptr_t output_details = 0;
        std::memcpy(&output_version, header.data() + 0x04, sizeof(output_version));
        std::memcpy(&output_reader, header.data() + 0x0c, sizeof(output_reader));
        std::memcpy(&output_context, header.data() + 0x10, sizeof(output_context));
        std::memcpy(&output_details, header.data() + 0x14, sizeof(output_details));
        const bool complete = reader && context && details && mission && list;
        assert(returned == header.data());
        assert(output_version == (complete ? 2177u : native_version));
        assert(output_reader == reader_value);
        assert(output_context == context_value);
        assert(output_details == details_value);
        assert(capture.matching == (complete ? 0u : 1u));
        assert(capture.unexpected == 0);
        std::printf("completion-probe:%02u:flags=%02x:header=%lu,%lu,%lu,%lu,"
                    "mission=%u,list=%u:out=%u:logs=%u/%u\n",
                    bits, bits, static_cast<unsigned long>(native_version),
                    static_cast<unsigned long>(reader_value),
                    static_cast<unsigned long>(context_value),
                    static_cast<unsigned long>(details_value),
                    static_cast<unsigned>(mission), static_cast<unsigned>(list),
                    output_version, capture.matching, capture.unexpected);
    }
    assert(InterlockedCompareExchange(&g_runtime_version_bypass_count, 0, 0) == 48);
    fail_allocate = saved_fail_allocate;

    // Seed each native counter at the requested boundaries. Capture only the
    // expected log prefix and event count; full payload comparison remains in
    // the existing frozen/current trace runner.
    struct CapCase { LONG seed; LONG after; bool emits; };
    const CapCase cap_cases[] = {
        {15, 16, true}, {16, 17, false},
        {std::numeric_limits<LONG>::max(), std::numeric_limits<LONG>::min(), true},
    };
    const auto cap = [&](const char* tag, LONG& counter, const char* prefix,
                         const auto& action, const char* auxiliary_prefix = nullptr) {
        for (const auto& test : cap_cases) {
            InterlockedExchange(&counter, test.seed);
            SolCompletionLogCapture capture{prefix, auxiliary_prefix};
            g_sol_completion_log_capture = &capture;
            action();
            g_sol_completion_log_capture = nullptr;
            const LONG after = InterlockedCompareExchange(&counter, 0, 0);
            assert(after == test.after);
            assert(capture.matching == static_cast<std::uint32_t>(test.emits));
            assert(capture.auxiliary_matching ==
                   static_cast<std::uint32_t>(auxiliary_prefix != nullptr));
            assert(capture.unexpected == 0);
            std::printf("completion-cap:%s:seed=%ld:after=%ld:logs=%u/%u/%u\n",
                        tag, static_cast<long>(test.seed), static_cast<long>(after),
                        capture.matching, capture.auxiliary_matching,
                        capture.unexpected);
        }
    };

    std::array<std::uint8_t, 0x200> game_object{};
    const std::int32_t team = 3;
    std::memcpy(game_object.data() + kGameObjectTeamOffset, &team, sizeof(team));
    void* null_ai = nullptr;
    std::memcpy(image + kAiGlobalPointerRva, &null_ai, sizeof(null_ai));
    cap("ai-guard", g_sol_ai_null_guard_count, "SOL phase2j AI-null guard #",
        [&] { (void)A2FOSOLMaps_IsAiControlledSafe(game_object.data()); });

    std::array<std::uint8_t, 0x200> nebula{};
    g_sol_nebula_simulate_hook.gateway = nullptr;
    cap("nebula-guard", g_sol_nebula_null_guard_count,
        "SOL phase2l Nebula null-state guard #",
        [&] { (void)sol_nebula_simulate_hook(nebula.data(), nullptr); });

    const auto saved_sources_for_cap = g_sources;
    g_sources.clear();
    g_sources = saved_sources_for_cap;

    cap("a1-craft-table", g_runtime_table_skip_count,
        "SOL phase2 load-order bridge #",
        [&] { (void)A2FOSOLMaps_LoadA2CraftClassTable(nullptr); });
    cap("post-mission-first", g_runtime_post_mission_skip_count,
        "SOL phase2j object-only bring-up: skipped first native post-mission table",
        [&] { (void)A2FOSOLMaps_LoadPostMissionA(nullptr); });
    cap("post-mission-second", g_runtime_post_mission_skip_count,
        "SOL phase2j object-only bring-up: skipped second native post-mission table",
        [&] { (void)A2FOSOLMaps_LoadPostMissionB(nullptr); });
    const auto sources_before_missing_map = g_sources;
    g_sources.clear();
    cap("map-details-discovery", g_map_details_hook_count,
        "SOL discovery MapDetails hook:",
        [&] { (void)a2fo_sol_load_map_details("missing.sol"); },
        "SOL MapDetails source not found: 'missing.sol'");
    g_sources = sources_before_missing_map;

    g_sol_completion_log_capture = nullptr;
    g_api = saved_api;
    fail_allocate = saved_fail_allocate;
    g_fixture_mission_ptr = saved_fixture_mission;
    g_selected_valid = saved_selected_valid;
    g_selected_map = saved_selected_map;
    g_selected_path = saved_selected_path;
    g_sources = saved_sources;
    g_sol_nebula_simulate_hook = saved_nebula_hook;
    InterlockedExchange(&g_runtime_version_bypass_count, saved_version_count);
    InterlockedExchange(&g_sol_ai_null_guard_count, saved_ai_count);
    InterlockedExchange(&g_sol_nebula_null_guard_count, saved_nebula_count);
    InterlockedExchange(&g_discovery_dispatch_count, saved_discovery_count);
    InterlockedExchange(&g_map_details_hook_count, saved_map_details_count);
    InterlockedExchange(&g_runtime_table_skip_count, saved_table_count);
    InterlockedExchange(&g_runtime_post_mission_skip_count, saved_post_count);
    std::memcpy(image + kAiMissionCurrentRva, &saved_mission_slot,
                sizeof(saved_mission_slot));
    std::memcpy(image + kGameObjectListPointerRva, &saved_list_slot,
                sizeof(saved_list_slot));
    std::memcpy(image + kAiGlobalPointerRva, &saved_ai_slot, sizeof(saved_ai_slot));
    std::memcpy(image + kLoadGameVersionProbeRva, old_probe_stub.data(),
                old_probe_stub.size());
    std::memcpy(image + kEmptyMissionFactoryRva,
                old_mission_factory_stub.data(), old_mission_factory_stub.size());
}
