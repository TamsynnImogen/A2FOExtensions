#include "../modules/A2FORefitMenus/scene_settings.hpp"

#include <cstdlib>
#include <limits>

using namespace a2fo::refit_menu::menu_scene;

void check(bool value) { if (!value) std::abort(); }

void check_size(SceneSize size, int width, int height) {
    check(size.valid && size.width == width && size.height == height);
}

int main() {
    check(parse_scene_render_width("").width == 0);
    check(!parse_scene_render_width(" \t\r\n").invalid);
    check(!parse_scene_render_width(" 0 ").invalid);
    check(parse_scene_render_width(" 320 ").width == 320);
    check(parse_scene_render_width("3840").width == 3840);
    for (const char* value : {"1", "319", "3841", "-320", "+320", "320px",
                              "320.0", "3 20", "999999999999999999999"}) {
        const auto parsed = parse_scene_render_width(value);
        check(parsed.invalid && parsed.width == 0);
    }

    check_size(target_size(1920, 1080, 0), 1920, 1080);
    check_size(target_size(1920, 1080, 1280), 1280, 720);
    check_size(target_size(1920, 1080, 320), 320, 180);
    check_size(target_size(800, 600, 1280), 800, 600); // Never upscale.
    check_size(target_size(80, 100, 0), 80, 100);
    check_size(target_size(1000, 3000, 0), 720, 2160); // Portrait height cap.
    check_size(target_size(7680, 2160, 0), 3840, 1080); // Ultra-wide aspect.

    const auto four_k = target_size(3840, 2160, 0);
    check(four_k.valid && four_k.width <= 3840 && four_k.height <= 2160);
    check(static_cast<long long>(four_k.width) * four_k.height <= 8000000);
    check(four_k.width < 3840); // The pixel cap applies before full 4K.

    for (const auto size : {target_size(79, 1080, 0), target_size(1920, 99, 0),
                            target_size(1920, 1080, 319), target_size(1920, 1080, 3841),
                            target_size(1920, 1080, -1),
                            target_size(std::numeric_limits<int>::max(), 100, 0)}) {
        check(!size.valid && size.width == 0 && size.height == 0);
    }

    const auto huge = target_size(std::numeric_limits<int>::max(),
                                  std::numeric_limits<int>::max(), 0);
    check(huge.valid && huge.width <= 3840 && huge.height <= 2160);
    check(static_cast<long long>(huge.width) * huge.height <= 8000000);
}
