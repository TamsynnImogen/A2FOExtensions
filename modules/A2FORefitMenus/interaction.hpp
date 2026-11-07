#pragma once
#include "presentation.hpp"

namespace a2fo::refit_menu {
// Pure application state. Tests feed data here; no window/input APIs.
struct Interaction {
    int hot = -1;
    int pressed = -1;
    int pending = -1;
    void move(const Layout& l, const Snapshot& s, int x, int y) {
        hot = hit_test(l, s, x, y);
        if (pressed != hot) pressed = -1;
    }
    void down(const Layout& l, const Snapshot& s, int x, int y) {
        hot = hit_test(l, s, x, y); pressed = hot;
    }
    bool request(const Snapshot& s, int row) {
        if (pending >= 0 || row < 0 || row >= static_cast<int>(entry_count) ||
            !s.entries[row].visible || !s.entries[row].enabled) return false;
        pending = row; return true;
    }
    bool up(const Layout& l, const Snapshot& s, int x, int y) {
        const int was = pressed; pressed = -1; hot = hit_test(l, s, x, y);
        return was >= 0 && was == hot && request(s, was);
    }
    void leave() { hot = pressed = -1; }
    int take(const Snapshot& s) {
        const int row = pending; pending = -1;
        if (row < 0 || row >= static_cast<int>(entry_count) ||
            !s.entries[row].visible || !s.entries[row].enabled) return -1;
        return row;
    }
};
} // namespace a2fo::refit_menu
