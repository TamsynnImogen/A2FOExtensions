#include "fire_arc.hpp"

#include <cstdint>
#include <cstring>

namespace {

struct ArcConfigC {
    std::int32_t mode;
    float yaw_degrees;
    float pitch_degrees;
    float yaw_angle_degrees;
    float pitch_angle_degrees;
    float cone_angle_degrees;
};

a2fo::fire_arcs::ArcConfig to_native(const ArcConfigC& input) noexcept {
    a2fo::fire_arcs::ArcConfig output{};
    output.mode = input.mode == 1
        ? a2fo::fire_arcs::ArcMode::cone
        : a2fo::fire_arcs::ArcMode::box;
    output.yaw_degrees = input.yaw_degrees;
    output.pitch_degrees = input.pitch_degrees;
    output.yaw_angle_degrees = input.yaw_angle_degrees;
    output.pitch_angle_degrees = input.pitch_angle_degrees;
    output.cone_angle_degrees = input.cone_angle_degrees;
    return output;
}

void from_native(const a2fo::fire_arcs::ArcConfig& input,
                 ArcConfigC* output) noexcept {
    if (!output) return;
    output->mode = input.mode == a2fo::fire_arcs::ArcMode::cone ? 1 : 0;
    output->yaw_degrees = input.yaw_degrees;
    output->pitch_degrees = input.pitch_degrees;
    output->yaw_angle_degrees = input.yaw_angle_degrees;
    output->pitch_angle_degrees = input.pitch_angle_degrees;
    output->cone_angle_degrees = input.cone_angle_degrees;
}

}  // namespace

extern "C" void a2fo_arclab_normalize(ArcConfigC* config) noexcept {
    if (!config) return;
    auto native = to_native(*config);
    a2fo::fire_arcs::normalize_config(&native);
    from_native(native, config);
}

extern "C" std::int32_t a2fo_arclab_allows(
    const ArcConfigC* config,
    const float* owner_matrix,
    const float* target_position) noexcept {
    if (!config || !owner_matrix || !target_position) return 0;
    auto native = to_native(*config);
    a2fo::fire_arcs::Matrix34 matrix{};
    std::memcpy(matrix.values, owner_matrix, sizeof(matrix.values));
    return a2fo::fire_arcs::allows_target(
        native, matrix, target_position) ? 1 : 0;
}
