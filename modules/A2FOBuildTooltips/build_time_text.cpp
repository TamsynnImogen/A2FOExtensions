#include "build_time_text.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

namespace a2fo::build_tooltips {

namespace {

constexpr char kBuildTimeIcon[] = "\x8a";

bool nearly_equal(float left, float right) noexcept {
    const float scale = std::fmax(1.0f,
                                  std::fmax(std::fabs(left), std::fabs(right)));
    return std::fabs(left - right) <= scale * 0.001f;
}

}  // namespace

float reconcile_global_build_time(float raw_seconds,
                                  float native_adjusted_seconds,
                                  float global_modifier) noexcept {
    if (!std::isfinite(native_adjusted_seconds) ||
        native_adjusted_seconds < 0.0f) {
        return native_adjusted_seconds;
    }
    if (!std::isfinite(raw_seconds) || raw_seconds < 0.0f ||
        !std::isfinite(global_modifier) || global_modifier < 0.0f) {
        return native_adjusted_seconds;
    }

    const float explicitly_adjusted = raw_seconds * global_modifier;
    if (!std::isfinite(explicitly_adjusted)) return native_adjusted_seconds;

    // If Armada already returned raw * global modifier (possibly within float
    // rounding), retain it. Only repair the unadjusted raw-time case; an
    // unfamiliar third value may contain a team/AI adjustment we must keep.
    if (nearly_equal(native_adjusted_seconds, explicitly_adjusted) ||
        !nearly_equal(native_adjusted_seconds, raw_seconds)) {
        return native_adjusted_seconds;
    }
    return explicitly_adjusted;
}

bool format_build_time_token(const char* separator, float seconds,
                             bool compact,
                             char* output,
                             std::size_t output_size) noexcept {
    if (!separator || !output || output_size == 0 ||
        !std::isfinite(seconds) || seconds < 0.0f) {
        return false;
    }

    const double rounded = std::floor(static_cast<double>(seconds) + 0.5);
    if (rounded > static_cast<double>(
                      std::numeric_limits<std::uint32_t>::max())) {
        return false;
    }
    const auto whole_seconds = static_cast<std::uint32_t>(rounded);
    const int written = compact
        ? std::snprintf(output, output_size, " %s%u %s", separator,
                        whole_seconds, kBuildTimeIcon)
        : std::snprintf(output, output_size, " %s%u second%s", separator,
                        whole_seconds, whole_seconds == 1 ? "" : "s");
    return written >= 0 &&
           static_cast<std::size_t>(written) < output_size;
}

bool format_resource_cost_token(const char* separator, const char* name,
                                std::int32_t cost, bool compact,
                                char* output,
                                std::size_t output_size) noexcept {
    if (!separator || !name || !*name || cost <= 0 || !output ||
        output_size == 0) {
        return false;
    }
    const int written = std::snprintf(
        output, output_size, " %s%ld %.*s", separator,
        static_cast<long>(cost), compact ? 1 : 0x7fffffff, name);
    return written >= 0 &&
           static_cast<std::size_t>(written) < output_size;
}

}  // namespace a2fo::build_tooltips
