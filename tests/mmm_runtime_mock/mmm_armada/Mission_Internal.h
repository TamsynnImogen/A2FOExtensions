#pragma once

namespace mmm_test {
void event(const char*);
extern bool pending_finish;
}
namespace mmm {
inline void mission_reset_pending_finish() {
    mmm_test::event("mission:reset");
    mmm_test::pending_finish = false;
}
inline bool mission_process_pending_finish() {
    mmm_test::event("mission:pending");
    const bool result = mmm_test::pending_finish;
    mmm_test::pending_finish = false;
    return result;
}
}
