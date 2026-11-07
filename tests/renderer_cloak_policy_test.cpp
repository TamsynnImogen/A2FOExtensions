#include "../core/renderer_cloak_policy.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

int main() {
    using namespace a2fo::cloak;
    unsigned errors = 0;
    auto check = [&](bool ok) { if (!ok) ++errors; };
    float value = -1;
    check(!visibility(0, 1, value) && value == -1);
    check(!visibility(4, 1, value));
    check(!visibility(1, std::numeric_limits<float>::quiet_NaN(), value));
    check(!visibility(3, std::numeric_limits<float>::infinity(), value));
    for (unsigned state : {1u, 3u}) {
        check(visibility(state, 0.2f, value) && value == 0);
        check(visibility(state, 0.6f, value) && std::fabs(value - 0.5f) < 1e-6f);
        check(visibility(state, 1, value) && value == 1);
        check(visibility(state, -10, value) && value == 0);
        check(visibility(state, 10, value) && value == 1);
    }
    check(visibility(2, 1, value) && value == 0);
    // Each context flag is independently necessary. Visible/unknown states,
    // authored alpha and additive effects never enter the replacement draw.
    for (unsigned flags = 0; flags < 16; ++flags)
        for (unsigned state = 0; state < 5; ++state)
            for (unsigned blend = 0; blend < 12; ++blend)
                check(eligible(flags & 1, flags & 2, flags & 4, flags & 8, state, blend) ==
                      (flags == 15 && state >= 1 && state <= 3 && blend == 1));
    if (errors) { std::printf("FAIL: cloak policy (%u)\n", errors); return 1; }
    std::puts("PASS: cloak progression and native draw admission");
}
