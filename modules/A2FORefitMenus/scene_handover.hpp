#pragma once
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>
#include <utility>

namespace a2fo::refit_menu {struct Artwork;}
namespace a2fo::refit_menu::menu_scene {
// The UI owns the native forms. This short lease lets the existing renderer
// paint their background while native construction blocks that UI thread.
// Never hold this lock during rendering, readback, asset loading or callbacks.
class SceneHandover {
public:
    struct State {
        std::uintptr_t source{}, parent{}, destination{};
        std::uint32_t thread{};
        std::uint64_t until{}, started{};
        std::string view;
        unsigned frames{};
        bool ready{};
        std::uintptr_t token{};
        bool launch{};
        std::shared_ptr<const Artwork> overlay;
        bool active(std::uint64_t now) const { return parent && now < until; }
    };
    void begin(std::uintptr_t source,std::uintptr_t parent,std::uint32_t thread,
               std::string view,std::uint64_t now,std::uintptr_t token=0,
               bool launch=false,std::shared_ptr<const Artwork> overlay={}) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_={source,parent,0,thread,now+10000,now,std::move(view),0,false,token,launch,std::move(overlay)};
    }
    State state() const { std::lock_guard<std::mutex> lock(mutex_); return state_; }
    bool try_state(State& out) const {
        std::unique_lock<std::mutex> lock(mutex_,std::try_to_lock);
        if(!lock.owns_lock())return false;
        out=state_;return true;
    }
    void destination(std::uintptr_t window,std::uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(state_.active(now))state_.destination=window;
    }
    template<class Draw> bool present(std::uint64_t now,const std::string& view,Draw&& draw) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!state_.active(now)||!draw(state_))return false;
        state_.ready=state_.ready||view==state_.view;++state_.frames;return true;
    }
    State finish() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto old=state_;state_={};return old;
    }
private:
    mutable std::mutex mutex_;
    State state_;
};
}
