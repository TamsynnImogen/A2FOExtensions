#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <string>

#include "../modules/A2FORefitMenus/presentation.hpp"

using namespace a2fo::refit_menu;

static bool overlaps(const Rect& a, const Rect& b) {
    return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 &&
           a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

static void check_layout(int width, int height) {
    const Snapshot snapshot = default_snapshot();
    const Layout geometry = layout(width, height, snapshot);
    assert(geometry.scale > 0.0f);
    assert(geometry.title.x >= 0 && geometry.title.y >= 0);
    assert(geometry.title.y >= static_cast<int>(40 * geometry.scale));
    assert(geometry.title.x + geometry.title.width <= width);
    assert(geometry.title.y + geometry.title.height <= height);
    for (std::size_t i = 0; i < entry_count; ++i) {
        const Rect& row = geometry.rows[i];
        assert(row.width > 0 && row.height > 0);
        assert(row.x >= 0 && row.y >= 0);
        assert(row.x + row.width <= width);
        assert(row.y + row.height <= height);
        assert(!overlaps(geometry.title, row));
        assert(!overlaps(geometry.footer, row));
        for (std::size_t j = 0; j < i; ++j)
            assert(!overlaps(geometry.rows[j], row));
        assert(hit_test(geometry, snapshot, row.x + row.width / 2,
                        row.y + row.height / 2) == static_cast<int>(i));
        assert(hit_test(geometry, snapshot, row.x + row.width,
                        row.y + row.height / 2) != static_cast<int>(i));
    }
    assert(geometry.footer.x >= 0 && geometry.footer.y >= 0);
    assert(geometry.footer.x + geometry.footer.width <= width);
    assert(geometry.footer.y + geometry.footer.height <= height);
    assert(hit_test(geometry, snapshot, -1, -1) == -1);
}

int main() {
    static_assert(entry_count == static_cast<std::size_t>(Action::count));
    const Snapshot defaults = default_snapshot();
    assert(defaults.entries[0].caption == "SINGLE PLAYER");
    assert(defaults.entries[4].caption == "MAP EDITOR");
    assert(defaults.entries[6].caption == "EXIT");
    for (const Entry& entry : defaults.entries)
        assert(entry.visible && entry.enabled);

    for (const auto& resolution : std::array<std::array<int, 2>, 8>{{
             {{640, 480}}, {{800, 600}}, {{1200, 900}}, {{1920, 1080}},
             {{2560, 1440}}, {{3440, 1440}}, {{480, 800}}, {{600, 1024}}
         }})
        check_layout(resolution[0], resolution[1]);

    Snapshot changed = defaults;
    changed.entries[1].visible = false;
    changed.entries[3].enabled = false;
    Layout geometry = layout(640, 480, changed);
    assert(geometry.rows[1].width == 0 && geometry.rows[1].height == 0);
    assert(geometry.rows[2].y > geometry.rows[0].y + geometry.rows[0].height);
    assert(geometry.rows[2].y < geometry.rows[3].y);
    assert(hit_test(geometry, changed, geometry.rows[0].x - 1,
                    geometry.rows[0].y) == -1);
    assert(hit_test(geometry, changed, geometry.rows[0].x,
                    geometry.rows[0].y + geometry.rows[0].height) == -1);
    assert(hit_test(geometry, changed, geometry.rows[3].x + 1,
                    geometry.rows[3].y + 1) == -1);
    assert(next_enabled(changed, 0, 1) == 2);
    assert(next_enabled(changed, 2, 1) == 4);
    assert(next_enabled(changed, 4, -1) == 2);
    assert(next_enabled(changed, 6, 1) == 0);
    assert(next_enabled(changed, 0, -1) == 6);
    assert(next_enabled(changed, -1, 1) == 0);
    assert(next_enabled(changed, -1, -1) == 6);
    assert(next_enabled(changed, 3, 0) == -1);
    assert(next_enabled(changed, 2, 0) == 2);
    for (Entry& entry : changed.entries) entry.enabled = false;
    assert(next_enabled(changed, -1, 1) == -1);
    assert(next_enabled(changed, 0, -1) == -1);

    assert(layout(0, 0, defaults).scale == 0.0f);
    assert(layout(-1, 480, defaults).rows[0].width == 0);
    assert(layout(80, 99, defaults).rows[0].height == 0);
    assert(hit_test(layout(0, 0, defaults), defaults, 0, 0) == -1);
}
