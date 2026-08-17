/* Host-testable RTS_CFG.h parser for extended upgrade-pod tiers. */

#pragma once

#include <cstdint>
#include <string_view>

namespace a2fo::upgrade_pods {

enum class MaximumTierSettingStatus {
    absent,
    valid,
    invalid,
};

// Reads the last `upgradePodMaximumTier = N;` assignment in one C-style
// configuration file. Comments and an optional declaration prefix are
// accepted. Valid tiers are 3 through 16.
MaximumTierSettingStatus parse_maximum_tier_setting(
    std::string_view source, std::uint32_t* maximum_tier);

}  // namespace a2fo::upgrade_pods
