#pragma once
#include <cstddef>
#include <cstdint>
#include <set>

namespace a2fo::native11 {
// Externally serialized. Tickets identify individual class-update scopes.
class ClassUpdateBarrier final {
public:
    using Ticket = std::uint64_t;
    static constexpr std::size_t max_active = 128;

    Ticket begin() noexcept;
    bool finish(Ticket ticket, bool completed) noexcept;
    bool active(Ticket ticket) const noexcept;
    bool quiescent() const noexcept;
    bool failed() const noexcept;
    std::size_t active_count() const noexcept;
    void clear() noexcept;

private:
    std::set<Ticket> active_;
    Ticket next_ticket_ = 1;
    bool failed_ = false;
};
} // namespace a2fo::native11
