#include "energy_systems.hpp"

#include <algorithm>
#include <cmath>

namespace a2fo::energy_systems {

StorePolicy normalize_policy(StorePolicy policy) noexcept {
    if (!std::isfinite(policy.maximum) || policy.maximum < 0.0f) {
        policy.maximum = 0.0f;
    }
    if (!std::isfinite(policy.recharge_per_second) ||
        policy.recharge_per_second < 0.0f) {
        policy.recharge_per_second = 0.0f;
    }
    if (policy.mode != RechargeMode::none &&
        policy.mode != RechargeMode::automatic &&
        policy.mode != RechargeMode::resupply_only) {
        policy.mode = RechargeMode::automatic;
    }
    return policy;
}

bool store_enabled(const StorePolicy& policy) noexcept {
    return std::isfinite(policy.maximum) && policy.maximum > 0.0f;
}

bool requires_resupply(const StorePolicy& policy) noexcept {
    return store_enabled(policy) &&
        policy.mode == RechargeMode::resupply_only;
}

float clamp_amount(float amount, const StorePolicy& unnormalized) noexcept {
    const StorePolicy policy = normalize_policy(unnormalized);
    if (!std::isfinite(amount)) return 0.0f;
    return std::clamp(amount, 0.0f, policy.maximum);
}

bool can_consume(float amount, float cost) noexcept {
    if (!std::isfinite(amount) || !std::isfinite(cost) || cost < 0.0f) {
        return false;
    }
    return amount + 0.0001f >= cost;
}

float consume(float amount, float cost) noexcept {
    if (!std::isfinite(amount)) return 0.0f;
    if (!can_consume(amount, cost)) return std::max(0.0f, amount);
    return std::max(0.0f, amount - cost);
}

float recharge(float amount, const StorePolicy& unnormalized,
               float elapsed_seconds, bool in_resupply_range) noexcept {
    const StorePolicy policy = normalize_policy(unnormalized);
    amount = clamp_amount(amount, policy);
    if (!std::isfinite(elapsed_seconds) || elapsed_seconds <= 0.0f ||
        policy.recharge_per_second <= 0.0f ||
        policy.mode == RechargeMode::none ||
        (policy.mode == RechargeMode::resupply_only &&
         !in_resupply_range)) {
        return amount;
    }
    return std::min(
        policy.maximum,
        amount + policy.recharge_per_second * elapsed_seconds);
}

float reload_seconds(float amount, const StorePolicy& unnormalized,
                     bool in_resupply_range) noexcept {
    const StorePolicy policy = normalize_policy(unnormalized);
    amount = clamp_amount(amount, policy);
    if (amount + 0.0001f >= policy.maximum) return 0.0f;
    if (policy.recharge_per_second <= 0.0f ||
        policy.mode == RechargeMode::none ||
        (policy.mode == RechargeMode::resupply_only &&
         !in_resupply_range)) {
        return -1.0f;
    }
    return (policy.maximum - amount) / policy.recharge_per_second;
}

}  // namespace a2fo::energy_systems
