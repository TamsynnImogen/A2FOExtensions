#include "../modules/A2FORefitMenus/loading_presentation.hpp"

#include <cassert>
#include <iostream>
#include <tuple>

using namespace a2fo::refit_menu;

static bool within(const Rect& r, int width, int height) {
    return r.width >= 0 && r.height >= 0 && r.x >= 0 && r.y >= 0 &&
           r.x + r.width <= width && r.y + r.height <= height;
}
static bool disjoint(const Rect& a, const Rect& b) {
    return a.width == 0 || a.height == 0 || b.width == 0 || b.height == 0 ||
        a.x + a.width <= b.x || b.x + b.width <= a.x ||
        a.y + a.height <= b.y || b.y + b.height <= a.y;
}

int main() {
    LoadingModel model;
    model.title = "A deliberately long mission title that should wrap safely";
    model.mode = "Instant Action";
    model.briefing = {"The first paragraph.", "The second paragraph."};
    model.objectives = {"Find a path", "Defend the outpost"};
    model.players = {{"One", "Federation", 0, true, 0.5f},
                     {"Two", "Klingon", 1, false, 1.0f}};
    model.time_seconds = 2.0;
    model.departure_shown = true;
    model.show_preview = true;
    for (auto size : {std::pair{800, 600}, std::pair{1280, 720},
                      std::pair{1920, 1080}, std::pair{2560, 1080},
                      std::pair{3840, 2160}}) {
        const auto [width, height] = size;
        const LoadingLayout layout = loading_layout(width, height, model);
        assert(layout.valid && layout.scale > 0.0f);
        for (const Rect& r : {layout.kicker, layout.title, layout.briefing,
                              layout.preview, layout.players, layout.status,
                              layout.percentage}) assert(within(r, width, height));
        assert(layout.kicker.y + layout.kicker.height <= layout.title.y);
        assert(layout.title.y + layout.title.height < layout.briefing.y);
        assert(layout.briefing.x + layout.briefing.width < layout.players.x);
        assert(layout.preview.x >= layout.players.x);
        assert(layout.preview.y + layout.preview.height <= layout.players.y);
        assert(layout.briefing.y + layout.briefing.height <= layout.status.y);
        assert(layout.status.x + layout.status.width <= layout.percentage.x);
    }
    model.players.clear();
    model.show_preview = false;
    auto solo = loading_layout(800, 600, model);
    assert(solo.valid && solo.preview.width == 0 && solo.players.width == 0);
    assert(solo.briefing.width > 0);
    model.show_preview = true;
    auto preview_only = loading_layout(800, 600, model);
    assert(preview_only.valid && preview_only.preview.width > 0);
    assert(preview_only.briefing.x + preview_only.briefing.width < preview_only.preview.x);
    model.players = {{"One", "Federation"}};
    model.kind = LoadingMode::instant_action;
    auto instant = loading_layout(800, 600, model);
    assert(instant.valid && instant.briefing.width == 0 && instant.objectives.width == 0);
    assert(instant.preview.width > 0 && instant.players.width > 0);
    model.kind = LoadingMode::multiplayer;
    auto multiplayer = loading_layout(800, 600, model);
    assert(multiplayer.valid && multiplayer.briefing.width == 0);
    assert(multiplayer.preview.width > 0 && multiplayer.players.width > 0);
    model.kind = LoadingMode::campaign;
    auto campaign = loading_layout(800, 600, model);
    assert(campaign.valid && campaign.briefing.width > 0 && campaign.objectives.width > 0);
    assert(campaign.preview.width == 0 && campaign.players.width == 0);
    model.kind = LoadingMode::unknown;
    auto unknown = loading_layout(800, 600, model);
    assert(unknown.briefing.width > 0 && unknown.preview.width > 0 && unknown.players.width > 0);
    assert(!loading_layout(799, 600, model).valid);
    assert(!loading_layout(800, 599, model).valid);
    assert(!loading_layout(3841, 2160, model).valid);
    LoadingModel low_bar;
    low_bar.kind = LoadingMode::instant_action;
    low_bar.show_preview = true;
    low_bar.players.assign(8, Player{"Long player name", "Species 8472", 1, false, 0.5f});
    low_bar.protected_regions = {{286, 57, 325, 543}, {40, 550, 720, 30}};
    const auto low_bar_layout = loading_layout(800, 600, low_bar);
    assert(low_bar_layout.valid && low_bar_layout.status.y + low_bar_layout.status.height < 550);
    for (const auto& box : {low_bar_layout.kicker, low_bar_layout.title,
                            low_bar_layout.preview, low_bar_layout.players,
                            low_bar_layout.status, low_bar_layout.percentage}) {
        assert(within(box, 800, 600));
        for (const auto& obstacle : low_bar.protected_regions) assert(disjoint(box, obstacle));
    }
    // A native FO tip occupies the middle of a 1280x800 client all the way
    // to the bottom; the bar stays native and sits below the presentation.
    for (auto [width, height, tip] : {
             std::tuple{1280, 800, Rect{457, 76, 520, 724}},
             std::tuple{1920, 800, Rect{685, 76, 780, 724}},
             std::tuple{1920, 1080, Rect{685, 103, 780, 977}}}) {
        LoadingModel crowded;
        crowded.kind = LoadingMode::instant_action;
        crowded.show_preview = true;
        crowded.title = "A deliberately long mission title that wraps";
        crowded.players.assign(8, Player{"A very long player name", "Species 8472 Undine faction", 1, false, 0.5f});
        crowded.bar_track = {width / 20, height - 30, width * 9 / 10, 5};
        crowded.protected_regions = {tip,
            {crowded.bar_track.x - 4, crowded.bar_track.y - 4,
             crowded.bar_track.width + 8, crowded.bar_track.height + 8},
            {width == 1280 ? 188 : 282, 104, width == 1280 ? 190 : 285, 180}};
        const auto placement = loading_layout(width, height, crowded);
        assert(placement.valid);
        assert(placement.players.height >= placement.scale * 72 * 8);
        assert(placement.briefing.width == 0);
        for (const auto& box : {placement.kicker, placement.title, placement.preview,
                                placement.players, placement.status, placement.percentage}) {
            assert(within(box, width, height));
            for (const auto& obstacle : crowded.protected_regions) assert(disjoint(box, obstacle));
        }
        crowded.kind = LoadingMode::campaign;
        crowded.briefing = {"Long briefing for the mission."};
        crowded.objectives = {"Defend the outpost", "Explore the nebula"};
        const auto campaign_placement = loading_layout(width, height, crowded);
        assert(campaign_placement.valid && campaign_placement.briefing.width > 0);
        assert(disjoint(campaign_placement.briefing, tip));
        assert(campaign_placement.players.width == 0 && campaign_placement.preview.width == 0);
    }
    std::cout << "refit loading layout OK\n";
}
