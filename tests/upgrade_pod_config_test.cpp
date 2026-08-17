#include "upgrade_pod_config.hpp"

#include <cassert>
#include <cstdint>

int main() {
    using a2fo::upgrade_pods::MaximumTierSettingStatus;
    using a2fo::upgrade_pods::parse_maximum_tier_setting;

    std::uint32_t tier = 6;
    assert(parse_maximum_tier_setting("int other = 4;", &tier) ==
           MaximumTierSettingStatus::absent);
    assert(tier == 6);

    assert(parse_maximum_tier_setting(
               "// inherited\nint upgradePodMaximumTier = 8;", &tier) ==
           MaximumTierSettingStatus::valid);
    assert(tier == 8);

    assert(parse_maximum_tier_setting(
               "upgradePodMaximumTier = 7; /* child */\n"
               "upgradePodMaximumTier = 12;", &tier) ==
           MaximumTierSettingStatus::valid);
    assert(tier == 12);

    assert(parse_maximum_tier_setting(
               "upgradePodMaximumTier = 17;", &tier) ==
           MaximumTierSettingStatus::invalid);
    assert(tier == 12);
    return 0;
}
