#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo::build_tooltips {

// Armada's native getter applies the global IA modifier conditionally by
// class metadata. Reconcile a result which is still exactly the raw ODF time,
// while preserving values which already contain native/team adjustments.
float reconcile_global_build_time(float raw_seconds,
                                  float native_adjusted_seconds,
                                  float global_modifier) noexcept;

// Formats build time as another compact token in Armada's native cost row.
// The supplied duration is already adjusted for the local team and game setup.
bool format_build_time_token(const char* separator, float seconds,
                             bool compact,
                             char* output,
                             std::size_t output_size) noexcept;

// Formats one additional-resource token for insertion immediately before
// Armada's native closing cost bracket. Compact form mirrors ButtonText's
// one-glyph resource labels; verbose form retains the full Race-specific name.
bool format_resource_cost_token(const char* separator, const char* name,
                                std::int32_t cost, bool compact,
                                char* output,
                                std::size_t output_size) noexcept;

}  // namespace a2fo::build_tooltips
