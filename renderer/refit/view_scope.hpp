#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace a2fo::refit {
// Externally serialized by the renderer owner. A nested or unowned view cannot
// inherit its parent's light identity, and a parent is never resumed after it.
class ViewScopes2 {
public:
    using Ticket=std::uint64_t;
    static constexpr Ticket overflow_ticket=std::numeric_limits<Ticket>::max();
    Ticket enter(std::uintptr_t camera,bool owned) noexcept {
        if(faulted_)return 0;
        for(unsigned i=0;i<depth_;++i)stack_[i].usable=false;
        if(serial_==overflow_ticket-1){fault();return 0;}
        const auto serial=++serial_;
        if(overflow_||depth_==stack_.size()){
            if(overflow_==std::numeric_limits<unsigned>::max()){fault();return 0;}
            ++overflow_;return overflow_ticket;
        }
        const bool usable=owned&&camera&&depth_==0;
        stack_[depth_++]={camera,serial,usable};return serial;
    }
    void leave(std::uintptr_t camera,Ticket ticket) noexcept {
        if(faulted_||!ticket)return;
        if(ticket==overflow_ticket){if(overflow_)--overflow_;else fault();return;}
        if(overflow_||!depth_||stack_[depth_-1].ticket!=ticket||stack_[depth_-1].camera!=camera){fault();return;}
        stack_[--depth_]=Scope{};
    }
    Ticket current(std::uintptr_t camera) const noexcept {
        return !faulted_&&!overflow_&&depth_==1&&stack_[0].usable&&stack_[0].camera==camera?
            stack_[0].ticket:0;
    }
    bool faulted() const noexcept{return faulted_;}
private:
    struct Scope {std::uintptr_t camera=0;Ticket ticket=0;bool usable=false;};
    std::array<Scope,16> stack_{};
    unsigned depth_=0,overflow_=0;Ticket serial_=0;bool faulted_=false;
    void fault() noexcept{faulted_=true;depth_=overflow_=0;stack_={};}
};
}
