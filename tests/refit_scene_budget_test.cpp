#include "../modules/A2FORefitMenus/scene_budget.hpp"
#include <cstdlib>
using namespace a2fo::refit_menu::menu_scene;
void check(bool value) { if (!value) std::abort(); }
int main() {
    check(animation_pause(0)==33&&animation_pause(10)==23&&animation_pause(28)==5);
    check(animation_pause(29)==4&&animation_pause(400)==4);
    check(max_render_failures == 3);
    check(retry_pause(0, 500, 500) == 0);
    check(retry_pause(1, 500, 500) == 1000);
    check(retry_pause(2, 500, 1499) == 1);
    check(retry_pause(2, 500, 1500) == 0);
    check(retry_pause(1, 0xfffffff0u, 30) == 954);
    check(frame_pause(1) == 67);
    check(frame_pause(100) == 300);
    check(frame_pause(0xffffffffu) == 1000);
    check(!frame_due(true, 500, 799, 100));
    check(frame_due(true, 500, 800, 100));
    check(frame_due(false, 500, 501, 100));
    check(!frame_due(true, 0xfffffff0u, 30, 1));
    check(frame_due(true, 0xfffffff0u, 60, 1));
}
