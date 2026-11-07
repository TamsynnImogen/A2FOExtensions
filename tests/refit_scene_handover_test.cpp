#include "../modules/A2FORefitMenus/scene_handover.hpp"
#include "../modules/A2FORefitMenus/scene_transition.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <thread>
using namespace a2fo::refit_menu::menu_scene;
using a2fo::refit_menu::Artwork;
void check(bool okay){if(!okay)std::abort();}
int main(){
    const auto scene=std::make_shared<Artwork>(Artwork{2,1,{0x000000ff,0x0000ff00}});
    const auto overlay=std::make_shared<Artwork>(Artwork{1,1,{0x00ff0000}});
    check(transition_image(scene,overlay,0)==overlay);
    const auto blended=transition_image(scene,overlay,90);
    check(blended->width==2&&blended->height==1&&blended->pixels[0]==0x0080007f&&blended->pixels[1]==0x00807f00);
    check(transition_image(scene,overlay,180)==scene&&transition_image(scene,{},90)==scene);
    const auto invalid=std::make_shared<Artwork>();
    check(transition_image(scene,invalid,90)==scene);
    SceneHandover h;unsigned draws{};
    check(!h.state().active(0));
    h.begin(1,2,7,"main.instantaction",100);
    check(h.state().active(100)&&!h.state().ready);
    check(h.present(110,"main",[&](const auto& s){++draws;return s.source==1&&s.parent==2&&s.thread==7;}));
    check(!h.state().ready&&h.state().frames==1);
    check(!h.present(120,"main.instantaction",[](const auto&){return false;}));
    check(!h.state().ready&&h.state().frames==1);
    h.destination(3,130);
    check(h.present(140,"main.instantaction",[](const auto& s){return s.destination==3;}));
    check(h.state().ready&&h.state().frames==2);
    check(!h.present(10100,"main.instantaction",[&](const auto&){++draws;return true;}));
    check(draws==1);
    h.destination(4,10100);check(h.state().destination==3);
    auto old=h.finish();check(old.frames==2&&!h.state().active(10100));
    check(!h.present(150,"main.instantaction",[](const auto&){return true;}));
    h.begin(5,6,7,"other",200);check(!h.state().ready&&!h.state().destination);
    // A completed UI frame must wait for an in-flight short copy, then prevent
    // every later copy. Rendering/asset work is outside this critical section.
    std::mutex m;std::condition_variable cv;bool copying{},release{};
    std::atomic<bool> ended{};
    std::thread painter([&]{h.present(210,"other",[&](const auto&){
        std::unique_lock<std::mutex> lock(m);copying=true;cv.notify_all();
        cv.wait(lock,[&]{return release;});return true;
    });});
    {std::unique_lock<std::mutex> lock(m);cv.wait(lock,[&]{return copying;});}
    SceneHandover::State during_copy;
    check(!h.try_state(during_copy)); // USER32 erase never waits for a worker GDI copy.
    std::thread finish([&]{h.finish();ended=true;});
    {std::lock_guard<std::mutex> lock(m);check(!ended);release=true;cv.notify_all();}
    painter.join();finish.join();check(ended&&!h.state().active(220));
    check(!h.present(230,"other",[](const auto&){return true;}));
}
