#define main a2fo_hybrid_stage8_unused_main
#include "rust_stage8_hybrid_runtime_native.cpp"
#undef main

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

static void** mutable_selection = nullptr;
static void* replacement_selection = nullptr;
static unsigned engine_calls = 0;
static unsigned popup_calls = 0;
extern "C" std::uintptr_t a2fo_call_thiscall_0(
    void* function, void* self) {
    ++engine_calls;
    std::printf("engine:%u:%u\n", engine_calls, self != nullptr);
    if (engine_calls == 1 && mutable_selection)
        mutable_selection[1] = replacement_selection;
    (void)function;
    return 0;
}
extern "C" std::uintptr_t a2fo_call_thiscall_3(
    void* function, void* self, std::uintptr_t, std::uintptr_t,
    std::uintptr_t) {
    ++popup_calls;
    std::printf("popup:%u:%u\n", popup_calls, self != nullptr);
    if (popup_calls == 1 && self) {
        *reinterpret_cast<std::uint32_t*>(
            a2fo::bytes(self) + a2fo::kPopupCurrentMenuOffset) = 0;
    }
    (void)function;
    return 0;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    std::set_terminate([] {
        try {
            if (auto error = std::current_exception())
                std::rethrow_exception(error);
        } catch (const std::runtime_error& error) {
            std::printf("terminate:%s\n", error.what());
            std::fflush(stdout);
            ExitProcess(77);
        } catch (...) {}
        ExitProcess(78);
    });
    A2FO_ModuleApi api{};
    api.log = fixture_log;
    a2fo::g_api = &api;
    InitializeCriticalSection(&a2fo::g_registry_lock);
    a2fo::g_registry_lock_ready = true;
    std::array<std::uint8_t, 0x500> producer{}, second{}, producer_class{};
    std::array<std::array<std::uint8_t, 0x500>, 3> targets{};
    std::array<std::uint32_t, 3> projects{{1, 2, 3}};
    for (unsigned index = 0; index < targets.size(); ++index)
        put(targets[index], a2fo::kClassProjectIdOffset,
            &projects[index]);
    put(producer, a2fo::kObjectClassOffset, producer_class.data());
    put(second, a2fo::kObjectClassOffset, producer_class.data());
    auto runtime = std::make_unique<a2fo::RuntimeClassLists>();
    auto* lists = runtime.get();
    lists->has_yard = true;
    lists->has_construct = true;
    lists->has_evolve = true;
    lists->has_explicit_research = true;
    lists->yard[0] = targets[0].data();
    lists->construct[0] = targets[1].data();
    lists->research[0] = targets[2].data();
    a2fo::g_runtime_class_lists[producer_class.data()] = std::move(runtime);
    auto explicit_lists = std::make_unique<a2fo::HybridBuildLists>();
    explicit_lists->add(a2fo::ProductionMethod::yard, 0, 1);
    explicit_lists->add(a2fo::ProductionMethod::construct, 0, 2);
    explicit_lists->add(a2fo::ProductionMethod::research, 0, 3);
    a2fo::g_class_lists[producer_class.data()] = std::move(explicit_lists);
    if (argc > 1 && std::strcmp(argv[1], "log-failure") == 0) {
        failure = "log";
        a2fo::retain_hybrid_research_menu_after_order(
            producer.data(), targets[2].data());
        return 79;
    }

    void* selected[2]{producer.data(), nullptr};
    std::array<std::uint8_t, 12> craft_array{};
    put(craft_array, 0, selected);
    put(craft_array, 8, std::int32_t{2});
    for (void* target : {static_cast<void*>(nullptr),
                         static_cast<void*>(producer.data()),
                         static_cast<void*>(second.data())}) {
        std::printf("selection:%u\n",
                    a2fo::selection_contains(craft_array.data(), target));
    }
    put(craft_array, 8, std::int32_t{257});
    std::printf("selection-invalid:%u\n",
                a2fo::selection_contains(craft_array.data(), producer.data()));
    put(craft_array, 8, std::int32_t{2});

    a2fo::g_armada = reinterpret_cast<HMODULE>(0x10000000);
    mutable_selection = selected;
    replacement_selection = second.data();
    a2fo::prepare_selected_hybrid_menus(craft_array.data(), a2fo::kBuildMenu);
    std::printf("selected-mutation:%u:%u\n",
                selected[1] == second.data(), engine_calls);
    mutable_selection = nullptr;
    for (auto menu : {a2fo::kBuildMenu, a2fo::kEvolveMenu,
                      a2fo::kResearchMenu, std::uint32_t{99}}) {
        a2fo::g_hybrid_build_palette =
            a2fo::HybridBuildPalette::construct;
        a2fo::prepare_hybrid_station_menu(producer.data(), menu);
        void** active = *reinterpret_cast<void***>(
            producer.data() + a2fo::kCurrentBuildClassOffset - sizeof(void*));
        std::printf("station-menu:%lu:%u:%u\n",
                    static_cast<unsigned long>(menu),
                    active == lists->construct.data(),
                    active == lists->evolve.data());
    }

    put(producer, a2fo::kCurrentBuildClassOffset, targets[0].data());
    put(producer, a2fo::kCurrentQueueIdOffset, std::uint32_t{77});
    put(producer, a2fo::kQueueCountOffset, std::uint32_t{2});
    std::array<std::uint8_t, 0x20> first{}, second_item{};
    put(first, 0, targets[0].data());
    put(first, a2fo::kQueueItemIdOffset, std::uint32_t{77});
    put(first, a2fo::kQueueItemNextOffset, second_item.data());
    put(second_item, 0, targets[1].data());
    put(second_item, a2fo::kQueueItemIdOffset, std::uint32_t{78});
    put(second_item, a2fo::kQueueItemNextOffset,
        static_cast<void*>(nullptr));
    put(producer, a2fo::kQueueHeadOffset, first.data());
    std::array<void*, a2fo::kNativeQueueCapacity> queue{};
    const auto count = a2fo::collect_hybrid_build_queue(
        producer.data(), queue);
    std::printf("queue:%lu:%u:%u:%u\n",
                static_cast<unsigned long>(count),
                queue[0] == targets[0].data(),
                queue[1] == targets[1].data(), queue[2] == nullptr);
    std::array<char, 260> sprite{};
    put(targets[2], a2fo::kClassBaseNameOffset, "researchpod");
    const bool named = a2fo::research_single_sprite_name(
        targets[2].data(), sprite);
    std::printf("sprite:%u:%s\n", named, sprite.data());

    a2fo::g_hybrid_construct_placement_press_station = producer.data();
    const bool defer_construct =
        a2fo::hybrid_production_should_defer_construct_order(
            producer.data(), targets[1].data());
    const bool defer_yard =
        a2fo::hybrid_production_should_defer_construct_order(
            producer.data(), targets[0].data());
    std::printf("construct-defer:%u:%u\n",
                defer_construct, defer_yard);
    a2fo::g_hybrid_construct_placement_press_station = nullptr;
    a2fo::g_control_button_press_depth = 1;
    a2fo::retain_hybrid_research_menu_after_order(
        producer.data(), targets[2].data());
    std::printf("research-retain:%u:%u:%lu\n",
        a2fo::g_post_press_research_menu_station == producer.data(),
        a2fo::g_retain_research_menu_station == producer.data(),
        static_cast<unsigned long>(a2fo::g_retain_research_menu_refreshes));

    auto submenu = std::make_unique<a2fo::LiveBuildSubmenuClass>();
    submenu->pages.emplace_back();
    submenu->pages.back().parent_class = targets[0].data();
    a2fo::g_live_build_submenus[producer_class.data()] =
        std::move(submenu);
    a2fo::g_last_single_build_submenu_selection = producer.data();
    std::array<std::uint8_t, 0x200> button{}, mode{}, popup{};
    put(button, a2fo::kControlButtonModeInfoOffset, mode.data());
    put(mode, a2fo::kModeInfoTypeOffset, std::uint32_t{1});
    put(mode, a2fo::kModeInfoTargetClassOffset, targets[0].data());
    a2fo::g_last_popup = popup.data();
    a2fo::g_last_popup_craft_array = craft_array.data();
    a2fo::g_popup_update_buttons_hook.gateway =
        reinterpret_cast<void*>(1);
    a2fo::g_fleet_ops = reinterpret_cast<HMODULE>(0x20000000);
    const bool opened = a2fo::handle_live_build_submenu_press(button.data());
    std::printf("submenu-open:%u:%u:%u\n", opened,
        a2fo::g_active_build_submenu_parent == targets[0].data(),
        popup_calls);
    put(mode, a2fo::kModeInfoTypeOffset, std::uint32_t{3});
    const bool backed = a2fo::handle_live_build_submenu_press(button.data());
    std::printf("submenu-back:%u:%u:%u\n", backed,
        a2fo::g_active_build_submenu_parent == nullptr, popup_calls);

    std::vector<std::uint8_t> fleet(0x248000);
    a2fo::g_fleet_ops = reinterpret_cast<HMODULE>(fleet.data());
    a2fo::g_last_single_hybrid_selection = producer.data();
    put(fleet, a2fo::kFoPopupConstructionButtonPointerRva,
        button.data());
    a2fo::select_hybrid_build_palette(button.data());
    std::printf("palette-construct:%u:%u\n",
        a2fo::g_hybrid_build_palette ==
            a2fo::HybridBuildPalette::construct,
        a2fo::g_hybrid_build_palette_press_armed);
    put(fleet, a2fo::kFoPopupSpareRootButtonPointerRva,
        second.data());
    a2fo::select_hybrid_build_palette(second.data());
    std::printf("palette-yard:%u:%u\n",
        a2fo::g_hybrid_build_palette == a2fo::HybridBuildPalette::yard,
        a2fo::g_hybrid_build_palette_press_armed);
    put(fleet, a2fo::kFoPopupButtonPointerArrayRva, button.data());
    put(button, a2fo::kControlButtonStateOffset, std::uint32_t{99});
    a2fo::apply_build_submenu_parent_states(producer.data());
    const auto parent_state = *reinterpret_cast<const std::uint32_t*>(
        button.data() + a2fo::kControlButtonStateOffset);
    std::printf("parent-state:%lu\n",
                static_cast<unsigned long>(parent_state));

    std::array<std::uint8_t, 0x40> button_list{};
    put(producer, a2fo::kCurrentBuildClassOffset, targets[2].data());
    put(producer, a2fo::kQueueHeadOffset, static_cast<void*>(nullptr));
    put(producer, a2fo::kCurrentBuildClassOffset - sizeof(void*),
        lists->research.data());
    put(producer, a2fo::kProducerButtonListOffset, button_list.data());
    put(button_list, a2fo::kButtonListEnabledMaskOffset,
        std::uint32_t{0xffffffff});
    put(mode, a2fo::kModeInfoTargetClassOffset, targets[2].data());
    put(button, a2fo::kControlButtonStateOffset, std::uint32_t{99});
    a2fo::disable_queued_research_buttons(producer.data());
    const auto enabled = *reinterpret_cast<const std::uint32_t*>(
        button_list.data() + a2fo::kButtonListEnabledMaskOffset);
    const auto research_button_state =
        *reinterpret_cast<const std::uint32_t*>(
            button.data() + a2fo::kControlButtonStateOffset);
    std::printf("research-mask:%lu:%lu\n",
                static_cast<unsigned long>(enabled),
                static_cast<unsigned long>(research_button_state));

    a2fo::g_live_build_submenus.clear();
    a2fo::g_class_lists.clear();
    a2fo::g_runtime_class_lists.clear();
    a2fo::g_registry_lock_ready = false;
    DeleteCriticalSection(&a2fo::g_registry_lock);
    std::puts("hybrid-stage9-fixture:PASS");
}
