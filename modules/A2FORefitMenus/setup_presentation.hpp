#pragma once

#include "presentation.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::refit_menu::setup {

enum class Kind { button, combo, label, picture };

struct Control {
    std::uintptr_t key = 0;
    Kind kind = Kind::label;
    Rect source;
    std::string text;
    bool message = false;
    bool visible = true;
    bool enabled = true;
    int mode = 0;
    bool open = false;
    bool down = true;
    int item_height = 16;
    Rect pull;
    std::vector<std::string> items;
    int selected = -1;
};

struct Model { std::vector<Control> controls; };
struct Placement {
    std::uintptr_t key = 0;
    Rect source;
    Rect target;
    Kind kind = Kind::label;
    int item = -1;
};
struct Layout {
    bool valid = false;
    float scale = 0.0f;
    Rect title, players, map, preview;
    std::vector<Placement> controls;
};

Layout arrange(int width, int height, const Model& model, int popup_scroll = 0);
struct NativeLayout {
    Model model;
    Layout geometry;
    int widget_width{},widget_height{};
};
// Admit only a unique complete stock or Fleet Ops widget-coordinate profile.
// The logical canvas and raw widget fields can use different authored sizes.
NativeLayout arrange_native(int width,int height,int canvas_width,int canvas_height,
                            const Model& raw,int popup_scroll=0);
bool map_point(const Layout& layout, int x, int y, int& sx, int& sy);
// Only the admitted two-button navigation profile; independent of translated text.
std::uintptr_t launch_at(const Model& model,const Layout& layout,int x,int y);
struct LaunchPress {
    std::uintptr_t key{};
    void cancel(){key=0;}
    bool release(std::uintptr_t over){const bool matched=over&&over==key;key=0;return matched;}
};
// Copy the first native picture placement (excluding synthetic item -2).
// Placement sources use the normalized 800 x 600 canvas.
Artwork loading_preview_snapshot(const Artwork& native, const Layout& layout);

#ifdef _WIN32
bool paint(HDC target, int width, int height, const Model& model,
           const Layout& layout, const Artwork* background,
           const Artwork* native_canvas, int hot_x = -1, int hot_y = -1);
#endif

} // namespace a2fo::refit_menu::setup
