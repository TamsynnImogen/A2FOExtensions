#include "directional_shields.hpp"
#include "../../rust/shields_ffi.h"

namespace a2fo::directional_shields {
namespace {
std::uint32_t mode_value(Mode mode) noexcept {
    return static_cast<std::uint32_t>(mode);
}
std::uint32_t facing_value(Facing facing) noexcept {
    return static_cast<std::uint32_t>(facing);
}
bool selected(bool local, Mode mode, const Matrix34& transform,
              const float* attacker, Facing* facing) noexcept {
    if (!facing) return false;
    std::uint32_t value = 0;
    const bool okay = local
        ? a2fo_rs_shields_select_local(mode_value(mode), transform.values, &value) != 0
        : a2fo_rs_shields_select(mode_value(mode), transform.values, attacker, &value) != 0;
    if (okay) *facing = static_cast<Facing>(value);
    return okay;
}
}  // namespace

std::size_t facing_index(Facing facing) noexcept {
    return a2fo_rs_shields_index(facing_value(facing));
}
bool facing_active(Mode mode, Facing facing) noexcept {
    return a2fo_rs_shields_active(mode_value(mode), facing_value(facing)) != 0;
}
ShieldPolicy normalize_policy(ShieldPolicy policy) noexcept {
    a2fo_rs_shields_normalize(mode_value(policy.mode), policy.maximum.data());
    return policy;
}
bool valid_policy(const ShieldPolicy& policy) noexcept {
    return a2fo_rs_shields_valid(mode_value(policy.mode), policy.maximum.data()) != 0;
}
float total_capacity(const ShieldPolicy& policy) noexcept {
    return a2fo_rs_shields_total(policy.maximum.data());
}
float total_current(const ShieldStores& stores) noexcept {
    return a2fo_rs_shields_total(stores.current.data());
}
float effect_aggregate_for_facing(const ShieldPolicy& policy,
                                 const ShieldStores& stores,
                                 Facing facing) noexcept {
    return a2fo_rs_shields_aggregate(mode_value(policy.mode), policy.maximum.data(),
                                    stores.current.data(), facing_value(facing));
}
bool should_clear_native_collapse_effect(float remaining, std::int32_t effect) noexcept {
    return a2fo_rs_shields_clear(remaining, effect) != 0;
}
bool should_suppress_native_impact_effect(std::int32_t type, bool resolved,
                                         float remaining) noexcept {
    return a2fo_rs_shields_suppress(type, resolved ? 1 : 0, remaining) != 0;
}
bool select_facing(Mode mode, const Matrix34& transform,
                   const float attacker[3], Facing* facing) noexcept {
    return selected(false, mode, transform, attacker, facing);
}
bool select_facing(const Matrix34& transform, const float attacker[3],
                   Facing* facing) noexcept {
    return select_facing(Mode::quad, transform, attacker, facing);
}
bool select_local_effect_facing(Mode mode, const Matrix34& transform,
                                Facing* facing) noexcept {
    return selected(true, mode, transform, nullptr, facing);
}
bool select_local_effect_facing(const Matrix34& transform, Facing* facing) noexcept {
    return select_local_effect_facing(Mode::quad, transform, facing);
}
void initialize_from_total(ShieldStores* stores, const ShieldPolicy& policy,
                           float native_total) noexcept {
    if (stores) a2fo_rs_shields_initialize(mode_value(policy.mode), policy.maximum.data(),
                                          stores->current.data(), native_total);
}
void reconcile_total(ShieldStores* stores, const ShieldPolicy& policy,
                     float native_total) noexcept {
    if (stores) a2fo_rs_shields_reconcile(mode_value(policy.mode), policy.maximum.data(),
                                         stores->current.data(), native_total);
}
}  // namespace a2fo::directional_shields
