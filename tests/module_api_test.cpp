#include "a2fo_module_api.h"

#include <cassert>
#include <cstddef>

int main() {
    A2FO_ModuleApi api{};
    api.struct_size = A2FO_MODULE_API_V4_BASE_SIZE;
    assert(!A2FO_MODULE_API_HAS(&api, api_revision));
    api.struct_size = sizeof(api);
    assert(A2FO_MODULE_API_HAS(&api, api_revision));
    assert(A2FO_MODULE_API_HAS(&api, capabilities));
    assert(A2FO_MODULE_API_HAS(&api, register_object_destroyed_handler));
    assert(A2FO_MODULE_API_HAS(&api, upgrade_pod_maximum_tier));
    assert(A2FO_MODULE_API_HAS(&api, get_original_classlabel));
    assert(A2FO_MODULE_API_HAS(&api, associate_evolver_cocoon_class));
    assert(A2FO_MODULE_API_HAS(&api, register_info_ini_defaults_handler));
    assert(A2FO_MODULE_API_HAS(&api, register_odf_overlay_directory));
    assert(A2FO_MODULE_API_HAS(&api, odf_overlay_directory_count));
    assert(A2FO_MODULE_API_HAS(&api, get_odf_overlay_directory));
    assert(A2FO_MODULE_API_HAS(&api, register_producer_event_handler));
    assert(A2FO_MODULE_API_HAS(&api, dispatch_producer_event));
    assert(A2FO_MODULE_API_HAS(&api, register_classlabel_odf_defaults));
    assert(A2FO_MODULE_API_HAS(&api, patch_bytes));
    assert(A2FO_MODULE_API_HAS(
        &api, register_game_object_class_loaded_handler));
    assert(A2FO_MODULE_API_HAS(&api, register_race_loaded_handler));
    assert(A2FO_MODULE_API_HAS(
        &api, register_weapon_class_loaded_handler));
    assert(A2FO_MODULE_API_HAS(&api, register_weapon_trigger_handler));
    assert(A2FO_MODULE_API_HAS(&api, register_craft_event_handler));
    assert(offsetof(A2FO_ModuleApi, api_revision) >=
           A2FO_MODULE_API_V4_BASE_SIZE);
    assert(A2FO_MODULE_API_VERSION == 4u);
    assert(A2FO_MODULE_API_REVISION >= 14u);
    assert(A2FO_MODULE_API_HAS(&api, get_settings_directory));
    assert(A2FO_CAP_CLASSLABEL_ODF_DEFAULTS == (1ull << 7));
    assert(A2FO_CAP_GAME_OBJECT_CLASS_LOADED == (1ull << 8));
    assert(A2FO_CAP_RACE_LOADED == (1ull << 9));
    assert(A2FO_CAP_WEAPON_CLASS_LOADED == (1ull << 10));
    assert(A2FO_CAP_WEAPON_TRIGGER_EVENTS == (1ull << 11));
    assert(A2FO_CAP_CRAFT_EVENTS == (1ull << 12));
    assert(A2FO_PRODUCER_EVENT_FINISHING == 3u);
    assert(A2FO_PRODUCER_EVENT_STARTING_EFFECT == 4u);
    assert(A2FO_PRODUCER_EVENT_CANCELLED == 5u);
    assert(A2FO_PRODUCER_EVENT_DELETED == 6u);
    assert(A2FO_PRODUCER_EVENT_CLEARED == 7u);
}
