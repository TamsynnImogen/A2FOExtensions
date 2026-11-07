#include <windows.h>

static bool query_mutation_armed = false;
static const void* query_mutation_target = nullptr;
static void** query_mutation_field = nullptr;
static void* query_mutation_replacement = nullptr;
static void apply_query_mutation(const void* address) {
    if (query_mutation_armed && address == query_mutation_target) {
        *query_mutation_field = query_mutation_replacement;
        query_mutation_armed = false;
    }
}
static SIZE_T WINAPI fixture_virtual_query(
    LPCVOID address, PMEMORY_BASIC_INFORMATION information,
    SIZE_T information_size) {
    apply_query_mutation(address);
    return ::VirtualQuery(address, information, information_size);
}
static BOOL WINAPI fixture_bad_read_ptr(const VOID* address,
                                        UINT_PTR size) {
    apply_query_mutation(address);
    return ::IsBadReadPtr(address, size);
}

#define main a2fo_hybrid_stage8_unused_main
#define VirtualQuery fixture_virtual_query
#define IsBadReadPtr fixture_bad_read_ptr
#include "rust_stage8_hybrid_runtime_native.cpp"
#undef IsBadReadPtr
#undef VirtualQuery
#undef main

static void arm_query_mutation(const void* address, void** field,
                               void* replacement) {
    query_mutation_target = address;
    query_mutation_field = field;
    query_mutation_replacement = replacement;
    query_mutation_armed = true;
}

template <class T, std::size_t N>
static void put(std::array<std::uint8_t, N>& buffer,
                std::size_t offset, T value) {
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}
template <class T>
static void put(std::vector<std::uint8_t>& buffer,
                std::size_t offset, T value) {
    std::memcpy(buffer.data() + offset, &value, sizeof(value));
}
static unsigned destructor_calls = 0;
static unsigned delete_calls = 0;
extern "C" std::uintptr_t a2fo_call_thiscall_0(void*, void*) {
    ++destructor_calls;
    return 0;
}
extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) { return 0; }

static void* class_alpha = nullptr;
static void* class_beta = nullptr;
static void* class_gamma = nullptr;
static void* class_delta = nullptr;
static void* __cdecl fake_find_name(const char* name) {
    if (!std::strcmp(name, "alpha")) return class_alpha;
    if (!std::strcmp(name, "beta")) return class_beta;
    return nullptr;
}
static void* __cdecl fake_find_project(const std::uint32_t* id) {
    if (*id == 91) return class_alpha;
    if (*id == 92) return class_beta;
    if (*id == 93) return class_gamma;
    if (*id == 94) return class_delta;
    return nullptr;
}
static void __cdecl fake_delete(void*) { ++delete_calls; }
static void install_fixture_jump(void* image, std::uintptr_t rva,
                                 void* target) {
    auto* location = static_cast<std::uint8_t*>(image) + rva;
    const std::int32_t relative = static_cast<std::int32_t>(
        reinterpret_cast<std::intptr_t>(target) -
        reinterpret_cast<std::intptr_t>(location + 5));
    location[0] = 0xe9;
    std::memcpy(location + 1, &relative, sizeof(relative));
}

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    A2FO_ModuleApi api{};
    api.log = fixture_log;
    a2fo::g_api = &api;
    InitializeCriticalSection(&a2fo::g_registry_lock);
    a2fo::g_registry_lock_ready = true;

    std::array<std::uint8_t, 0x500> producer{};
    std::array<std::array<std::uint8_t, 0x20>, 3> nodes{};
    int target = 1, other = 2;
    for (unsigned index = 0; index < nodes.size(); ++index) {
        put(nodes[index], 0, static_cast<void*>(&target));
        put(nodes[index], a2fo::kQueueItemIdOffset,
            std::uint32_t{index + 1});
        put(nodes[index], a2fo::kQueueItemNextOffset,
            index + 1 < nodes.size()
                ? static_cast<void*>(nodes[index + 1].data()) : nullptr);
    }
    put(nodes[1], 0, static_cast<void*>(&other));
    put(producer, a2fo::kQueueHeadOffset,
        static_cast<void*>(nodes[0].data()));
    std::printf("queue-preferred:%u\n", a2fo::queued_id_for_target(
        producer.data(), &target, 3));
    std::printf("queue-last:%u\n", a2fo::queued_id_for_target(
        producer.data(), &target, 0));
    std::printf("queue-absent:%u\n", a2fo::queued_id_for_target(
        producer.data(), &target, 9));
    put(nodes[1], a2fo::kQueueItemNextOffset,
        reinterpret_cast<void*>(1));
    std::printf("queue-invalid:%u\n", a2fo::queued_id_for_target(
        producer.data(), &target, 0));
    put(nodes[1], a2fo::kQueueItemNextOffset,
        static_cast<void*>(nodes[0].data()));
    std::printf("queue-cycle:%u\n", a2fo::queued_id_for_target(
        producer.data(), &target, 0));

    a2fo::HybridConstructionState state{};
    int first = 10, second = 20;
    state.placements[0] = {7, &target, &first};
    state.placements[1] = {7, &target, &second};
    std::printf("placement-first:%u\n",
        a2fo::placement_interface_for_queue_id_locked(state, 7) == &first);
    std::printf("placement-absent:%u\n",
        a2fo::placement_interface_for_queue_id_locked(state, 8) == nullptr);

    std::array<std::uint8_t, 0x500> producer_class{}, target_class{};
    std::uint32_t project_id = 91;
    put(producer, a2fo::kObjectClassOffset,
        static_cast<void*>(producer_class.data()));
    put(target_class, a2fo::kClassProjectIdOffset, &project_id);
    auto methods = std::make_unique<a2fo::HybridBuildLists>();
    methods->add(a2fo::ProductionMethod::construct, 0, project_id);
    a2fo::g_class_lists[producer_class.data()] = std::move(methods);
    put(nodes[2], 0, static_cast<void*>(target_class.data()));
    put(nodes[1], a2fo::kQueueItemNextOffset,
        static_cast<void*>(nodes[2].data()));
    std::array<std::uint8_t, a2fo::kBuildPositionInterfaceMatrixOffset +
        a2fo::kBuildPositionInterfaceMatrixSize> pending_matrix{};
    a2fo::HybridConstructionState pending{};
    pending.pending_interface = pending_matrix.data();
    pending.pending_target_class = target_class.data();
    pending.pending_queue_id = 3;
    pending.placements[2].queue_id = 3;
    a2fo::g_hybrid_construction[producer.data()] = pending;
    a2fo::bind_pending_construct_placement(
        producer.data(), target_class.data(), true);
    std::printf("bind-later-exact:%u:%u\n",
        a2fo::g_hybrid_construction[producer.data()].placements[2]
            .build_interface == pending_matrix.data(),
        a2fo::g_hybrid_construction[producer.data()].placements[0]
            .build_interface == nullptr);
    pending = {};
    pending.pending_interface = pending_matrix.data();
    pending.pending_target_class = target_class.data();
    pending.pending_queue_id = 3;
    a2fo::g_hybrid_construction[producer.data()] = pending;
    a2fo::bind_pending_construct_placement(
        producer.data(), target_class.data(), true);
    std::printf("bind-first-empty:%u\n",
        a2fo::g_hybrid_construction[producer.data()].placements[0]
            .build_interface == pending_matrix.data());
    a2fo::clear_construct_placements(producer.data());
    std::printf("clear-placements:%u\n",
        a2fo::g_hybrid_construction[producer.data()].placements[0]
            .build_interface == nullptr);

    auto menus = std::make_unique<a2fo::LiveBuildSubmenuClass>();
    menus->pages.emplace_back();
    menus->pages.back().parent_class = &target;
    menus->pages.emplace_back();
    menus->pages.back().parent_class = &target;
    std::printf("submenu-first:%u\n",
        a2fo::submenu_page_for_parent(menus.get(), &target) ==
            &menus->pages[0]);

    std::array<std::uint8_t, a2fo::kBuildPositionInterfaceMatrixOffset +
        a2fo::kBuildPositionInterfaceMatrixSize> matrix{};
    state.placements[0] = {7, &target, matrix.data()};
    state.placements[1] = {8, &target, matrix.data()};
    a2fo::g_hybrid_construction[producer.data()] = state;
    a2fo::g_hybrid_construction_state_count = 1;
    std::array<a2fo::QueuedConstructionGhost,
               a2fo::kNativeQueueCapacity> ghosts{};
    std::printf("ghosts:%lu\n", static_cast<unsigned long>(
        a2fo::collect_queued_construction_ghosts(producer.data(), ghosts)));

    std::vector<std::uint8_t> fleet(0x250000);
    a2fo::g_fleet_ops = reinterpret_cast<HMODULE>(fleet.data());
    std::array<std::array<std::uint8_t, 0x100>, 3> buttons{};
    std::array<void*, a2fo::kFoPopupButtonCount> button_ptrs{};
    for (unsigned index = 0; index < buttons.size(); ++index)
        button_ptrs[index] = buttons[index].data();
    std::memcpy(fleet.data() + a2fo::kFoPopupButtonPointerArrayRva,
                button_ptrs.data(), sizeof(button_ptrs));
    int yard_mode = 3;
    a2fo::g_hybrid_root_build_mode_info = &yard_mode;
    put(buttons[0], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(&yard_mode));
    put(buttons[1], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(&yard_mode));
    put(buttons[2], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(a2fo::g_hybrid_construct_mode_info.data()));
    a2fo::RuntimeClassLists lists{};
    lists.has_yard = true;
    lists.has_construct = true;
    a2fo::rebase_hybrid_build_root_buttons_after_compaction(&lists);
    std::printf("roots:%u:%u\n",
        a2fo::g_last_hybrid_yard_root_button == buttons[0].data(),
        a2fo::g_last_hybrid_construct_root_button == buttons[2].data());

    // The original Refit scan reads each binding's ModeInfo after the
    // checked button read. Mutate the field at that exact check.
    void** backing_buttons = reinterpret_cast<void**>(
        fleet.data() + a2fo::kFoPopupButtonPointerArrayRva);
    int old_mode = 4, new_mode = 5;
    put(buttons[0], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(&new_mode));
    a2fo::g_refit_bindings[0] = {
        buttons[2].data(), &old_mode, nullptr, nullptr,
        a2fo::RefitButtonKind::destination};
    a2fo::g_refit_binding_count = 1;
    a2fo::g_refit_back_mode_info = nullptr;
    arm_query_mutation(
        buttons[0].data() + a2fo::kControlButtonModeInfoOffset,
        &a2fo::g_refit_bindings[0].mode_info, &new_mode);
    a2fo::rebase_refit_buttons_after_layout();
    const bool refit_live_mode = !query_mutation_armed &&
        a2fo::g_refit_bindings[0].button == buttons[0].data();
    std::printf("rebase-live-mode:%u\n", refit_live_mode);
    if (!refit_live_mode) return 81;

    a2fo::g_refit_bindings[0].button = buttons[2].data();
    a2fo::g_refit_bindings[0].mode_info = &new_mode;
    backing_buttons[0] = buttons[0].data();
    arm_query_mutation(
        buttons[0].data() + a2fo::kControlButtonModeInfoOffset,
        &backing_buttons[0], buttons[1].data());
    a2fo::rebase_refit_buttons_after_layout();
    const bool refit_cached_button = !query_mutation_armed &&
        a2fo::g_refit_bindings[0].button == buttons[0].data();
    std::printf("rebase-cached-button:%u\n", refit_cached_button);
    if (!refit_cached_button) return 82;
    a2fo::g_refit_binding_count = 0;

    backing_buttons[0] = buttons[0].data();
    backing_buttons[1] = buttons[1].data();
    put(buttons[0], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(&yard_mode));
    put(buttons[1], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(&yard_mode));
    put(buttons[2], a2fo::kControlButtonModeInfoOffset,
        static_cast<void*>(nullptr));
    arm_query_mutation(
        buttons[0].data() + a2fo::kControlButtonModeInfoOffset,
        &backing_buttons[0], buttons[1].data());
    a2fo::rebase_hybrid_build_root_buttons_after_compaction(&lists);
    const bool roots_cached_buttons = !query_mutation_armed &&
        a2fo::g_last_hybrid_yard_root_button == buttons[0].data() &&
        a2fo::g_last_hybrid_construct_root_button == buttons[1].data();
    std::printf("roots-cached-buttons:%u\n", roots_cached_buttons);
    if (!roots_cached_buttons) return 83;
    backing_buttons[0] = buttons[0].data();

    void* engine_image = VirtualAlloc(
        nullptr, 0x1000000, MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE);
    if (!engine_image) return 80;
    a2fo::g_armada = static_cast<HMODULE>(engine_image);
    install_fixture_jump(engine_image, a2fo::kGameObjectClassFindRva,
                         reinterpret_cast<void*>(&fake_find_name));
    install_fixture_jump(engine_image,
                         a2fo::kGameObjectClassFindProjectIdRva,
                         reinterpret_cast<void*>(&fake_find_project));
    install_fixture_jump(engine_image, a2fo::kGameOperatorDeleteRva,
                         reinterpret_cast<void*>(&fake_delete));
    class_alpha = target_class.data();
    class_beta = producer_class.data();
    std::array<void*, a2fo::kRuntimeBuildListCapacity> original_items{};
    original_items[0] = &yard_mode;
    original_items[1] = &yard_mode;
    original_items[2] = &project_id;
    std::array<std::uint8_t, 0x500> submenu_class{};
    put(submenu_class, a2fo::kProducerBuildItemsOffset,
        original_items.data());
    a2fo::build_submenu::Config submenu_config{};
    submenu_config.pages[0].children[0] = "alpha";
    submenu_config.pages[0].children[1] = "missing";
    submenu_config.pages[1].children[0] = "beta";
    submenu_config.pages[2].children[0] = "beta";
    const bool registered = a2fo::register_live_build_submenus(
        submenu_class.data(), submenu_config, "fixture.odf");
    const auto* published_submenu = a2fo::live_submenus_for_class(
        submenu_class.data());
    std::printf("submenu-register:%u:%lu:%u:%u:%u\n", registered,
        static_cast<unsigned long>(published_submenu
            ? published_submenu->pages.size() : 0),
        published_submenu && published_submenu->effective_items[0] ==
            class_alpha,
        published_submenu && published_submenu->effective_items[1] ==
            &yard_mode,
        published_submenu && published_submenu->effective_items[2] ==
            class_beta);
    std::array<std::uint8_t, 0x500> overflow_class{};
    put(overflow_class, a2fo::kProducerBuildItemsOffset,
        original_items.data());
    a2fo::build_submenu::Config overflow_config{};
    for (auto& child : overflow_config.pages[0].children)
        child = "alpha";
    std::printf("submenu-overflow:%u\n",
        a2fo::register_live_build_submenus(
            overflow_class.data(), overflow_config, "overflow.odf"));

    std::array<std::uint8_t, 0x500> station_class{};
    std::array<void*, a2fo::kRuntimeBuildListCapacity> legacy{};
    put(station_class, a2fo::kProducerBuildItemsOffset, legacy.data());
    auto publish_methods = std::make_unique<a2fo::HybridBuildLists>();
    publish_methods->add(a2fo::ProductionMethod::construct, 0, 91);
    publish_methods->add(a2fo::ProductionMethod::yard, 0, 92);
    publish_methods->add(a2fo::ProductionMethod::research, 0, 91);
    publish_methods->add(a2fo::ProductionMethod::evolve, 0, 92);
    a2fo::g_class_lists[station_class.data()] = std::move(publish_methods);
    const std::size_t published_count =
        a2fo::publish_research_station_hybrid_items(station_class.data());
    const auto* runtime =
        a2fo::g_runtime_class_lists[station_class.data()].get();
    std::printf("publish:%lu:%u:%u:%u:%u\n",
        static_cast<unsigned long>(published_count),
        runtime && runtime->has_construct,
        runtime && runtime->has_yard,
        runtime && runtime->has_explicit_research,
        runtime && runtime->has_evolve);

    // The case above retains duplicate project IDs and therefore credits
    // only construct and yard. Distinct IDs exercise all four native slots.
    std::array<std::uint8_t, 0x500> gamma_class{}, delta_class{};
    std::uint32_t gamma_project = 93, delta_project = 94;
    put(gamma_class, a2fo::kClassProjectIdOffset, &gamma_project);
    put(delta_class, a2fo::kClassProjectIdOffset, &delta_project);
    class_gamma = gamma_class.data();
    class_delta = delta_class.data();
    std::array<std::uint8_t, 0x500> all_station_class{};
    std::array<void*, a2fo::kRuntimeBuildListCapacity> all_legacy{};
    put(all_station_class, a2fo::kProducerBuildItemsOffset,
        all_legacy.data());
    auto all_methods = std::make_unique<a2fo::HybridBuildLists>();
    const bool all_added =
        all_methods->add(a2fo::ProductionMethod::construct, 0, 91) ==
            a2fo::AddListEntryResult::added &&
        all_methods->add(a2fo::ProductionMethod::yard, 0, 92) ==
            a2fo::AddListEntryResult::added &&
        all_methods->add(a2fo::ProductionMethod::research, 0, 93) ==
            a2fo::AddListEntryResult::added &&
        all_methods->add(a2fo::ProductionMethod::evolve, 0, 94) ==
            a2fo::AddListEntryResult::added;
    if (!all_added) return 84;
    a2fo::g_class_lists[all_station_class.data()] =
        std::move(all_methods);
    const std::size_t all_count =
        a2fo::publish_research_station_hybrid_items(
            all_station_class.data());
    const auto all_found = a2fo::g_runtime_class_lists.find(
        all_station_class.data());
    const auto* all_runtime = all_found ==
            a2fo::g_runtime_class_lists.end()
        ? nullptr : all_found->second.get();
    const bool all_construct = all_runtime &&
        all_runtime->has_construct &&
        all_runtime->construct[0] == class_alpha;
    const bool all_yard = all_runtime && all_runtime->has_yard &&
        all_runtime->yard[0] == class_beta;
    const bool all_research = all_runtime &&
        all_runtime->has_explicit_research &&
        all_runtime->research[0] == class_gamma;
    const bool all_evolve = all_runtime && all_runtime->has_evolve &&
        all_runtime->evolve[0] == class_delta;
    std::printf("publish-all:%lu:%u:%u:%u:%u\n",
        static_cast<unsigned long>(all_count), all_construct, all_yard,
        all_research, all_evolve);
    if (all_count != 4 || !all_construct || !all_yard ||
        !all_research || !all_evolve) return 85;
    a2fo::HybridConstructionState dedup{};
    dedup.cursor_interface = pending_matrix.data();
    dedup.pending_interface = pending_matrix.data();
    dedup.placements[0].build_interface = pending_matrix.data();
    dedup.placements[1].build_interface = matrix.data();
    dedup.placements[2].build_interface = matrix.data();
    a2fo::g_hybrid_construction[producer.data()] = dedup;
    a2fo::g_hybrid_construction_state_count = 1;
    a2fo::remove_hybrid_construction_state(producer.data());
    std::printf("remove-dedup:%u:%u:%u\n", destructor_calls,
                delete_calls,
                a2fo::g_hybrid_construction.find(producer.data()) ==
                    a2fo::g_hybrid_construction.end());
    a2fo::g_armada = nullptr;
    VirtualFree(engine_image, 0, MEM_RELEASE);

    a2fo::g_hybrid_construction.clear();
    a2fo::g_live_build_submenus.clear();
    a2fo::g_runtime_class_lists.clear();
    a2fo::g_class_lists.clear();
    a2fo::g_hybrid_construction_state_count = 0;
    a2fo::g_registry_lock_ready = false;
    DeleteCriticalSection(&a2fo::g_registry_lock);
    std::puts("hybrid-stage10-fixture:PASS");
}
