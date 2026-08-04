#include "a2fo_module_api.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {
HMODULE fleet_ops = nullptr;
void* fake_armada = nullptr;
unsigned bink_call_patch_count = 0;
bool hybrid_research_start_hooked = false;
bool hybrid_research_cancel_hooked = false;
bool hybrid_research_can_build_hooked = false;
bool hybrid_research_item_conflict_hooked = false;
bool hybrid_research_button_update_hooked = false;
bool hybrid_research_matrix_hooked = false;
bool hybrid_research_finish_hooked = false;
bool hybrid_control_button_press_hooked = false;
bool hybrid_race_icon_render_hooked = false;
bool hybrid_ship_display_single_object_display_patched = false;
bool hybrid_ship_display_single_object_simulate_patched = false;
bool hybrid_producer_is_busy_hooked = false;
bool hybrid_producer_pop_checked_hooked = false;
bool hybrid_popup_update_hooked = false;
bool hybrid_build_button_bind_patched = false;
bool hybrid_hover_wireframe_patched = false;

std::uint32_t A2FO_CALL upgrade_pod_maximum_tier() { return 6; }

template <std::size_t Size>
void set_signature(std::uintptr_t rva, const std::uint8_t (&value)[Size]) {
    std::memcpy(static_cast<std::uint8_t*>(fake_armada) + rva,
                value, Size);
}

void prepare_armada_signatures() {
    const std::uint8_t queue_class_command[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
    const std::uint8_t dequeue_class_command[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c};
    const std::uint8_t dtor[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t simulate[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1};
    const std::uint8_t load[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x10};
    const std::uint8_t save[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08};
    const std::uint8_t find_by_project_id[] =
        {0x55, 0x8b, 0xec, 0xa1, 0xf8, 0x0b, 0x74, 0x00};
    const std::uint8_t pod_detach[] =
        {0x56, 0x8b, 0xf1, 0x57, 0x8b, 0x7e, 0x40};
    const std::uint8_t pod_attach[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x53, 0x56};
    const std::uint8_t station_constructor[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0x7d, 0x08};
    const std::uint8_t station_destructor[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t team_manager[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t set_multiplier[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08, 0x8b, 0x45, 0x0c};
    const std::uint8_t find_by_name[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t parameter_project_id[] =
        {0x55, 0x8b, 0xec, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00};
    // Native modules load after the core has detoured GetString. Hybrid
    // initialization must accept and call this dispatcher entry rather than
    // requiring Armada's original prologue.
    const std::uint8_t parameter_string_dispatcher[] =
        {0xe9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    const std::uint8_t find_lazy_by_project_id[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
    const std::uint8_t producer_matrix[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x58, 0x02, 0x00, 0x00};
    const std::uint8_t producer_is_busy[] =
        {0x8b, 0x81, 0x54, 0x02, 0x00, 0x00, 0x85, 0xc0};
    const std::uint8_t cleanup_is_busy_query[] =
        {0x8b, 0x73, 0x30, 0x8b, 0xce, 0x8b, 0x06,
         0xff, 0x90, 0x38, 0x01, 0x00, 0x00, 0x84, 0xc0};
    const std::uint8_t admission_is_busy_query[] =
        {0x8b, 0x06, 0x8b, 0xce, 0xff, 0x90, 0x38,
         0x01, 0x00, 0x00, 0x8b, 0x7d, 0x08, 0x84, 0xc0};
    const std::uint8_t queue_pop_query[] = {0x8b, 0xce, 0xe8};
    const std::uint8_t research_start[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x56};
    const std::uint8_t research_cancel[] =
        {0x55, 0x8b, 0xec, 0x51, 0x56, 0x57};
    const std::uint8_t research_can_build[] =
        {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x54, 0x02, 0x00, 0x00};
    const std::uint8_t research_item_conflict[] =
        {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
    const std::uint8_t research_button_update[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
    const std::uint8_t research_matrix[] =
        {0x55, 0x8b, 0xec, 0x51, 0x56, 0x57};
    const std::uint8_t control_button_press[] =
        {0x8b, 0xc1, 0x56, 0x8b, 0x90, 0x88, 0x00, 0x00, 0x00};
    const std::uint8_t race_icon_render[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t ship_display_single_object_display_call[] =
        {0xe8, 0x1a, 0xbc, 0x4f, 0x5a};
    const std::uint8_t ship_display_single_object_simulate_call[] =
        {0xe8, 0x2b, 0xc1, 0x4f, 0x5a};
    const std::uint8_t bink_set_dibits_call[] =
        {0xff, 0x15, 0x98, 0x7a, 0x7b, 0x00};
    const std::uint8_t bink_texture_render[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t bink_texture_render_call1[] =
        {0xe8, 0xa6, 0xe5, 0xf7, 0xff};
    const std::uint8_t bink_texture_render_call2[] =
        {0xe8, 0xf8, 0xe1, 0xf7, 0xff};
    set_signature(0x0d4280, queue_class_command);
    set_signature(0x0d45f0, dequeue_class_command);
    set_signature(0x0b77d0, dtor);
    set_signature(0x0b7840, simulate);
    set_signature(0x0b88d0, load);
    set_signature(0x0b8aa0, save);
    set_signature(0x0cd150, find_by_project_id);
    set_signature(0x0b95a0, pod_detach);
    set_signature(0x0b95f0, pod_attach);
    set_signature(0x0b99b0, station_constructor);
    set_signature(0x0b9b50, station_destructor);
    set_signature(0x096340, team_manager);
    set_signature(0x0987d0, set_multiplier);
    set_signature(0x0cd370, find_by_name);
    set_signature(0x135200, parameter_project_id);
    set_signature(0x135350, parameter_string_dispatcher);
    set_signature(0x0cd1f0, find_lazy_by_project_id);
    set_signature(0x0b9170, producer_matrix);
    set_signature(0x0b0210, producer_is_busy);
    set_signature(0x031194, cleanup_is_busy_query);
    set_signature(0x031495, admission_is_busy_query);
    set_signature(0x0311a5, queue_pop_query);
    set_signature(0x031525, queue_pop_query);
    set_signature(0x0ba0e0, research_start);
    set_signature(0x0ba1b0, research_cancel);
    set_signature(0x0ba280, research_can_build);
    set_signature(0x0ba4a0, research_item_conflict);
    set_signature(0x0b9d70, research_button_update);
    set_signature(0x0babd0, research_matrix);
    set_signature(0x0e69e0, control_button_press);
    set_signature(0x0ee530, race_icon_render);
    set_signature(0x0f2c49, ship_display_single_object_display_call);
    set_signature(0x0f29e4, ship_display_single_object_simulate_call);
    set_signature(0x0639ba, bink_set_dibits_call);
    set_signature(0x064350, bink_texture_render);
    set_signature(0x0e5da5, bink_texture_render_call1);
    set_signature(0x0e6153, bink_texture_render_call2);
}

void A2FO_CALL log_line(const char* module, const char* message) {
    if (message && (std::strstr(message, "signature") ||
                    std::strstr(message, "disabled"))) {
        std::fprintf(stderr, "[%s] %s\n", module ? module : "module",
                     message);
    }
}
void* A2FO_CALL armada_module() { return fake_armada; }
void* A2FO_CALL fleetops_module() { return fleet_ops; }
bool A2FO_CALL register_lookup(
    const char*, A2FO_FofsItemLookupHandler handler, void*) {
    return handler != nullptr;
}
bool A2FO_CALL register_alias(const char*, const char*, const char*) {
    return true;
}
bool A2FO_CALL register_cocoon(const char*, const char*) { return true; }
bool A2FO_CALL install_hook(void* target, void* replacement,
                            std::size_t length,
                            const std::uint8_t* expected,
                            A2FO_InlineHook* hook) {
    if (!target || !replacement || !expected || !hook || length == 0 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    hook->target = target;
    hook->gateway = target;
    hook->length = length;
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0ba0e0) {
        hybrid_research_start_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0ba1b0) {
        hybrid_research_cancel_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0ba280) {
        hybrid_research_can_build_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0ba4a0) {
        hybrid_research_item_conflict_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b9d70) {
        hybrid_research_button_update_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0babd0) {
        hybrid_research_matrix_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0e69e0) {
        hybrid_control_button_press_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0ee530) {
        hybrid_race_icon_render_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b0210) {
        hybrid_producer_is_busy_hooked = true;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e9b3c) {
        hybrid_research_finish_hooked = true;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x122b04) {
        hybrid_producer_pop_checked_hooked = true;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e6c70) {
        hybrid_popup_update_hooked = true;
    }
    return true;
}
bool A2FO_CALL patch_call(void* target, void* replacement,
                          const std::uint8_t* expected,
                          std::size_t length) {
    if (!target || !replacement || !expected || length == 0 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e6d97) {
        hybrid_build_button_bind_patched = true;
    } else if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e8572) {
        hybrid_hover_wireframe_patched = true;
    } else if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x0f2c49) {
        hybrid_ship_display_single_object_display_patched = true;
    } else if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x0f29e4) {
        hybrid_ship_display_single_object_simulate_patched = true;
    } else {
        ++bink_call_patch_count;
    }
    return true;
}
}

int main() {
    fake_armada = VirtualAlloc(nullptr, 0x00410000,
                              MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!fake_armada) return 1;
    prepare_armada_signatures();
    // The real FleetOpsHook DllMain assumes Armada has already initialized
    // fixed engine globals. Map the PE image and its sections for identity and
    // signature validation without resolving imports or executing DllMain.
    fleet_ops = LoadLibraryExA("FleetOpsHook.fixture.dll", nullptr,
                               DONT_RESOLVE_DLL_REFERENCES);
    if (!fleet_ops) return 2;
    void** object_button_press_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
        0x240c00);
    void* object_button_press_original =
        static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
        0x10ad40;
    *object_button_press_slot = object_button_press_original;
    HMODULE feature = LoadLibraryA("modules\\A2FOFeaturePack.dll");
    if (!feature) return 3;

    A2FO_ModuleInitFn init = nullptr;
    FARPROC address = GetProcAddress(feature, "A2FO_ModuleInit");
    std::memcpy(&init, &address, sizeof(init));
    if (!init) return 4;

    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &log_line;
    api.armada_module = &armada_module;
    api.fleetops_module = &fleetops_module;
    api.install_inline_hook = &install_hook;
    api.patch_call = &patch_call;
    api.register_fofs_item_lookup_handler = &register_lookup;
    api.register_classlabel_alias = &register_alias;
    api.register_evolver_cocoon_command = &register_cocoon;
    api.api_revision = A2FO_MODULE_API_REVISION;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH |
        A2FO_CAP_UPGRADE_POD_POLICY;
    api.upgrade_pod_maximum_tier = &upgrade_pod_maximum_tier;
    if (!init(&api)) return 5;
    if (bink_call_patch_count != 4) return 6;
    if (!hybrid_research_start_hooked ||
        !hybrid_research_cancel_hooked ||
        !hybrid_research_can_build_hooked ||
        !hybrid_research_item_conflict_hooked ||
        hybrid_research_button_update_hooked ||
        !hybrid_research_matrix_hooked ||
        !hybrid_research_finish_hooked ||
        !hybrid_control_button_press_hooked ||
        !hybrid_race_icon_render_hooked ||
        !hybrid_ship_display_single_object_display_patched ||
        !hybrid_ship_display_single_object_simulate_patched ||
        !hybrid_producer_is_busy_hooked ||
        !hybrid_producer_pop_checked_hooked ||
        !hybrid_popup_update_hooked ||
        !hybrid_build_button_bind_patched ||
        !hybrid_hover_wireframe_patched ||
        *object_button_press_slot == object_button_press_original) {
        std::fprintf(
            stderr,
            "hybrid hooks: start=%d cancel=%d can=%d conflict=%d "
            "button_update=%d matrix=%d finish=%d control_press=%d race=%d "
            "display=%d simulate=%d "
            "busy=%d pop=%d popup=%d bind=%d hover=%d "
            "press=%d\n",
            hybrid_research_start_hooked, hybrid_research_cancel_hooked,
            hybrid_research_can_build_hooked,
            hybrid_research_item_conflict_hooked,
            hybrid_research_button_update_hooked,
            hybrid_research_matrix_hooked, hybrid_research_finish_hooked,
            hybrid_control_button_press_hooked,
            hybrid_race_icon_render_hooked,
            hybrid_ship_display_single_object_display_patched,
            hybrid_ship_display_single_object_simulate_patched,
            hybrid_producer_is_busy_hooked,
            hybrid_producer_pop_checked_hooked,
            hybrid_popup_update_hooked, hybrid_build_button_bind_patched,
            hybrid_hover_wireframe_patched,
            *object_button_press_slot != object_button_press_original);
        return 7;
    }

    FreeLibrary(feature);
    FreeLibrary(fleet_ops);
    VirtualFree(fake_armada, 0, MEM_RELEASE);
}
