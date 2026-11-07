#include "class_update_barrier.hpp"
#include <limits>

namespace a2fo::native11 {
ClassUpdateBarrier::Ticket ClassUpdateBarrier::begin() noexcept {
    if (failed_ || active_.size() >= max_active ||
        next_ticket_ == std::numeric_limits<Ticket>::max()) {
        failed_ = true;
        return 0;
    }
    try {
        const Ticket ticket = next_ticket_;
        active_.insert(ticket);
        ++next_ticket_;
        return ticket;
    } catch (...) {
        failed_ = true;
        return 0;
    }
}

bool ClassUpdateBarrier::finish(Ticket ticket, bool completed) noexcept {
    const auto it = active_.find(ticket);
    if (it == active_.end()) return false;
    active_.erase(it);
    if (!completed) failed_ = true;
    return true;
}

bool ClassUpdateBarrier::active(Ticket ticket) const noexcept {
    return active_.find(ticket) != active_.end();
}
bool ClassUpdateBarrier::quiescent() const noexcept {
    return active_.empty() && !failed_;
}
bool ClassUpdateBarrier::failed() const noexcept { return failed_; }
std::size_t ClassUpdateBarrier::active_count() const noexcept { return active_.size(); }
void ClassUpdateBarrier::clear() noexcept {
    active_.clear();
    failed_ = next_ticket_ == std::numeric_limits<Ticket>::max();
}
} // namespace a2fo::native11
