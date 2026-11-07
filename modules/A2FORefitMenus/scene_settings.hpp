#pragma once

#include <cstdint>
#include <string_view>

namespace a2fo::refit_menu::menu_scene {

struct SceneRenderWidth {
    int width = 0; // 0 selects the client width, subject to render limits.
    bool invalid = false;
};

inline SceneRenderWidth parse_scene_render_width(std::string_view value) {
    constexpr auto space = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
               c == '\v' || c == '\f';
    };
    while (!value.empty() && space(value.front())) value.remove_prefix(1);
    while (!value.empty() && space(value.back())) value.remove_suffix(1);
    if (value.empty()) return {};

    int width = 0;
    for (char c : value) {
        if (c < '0' || c > '9') return {0, true};
        // Anything larger than the supported maximum is invalid; this also
        // avoids overflow for arbitrarily long INI values.
        if (width > (3840 - (c - '0')) / 10) return {0, true};
        width = width * 10 + (c - '0');
    }
    if (width != 0 && width < 320) return {0, true};
    return {width, false};
}

struct SceneSize {
    int width = 0;
    int height = 0;
    bool valid = false;
};

inline SceneSize target_size(int client_width, int client_height, int configured_width) {
    if (client_width < 80 || client_height < 100 ||
        (configured_width != 0 && (configured_width < 320 || configured_width > 3840))) {
        return {};
    }

    int low = 1;
    int high = client_width < 3840 ? client_width : 3840;
    if (configured_width != 0 && configured_width < high) high = configured_width;
    int best_width = 0;
    int best_height = 0;
    while (low <= high) {
        const int width = low + (high - low) / 2;
        const auto height = static_cast<std::int64_t>(client_height) * width / client_width;
        if (height >= 1 && height <= 2160 &&
            static_cast<std::int64_t>(width) * height <= 8000000) {
            best_width = width;
            best_height = static_cast<int>(height);
            low = width + 1;
        } else if (height < 1) {
            low = width + 1;
        } else {
            high = width - 1;
        }
    }
    if (best_width == 0) return {};
    return {best_width, best_height, true};
}


} // namespace a2fo::refit_menu::menu_scene
