#pragma once
#include "../modules/A2FOAnimations/native_observation.hpp"
namespace a2fo {
// Process-lifetime builtin source; intentionally absent from public exports.
bool builtin_animations_native_playback(HMODULE,animations::NativePlaybackProof&) noexcept;
}
