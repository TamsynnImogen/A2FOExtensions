#pragma once
#include "animation_phase.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Owned, CPU-only channel policy. The caller supplies fresh authoritative
// timing, target ownership and visibility separately. Matrix34 elements stay
// in native order; this kernel does not transpose or interpret a transform.
struct ScalarAnimationChannel {
    AnimationPhaseInput timing;
    std::uint32_t interpolation=1;
    std::vector<float> keys;
};
struct MatrixAnimationChannel {
    AnimationPhaseInput timing;
    std::uint32_t interpolation=1;
    std::vector<std::array<float,12>> keys;
};
struct ScalarAnimationSample {
    AnimationPhase phase{};
    bool writes=false;
    float value=0;
};
struct MatrixAnimationSample {
    AnimationPhase phase{};
    bool writes=false;
    std::array<float,12> value{};
};
// Modes: 0 no write, 1 selected key, 2 linear with wrap, 3 linear with an
// explicit scalar guard key. Matrices step in modes 1/2/3. Finished timing
// forces mode 1. The native phase fraction is deliberately not clamped.
// Output changes only on success; no engine object or target is written.
bool evaluate_scalar_animation_channel(const ScalarAnimationChannel&,
    ScalarAnimationSample&,std::string& error) noexcept;
bool evaluate_matrix_animation_channel(const MatrixAnimationChannel&,
    MatrixAnimationSample&,std::string& error) noexcept;
} // namespace a2fo::native11
