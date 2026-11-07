#include "upgrade_pod_config.hpp"
#include "../../rust/config_ffi.h"
namespace a2fo::upgrade_pods {
MaximumTierSettingStatus parse_maximum_tier_setting(std::string_view source,
                                                    std::uint32_t *maximum_tier) {
    return static_cast<MaximumTierSettingStatus>(a2fo_rs_config_upgrade(
        {reinterpret_cast<const uint8_t *>(source.data()), source.size()}, maximum_tier));
}
} // namespace a2fo::upgrade_pods
