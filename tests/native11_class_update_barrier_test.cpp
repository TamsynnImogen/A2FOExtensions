#include "../renderer/native11/class_update_barrier.hpp"
#include <cstdio>
#include <cstdlib>
#include <set>

using a2fo::native11::ClassUpdateBarrier;
namespace {
void need(bool value, const char* why) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", why); std::exit(1); }
}
}

int main() {
    ClassUpdateBarrier barrier;
    need(barrier.quiescent() && !barrier.failed() && barrier.active_count() == 0,
         "initially quiescent");
    const auto outer = barrier.begin();
    const auto inner = barrier.begin();
    need(outer != 0 && inner > outer && barrier.active(outer) && barrier.active(inner) &&
             barrier.active_count() == 2 && !barrier.quiescent(),
         "nested scopes have distinct active tickets");
    need(barrier.finish(outer, true) && !barrier.active(outer) && barrier.active(inner) &&
             barrier.active_count() == 1 && !barrier.quiescent(),
         "out-of-order completion retains inner scope");
    need(!barrier.finish(0, false) && !barrier.finish(outer, false) && !barrier.failed(),
         "invalid and finished tickets have no effect");
    need(barrier.finish(inner, true) && barrier.quiescent(), "all completed scopes quiesce");

    const auto aborted = barrier.begin();
    need(barrier.finish(aborted, false) && barrier.failed() && !barrier.quiescent() &&
             barrier.begin() == 0,
         "abort faults the barrier and rejects new scopes");
    barrier.clear();
    need(barrier.quiescent() && !barrier.failed(), "clear resets the abort fault");

    std::set<ClassUpdateBarrier::Ticket> seen{outer, inner, aborted};
    for (int round = 0; round != 3; ++round) {
        const auto stale = barrier.begin();
        need(stale != 0 && seen.insert(stale).second, "tickets stay unique across clears");
        barrier.clear();
        const auto current = barrier.begin();
        need(current != 0 && seen.insert(current).second && barrier.active(current),
             "new scope survives clear");
        need(!barrier.finish(stale, false) && !barrier.active(stale) &&
                 barrier.active(current) && barrier.active_count() == 1 && !barrier.failed(),
             "stale abort cannot release or fault new scope");
        need(barrier.finish(current, true) && barrier.quiescent(),
             "current scope completes after stale finish");
    }

    std::set<ClassUpdateBarrier::Ticket> full;
    for (std::size_t i = 0; i != ClassUpdateBarrier::max_active; ++i) {
        const auto ticket = barrier.begin();
        need(ticket != 0 && full.insert(ticket).second, "capacity accepts each allowed scope");
    }
    need(barrier.active_count() == ClassUpdateBarrier::max_active && !barrier.quiescent(),
         "capacity is active");
    need(barrier.begin() == 0 && barrier.failed() &&
             barrier.active_count() == ClassUpdateBarrier::max_active,
         "capacity overflow faults without adding a ticket");
    for (const auto ticket : full)
        need(barrier.finish(ticket, true), "valid tickets finish while faulted");
    need(barrier.active_count() == 0 && barrier.failed() && !barrier.quiescent(),
         "fault persists after draining active scopes");
    barrier.clear();
    need(barrier.quiescent(), "clear recovers capacity fault");
    const auto after_capacity = barrier.begin();
    need(after_capacity != 0 && !full.count(after_capacity) &&
             barrier.finish(after_capacity, true) && barrier.quiescent(),
         "new ticket follows capacity recovery");
    std::puts("native11 class update barrier: PASS");
}
