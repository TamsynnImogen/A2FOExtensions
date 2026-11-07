#include "../modules/A2FORefitMenus/scene_catalogue.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using a2fo::refit_menu::menu_scene::SceneView;
using a2fo::refit_menu::menu_scene::load_scene_catalogue;
using a2fo::refit_menu::menu_scene::parse_scene_catalogue;

void check(bool value) { if (!value) std::abort(); }

int main() {
    std::vector<SceneView> views{{"previous", "Previous"}};
    std::string error;
    std::string many = "; comments and ordinary recipe sections are ignored\n[Scene]\nSky=mbgblue\n";
    for (int i = 0; i < 12; ++i) {
        many += "[View.Campaign" + std::to_string(i) + "]\n";
        many += "Group=custom\n";
        if (i == 7) many += "Label=  \"Frontier Expedition\" ; inline comment\n";
    }
    check(parse_scene_catalogue(many, views, &error) && error.empty());
    check(views.size() == 12);
    check(views[0].id == "campaign0" && views[0].label == "campaign0");
    check(views[7].id == "campaign7" && views[7].label == "Frontier Expedition");
    check(views[11].id == "campaign11");

    { // The checked-in renderer recipe remains a valid input.
        std::ifstream file("third_party/armada2-refit/menu-renderer/menus/map.ini");
        check(static_cast<bool>(file));
        const std::string recipe(std::istreambuf_iterator<char>{file}, {});
        check(parse_scene_catalogue(recipe, views, &error));
        check(views.size() == 6 && views[0].id == "main" && views[4].id == "borg");
        check(views[5].id == "main.instantaction" && views[5].label == "Instant Action");
    }

    check(parse_scene_catalogue(many, views, &error));

    const auto original = views;
    for (const std::string& bad : std::vector<std::string>{
             "[View]\nLabel=ignored\n[View.]\n",
             "[View.good\nLabel=missing bracket\n",
             "[View.Mixed]\n[View.mixed]\n",
             "[View." + std::string(129, 'a') + "]\n",
             "[View.valid]\nLabel=" + std::string(129, 'a') + "\n",
             std::string(1024 * 1024 + 1, 'a')}) {
        check(!parse_scene_catalogue(bad, views, &error) && !error.empty());
        check(views.size() == original.size() && views[7].label == original[7].label);
    }
    std::string limit;
    for (int i = 0; i < 128; ++i) limit += "[View.extra" + std::to_string(i) + "]\n";
    check(parse_scene_catalogue(limit, views) && views.size() == 128);
    check(!parse_scene_catalogue(limit + "[View.overflow]\n", views, &error));
    check(views.size() == 128);

    namespace fs = std::filesystem;
    const auto base = fs::temp_directory_path() / ("refit-scene-catalogue-" + std::to_string(std::rand()));
    const auto low = base / "low" / "RefitScene";
    const auto high = base / "high" / "RefitScene";
    fs::create_directories(low / "Menu");
    fs::create_directories(high / "Menu");
    { std::ofstream file(low / "Menu" / "map.ini"); file << "[View.low]\nLabel=Low\n"; }
    { std::ofstream file(high / "Menu" / "map.ini"); file << "[View.high]\nLabel=High\n"; }
    check(load_scene_catalogue(low.string(), views, &error));
    check(views.size() == 1 && views[0].id == "low");
    check(load_scene_catalogue(high.string(), views, &error));
    check(views.size() == 1 && views[0].id == "high" && views[0].label == "High");
    check(!load_scene_catalogue((base / "missing").string(), views, &error));
    check(views.size() == 1 && views[0].id == "high");
    { std::ofstream file(high / "Menu" / "map.ini"); file << std::string(1024 * 1024 + 1, 'x'); }
    check(!load_scene_catalogue(high.string(), views, &error));
    check(views.size() == 1 && views[0].id == "high");
    fs::remove_all(base);
}
