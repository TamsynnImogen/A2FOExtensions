#include "../renderer/refit/view_scope.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

using a2fo::refit::ViewScopes2;
namespace {
constexpr std::uintptr_t camera_a=0x1110;
constexpr std::uintptr_t camera_b=0x2220;
}
int main(){
    ViewScopes2 scopes;
    assert(!scopes.faulted()&&scopes.current(camera_a)==0);
    const auto first=scopes.enter(camera_a,true);
    assert(first!=0&&first!=ViewScopes2::overflow_ticket);
    assert(scopes.current(camera_a)==first&&scopes.current(camera_b)==0&&
           scopes.current(0)==0);
    scopes.leave(camera_b,0); // A disabled source cannot disturb an active scope.
    assert(scopes.current(camera_a)==first&&!scopes.faulted());
    scopes.leave(camera_a,first);
    assert(scopes.current(camera_a)==0&&!scopes.faulted());
    const auto second=scopes.enter(camera_a,true);
    assert(second!=first&&scopes.current(camera_a)==second);
    const auto nested=scopes.enter(camera_a,true);
    assert(nested!=second&&nested!=ViewScopes2::overflow_ticket);
    assert(scopes.current(camera_a)==0); // Same-camera recursion invalidates parent.
    scopes.leave(camera_a,nested);
    assert(scopes.current(camera_a)==0); // Parent never resumes in this view.
    scopes.leave(camera_a,second);
    const auto third=scopes.enter(camera_a,true);
    assert(third!=second&&scopes.current(camera_a)==third);
    scopes.leave(camera_a,third);

    const auto unowned=scopes.enter(camera_a,false);
    assert(unowned!=0&&scopes.current(camera_a)==0);
    scopes.leave(camera_a,unowned);
    const auto null_camera=scopes.enter(0,true);
    assert(null_camera!=0&&scopes.current(0)==0&&scopes.current(camera_a)==0);
    scopes.leave(0,null_camera);
    const auto parent=scopes.enter(camera_a,true);
    const auto unowned_child=scopes.enter(camera_b,false);
    assert(parent!=0&&unowned_child!=0&&scopes.current(camera_a)==0&&
           scopes.current(camera_b)==0);
    scopes.leave(camera_b,unowned_child);
    assert(scopes.current(camera_a)==0);
    scopes.leave(camera_a,parent);

    std::array<ViewScopes2::Ticket,16> tickets{};
    tickets[0]=scopes.enter(camera_a,true);
    assert(scopes.current(camera_a)==tickets[0]);
    for(unsigned i=1;i<tickets.size();++i){
        tickets[i]=scopes.enter(camera_b,true);
        assert(tickets[i]!=0&&tickets[i]!=ViewScopes2::overflow_ticket&&
               tickets[i]!=tickets[i-1]&&scopes.current(camera_b)==0);
    }
    const auto overflow_one=scopes.enter(camera_a,true);
    const auto overflow_two=scopes.enter(camera_b,false);
    assert(overflow_one==ViewScopes2::overflow_ticket&&
           overflow_two==ViewScopes2::overflow_ticket&&
           scopes.current(camera_a)==0&&!scopes.faulted());
    scopes.leave(camera_b,overflow_two);
    scopes.leave(camera_a,overflow_one);
    assert(!scopes.faulted()&&scopes.current(camera_a)==0);
    for(unsigned i=tickets.size();i-->1;)scopes.leave(camera_b,tickets[i]);
    assert(scopes.current(camera_a)==0); // Overflow also permanently invalidated root.
    scopes.leave(camera_a,tickets[0]);
    const auto after_overflow=scopes.enter(camera_b,true);
    assert(after_overflow!=0&&after_overflow!=ViewScopes2::overflow_ticket&&
           after_overflow!=tickets[0]&&scopes.current(camera_b)==after_overflow);
    scopes.leave(camera_b,after_overflow);

    ViewScopes2 wrong_camera;
    const auto wc=wrong_camera.enter(camera_a,true);
    wrong_camera.leave(camera_b,wc);
    assert(wrong_camera.faulted()&&wrong_camera.current(camera_a)==0&&
           wrong_camera.enter(camera_a,true)==0);
    wrong_camera.leave(camera_a,wc);
    assert(wrong_camera.faulted());

    ViewScopes2 out_of_order;
    const auto outer=out_of_order.enter(camera_a,true);
    const auto inner=out_of_order.enter(camera_b,true);
    assert(outer&&inner&&!out_of_order.faulted());
    out_of_order.leave(camera_a,outer);
    assert(out_of_order.faulted()&&out_of_order.current(camera_a)==0&&
           out_of_order.enter(camera_b,true)==0);
    out_of_order.leave(camera_b,inner);
    assert(out_of_order.faulted());

    ViewScopes2 bad_sentinel;
    bad_sentinel.leave(camera_a,ViewScopes2::overflow_ticket);
    assert(bad_sentinel.faulted()&&bad_sentinel.enter(camera_a,true)==0);
    std::puts("PASS ViewScopes2 ownership, nesting, overflow and fault guards");
}
