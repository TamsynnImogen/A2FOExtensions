#pragma once

#include "presentation.hpp"
#include "loading_theme.hpp"

#include <string>
#include <utility>
#include <vector>

namespace a2fo::refit_menu {

struct Player {
    std::string name;
    std::string detail;
    int team = 0;
    bool local = false;
    float fraction = -1.0f;

    Player() = default;
    Player(std::string player_name, std::string player_detail, int player_team = 0,
           bool is_local = false, float load_fraction = -1.0f)
        : name(std::move(player_name)), detail(std::move(player_detail)),
          team(player_team), local(is_local), fraction(load_fraction) {}
};

enum class LoadingMode { unknown, instant_action, campaign, multiplayer };

struct LoadingModel {
    LoadingMode kind = LoadingMode::unknown;
    std::string title;
    std::string mode;
    std::string status;
    std::vector<std::string> briefing;
    std::vector<std::string> objectives;
    std::string hint;
    std::vector<Player> players;
    bool show_preview = false;
    float progress = 0.0f;
    LoadingTheme theme;
    double time_seconds = 0.0;
    bool departure_shown = false;
    // Full configured native bar extent, before the engine applies progress.
    // Only its track/frame is ours; FO draws the ODF-coloured fill afterwards.
    Rect bar_track;
    // Client-pixel rectangles occupied by native loadscreen.odf overlays.
    std::vector<Rect> protected_regions;
};

struct LoadingLayout {
    bool valid = false;
    float scale = 0.0f;
    Rect kicker, title, briefing, objectives, hint;
    Rect preview, players, status, percentage;
};

LoadingLayout loading_layout(int width, int height, const LoadingModel& model);

#ifdef _WIN32
bool paint_loading(HDC target, int width, int height, const LoadingModel& model,
                   const Artwork* backdrop = nullptr, const Artwork* preview = nullptr,
                   bool transparent_overlay = false);
#endif

} // namespace a2fo::refit_menu
