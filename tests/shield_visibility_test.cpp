#include "shield_visibility.hpp"

#include <cstdio>
#include <limits>

namespace {

using a2fo::shields::EffectAction;

bool expect_action(const char* name, bool configured, float shields,
                   int effect_id, EffectAction expected) {
    const EffectAction actual = a2fo::shields::choose_effect_action(
        configured, shields, effect_id);
    if (actual == expected) return true;
    std::fprintf(stderr, "%s selected action %d instead of %d\n",
                 name, static_cast<int>(actual),
                 static_cast<int>(expected));
    return false;
}

}  // namespace

int main() {
    if (!expect_action("default native behavior", false, 100.0f, -1,
                       EffectAction::none) ||
        !expect_action("show while shields are up", true, 100.0f, -1,
                       EffectAction::show) ||
        !expect_action("keep existing effect", true, 50.0f, 12,
                       EffectAction::none) ||
        !expect_action("hide at zero", true, 0.0f, 12,
                       EffectAction::hide) ||
        !expect_action("do not hide absent effect", true, 0.0f, -1,
                       EffectAction::none) ||
        !expect_action("hide invalid shield state", true,
                       std::numeric_limits<float>::quiet_NaN(), 12,
                       EffectAction::hide)) {
        return 1;
    }
    std::puts("shield visibility tests passed");
    return 0;
}
