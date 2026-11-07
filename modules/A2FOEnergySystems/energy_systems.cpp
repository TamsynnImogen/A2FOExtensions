// Value adapters for the Rust store policy; engine hooks remain in module.cpp.
#include "energy_systems.hpp"
#include "../../rust/energy_ffi.h"

namespace a2fo::energy_systems {
namespace {
A2foEnergyPolicy view(const StorePolicy& p) noexcept {
    return {p.maximum, p.recharge_per_second, static_cast<std::uint32_t>(p.mode)};
}
}
StorePolicy normalize_policy(StorePolicy policy) noexcept {
    const auto input = view(policy);
    A2foEnergyPolicy result{};
    a2fo_rs_energy_normalize(&input, &result);
    return {result.maximum, result.recharge_per_second, static_cast<RechargeMode>(result.mode)};
}
bool store_enabled(const StorePolicy& policy) noexcept {
    const auto p = view(policy); return a2fo_rs_energy_enabled(&p) != 0;
}
bool requires_resupply(const StorePolicy& policy) noexcept {
    const auto p = view(policy); return a2fo_rs_energy_requires_resupply(&p) != 0;
}
float clamp_amount(float amount, const StorePolicy& policy) noexcept {
    const auto p = view(policy); return a2fo_rs_energy_clamp(amount, &p);
}
bool can_consume(float amount, float cost) noexcept { return a2fo_rs_energy_can_consume(amount, cost) != 0; }
float consume(float amount, float cost) noexcept { return a2fo_rs_energy_consume(amount, cost); }
float recharge(float amount, const StorePolicy& policy, float elapsed, bool in_range) noexcept {
    const auto p = view(policy); return a2fo_rs_energy_recharge(amount, &p, elapsed, in_range);
}
float reload_seconds(float amount, const StorePolicy& policy, bool in_range) noexcept {
    const auto p = view(policy); return a2fo_rs_energy_reload(amount, &p, in_range);
}
} // namespace a2fo::energy_systems
