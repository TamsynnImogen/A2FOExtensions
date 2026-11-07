#include "../renderer/native11/animation_phase.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using a2fo::native11::AnimationPhase;
using a2fo::native11::AnimationPhaseInput;
using a2fo::native11::evaluate_animation_phase;

namespace {
void need(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

unsigned short control_word() {
    unsigned short value = 0;
    asm volatile("fnstcw %0" : "=m"(value));
    return value;
}

void set_control_word(unsigned short value) {
    asm volatile("fldcw %0" :: "m"(value) : "memory");
}

bool close(float actual, float expected) {
    return std::fabs(actual - expected) <= 1.0e-6f;
}

void expect_phase(const AnimationPhaseInput& input, std::uint32_t key,
                  float fraction, bool finished, const char* message) {
    AnimationPhase output{99, 99.0f, false};
    std::string error = "stale error";
    need(evaluate_animation_phase(input, output, error), message);
    need(error.empty(), "success clears prior error");
    need(output.key == key && close(output.fraction, fraction) &&
             output.finished == finished,
         message);
}

void expect_failure(const AnimationPhaseInput& input, const char* message) {
    AnimationPhase output{17, 2.5f, true};
    std::string error;
    need(!evaluate_animation_phase(input, output, error), message);
    need(output.key == 17 && output.fraction == 2.5f && output.finished,
         "failure leaves output unchanged");
    need(!error.empty(), "failure reports error");
}

void run_cases() {
    AnimationPhaseInput forward;
    forward.duration = 1.0f;
    forward.flags = 0;
    forward.key_count = 3;
    forward.elapsed = 1.0f;
    expect_phase(forward, 2, -1.0f, true, "forward nonloop endpoint");

    AnimationPhaseInput reverse = forward;
    reverse.flags = 4;
    expect_phase(reverse, 0, -1.0f, true, "reverse completed endpoint");

    AnimationPhaseInput loop;
    loop.duration = 1.0f;
    loop.flags = 1;
    loop.key_count = 2;
    loop.timestamps = {0.0f, 0.5f};
    loop.elapsed = 1.25f;
    expect_phase(loop, 0, 0.5f, false, "loop wraps into first interval");

    AnimationPhaseInput explicit_times;
    explicit_times.duration = 1.0f;
    explicit_times.flags = 1;
    explicit_times.key_count = 3;
    explicit_times.timestamps = {0.0f, 0.25f, 0.75f};
    explicit_times.elapsed = 0.25f;
    expect_phase(explicit_times, 1, 0.0f, false,
                 "explicit timestamp exact tie selects lower key");
    explicit_times.elapsed = 0.5f;
    expect_phase(explicit_times, 1, 0.5f, false,
                 "explicit timestamp interpolation");

    AnimationPhaseInput uniform;
    uniform.duration = 1.0f;
    uniform.flags = 1;
    uniform.key_count = 2;
    uniform.elapsed = 0.51f;
    AnimationPhase uniform_result;
    std::string error;
    need(evaluate_animation_phase(uniform, uniform_result, error),
         "uniform native selection bias accepts fraction beyond one");
    need(uniform_result.key == 0 && uniform_result.fraction > 1.0f,
         "uniform native fraction remains unclamped");

    AnimationPhaseInput invalid;
    invalid.duration = 1.0f;
    invalid.key_count = 1;
    invalid.elapsed = std::numeric_limits<float>::quiet_NaN();
    expect_failure(invalid, "NaN elapsed rejected");
    invalid.elapsed = -0.01f;
    expect_failure(invalid, "negative elapsed rejected");
    invalid.elapsed = 0.0f;
    invalid.duration = 0.0f;
    expect_failure(invalid, "zero duration rejected");
    invalid.duration = 1.0f;
    invalid.key_count = 65536;
    expect_failure(invalid, "excessive key count rejected");

    invalid.key_count = 2;
    invalid.timestamps = {0.1f, 0.5f};
    expect_failure(invalid, "nonzero first timestamp rejected");
    invalid.timestamps = {0.0f, 0.0f};
    expect_failure(invalid, "duplicate timestamps rejected");
    invalid.timestamps = {0.0f, 1.0f};
    expect_failure(invalid, "timestamp at duration rejected");
    invalid.timestamps = {0.0f};
    expect_failure(invalid, "timestamp length mismatch rejected");

    AnimationPhaseInput overflow;
    overflow.duration = 1.0f;
    overflow.flags = 1;
    overflow.key_count = 1;
    overflow.elapsed = 2147483648.0f;
    expect_failure(overflow, "cycle count outside signed32 rejected");
}
} // namespace

int main() {
    const unsigned short original = control_word();
    for (unsigned rounding = 0; rounding != 4; ++rounding) {
        const auto selected = static_cast<unsigned short>(
            (original & ~0x0c00u) | (rounding << 10));
        set_control_word(selected);
        run_cases();
        need(control_word() == selected, "x87 control word restored after success/failure");
    }
    set_control_word(original);
    std::puts("PASS Native11 animation phase public contract cases");
    return 0;
}
