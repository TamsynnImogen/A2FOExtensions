#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <cstdint>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace a2fo::refit_menu {

enum class Action {
    single_player,
    instant_action,
    multiplayer,
    options,
    map_editor,
    credits,
    exit,
    count
};

constexpr std::size_t entry_count = 7;
static_assert(static_cast<std::size_t>(Action::count) == entry_count);

struct Entry {
    std::string caption;
    bool visible = true;
    bool enabled = true;
};

struct Snapshot {
    std::array<Entry, entry_count> entries;
    std::string version;
};

// Top-down 32-bit BI_RGB pixels, one 0x00RRGGBB value per pixel.
struct Artwork {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int px, int py) const;
};

struct Layout {
    Rect title;
    std::array<Rect, entry_count> rows;
    Rect footer;
    float scale = 0.0f;
};

Snapshot default_snapshot();
Layout layout(int width, int height, const Snapshot& snapshot);
int hit_test(const Layout& geometry, const Snapshot& snapshot, int x, int y);
int next_enabled(const Snapshot& snapshot, int current, int direction);

#ifdef _WIN32
bool paint(HDC target, int width, int height, const Snapshot& snapshot,
           int hot, int pressed, const Artwork* artwork = nullptr);
#endif

} // namespace a2fo::refit_menu
