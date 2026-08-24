#include "energy_systems.hpp"

#include <cassert>
#include <cmath>

using a2fo::energy_systems::RechargeMode;
using a2fo::energy_systems::StorePolicy;

int main() {
    StorePolicy automatic{10.0f, 2.0f, RechargeMode::automatic};
    assert(std::fabs(a2fo::energy_systems::recharge(
                         4.0f, automatic, 1.5f, false) - 7.0f) < 0.001f);
    assert(a2fo::energy_systems::recharge(
               9.0f, automatic, 2.0f, false) == 10.0f);

    StorePolicy docked{12.0f, 4.0f, RechargeMode::resupply_only};
    assert(a2fo::energy_systems::store_enabled(automatic));
    assert(!a2fo::energy_systems::requires_resupply(automatic));
    assert(a2fo::energy_systems::store_enabled(docked));
    assert(a2fo::energy_systems::requires_resupply(docked));
    assert(!a2fo::energy_systems::store_enabled(StorePolicy{}));
    assert(!a2fo::energy_systems::requires_resupply(StorePolicy{}));
    assert(a2fo::energy_systems::recharge(
               3.0f, docked, 1.0f, false) == 3.0f);
    assert(a2fo::energy_systems::recharge(
               3.0f, docked, 1.0f, true) == 7.0f);
    assert(std::fabs(a2fo::energy_systems::reload_seconds(
                         4.0f, automatic, false) - 3.0f) < 0.001f);
    assert(a2fo::energy_systems::reload_seconds(
               10.0f, automatic, false) == 0.0f);
    assert(a2fo::energy_systems::reload_seconds(
               3.0f, docked, false) == -1.0f);
    assert(std::fabs(a2fo::energy_systems::reload_seconds(
                         3.0f, docked, true) - 2.25f) < 0.001f);

    assert(a2fo::energy_systems::can_consume(2.0f, 2.0f));
    assert(!a2fo::energy_systems::can_consume(1.0f, 2.0f));
    assert(a2fo::energy_systems::consume(5.0f, 2.0f) == 3.0f);

    // A four-projectile volley charges the declared cost four times, once for
    // each projectile that is actually launched, including pooled reuse.
    float quantum_magazine = 20.0f;
    for (int projectile = 0; projectile < 4; ++projectile) {
        assert(a2fo::energy_systems::can_consume(
            quantum_magazine, 1.0f));
        quantum_magazine = a2fo::energy_systems::consume(
            quantum_magazine, 1.0f);
    }
    assert(quantum_magazine == 16.0f);

    assert(a2fo::energy_systems::clamp_amount(20.0f, automatic) == 10.0f);
    return 0;
}
