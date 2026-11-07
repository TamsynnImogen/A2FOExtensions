#include "shield_visibility.hpp"
#include "../../rust/lead_ffi.h"
namespace a2fo::shields {
EffectAction choose_effect_action(bool configured, float current, std::int32_t effect) noexcept {
    return static_cast<EffectAction>(a2fo_rs_lead_shield_action(configured, current, effect));
}
bool global_scan_due(std::uint32_t now, std::uint32_t last, bool scanned,
                     std::uint32_t interval) noexcept {
    return a2fo_rs_lead_scan_due(now, last, scanned, interval) != 0;
}
} // namespace a2fo::shields
