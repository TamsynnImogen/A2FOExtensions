#include "../modules/A2FORefitMenus/scene_mailbox.hpp"
#include <atomic>
#include <cstdlib>
#include <thread>
using namespace a2fo::refit_menu;
using namespace a2fo::refit_menu::menu_scene;
static void check(bool value) { if (!value) std::abort(); }
static Artwork image(unsigned color) { return {1, 1, {color}}; }
int main() {
    Mailbox box;
    check(!box.snapshot().image);
    box.loading(true);
    check(box.snapshot().revision==1&&box.snapshot().image_revision==0);
    check(!box.needs_presentation(0));
    box.loading(false);
    check(box.snapshot().revision==2&&box.snapshot().image_revision==0);
    check(box.request("main", 640, 360, true));
    check(!box.request("main", 640, 360, true));
    auto request = box.request();
    check(box.publish(image(1), request));
    auto first = box.snapshot();
    check(first.image && first.image->pixels[0] == 1);
    check(first.image_revision==1);
    check(box.active(false));
    // A hidden/destroyed menu retains both its frame and selected view.
    check(box.snapshot().image == first.image);
    check(box.request("main", 640, 360, true));
    check(box.snapshot().image == first.image);
    // A blocked producer must not hold the exchange lock or freeze UI reads.
    std::atomic<bool> started{false}, release{false};
    std::thread producer([&] {
        auto work = box.request();
        box.loading(true);
        started = true;
        while (!release.load()) std::this_thread::yield();
        // The UI selects another view while this old draw is in flight.
        check(!box.publish(image(2), work));
    });
    while (!started.load()) std::this_thread::yield();
    check(box.snapshot().loading);
    check(box.snapshot().image_revision==first.image_revision);
    for (int i = 0; i < 1000; ++i) check(box.snapshot().image == first.image);
    check(box.request("custom-campaign-42", 640, 360, true));
    release = true;
    producer.join();
    check(box.publish(image(3), box.request()));
    auto second = box.snapshot();
    check(second.image_revision==first.image_revision+1);
    check(second.image->pixels[0] == 3 && !second.loading);
    check(first.image->pixels[0] == 1); // Old borrowed snapshot is still valid.
    check(box.request("custom-campaign-42", 640, 400, true));
    check(!box.publish(image(4), request)); // Stale dimensions also rejected.
    const auto menu_request = box.request();
    check(box.request("custom-campaign-42", 640, 400, true, true, 2000));
    const auto departure = box.request();
    check(!box.active(false)); // An obsolete menu cannot pause loading.
    check(!box.publish(image(6), menu_request));
    check(box.publish(image(7), departure));
    check(box.snapshot().departure);
    check(!box.request("custom-campaign-42", 640, 400, true, true, 3000));
    check(box.request().lease_until == 3000); // Renewal needs no new image epoch.
    check(box.request("custom-campaign-42", 640, 400, true, true, 4000, true));
    check(!box.publish(image(8), departure)); // Repeated same-map load is fresh.
    check(box.end_departure());
    check(box.publish(image(9), box.request()));
    second = box.snapshot();
    check(!second.departure && !box.request().active);
    box.log("worker message");
    check(box.logs().size() == 1 && box.logs().empty());
    box.stop();
    check(box.request().stop && !box.request().active);
    check(!box.request("main", 640, 360, true));
    check(!box.publish(image(5), box.request()));
    check(box.snapshot().image == second.image);
    // A stalled consumer leaves precisely one pending GPU frame. Repainting an
    // older image cannot advance the animation clock or permit another render.
    Mailbox paced;
    paced.request("main",1,1,true,true,10000);
    const auto work=paced.request();
    check(paced.publish(image(10),work));
    const auto ready=paced.snapshot().image;
    check(paced.needs_presentation(0)&&!paced.needs_presentation(paced.snapshot().image_revision));
    check(paced.awaiting_presentation(paced.request()));
    check(paced.presented(ready,1000));
    check(!paced.needs_presentation(0));
    check(!paced.awaiting_presentation(paced.request()));
    check(paced.request().animation_time>1.032&&paced.request().animation_time<1.034);
    check(!paced.presented(ready,2500));
    check(!paced.publish(image(11),work)); // Already consumed render ticket.
    check(paced.publish(image(12),paced.request()));
    check(paced.presented(paced.snapshot().image,2500));
    check(paced.request().animation_time>1.082&&paced.request().animation_time<1.084);
    check(!paced.presented(ready,2600));
    check(paced.publish(image(13),paced.request()));
    const auto pending=paced.snapshot().image;
    paced.request("other",1,1,true,true,10000);
    check(!paced.needs_presentation(0));
    check(!paced.presented(pending,5000)); // Wrong camera cannot start its clock.
    check(paced.publish(image(14),paced.request()));
    const auto old_size=paced.snapshot().image;
    paced.request("other",2,2,true,true,10000);
    check(!paced.needs_presentation(0));
    check(!paced.presented(old_size,6000)); // Retained resize fallback is not the new frame.
}
