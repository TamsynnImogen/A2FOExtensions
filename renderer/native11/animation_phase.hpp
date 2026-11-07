#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
// Owned inputs to the pinned native animation clock policy. This is not a
// per-instance pose evaluator: the caller must supply authoritative timing.
struct AnimationPhaseInput {
    float elapsed=0,offset=0,duration=0;
    std::uint32_t flags=3,key_count=0; // bit0 loops; bit2 reverses
    std::vector<float> timestamps; // empty: native uniform-key policy
};
struct AnimationPhase {
    std::uint32_t key=0;
    float fraction=0; // native interpolation parameter; deliberately unclamped
    bool finished=false; // nonloop endpoint uses native -1 sentinel fraction
};
// Supported subset: finite nonnegative time/offset, positive duration, 1..65535
// keys, optional strictly increasing times starting at zero and below duration.
// Loop cycle counts must fit signed32. Rejects unsupported/nonfinite results.
// Failure leaves output unchanged; x87 control word is restored on every path.
bool evaluate_animation_phase(const AnimationPhaseInput&,AnimationPhase&,
                              std::string& error) noexcept;
}
