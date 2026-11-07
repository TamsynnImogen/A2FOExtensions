#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

#include "../modules/A2FORefitMenus/scene_composition.hpp"

using a2fo::refit_menu::Artwork;
using a2fo::refit_menu::scene::Backgrounds;

static Artwork solid(int width, int height, std::uint32_t color) {
    return {width, height, std::vector<std::uint32_t>(
        static_cast<std::size_t>(width) * height, color)};
}

int main() {
    Backgrounds backgrounds;
    Artwork base = solid(64, 64, 0x00112233u);
    Artwork labelled = base;
    for (int y = 2; y < 5; ++y)
        for (int x = 2; x < 18; ++x)
            labelled.pixels[y * 64 + x] = 0x00ffffffu;
    labelled.pixels[8 * 64 + 8] = 0x00152537u; // +4 per channel: preserve label edges.
    labelled.pixels[9 * 64 + 9] = 0x00152737u; // +5 green: native overlay.
    labelled.pixels[11 * 64 + 11] = 0x00112234u; // +1 blue: native overlay edge.
    labelled.pixels[10 * 64 + 10] |= 0xff000000u; // Ignore alpha.

    Artwork live = solid(2, 2, 0x00000001u);
    live.pixels = {0x00000102u, 0x00030405u,
                   0x00060708u, 0x00090a0bu};
    Artwork out = solid(1, 1, 0x00abcdefu);
    assert(!backgrounds.matches(labelled));
    assert(!backgrounds.compose(labelled, live, out));
    assert(out.width == 1 && out.pixels[0] == 0x00abcdefu);
    backgrounds.remember("menu", base);
    assert(backgrounds.matches(labelled));
    assert(backgrounds.key(labelled)=="menu");
    assert(out.width == 1 && out.pixels[0] == 0x00abcdefu);
    assert(backgrounds.compose(labelled, live, out));
    assert(out.width == 64 && out.height == 64);
    assert(out.pixels[0] == 0x00000001u);
    assert(out.pixels[63] == 0x00010202u);
    assert(out.pixels[63 * 64] == 0x00030304u);
    assert(out.pixels[63 * 64 + 63] == 0x00040505u);
    assert(out.pixels[2 * 64 + 2] == 0x00ffffffu);
    assert(out.pixels[8 * 64 + 8] == 0x00152537u);
    assert(out.pixels[9 * 64 + 9] == 0x00152737u);
    assert(out.pixels[11 * 64 + 11] == 0x00112234u);
    assert(out.pixels[10 * 64 + 10] == 0x00000001u);
    const auto* reused = out.pixels.data();
    assert(backgrounds.compose(labelled, live, out));
    assert(out.pixels.data() == reused);
    Artwork aliased = labelled;
    assert(backgrounds.compose(aliased, live, aliased));
    assert(aliased.pixels == out.pixels);

    Artwork mismatch = solid(64, 64, 0x00000000u);
    Artwork before = out;
    assert(!backgrounds.matches(mismatch));
    assert(!backgrounds.compose(mismatch, live, out));
    assert(out.pixels == before.pixels);
    assert(!backgrounds.compose(solid(32, 128, 0x00112233u), live, out));
    assert(!backgrounds.compose(labelled, Artwork{}, out));
    assert(!backgrounds.compose(Artwork{64, 64, {}}, live, out));
    assert(out.pixels == before.pixels);

    // An almost-black image must still pass the full-image identity check.
    Backgrounds black;
    Artwork black_base = solid(64, 64, 0x00000000u);
    // Change over 10% of pixels away from the 16x16 sample coordinates.
    int changed = 0;
    for (int y = 0; y < 64 && changed < 500; ++y) {
        for (int x = 0; x < 64 && changed < 500; ++x) {
            bool sampled = false;
            for (int s = 0; s < 16; ++s)
                sampled |= x == s * 63 / 15;
            if (sampled) continue;
            black_base.pixels[y * 64 + x] = 0x00010101u;
            ++changed;
        }
    }
    black.remember("black", black_base);
    assert(!black.matches(solid(64, 64, 0x00000000u)));
    assert(!black.compose(solid(64, 64, 0x00000000u), live, out));

    Backgrounds limited;
    for (unsigned i = 1; i <= 17; ++i)
        limited.remember("small" + std::to_string(i), solid(8, 8, i));
    assert(!limited.matches(solid(8, 8, 1)));
    assert(!limited.compose(solid(8, 8, 1), live, out));
    assert(limited.matches(solid(8, 8, 17)));
    assert(limited.compose(solid(8, 8, 17), live, out));
    limited.remember("oversize", solid(8193, 1, 0x00123456u));
    assert(limited.compose(solid(8, 8, 17), live, out));

    Backgrounds budget;
    budget.remember("first", solid(2000, 2000, 0x00101010u));
    budget.remember("second", solid(2000, 2000, 0x00202020u));
    budget.remember("third", solid(2, 2, 0x00303030u));
    assert(!budget.matches(solid(2000, 2000, 0x00101010u)));
    assert(!budget.compose(solid(2000, 2000, 0x00101010u), live, out));
    assert(budget.matches(solid(2000, 2000, 0x00202020u)));
    assert(budget.compose(solid(2000, 2000, 0x00202020u), live, out));

    // Instant Action panels are drawn over the scene, while the stock
    // lower-left ship illustration and other unchanged artwork disappear.
    const auto check_setup = [](const std::string& key, int width, int height,
                                bool styled) {
        Backgrounds setup;
        Artwork original = solid(width, height, 0x00003090u);
        const auto at = [width, height](int design_x, int design_y) {
            return static_cast<std::size_t>(design_y * height / 600) * width +
                   design_x * width / 800;
        };
        original.pixels[at(32, 20)] = 0x00aeb1b5u; // Embedded bright neutral title.
        original.pixels[at(600, 20)] = 0x00151b23u; // Dark background is not a caption.
        Artwork current = original;
        current.pixels[at(100, 100)] = 0x00003091u; // +1 blue native control edge.
        current.pixels[at(101, 400)] = 0x00003091u; // Dynamic content over scene.
        Artwork scene = solid(1, 1, 0x00102030u);
        Artwork composed;
        setup.remember(key, original);
        assert(setup.compose(current, scene, composed));
        assert(composed.pixels[at(32, 20)] ==
               (styled ? 0x00aeb1b5u : 0x00081018u));
        assert(composed.pixels[at(600, 20)] ==
               (styled ? 0x00060c16u : 0x00081018u));
        for (const auto point : {std::pair<int, int>{100, 150}, {600, 150},
                                 {450, 400}, {650, 400}, {100, 575}})
            assert(composed.pixels[at(point.first, point.second)] ==
                   (styled ? 0x00060c16u : 0x00081018u));
        assert(composed.pixels[at(14, 100)] ==
               (styled ? 0x008e5a1au : 0x00081018u));
        assert(composed.pixels[at(100, 100)] == 0x00003091u);
        assert(composed.pixels[at(100, 400)] == 0x00081018u);
        assert(composed.pixels[at(101, 400)] == 0x00003091u);
        assert(composed.pixels[at(0, 0)] == 0x00081018u);
    };
    check_setup("bitmaps\\MultiSetup\\InstActionBkg.BMP", 800, 600, true);
    check_setup("multisetup/MultiPlaySetupBkg.png", 800, 600, true);
    check_setup("multisetup/instactionbkg.png", 1200, 900, true);
    check_setup("multisetup/notinstactionbkg.bmp", 800, 600, false);
    check_setup("other/multisetup/instactionbkg.bmp", 800, 600, false);
    check_setup("multisetup/instactionbkg_high.bmp", 800, 600, false);
}
