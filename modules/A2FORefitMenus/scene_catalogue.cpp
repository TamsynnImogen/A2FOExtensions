#include "scene_catalogue.hpp"
#include "../../rust/ui_ffi.h"
#include <cstdint>
#include <fstream>
#include <utility>
namespace a2fo::refit_menu::menu_scene {
namespace {
constexpr std::size_t max_text = 1024 * 1024;
bool fail(std::string *error, const char *reason) {
    if (error)
        *error = reason;
    return false;
}
const char *reason(unsigned code) {
    switch (code) {
    case 1:
        return "menu recipe exceeds 1 MiB";
    case 2:
        return "malformed INI section";
    case 3:
        return "invalid view ID length";
    case 4:
        return "view ID contains control character";
    case 5:
        return "too many views";
    case 6:
        return "duplicate view ID";
    case 7:
        return "view label exceeds 128 bytes";
    default:
        return "menu recipe exceeds output capacity";
    }
}
} // namespace
bool parse_scene_catalogue(std::string_view text, std::vector<SceneView> &views,
                           std::string *error) {
    A2foRsSceneView records[128]{};
    std::vector<std::uint8_t> storage(text.size() * 2 + 1);
    std::size_t used = 0, count = 0;
    const auto status =
        a2fo_rs_ui_catalogue(reinterpret_cast<const std::uint8_t *>(text.data()), text.size(),
                             records, 128, storage.data(), storage.size(), &used, &count);
    if (status)
        return fail(error, reason(status));
    std::vector<SceneView> parsed;
    parsed.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto &r = records[i];
        parsed.push_back(
            {std::string(reinterpret_cast<const char *>(storage.data() + r.id_offset), r.id_len),
             std::string(reinterpret_cast<const char *>(storage.data() + r.label_offset),
                         r.label_len)});
    }
    views = std::move(parsed);
    if (error)
        error->clear();
    return true;
}
bool load_scene_catalogue(const std::string &root, std::vector<SceneView> &views,
                          std::string *error) {
    if (root.empty())
        return fail(error, "empty resource root");
    std::string path = root;
    if (path.back() != '/' && path.back() != '\\')
        path += '/';
    path += "Menu/map.ini";
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return fail(error, "cannot open Menu/map.ini");
    std::string text;
    text.reserve(max_text + 1);
    char chunk[4096];
    while (file && text.size() <= max_text) {
        file.read(chunk, sizeof(chunk));
        text.append(chunk, static_cast<std::size_t>(file.gcount()));
    }
    if (file.bad())
        return fail(error, "cannot read Menu/map.ini");
    return parse_scene_catalogue(text, views, error);
}
} // namespace a2fo::refit_menu::menu_scene
