#pragma once

#include <cstdint>

namespace a2fo::energy_systems {

enum class RechargeMode : std::uint32_t {
    none = 0,
    automatic = 1,
    resupply_only = 2,
};

struct StorePolicy {
    float maximum = 0.0f;
    float recharge_per_second = 0.0f;
    RechargeMode mode = RechargeMode::automatic;
};

struct Stores {
    float photon = 0.0f;
    float quantum = 0.0f;
};

StorePolicy normalize_policy(StorePolicy policy) noexcept;
float clamp_amount(float amount, const StorePolicy& policy) noexcept;
bool can_consume(float amount, float cost) noexcept;
float consume(float amount, float cost) noexcept;
float recharge(float amount, const StorePolicy& policy,
               float elapsed_seconds, bool in_resupply_range) noexcept;
// Returns zero when full, a positive remaining duration while the store can
// recharge, and -1 when recharge is currently unavailable.
float reload_seconds(float amount, const StorePolicy& policy,
                     bool in_resupply_range) noexcept;

}  // namespace a2fo::energy_systems
