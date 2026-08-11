#include "a2fo_module_api.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {
using FopenFn = FILE* (__cdecl*)(const char*, const char*);

HMODULE fleet_ops = nullptr;
void* fake_armada = nullptr;
void* original_armada_fopen = nullptr;
unsigned bink_call_patch_count = 0;
bool hybrid_research_start_hooked = false;
bool hybrid_research_cancel_hooked = false;
bool hybrid_research_can_build_hooked = false;
bool hybrid_research_item_conflict_hooked = false;
bool hybrid_research_button_update_hooked = false;
bool hybrid_research_matrix_hooked = false;
bool hybrid_research_finish_hooked = false;
bool hybrid_producer_get_action_hooked = false;
bool hybrid_build_position_interface_load_hooked = false;
bool hybrid_producer_start_effect_hooked = false;
bool hybrid_producer_cancel_effect_hooked = false;
bool hybrid_producer_update_effect_hooked = false;
bool hybrid_producer_stop_effect_hooked = false;
bool hybrid_craft_render_internal_hooked = false;
bool hybrid_shield_render_observer_linked = false;
bool hybrid_control_button_press_hooked = false;
bool hybrid_mode_info_button_name_hooked = false;
bool hybrid_race_icon_render_hooked = false;
bool hybrid_ship_display_single_object_display_patched = false;
bool hybrid_ship_display_single_object_simulate_patched = false;
bool hybrid_producer_is_busy_hooked = false;
bool hybrid_producer_pop_checked_hooked = false;
bool hybrid_popup_update_hooked = false;
bool hybrid_construction_hardpoint_call_patched = false;
bool hybrid_build_button_bind_patched = false;
bool hybrid_evolve_button_bind_patched = false;
bool hybrid_ai_button_bind_patched = false;
bool hybrid_hover_wireframe_patched = false;
bool wingman_alias_registered = false;
bool wingman_odf_defaults_registered = false;
bool constructionrig_odf_defaults_registered = false;
bool freighter_odf_defaults_registered = false;
bool addon_odf_overlay_registered = false;
bool producer_event_handler_registered = false;
A2FO_ProducerEventHandler producer_event_handler = nullptr;
void* producer_event_user_data = nullptr;
unsigned a1_officer_system_hook_count = 0;
bool hybridbuild_alias_registered = false;
bool info_ini_handler_registered = false;
bool rgb_lock_surface_hooked = false;
bool rgb_blend_guard_hooked = false;
bool rgb_scan_grid_guard_hooked = false;
bool rgb_file_exists_hooked = false;
bool rgb_open_read_hooked = false;
bool cheats_show_me_the_money_hooked = false;
bool cheats_chat_init_hooked = false;
bool cheats_rts_config_loaded = false;
bool edit_menu_update_hooked = false;
bool always_show_shields_starbase_hooked = false;
bool always_show_shields_publish_hooked = false;
bool always_show_shields_render_list_hooked = false;
bool turret_alias_registered = false;
bool turret_odf_defaults_registered = false;
unsigned turret_hook_count = 0;
bool turret_craft_simulate_chained = false;
bool turret_class_constructor_chained = false;
bool turret_shield_visibility_linked = false;
bool turret_fire_arc_trigger_filter_linked = false;
bool turret_normal_weapon_tech_trigger_filter_linked = false;
bool normal_weapon_tech_initialized = false;
unsigned fire_arc_hook_count = 0;
bool fire_arc_class_constructor_chained = false;
bool fire_arc_target_check_chained = false;
bool fire_arc_icon_hover_hooked = false;
char extension_root_path[MAX_PATH] = ".";
char parent_extension_root_path[MAX_PATH] = ".";

struct FixtureCheatRegistration {
    const char* command;
    std::uint8_t multiplayer_allowed;
    std::uint8_t reserved[3];
    void* handler;
};
static_assert(sizeof(FixtureCheatRegistration) == 12,
              "unexpected 32-bit cheat registration layout");

struct FixtureCheatRegistry {
    std::uint32_t reference_count = 0xffffffffu;
    std::uint32_t length = 4;
    FixtureCheatRegistration registrations[4]{
        {"m", 1, {0, 0, 0}, nullptr},
        {"dis", 1, {0, 0, 0}, nullptr},
        {"crash", 1, {0, 0, 0}, nullptr},
        {"elim", 1, {0, 0, 0}, nullptr},
    };
};
FixtureCheatRegistry cheat_registry_fixture{};

bool write_fixture_file(const std::string& path, const void* contents,
                        DWORD size) {
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, contents, size, &written, nullptr);
    CloseHandle(file);
    return ok && written == size;
}

std::uint32_t A2FO_CALL upgrade_pod_maximum_tier() { return 6; }
bool A2FO_CALL get_original_classlabel(
    void*, char* output, std::uint32_t output_size) {
    constexpr char label[] = "hybridbuild";
    if (!output || output_size < sizeof(label)) return false;
    std::memcpy(output, label, sizeof(label));
    return true;
}
bool A2FO_CALL associate_evolver_cocoon_class(void*, void*) {
    return true;
}
bool A2FO_CALL register_producer_event_handler(
    const char*, A2FO_ProducerEventHandler handler, void* user_data) {
    producer_event_handler_registered = handler != nullptr;
    producer_event_handler = handler;
    producer_event_user_data = user_data;
    return producer_event_handler_registered;
}
bool A2FO_CALL dispatch_producer_event(const A2FO_ProducerEvent* event) {
    return producer_event_handler
        ? producer_event_handler(event, producer_event_user_data)
        : true;
}

template <std::size_t Size>
void set_signature(std::uintptr_t rva, const std::uint8_t (&value)[Size]) {
    std::memcpy(static_cast<std::uint8_t*>(fake_armada) + rva,
                value, Size);
}

void prepare_armada_signatures() {
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(fake_armada);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x100;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(
        static_cast<std::uint8_t*>(fake_armada) + dos->e_lfanew);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = 0x3c4c76bd;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = 0x00403999;

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
    const std::uint8_t producer_get_action[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57};
    const std::uint8_t construction_rig_get_action[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57, 0x8b, 0xf9};
    const std::uint8_t construction_rig_start[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x30, 0x56};
    const std::uint8_t construction_rig_start_hardpoint_call[] =
        {0xe8, 0x73, 0x5c, 0xfc, 0xff};
    const std::uint8_t construction_rig_cancel[] =
        {0x56, 0x8b, 0xf1, 0x57, 0xb9, 0x48, 0x6e, 0x73, 0x00};
    const std::uint8_t construction_rig_finish[] =
        {0x55, 0x8b, 0xec, 0x51, 0x53, 0x56, 0x57};
    const std::uint8_t construction_rig_remove_object[] =
        {0x56, 0x57, 0x8b, 0xf9, 0x8b, 0x87, 0xb4, 0x02, 0x00, 0x00};
    const std::uint8_t construction_rig_matrix[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x56};
    const std::uint8_t build_position_constructor[] =
        {0x55, 0x8b, 0xec, 0x8b, 0xc1, 0x56};
    const std::uint8_t build_position_destructor[] =
        {0x56, 0x8b, 0x71, 0x34, 0x85, 0xf6};
    const std::uint8_t placeholder_render_internal[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
    const std::uint8_t game_operator_new[] =
        {0x55, 0x8b, 0xec, 0xa1, 0x00, 0xec, 0x7a, 0x00};
    const std::uint8_t game_operator_delete[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x85, 0xc0};
    const std::uint8_t producer_matrix[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x58, 0x02, 0x00, 0x00};
    const std::uint8_t producer_start_effect[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x30, 0x56};
    const std::uint8_t producer_cancel_effect[] =
        {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x68, 0x02, 0x00, 0x00};
    const std::uint8_t producer_update_effect[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x81, 0x68, 0x02, 0x00, 0x00};
    const std::uint8_t producer_stop_effect[] =
        {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0x68, 0x02, 0x00, 0x00};
    const std::uint8_t producer_is_busy[] =
        {0x8b, 0x81, 0x54, 0x02, 0x00, 0x00, 0x85, 0xc0};
    const std::uint8_t cleanup_is_busy_query[] =
        {0x8b, 0x73, 0x30, 0x8b, 0xce, 0x8b, 0x06,
         0xff, 0x90, 0x38, 0x01, 0x00, 0x00, 0x84, 0xc0};
    const std::uint8_t admission_is_busy_query[] =
        {0x8b, 0x06, 0x8b, 0xce, 0xff, 0x90, 0x38,
         0x01, 0x00, 0x00, 0x8b, 0x7d, 0x08, 0x84, 0xc0};
    const std::uint8_t queue_pop_query[] = {0x8b, 0xce, 0xe8};
    const std::uint8_t build_position_interface_load[] =
        {0x8b, 0x8e, 0xa4, 0x02, 0x00, 0x00};
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
    const std::uint8_t evolver_swap_objects[] =
        {0x55, 0x8b, 0xec, 0x53, 0x56, 0x57};
    const std::uint8_t evolver_matrix[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57};
    const std::uint8_t evolver_start_effect[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t evolver_remove_effect[] =
        {0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xac, 0x02, 0x00, 0x00};
    const std::uint8_t evolver_update_effect[] =
        {0x55, 0x8b, 0xec, 0x81, 0xec, 0xac, 0x00, 0x00, 0x00};
    const std::uint8_t evolver_render_internal[] =
        {0x55, 0x8b, 0xec, 0x53, 0x56};
    const std::uint8_t control_button_press[] =
        {0x8b, 0xc1, 0x56, 0x8b, 0x90, 0x88, 0x00, 0x00, 0x00};
    const std::uint8_t mode_info_button_name[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x48, 0x53};
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
    const std::uint8_t rgb_file_exists[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t rgb_open_read[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t rgb_reader[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
    const std::uint8_t rgb_read_exists_call[] =
        {0xe8, 0x26, 0xd1, 0xff, 0xff};
    const std::uint8_t rgb_load_exists_call[] =
        {0xe8, 0x78, 0xcb, 0xff, 0xff};
    const std::uint8_t rgb_lock_surface[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57};
    const std::uint8_t rgb_blend_xrgb888[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x40};
    const std::uint8_t rgb_update_scan_grid[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28};
    const std::uint8_t rgb_sprite_get_texture[] =
        {0x8b, 0x41, 0x58, 0xc3};
    const std::uint8_t rgb_unlock_surface[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57};
    const std::uint8_t cheats_add_crew_capacity[] =
        {0x55, 0x8b, 0xec, 0x51, 0x8a,
         0x81, 0x4e, 0x02, 0x00, 0x00};
    const std::uint8_t cheats_disable_shield_generator[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x91, 0xe0, 0x01, 0x00, 0x00};
    const std::uint8_t cheats_entity_get_transform[] =
        {0x8b, 0x41, 0x04, 0x83, 0xc0, 0x44, 0xc3};
    const std::uint8_t cheats_queue_command_vector[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0xf1,
         0x8b, 0x0d, 0x88, 0xb8, 0x76, 0x00};
    const std::uint8_t cheats_eliminate_team[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x55, 0x08,
         0x53, 0x8b, 0xd9, 0x83, 0x7c};
    const std::uint8_t a1_nebula_set_textures_recursive[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b, 0x75, 0x08};
    const std::uint8_t a1_rtime_load_name[] =
        {0x83, 0xc4, 0x18, 0x33, 0xc0};
    const std::uint8_t a1_starbase_geometry[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t a1_starbase_class_build[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t a1_starbase_finish_build[] =
        {0x53, 0x56, 0x57, 0x8b, 0xf1};
    const std::uint8_t a1_starbase_clear_team[] =
        {0x55, 0x8b, 0xec, 0x51, 0x56};
    const std::uint8_t a1_starbase_set_team[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t a1_starbase_load_save[] =
        {0x55, 0x8b, 0xec, 0x56, 0x57};
    const std::uint8_t a1_officer_upgrade_class_build[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t a1_producer_pop_build_queue_item[] =
        {0x53, 0x56, 0x8b, 0xf1, 0x33, 0xdb};
    const std::uint8_t turret_game_object_class_constructor[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t turret_craft_cleanup[] =
        {0x56, 0x8b, 0xf1, 0x8b, 0x8e, 0x34, 0x02, 0x00, 0x00};
    const std::uint8_t turret_craft_post_load[] =
        {0x53, 0x56, 0x57, 0x8b, 0xf1};
    const std::uint8_t turret_craft_simulate[] =
        {0x55, 0x8b, 0xec, 0x53, 0x8b, 0x5d, 0x08};
    const std::uint8_t always_show_starbase_simulate[] =
        {0x55, 0x8b, 0xec, 0x53, 0x8b, 0x5d, 0x08};
    const std::uint8_t always_show_publish[] =
        {0x55, 0x8b, 0xec, 0x56, 0x8b,
         0x75, 0x0c, 0x8b, 0x46, 0x40};
    const std::uint8_t always_show_render_list[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c,
         0xa1, 0x10, 0xb6, 0x76, 0x00};
    const std::uint8_t turret_weapon_set_target[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
    const std::uint8_t turret_craft_get_policy[] =
        {0x8b, 0x41, 0x44, 0x6a, 0x00};
    const std::uint8_t turret_craft_set_policy[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x41, 0x44};
    const std::uint8_t turret_get_current_command[] =
        {0x8d, 0x41, 0x4c, 0xc3};
    const std::uint8_t turret_set_targetless_command[] =
        {0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};
    const std::uint8_t turret_set_object_command[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x14};
    const std::uint8_t fire_arc_weapon_class_constructor[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff};
    const std::uint8_t fire_arc_can_fire_at[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c};
    const std::uint8_t fire_arc_world_transform[] =
        {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x49, 0x04};
    const std::uint8_t fire_arc_draw_line[] =
        {0x55, 0x8b, 0xec, 0xa1, 0x08, 0xd5, 0x7a, 0x00};
    const std::uint8_t fire_arc_mouse_over[] =
        {0x8a, 0x41, 0x18, 0x84, 0xc0, 0x74, 0x12};
    const std::uint8_t fire_arc_get_target[] =
        {0x8b, 0x49, 0x38, 0x51, 0xe8};
    const std::uint8_t normal_weapon_tech_get_owner[] =
        {0x8b, 0x49, 0x18, 0x51, 0xe8};
    const std::uint8_t edit_menu_update[] =
        {0x55, 0x8b, 0xec, 0x81, 0xec, 0xc4, 0x00, 0x00, 0x00};
    const std::uint8_t create_shield_hit[] =
        {0x55, 0x8b, 0xec, 0x6a, 0xff,
         0x68, 0x87, 0xb5, 0x69, 0x00};
    const std::uint8_t stop_shield_effect[] =
        {0x55, 0x8b, 0xec, 0x83, 0xec, 0x08, 0x8d, 0x45};
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
    set_signature(0x11c610, edit_menu_update);
    set_signature(0x0743b0, create_shield_hit);
    set_signature(0x074770, stop_shield_effect);
    set_signature(0x0b80f0, producer_get_action);
    set_signature(0x0afa30, construction_rig_get_action);
    set_signature(0x0afbc0, construction_rig_start);
    set_signature(0x0afbe8, construction_rig_start_hardpoint_call);
    set_signature(0x0aff00, construction_rig_cancel);
    set_signature(0x0aff90, construction_rig_finish);
    set_signature(0x0afea0, construction_rig_remove_object);
    set_signature(0x0afba0, construction_rig_matrix);
    set_signature(0x0adc40, build_position_constructor);
    set_signature(0x0adc70, build_position_destructor);
    set_signature(0x073aa0, placeholder_render_internal);
    set_signature(0x252710, game_operator_new);
    set_signature(0x2527d0, game_operator_delete);
    set_signature(0x0b9170, producer_matrix);
    set_signature(0x0b8140, producer_start_effect);
    set_signature(0x0b8470, producer_cancel_effect);
    set_signature(0x0b8dd0, producer_update_effect);
    set_signature(0x0b8f30, producer_stop_effect);
    set_signature(0x0b0210, producer_is_busy);
    set_signature(0x031194, cleanup_is_busy_query);
    set_signature(0x031495, admission_is_busy_query);
    set_signature(0x0311a5, queue_pop_query);
    set_signature(0x031525, queue_pop_query);
    set_signature(0x0314d4, build_position_interface_load);
    set_signature(0x0ba0e0, research_start);
    set_signature(0x0ba1b0, research_cancel);
    set_signature(0x0ba280, research_can_build);
    set_signature(0x0ba4a0, research_item_conflict);
    set_signature(0x0b9d70, research_button_update);
    set_signature(0x0babd0, research_matrix);
    set_signature(0x0b0e10, evolver_swap_objects);
    set_signature(0x0b1150, evolver_matrix);
    set_signature(0x0b04f0, evolver_start_effect);
    set_signature(0x0b0770, evolver_remove_effect);
    set_signature(0x0b08d0, evolver_remove_effect);
    set_signature(0x0b0970, evolver_remove_effect);
    set_signature(0x0b0a10, evolver_update_effect);
    set_signature(0x0b1170, evolver_render_internal);
    set_signature(0x0e69e0, control_button_press);
    set_signature(0x0e7950, mode_info_button_name);
    set_signature(0x0ee530, race_icon_render);
    set_signature(0x0f2c49, ship_display_single_object_display_call);
    set_signature(0x0f29e4, ship_display_single_object_simulate_call);
    set_signature(0x0639ba, bink_set_dibits_call);
    set_signature(0x064350, bink_texture_render);
    set_signature(0x0e5da5, bink_texture_render_call1);
    set_signature(0x0e6153, bink_texture_render_call2);
    set_signature(0x2400d0, rgb_file_exists);
    set_signature(0x240150, rgb_open_read);
    set_signature(0x242ee0, rgb_reader);
    set_signature(0x242fa5, rgb_read_exists_call);
    set_signature(0x2434b0, rgb_reader);
    set_signature(0x243553, rgb_load_exists_call);
    set_signature(0x242780, rgb_lock_surface);
    set_signature(0x0e7c80, rgb_blend_xrgb888);
    set_signature(0x0e96c0, rgb_update_scan_grid);
    set_signature(0x23b150, rgb_sprite_get_texture);
    set_signature(0x2437b0, rgb_unlock_surface);
    set_signature(0x000975b0, cheats_add_crew_capacity);
    set_signature(0x000ca010, cheats_disable_shield_generator);
    set_signature(0x000cfd50, cheats_entity_get_transform);
    set_signature(0x000d4490, cheats_queue_command_vector);
    set_signature(0x0007e8a0, cheats_eliminate_team);
    set_signature(0x0009dd40, a1_nebula_set_textures_recursive);
    set_signature(0x0013c2da, a1_rtime_load_name);
    set_signature(0x000bda00, a1_starbase_geometry);
    set_signature(0x000ab710, a1_starbase_class_build);
    set_signature(0x000bad90, a1_starbase_finish_build);
    set_signature(0x000bda30, a1_starbase_clear_team);
    set_signature(0x000bda70, a1_starbase_set_team);
    set_signature(0x000bdaa0, a1_starbase_load_save);
    set_signature(0x000bdae0, a1_starbase_load_save);
    set_signature(0x000ce910, a1_officer_upgrade_class_build);
    set_signature(0x000b79b0, a1_producer_pop_build_queue_item);
    set_signature(0x000cc480, turret_game_object_class_constructor);
    set_signature(0x000c1fd0, turret_craft_cleanup);
    set_signature(0x000c2870, turret_craft_post_load);
    set_signature(0x000c6530, turret_craft_simulate);
    set_signature(0x000c9a20, turret_craft_get_policy);
    set_signature(0x000c9a50, turret_craft_set_policy);
    set_signature(0x000c9ae0, turret_craft_get_policy);
    set_signature(0x000c9b10, turret_craft_set_policy);
    set_signature(0x000d19c0, turret_get_current_command);
    set_signature(0x000d1a40, turret_set_targetless_command);
    set_signature(0x000d1af0, turret_set_object_command);
    set_signature(0x000bdb10, always_show_starbase_simulate);
    set_signature(0x0002b910, always_show_publish);
    set_signature(0x00072b60, always_show_render_list);
    set_signature(0x00271290, turret_weapon_set_target);
    set_signature(0x00271340, turret_weapon_set_target);
    set_signature(0x00264e30, fire_arc_weapon_class_constructor);
    set_signature(0x0026f8c0, fire_arc_can_fire_at);
    set_signature(0x000cff90, fire_arc_world_transform);
    set_signature(0x0011b130, fire_arc_draw_line);
    set_signature(0x0010c140, fire_arc_mouse_over);
    set_signature(0x00271300, fire_arc_get_target);
    set_signature(0x00271050, normal_weapon_tech_get_owner);
    constexpr char rgb_literal[] = "Textures\\RGB\\";
    std::memcpy(static_cast<std::uint8_t*>(fake_armada) + 0x32d178,
                rgb_literal, sizeof(rgb_literal));

    HMODULE msvcrt = GetModuleHandleA("msvcrt.dll");
    FARPROC fopen_export = msvcrt ? GetProcAddress(msvcrt, "fopen") : nullptr;
    static_assert(sizeof(fopen_export) == sizeof(original_armada_fopen),
                  "32-bit FARPROC and object pointers must match");
    std::memcpy(&original_armada_fopen, &fopen_export,
                sizeof(original_armada_fopen));
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(fake_armada) + 0x3b7f8c) =
        original_armada_fopen;
}

void A2FO_CALL log_line(const char* module, const char* message) {
    if (module && message && std::strcmp(module, "A2FOCheats") == 0 &&
        std::strcmp(
            message,
            "showmethemoney grants: Dilithium=1111, Tritanium=2222, "
            "Metal=7777, Supplies=4444, Crew=8888 (RTS_CFG.h)") == 0) {
        cheats_rts_config_loaded = true;
    }
    if (module && message && std::strcmp(module, "A2FOTurrets") == 0 &&
        std::strcmp(
            message,
            "Shield visibility callbacks linked through "
            "A2FOAlwaysShowShields") == 0) {
        turret_shield_visibility_linked = true;
    }
    if (module && message && std::strcmp(module, "A2FOHybridBuild") == 0 &&
        std::strcmp(
            message,
            "Shield visibility observer linked through the Fleet Ops "
            "Craft render boundary") == 0) {
        hybrid_shield_render_observer_linked = true;
    }
    if (module && message && std::strcmp(module, "A2FOTurrets") == 0 &&
        std::strcmp(
            message,
            "Full 3D weapon-trigger filtering linked through A2FOFireArcs") == 0) {
        turret_fire_arc_trigger_filter_linked = true;
    }
    if (module && message && std::strcmp(module, "A2FOTurrets") == 0 &&
        std::strcmp(
            message,
            "Normal-weapon trigger filtering linked through A2FONormalWeaponTech") == 0) {
        turret_normal_weapon_tech_trigger_filter_linked = true;
    }
    if (module && message &&
        std::strcmp(module, "A2FONormalWeaponTech") == 0 &&
        std::strcmp(
            message,
            "Normal-weapon technology-tree filter initialized; unlisted weapons default to 0 (available)") == 0) {
        normal_weapon_tech_initialized = true;
    }
    if (message && (std::strstr(message, "signature") ||
                    std::strstr(message, "disabled") ||
                    (module && std::strcmp(
                         module, "A2FOFireArcs") == 0))) {
        std::fprintf(stderr, "[%s] %s\n", module ? module : "module",
                     message);
    }
}
void* A2FO_CALL armada_module() { return fake_armada; }
void* A2FO_CALL fleetops_module() { return fleet_ops; }
const char* A2FO_CALL root_directory() { return "."; }
std::uint32_t A2FO_CALL extension_root_count() { return 2; }
const char* A2FO_CALL extension_root(std::uint32_t index) {
    if (index == 0) return parent_extension_root_path;
    return index == 1 ? extension_root_path : nullptr;
}
bool A2FO_CALL register_lookup(
    const char*, A2FO_FofsItemLookupHandler handler, void*) {
    return handler != nullptr;
}
bool A2FO_CALL register_alias(const char*, const char* source,
                              const char* target) {
    if (!source || !target) return false;
    if (std::strcmp(source, "wingman") == 0 &&
        std::strcmp(target, "craft") == 0) {
        wingman_alias_registered = true;
        return true;
    }
    if (std::strcmp(source, "hybridbuild") == 0 &&
        std::strcmp(target, "research") == 0) {
        hybridbuild_alias_registered = true;
        return true;
    }
    if (std::strcmp(source, "turret") == 0 &&
        std::strcmp(target, "sensor") == 0) {
        turret_alias_registered = true;
        return true;
    }
    return false;
}
bool A2FO_CALL register_cocoon(const char*, const char*) { return true; }
bool A2FO_CALL register_classlabel_odf_defaults(
    const char*, const char* classlabel,
    const A2FO_ClasslabelOdfDefault* defaults,
    std::uint32_t default_count) {
    constexpr A2FO_ClasslabelOdfDefault wingman_expected[] = {
        {"enginesHitPercent", "5.0f"},
        {"lifeSupportHitPercent", "8.5f"},
        {"weaponsHitPercent", "5.0f"},
        {"shieldGeneratorHitPercent", "8.0f"},
        {"sensorsHitPercent", "8.0f"},
        {"crewHitPercent", "8.5f"},
        {"hullHitPercent", "57.0f"},
        {"ship", "1"},
        {"has_hitpoints", "1"},
        {"has_crew", "1"},
        {"transporter", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"can_explore", "1"},
    };
    constexpr A2FO_ClasslabelOdfDefault constructionrig_expected[] = {
        {"shipclass", "construction"},
        {"builder_facility", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"SHOW_SW_AUTONOMY", "1"},
        {"shipType", "N"},
        {"hotkeyLabel", "HOTKEY_F1"},
    };
    constexpr A2FO_ClasslabelOdfDefault freighter_expected[] = {
        {"shipclass", "mining"},
        {"maxDilithium", "150"},
        {"alert", "1"},
        {"miner", "1"},
        {"SHOW_MOVEMENT_AUTONOMY", "1"},
        {"resourcesCanHandle", "dilithium"},
        {"hotkeyLabel", "HOTKEY_F2"},
    };
    constexpr A2FO_ClasslabelOdfDefault turret_expected[] = {
        {"ignoreInterface", "1"},
        {"avoidMe", "0"},
        {"avoidanceClass", "0"},
        {"mapIcon", "mapicon_empty"},
        {"footprintBuffer", "0.0f"},
        {"createFootprint", "0"},
        {"destroyFootprint", "0"},
    };
    if (!classlabel || !defaults) {
        return false;
    }
    const A2FO_ClasslabelOdfDefault* expected = nullptr;
    std::uint32_t expected_count = 0;
    bool* registered = nullptr;
    if (std::strcmp(classlabel, "wingman") == 0) {
        expected = wingman_expected;
        expected_count = static_cast<std::uint32_t>(
            sizeof(wingman_expected) / sizeof(wingman_expected[0]));
        registered = &wingman_odf_defaults_registered;
    } else if (std::strcmp(classlabel, "constructionrig") == 0) {
        expected = constructionrig_expected;
        expected_count = static_cast<std::uint32_t>(
            sizeof(constructionrig_expected) /
            sizeof(constructionrig_expected[0]));
        registered = &constructionrig_odf_defaults_registered;
    } else if (std::strcmp(classlabel, "freighter") == 0) {
        expected = freighter_expected;
        expected_count = static_cast<std::uint32_t>(
            sizeof(freighter_expected) / sizeof(freighter_expected[0]));
        registered = &freighter_odf_defaults_registered;
    } else if (std::strcmp(classlabel, "turret") == 0) {
        expected = turret_expected;
        expected_count = static_cast<std::uint32_t>(
            sizeof(turret_expected) / sizeof(turret_expected[0]));
        registered = &turret_odf_defaults_registered;
    } else {
        return false;
    }
    if (default_count != expected_count) return false;
    for (std::uint32_t index = 0; index < default_count; ++index) {
        if (!defaults[index].command || !defaults[index].value ||
            std::strcmp(defaults[index].command,
                        expected[index].command) != 0 ||
            std::strcmp(defaults[index].value, expected[index].value) != 0) {
            return false;
        }
    }
    *registered = true;
    return true;
}
bool A2FO_CALL register_odf_overlay(
    const char*, const char* directory, std::uint32_t precedence) {
    if (!directory || std::strcmp(directory, "Addon") != 0 ||
        precedence != A2FO_ODF_OVERLAY_OVERRIDE) {
        return false;
    }
    addon_odf_overlay_registered = true;
    return true;
}
std::uint32_t A2FO_CALL odf_overlay_count() {
    return addon_odf_overlay_registered ? 1u : 0u;
}
bool A2FO_CALL get_odf_overlay(
    std::uint32_t index, char* directory, std::uint32_t directory_size,
    std::uint32_t* precedence) {
    constexpr char value[] = "Addon";
    if (!addon_odf_overlay_registered || index != 0 || !directory ||
        directory_size < sizeof(value) || !precedence) {
        return false;
    }
    std::memcpy(directory, value, sizeof(value));
    *precedence = A2FO_ODF_OVERLAY_OVERRIDE;
    return true;
}
bool A2FO_CALL register_info_ini_defaults(
    const char*, A2FO_InfoIniDefaultsHandler handler, void*) {
    info_ini_handler_registered = handler != nullptr;
    return info_ini_handler_registered;
}
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
            0x0b80f0) {
        hybrid_producer_get_action_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0314d4) {
        hybrid_build_position_interface_load_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b8140) {
        hybrid_producer_start_effect_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b8470) {
        hybrid_producer_cancel_effect_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b8dd0) {
        hybrid_producer_update_effect_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0b8f30) {
        hybrid_producer_stop_effect_hooked = true;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1dc1bc) {
        hybrid_craft_render_internal_hooked = true;
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
            0x0e7950) {
        hybrid_mode_info_button_name_hooked = true;
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
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1fc320) {
        cheats_show_me_the_money_hooked = true;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1fccc8) {
        cheats_chat_init_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x11c610) {
        edit_menu_update_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x242780) {
        rgb_lock_surface_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0e7c80) {
        rgb_blend_guard_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0e96c0) {
        rgb_scan_grid_guard_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x2400d0) {
        rgb_file_exists_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x240150) {
        rgb_open_read_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x000bdb10) {
        always_show_shields_starbase_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x0002b910) {
        always_show_shields_publish_hooked = true;
    }
    if (fake_armada && target == static_cast<std::uint8_t*>(fake_armada) +
            0x00072b60) {
        always_show_shields_render_list_hooked = true;
    }
    if (fake_armada &&
        (target == static_cast<std::uint8_t*>(fake_armada) + 0x000cc480 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000c1fd0 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000c2870 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000c6530 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x00271290 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x00271340)) {
        ++turret_hook_count;
    }
    if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x0026f8c0) {
        ++fire_arc_hook_count;
    }
    if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x001ed458) {
        fire_arc_icon_hover_hooked = true;
        ++fire_arc_hook_count;
    }
    if (fake_armada &&
        (target == static_cast<std::uint8_t*>(fake_armada) + 0x000ab710 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000bad90 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000bda30 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000bda70 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000bdaa0 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000bdae0 ||
         target == static_cast<std::uint8_t*>(fake_armada) + 0x000ce910)) {
        ++a1_officer_system_hook_count;
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
    } else if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x0afbe8) {
        hybrid_construction_hardpoint_call_patched = true;
    } else if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e6e3f) {
        hybrid_evolve_button_bind_patched = true;
    } else if (fleet_ops && target ==
            static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
                0x1e6f41) {
        hybrid_ai_button_bind_patched = true;
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
bool A2FO_CALL patch_jump(void* target, void* replacement,
                          const std::uint8_t* expected,
                          std::size_t length) {
    if (!target || !replacement || !expected || length < 5 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x000c6530) {
        turret_craft_simulate_chained = true;
        ++turret_hook_count;
    }
    if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x000cc480) {
        turret_class_constructor_chained = true;
        ++turret_hook_count;
    }
    if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x00264e30) {
        fire_arc_class_constructor_chained = true;
        ++fire_arc_hook_count;
    }
    if (fake_armada && target ==
            static_cast<std::uint8_t*>(fake_armada) + 0x0026f8c0) {
        fire_arc_target_check_chained = true;
        ++fire_arc_hook_count;
    }
    return true;
}
}

int main() {
    const unsigned long fixture_nonce =
        static_cast<unsigned long>(GetTickCount());
    std::snprintf(extension_root_path, sizeof(extension_root_path),
                  "rgb-module-fixture-%lu-%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()),
                  fixture_nonce);
    std::snprintf(parent_extension_root_path,
                  sizeof(parent_extension_root_path),
                  "rgb-parent-fixture-%lu-%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()),
                  fixture_nonce);
    if (!CreateDirectoryA(parent_extension_root_path, nullptr) ||
        !CreateDirectoryA(extension_root_path, nullptr)) return 10;
    const std::string a1_marker_path =
        std::string(extension_root_path) + "\\a1compat.ini";
    const std::string parent_rts_config_path =
        std::string(parent_extension_root_path) + "\\RTS_CFG.h";
    const std::string active_rts_config_path =
        std::string(extension_root_path) + "\\RTS_CFG.h";
    constexpr char parent_rts_config[] =
        "int SHOWMETHEMONEY_DILITHIUM = 1111;\r\n"
        "float SHOWMETHEMONEY_TRITANIUM = 2222.0f;\r\n"
        "int SHOWMETHEMONEY_METAL = 3333;\r\n"
        "int SHOWMETHEMONEY_SUPPLIES = 4444;\r\n"
        "int SHOWMETHEMONEY_CREW = 5555;\r\n";
    constexpr char active_rts_config[] =
        "// Per-field active-mod overrides.\r\n"
        "int SHOWMETHEMONEY_METAL = 7777;\r\n"
        "int SHOWMETHEMONEY_SUPPLIES = -5; // rejected\r\n"
        "int SHOWMETHEMONEY_CREW = 8888;\r\n";
    if (!write_fixture_file(parent_rts_config_path, parent_rts_config,
                            sizeof(parent_rts_config)) ||
        !write_fixture_file(active_rts_config_path, active_rts_config,
                            sizeof(active_rts_config))) {
        return 105;
    }

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
    *reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
        0x24a304) = cheat_registry_fixture.registrations;
    void** object_button_press_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
        0x240c00);
    void* object_button_press_original =
        static_cast<std::uint8_t*>(static_cast<void*>(fleet_ops)) +
        0x10ad40;
    *object_button_press_slot = object_button_press_original;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &log_line;
    api.armada_module = &armada_module;
    api.fleetops_module = &fleetops_module;
    api.root_directory = &root_directory;
    api.install_inline_hook = &install_hook;
    api.patch_jump = &patch_jump;
    api.patch_call = &patch_call;
    api.register_fofs_item_lookup_handler = &register_lookup;
    api.extension_root_count = &extension_root_count;
    api.extension_root = &extension_root;
    api.register_classlabel_alias = &register_alias;
    api.register_evolver_cocoon_command = &register_cocoon;
    api.api_revision = A2FO_MODULE_API_REVISION;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH |
        A2FO_CAP_UPGRADE_POD_POLICY |
        A2FO_CAP_ORIGINAL_CLASSLABEL |
        A2FO_CAP_COCOON_CLASS_ASSOCIATION |
        A2FO_CAP_INFO_INI_DEFAULTS |
        A2FO_CAP_ODF_OVERLAY_DIRECTORIES |
        A2FO_CAP_PRODUCER_EVENTS |
        A2FO_CAP_CLASSLABEL_ODF_DEFAULTS;
    api.upgrade_pod_maximum_tier = &upgrade_pod_maximum_tier;
    api.get_original_classlabel = &get_original_classlabel;
    api.associate_evolver_cocoon_class =
        &associate_evolver_cocoon_class;
    api.register_info_ini_defaults_handler =
        &register_info_ini_defaults;
    api.register_odf_overlay_directory = &register_odf_overlay;
    api.odf_overlay_directory_count = &odf_overlay_count;
    api.get_odf_overlay_directory = &get_odf_overlay;
    api.register_producer_event_handler =
        &register_producer_event_handler;
    api.dispatch_producer_event = &dispatch_producer_event;
    api.register_classlabel_odf_defaults =
        &register_classlabel_odf_defaults;

    const auto initialize_module = [&api](const char* path) -> HMODULE {
        HMODULE module = LoadLibraryA(path);
        if (!module) return nullptr;
        A2FO_ModuleInitFn init = nullptr;
        FARPROC address = GetProcAddress(module, "A2FO_ModuleInit");
        std::memcpy(&init, &address, sizeof(init));
        if (!init || !init(&api)) {
            FreeLibrary(module);
            return nullptr;
        }
        return module;
    };
    const auto shutdown_module = [](HMODULE module) {
        if (!module) return;
        A2FO_ModuleShutdownFn shutdown = nullptr;
        FARPROC address = GetProcAddress(module, "A2FO_ModuleShutdown");
        std::memcpy(&shutdown, &address, sizeof(shutdown));
        if (shutdown) shutdown();
    };

    void** armada_fopen_slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(fake_armada) + 0x3b7f8c);
    if (!original_armada_fopen ||
        *armada_fopen_slot != original_armada_fopen) {
        return 14;
    }

    // With no legacy folder the optional module must load without changing
    // Armada's native fopen import.
    HMODULE inactive_rgb = initialize_module(
        "modules\\A2FORGBTextures.dll");
    if (!inactive_rgb || *armada_fopen_slot != original_armada_fopen) return 11;
    shutdown_module(inactive_rgb);
    FreeLibrary(inactive_rgb);

    HMODULE feature = initialize_module(
        "modules\\A2FOFeaturePack.dll");
    if (!feature) return 3;
    HMODULE cheats = initialize_module(
        "modules\\A2FOCheats.dll");
    if (!cheats || !cheats_show_me_the_money_hooked ||
        !cheats_chat_init_hooked || !cheats_rts_config_loaded) return 102;
    for (const FixtureCheatRegistration& registration :
         cheat_registry_fixture.registrations) {
        if (registration.multiplayer_allowed != 0 ||
            !registration.handler) return 103;
    }
    if (cheat_registry_fixture.registrations[0].handler ==
            cheat_registry_fixture.registrations[1].handler ||
        cheat_registry_fixture.registrations[1].handler ==
            cheat_registry_fixture.registrations[2].handler ||
        cheat_registry_fixture.registrations[2].handler ==
            cheat_registry_fixture.registrations[3].handler) {
        return 104;
    }
    HMODULE edit_menu = initialize_module(
        "modules\\A2FOEditMenu.dll");
    if (!edit_menu || !edit_menu_update_hooked) return 109;
    HMODULE always_show_shields = initialize_module(
        "modules\\A2FOAlwaysShowShields.dll");
    if (!always_show_shields || !always_show_shields_starbase_hooked ||
        !always_show_shields_publish_hooked ||
        !always_show_shields_render_list_hooked ||
        !GetProcAddress(
            always_show_shields,
            "A2FOAlwaysShowShields_RegisterClass") ||
        !GetProcAddress(
            always_show_shields,
            "A2FOAlwaysShowShields_UpdateCraft") ||
        !GetProcAddress(
            always_show_shields,
            "A2FOAlwaysShowShields_CleanupCraft")) {
        return 110;
    }
    // Fleet Operations installs its own GameObjectClass-constructor and
    // Craft::Simulate detours before A2FO's native modules load. Reproduce
    // that live absolute push/ret state so the turret smoke proves both
    // explicit chains rather than only the stock-prologue fallback.
    const auto install_fo_absolute_detour = [](std::uint8_t* site,
                                                const void* handler) {
        site[0] = 0x68;
        const std::uint32_t encoded_handler = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(handler));
        std::memcpy(site + 1, &encoded_handler, sizeof(encoded_handler));
        site[5] = 0xc3;
    };
    install_fo_absolute_detour(
        static_cast<std::uint8_t*>(fake_armada) + 0x000cc480,
        static_cast<const std::uint8_t*>(static_cast<void*>(fleet_ops)) +
            0x0010bd80);
    install_fo_absolute_detour(
        static_cast<std::uint8_t*>(fake_armada) + 0x000c6530,
        static_cast<const std::uint8_t*>(static_cast<void*>(fleet_ops)) +
            0x001dcebc);
    install_fo_absolute_detour(
        static_cast<std::uint8_t*>(fake_armada) + 0x00264e30,
        static_cast<const std::uint8_t*>(static_cast<void*>(fleet_ops)) +
            0x0010ef74);
    install_fo_absolute_detour(
        static_cast<std::uint8_t*>(fake_armada) + 0x0026f8c0,
        static_cast<const std::uint8_t*>(static_cast<void*>(fleet_ops)) +
            0x001358ac);

    HMODULE fire_arcs = initialize_module(
        "modules\\A2FOFireArcs.dll");
    if (!fire_arcs || fire_arc_hook_count != 3 ||
        !fire_arc_class_constructor_chained ||
        !fire_arc_target_check_chained ||
        !fire_arc_icon_hover_hooked ||
        !GetProcAddress(
            fire_arcs, "A2FOFireArcs_AllowWeaponTrigger")) {
        std::fprintf(
            stderr,
            "A2FOFireArcs smoke state: module=%p hooks=%u constructorChained=%d targetCheckChained=%d iconHover=%d\n",
            static_cast<void*>(fire_arcs), fire_arc_hook_count,
            fire_arc_class_constructor_chained ? 1 : 0,
            fire_arc_target_check_chained ? 1 : 0,
            fire_arc_icon_hover_hooked ? 1 : 0);
        return 107;
    }

    HMODULE normal_weapon_tech = initialize_module(
        "modules\\A2FONormalWeaponTech.dll");
    if (!normal_weapon_tech || !normal_weapon_tech_initialized ||
        !GetProcAddress(
            normal_weapon_tech,
            "A2FONormalWeaponTech_AllowWeaponTrigger")) {
        std::fprintf(
            stderr,
            "A2FONormalWeaponTech smoke state: module=%p initialized=%d\n",
            static_cast<void*>(normal_weapon_tech),
            normal_weapon_tech_initialized ? 1 : 0);
        return 108;
    }

    HMODULE turrets = initialize_module(
        "modules\\A2FOTurrets.dll");
    if (!turrets || !turret_alias_registered ||
        !turret_odf_defaults_registered || turret_hook_count != 6 ||
        !turret_craft_simulate_chained ||
        !turret_class_constructor_chained ||
        !turret_shield_visibility_linked ||
        !turret_fire_arc_trigger_filter_linked ||
        !turret_normal_weapon_tech_trigger_filter_linked) {
        std::fprintf(
            stderr,
            "A2FOTurrets smoke state: module=%p alias=%d defaults=%d "
            "hooks=%u constructorChained=%d simulateChained=%d\n",
            static_cast<void*>(turrets),
            turret_alias_registered ? 1 : 0,
            turret_odf_defaults_registered ? 1 : 0,
            turret_hook_count,
            turret_class_constructor_chained ? 1 : 0,
            turret_craft_simulate_chained ? 1 : 0);
        return 106;
    }
    HMODULE inactive_a1 = initialize_module(
        "sta1-classic\\modules\\A1Compat.dll");
    if (inactive_a1 || wingman_alias_registered ||
        wingman_odf_defaults_registered ||
        constructionrig_odf_defaults_registered ||
        freighter_odf_defaults_registered ||
        addon_odf_overlay_registered) return 25;
    constexpr char a1_marker[] = "[A1Compat]\r\nEnabled=1\r\n";
    if (!write_fixture_file(a1_marker_path, a1_marker,
                            sizeof(a1_marker))) return 26;
    HMODULE a1_compat = initialize_module(
        "sta1-classic\\modules\\A1Compat.dll");
    if (!a1_compat || !wingman_alias_registered ||
        !wingman_odf_defaults_registered ||
        !constructionrig_odf_defaults_registered ||
        !freighter_odf_defaults_registered ||
        !addon_odf_overlay_registered ||
        !producer_event_handler_registered ||
        a1_officer_system_hook_count != 6) {
        std::fprintf(
            stderr,
            "A1Compat smoke state: module=%p alias=%d wingmanDefaults=%d "
            "constructionrigDefaults=%d freighterDefaults=%d overlay=%d "
            "producer=%d "
            "officerHooks=%u\n",
            static_cast<void*>(a1_compat),
            wingman_alias_registered ? 1 : 0,
            wingman_odf_defaults_registered ? 1 : 0,
            constructionrig_odf_defaults_registered ? 1 : 0,
            freighter_odf_defaults_registered ? 1 : 0,
            addon_odf_overlay_registered ? 1 : 0,
            producer_event_handler_registered ? 1 : 0,
            a1_officer_system_hook_count);
        return 27;
    }
    HMODULE hybrid = initialize_module(
        "modules\\A2FOHybridBuild.dll");
    if (!hybrid) return 4;
    HMODULE info = initialize_module(
        "modules\\A2FOInfoIni.dll");
    if (!info) return 5;
    const std::string textures_path =
        std::string(extension_root_path) + "\\textures";
    const std::string parent_textures_path =
        std::string(parent_extension_root_path) + "\\textures";
    const std::string parent_rgb_path = parent_textures_path + "\\rgb";
    const std::string rgb_path = textures_path + "\\rgb";
    const std::string index8_path = textures_path + "\\index8";
    const std::string compressed_path = textures_path + "\\compressed";
    const std::string rgb_file_path = rgb_path + "\\Probe.TGA";
    const std::string index8_file_path = index8_path + "\\IndexOnly.TGA";
    const std::string compressed_file_path =
        compressed_path + "\\CompressedOnly.TGA";
    const std::string root_winner_path = textures_path + "\\RootWins.TGA";
    const std::string rgb_loser_path = rgb_path + "\\RootWins.TGA";
    const std::string parent_index8_loser_path =
        parent_rgb_path + "\\IndexOnly.TGA";
    const std::string parent_compressed_loser_path =
        parent_rgb_path + "\\CompressedOnly.TGA";
    if (!CreateDirectoryA(parent_textures_path.c_str(), nullptr) ||
        !CreateDirectoryA(parent_rgb_path.c_str(), nullptr) ||
        !CreateDirectoryA(textures_path.c_str(), nullptr) ||
        !CreateDirectoryA(rgb_path.c_str(), nullptr) ||
        !CreateDirectoryA(index8_path.c_str(), nullptr) ||
        !CreateDirectoryA(compressed_path.c_str(), nullptr)) {
        return 12;
    }
    constexpr char rgb_contents[] = "rgb-probe";
    // Two 2x1 TGA fixtures exercise the non-RGB preparation path: a
    // colour-mapped 8-bit image and an RLE-compressed 24-bit image.
    constexpr std::uint8_t index8_contents[] = {
        0x00, 0x01, 0x01, 0x00, 0x00, 0x02, 0x00, 0x18,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00,
        0x08, 0x00,
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c,
        0x00, 0x01,
    };
    constexpr std::uint8_t compressed_contents[] = {
        0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00,
        0x18, 0x00,
        0x01, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    };
    constexpr char root_contents[] = "root-winner";
    constexpr char rgb_loser_contents[] = "rgb-loser--";
    constexpr char parent_loser_contents[] = "parent-rgb-loser";
    if (!write_fixture_file(rgb_file_path, rgb_contents,
                            sizeof(rgb_contents))) return 15;
    if (!write_fixture_file(index8_file_path, index8_contents,
                            sizeof(index8_contents))) return 90;
    if (!write_fixture_file(compressed_file_path, compressed_contents,
                            sizeof(compressed_contents))) return 91;
    if (!write_fixture_file(root_winner_path, root_contents,
                            sizeof(root_contents))) return 16;
    if (!write_fixture_file(rgb_loser_path, rgb_loser_contents,
                            sizeof(rgb_loser_contents))) return 20;
    if (!write_fixture_file(parent_index8_loser_path, parent_loser_contents,
                            sizeof(parent_loser_contents))) return 100;
    if (!write_fixture_file(parent_compressed_loser_path,
                            parent_loser_contents,
                            sizeof(parent_loser_contents))) return 101;

    // Reproduce Fleet Operations' startup rewrite of Armada's original
    // Textures\RGB\ prefix. The callback route now deliberately requires the
    // retained native TGA loader entries because they produce the stock pixel
    // representation expected by Armada 1/2 consumers.
    constexpr char fleetops_texture_literal[] = "Textures\\";
    std::memcpy(static_cast<std::uint8_t*>(fake_armada) + 0x32d178,
                fleetops_texture_literal,
                sizeof(fleetops_texture_literal));

    HMODULE rgb = initialize_module(
        "modules\\A2FORGBTextures.dll");
    if (!rgb || *armada_fopen_slot == original_armada_fopen) return 13;
    if (!rgb_lock_surface_hooked || !rgb_blend_guard_hooked ||
        !rgb_scan_grid_guard_hooked ||
        !rgb_file_exists_hooked || !rgb_open_read_hooked) {
        return 23;
    }
    FopenFn redirected_fopen = nullptr;
    std::memcpy(&redirected_fopen, armada_fopen_slot,
                sizeof(redirected_fopen));
    FILE* redirected_file = redirected_fopen(
        "Textures\\probe.tga", "rb");
    if (!redirected_file) return 17;
    char redirected_contents[sizeof(rgb_contents)]{};
    const std::size_t redirected_read = std::fread(
        redirected_contents, 1, sizeof(redirected_contents), redirected_file);
    std::fclose(redirected_file);
    if (redirected_read != sizeof(redirected_contents) ||
        std::memcmp(redirected_contents, rgb_contents,
                    sizeof(rgb_contents)) != 0) {
        return 18;
    }

    // Folder-qualified requests keep their native namespace. These checks use
    // simple byte fixtures because the smoke test validates routing rather than
    // the proprietary texture decoder.
    FILE* redirected_index8 = redirected_fopen(
        "Textures\\Index8\\indexonly.tga", "rb");
    if (!redirected_index8) return 92;
    char index8_readback[sizeof(index8_contents)]{};
    const std::size_t index8_read = std::fread(
        index8_readback, 1, sizeof(index8_readback), redirected_index8);
    std::fclose(redirected_index8);
    if (index8_read != sizeof(index8_readback) ||
        std::memcmp(index8_readback, index8_contents,
                    sizeof(index8_contents)) != 0) {
        return 93;
    }

    FILE* redirected_compressed = redirected_fopen(
        "Textures\\Compressed\\compressedonly.tga", "rb");
    if (!redirected_compressed) return 94;
    char compressed_readback[sizeof(compressed_contents)]{};
    const std::size_t compressed_read = std::fread(
        compressed_readback, 1, sizeof(compressed_readback),
        redirected_compressed);
    std::fclose(redirected_compressed);
    if (compressed_read != sizeof(compressed_readback) ||
        std::memcmp(compressed_readback, compressed_contents,
                    sizeof(compressed_contents)) != 0) {
        return 95;
    }

    // Fleet Operations flattens Armada's sole generated legacy pathname. The
    // bridge must expand Index8 and RLE files before returning them through
    // that retained true-colour loader path.
    FILE* prepared_index8_file = redirected_fopen(
        "Textures\\indexonly.tga", "rb");
    if (!prepared_index8_file) return 96;
    std::uint8_t prepared_index8[24]{};
    const std::size_t prepared_index8_read = std::fread(
        prepared_index8, 1, sizeof(prepared_index8), prepared_index8_file);
    std::fclose(prepared_index8_file);
    constexpr std::uint8_t expected_index8_pixels[] = {
        0x0a, 0x14, 0x1e, 0x28, 0x32, 0x3c,
    };
    if (prepared_index8_read != sizeof(prepared_index8) ||
        prepared_index8[1] != 0 || prepared_index8[2] != 2 ||
        prepared_index8[12] != 2 || prepared_index8[14] != 1 ||
        prepared_index8[16] != 24 ||
        std::memcmp(prepared_index8 + 18, expected_index8_pixels,
                    sizeof(expected_index8_pixels)) != 0) {
        return 97;
    }

    FILE* prepared_compressed_file = redirected_fopen(
        "Textures\\compressedonly.tga", "rb");
    if (!prepared_compressed_file) return 98;
    std::uint8_t prepared_compressed[24]{};
    const std::size_t prepared_compressed_read = std::fread(
        prepared_compressed, 1, sizeof(prepared_compressed),
        prepared_compressed_file);
    std::fclose(prepared_compressed_file);
    constexpr std::uint8_t expected_compressed_pixels[] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    };
    if (prepared_compressed_read != sizeof(prepared_compressed) ||
        prepared_compressed[1] != 0 || prepared_compressed[2] != 2 ||
        prepared_compressed[12] != 2 || prepared_compressed[14] != 1 ||
        prepared_compressed[16] != 24 ||
        std::memcmp(prepared_compressed + 18, expected_compressed_pixels,
                    sizeof(expected_compressed_pixels)) != 0) {
        return 99;
    }

    // Never feed TGA bytes to Fleet Ops' DDS enhancement path. Its failed DDS
    // lookup must fall through to Armada's normal root-level TGA request,
    // which the FileExists/OpenRead hooks redirect instead.
    FILE* redirected_dds = redirected_fopen(
        "Textures\\probe.dds", "rb");
    if (redirected_dds) {
        std::fclose(redirected_dds);
        return 24;
    }
    FILE* root_winner = redirected_fopen(
        "Textures\\rootwins.tga", "rb");
    if (!root_winner) return 21;
    char root_readback[sizeof(root_contents)]{};
    const std::size_t root_read = std::fread(
        root_readback, 1, sizeof(root_readback), root_winner);
    std::fclose(root_winner);
    if (root_read != sizeof(root_readback) ||
        std::memcmp(root_readback, root_contents, sizeof(root_contents)) != 0) {
        return 22;
    }
    shutdown_module(rgb);
    if (*armada_fopen_slot != original_armada_fopen) return 19;
    FreeLibrary(rgb);
    rgb = nullptr;
    if (!wingman_alias_registered || !addon_odf_overlay_registered ||
        !hybridbuild_alias_registered || !turret_alias_registered) return 8;
    if (!info_ini_handler_registered) return 9;
    if (bink_call_patch_count != 4) return 6;
    if (!hybrid_research_start_hooked ||
        !hybrid_research_cancel_hooked ||
        !hybrid_research_can_build_hooked ||
        !hybrid_research_item_conflict_hooked ||
        hybrid_research_button_update_hooked ||
        !hybrid_research_matrix_hooked ||
        !hybrid_research_finish_hooked ||
        !hybrid_producer_get_action_hooked ||
        !hybrid_build_position_interface_load_hooked ||
        !hybrid_producer_start_effect_hooked ||
        !hybrid_producer_cancel_effect_hooked ||
        !hybrid_producer_update_effect_hooked ||
        !hybrid_producer_stop_effect_hooked ||
        !hybrid_craft_render_internal_hooked ||
        !hybrid_shield_render_observer_linked ||
        !hybrid_control_button_press_hooked ||
        !hybrid_mode_info_button_name_hooked ||
        !hybrid_race_icon_render_hooked ||
        !hybrid_ship_display_single_object_display_patched ||
        !hybrid_ship_display_single_object_simulate_patched ||
        !hybrid_producer_is_busy_hooked ||
        !hybrid_producer_pop_checked_hooked ||
        !hybrid_popup_update_hooked ||
        !hybrid_construction_hardpoint_call_patched ||
        !hybrid_build_button_bind_patched ||
        !hybrid_evolve_button_bind_patched ||
        !hybrid_ai_button_bind_patched ||
        !hybrid_hover_wireframe_patched ||
        *object_button_press_slot == object_button_press_original) {
        std::fprintf(
            stderr,
            "hybrid hooks: start=%d cancel=%d can=%d conflict=%d "
            "button_update=%d matrix=%d finish=%d get_action=%d place=%d "
            "effects=%d/%d/%d/%d craft_render=%d control_press=%d "
            "button_name=%d race=%d "
            "display=%d simulate=%d "
            "busy=%d pop=%d popup=%d hardpoint=%d build_bind=%d "
            "evolve_bind=%d ai_bind=%d hover=%d "
            "press=%d\n",
            hybrid_research_start_hooked, hybrid_research_cancel_hooked,
            hybrid_research_can_build_hooked,
            hybrid_research_item_conflict_hooked,
            hybrid_research_button_update_hooked,
            hybrid_research_matrix_hooked, hybrid_research_finish_hooked,
            hybrid_producer_get_action_hooked,
            hybrid_build_position_interface_load_hooked,
            hybrid_producer_start_effect_hooked,
            hybrid_producer_cancel_effect_hooked,
            hybrid_producer_update_effect_hooked,
            hybrid_producer_stop_effect_hooked,
            hybrid_craft_render_internal_hooked,
            hybrid_control_button_press_hooked,
            hybrid_mode_info_button_name_hooked,
            hybrid_race_icon_render_hooked,
            hybrid_ship_display_single_object_display_patched,
            hybrid_ship_display_single_object_simulate_patched,
            hybrid_producer_is_busy_hooked,
            hybrid_producer_pop_checked_hooked,
            hybrid_popup_update_hooked,
            hybrid_construction_hardpoint_call_patched,
            hybrid_build_button_bind_patched,
            hybrid_evolve_button_bind_patched,
            hybrid_ai_button_bind_patched,
            hybrid_hover_wireframe_patched,
            *object_button_press_slot != object_button_press_original);
        return 7;
    }

    FreeLibrary(info);
    FreeLibrary(hybrid);
    FreeLibrary(turrets);
    FreeLibrary(always_show_shields);
    FreeLibrary(normal_weapon_tech);
    FreeLibrary(fire_arcs);
    FreeLibrary(edit_menu);
    FreeLibrary(cheats);
    shutdown_module(a1_compat);
    FreeLibrary(a1_compat);
    FreeLibrary(feature);
    FreeLibrary(fleet_ops);
    VirtualFree(fake_armada, 0, MEM_RELEASE);
    DeleteFileA(rgb_file_path.c_str());
    DeleteFileA(index8_file_path.c_str());
    DeleteFileA(compressed_file_path.c_str());
    DeleteFileA(root_winner_path.c_str());
    DeleteFileA(rgb_loser_path.c_str());
    DeleteFileA(parent_index8_loser_path.c_str());
    DeleteFileA(parent_compressed_loser_path.c_str());
    DeleteFileA(a1_marker_path.c_str());
    DeleteFileA(parent_rts_config_path.c_str());
    DeleteFileA(active_rts_config_path.c_str());
    RemoveDirectoryA(rgb_path.c_str());
    RemoveDirectoryA(index8_path.c_str());
    RemoveDirectoryA(compressed_path.c_str());
    RemoveDirectoryA(textures_path.c_str());
    RemoveDirectoryA(parent_rgb_path.c_str());
    RemoveDirectoryA(parent_textures_path.c_str());
    RemoveDirectoryA(extension_root_path);
    RemoveDirectoryA(parent_extension_root_path);
}
