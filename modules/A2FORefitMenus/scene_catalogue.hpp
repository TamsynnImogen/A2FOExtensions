#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace a2fo::refit_menu::menu_scene {

struct SceneView {
    std::string id;
    std::string label;
};

// Reads view names from one menu recipe, in source order. On failure, views is
// unchanged. IDs are ASCII case folded, as in the scene provider's INI parser.
bool parse_scene_catalogue(std::string_view text, std::vector<SceneView>& views,
                           std::string* error = nullptr);

// Reads the same single recipe selected for the provider: resource_root/Menu/map.ini.
// A missing or invalid file fails without changing views.
bool load_scene_catalogue(const std::string& resource_root, std::vector<SceneView>& views,
                          std::string* error = nullptr);

} // namespace a2fo::refit_menu::menu_scene
