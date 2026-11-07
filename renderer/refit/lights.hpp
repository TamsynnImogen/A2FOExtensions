#pragma once
#include "../native11/world_lighting.hpp"
#include "host.h"
#include <string>
#include <vector>

namespace a2fo::refit {
// Copies Storm-registered lights first, then the ODF world's source-ID order.
// ABI2 carries colour lights only: ODF shadow settings are deliberately not
// represented here, and this adapter makes no shadow-parity claim.
// Failure leaves out unchanged.
bool append_world_lights2(const std::vector<A2FORefitLight>& registered,
                          const native11::LightingFrame& odf,
                          std::vector<A2FORefitLight>& out,
                          std::string& error) noexcept;

// ABI3: as append_world_lights2, but ODF spot sources become A2FORefitSpot3
// records (at most A2FO_REFIT_MAX_SPOTS; full cone degrees converted to radians,
// 0 < inner <= outer < 180, range > 0) instead of being refused. Directional and
// point sources are identical to the ABI2 conversion. Failure leaves both outputs
// unchanged.
bool append_world_lights3(const std::vector<A2FORefitLight>& registered,
                          const native11::LightingFrame& odf,
                          std::vector<A2FORefitLight>& out,
                          std::vector<A2FORefitSpot3>& spots,
                          std::string& error) noexcept;

// Freshness of the full world-light source, including fields ABI2 omits.
bool equal_world_lighting2(const native11::LightingFrame& a,
                           const native11::LightingFrame& b) noexcept;
} // namespace a2fo::refit
