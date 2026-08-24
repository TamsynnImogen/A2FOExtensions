/* Pure transform interpolation for the refit-yard docking transition. */

#pragma once

#include <algorithm>
#include <cmath>

namespace a2fo::refit {

struct DockingTransform {
    float values[12]{};
};

inline DockingTransform docking_approach_transform(
    const DockingTransform& hardpoint, float clearance) noexcept {
    DockingTransform result = hardpoint;
    clearance = std::max(clearance, 0.0f);

    // Armada's OrientedQueueManager backs away from a dock hardpoint along
    // its negative forward axis until the queue entry lies outside the host
    // object's collision sphere. Mirror that convention here so ordinary GO
    // pathfinding brings the refit source to the same side of the yard before
    // the short controlled docking transition begins.
    float forward_x = hardpoint.values[6];
    float forward_y = hardpoint.values[7];
    float forward_z = hardpoint.values[8];
    const float length_squared = forward_x * forward_x +
        forward_y * forward_y + forward_z * forward_z;
    if (!std::isfinite(length_squared) || length_squared <= 0.000001f) {
        forward_x = 0.0f;
        forward_y = 0.0f;
        forward_z = 1.0f;
    } else {
        const float inverse_length = 1.0f / std::sqrt(length_squared);
        forward_x *= inverse_length;
        forward_y *= inverse_length;
        forward_z *= inverse_length;
    }
    result.values[9] -= forward_x * clearance;
    result.values[10] -= forward_y * clearance;
    result.values[11] -= forward_z * clearance;
    return result;
}

namespace detail {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

inline Quaternion normalize(Quaternion value) noexcept {
    const float length_squared = value.x * value.x + value.y * value.y +
        value.z * value.z + value.w * value.w;
    if (!std::isfinite(length_squared) || length_squared <= 0.000001f) {
        return {};
    }
    const float inverse_length = 1.0f / std::sqrt(length_squared);
    value.x *= inverse_length;
    value.y *= inverse_length;
    value.z *= inverse_length;
    value.w *= inverse_length;
    return value;
}

inline Quaternion orientation_quaternion(
    const DockingTransform& transform) noexcept {
    // Matrix34 stores its right, up, and forward basis vectors contiguously,
    // making them the three columns of this conventional rotation matrix.
    const float m00 = transform.values[0];
    const float m10 = transform.values[1];
    const float m20 = transform.values[2];
    const float m01 = transform.values[3];
    const float m11 = transform.values[4];
    const float m21 = transform.values[5];
    const float m02 = transform.values[6];
    const float m12 = transform.values[7];
    const float m22 = transform.values[8];
    Quaternion result{};
    const float trace = m00 + m11 + m22;
    if (trace > 0.0f) {
        const float scale = std::sqrt(trace + 1.0f) * 2.0f;
        result.w = 0.25f * scale;
        result.x = (m21 - m12) / scale;
        result.y = (m02 - m20) / scale;
        result.z = (m10 - m01) / scale;
    } else if (m00 > m11 && m00 > m22) {
        const float scale = std::sqrt(
            std::max(0.0f, 1.0f + m00 - m11 - m22)) * 2.0f;
        if (scale <= 0.000001f) return {};
        result.w = (m21 - m12) / scale;
        result.x = 0.25f * scale;
        result.y = (m01 + m10) / scale;
        result.z = (m02 + m20) / scale;
    } else if (m11 > m22) {
        const float scale = std::sqrt(
            std::max(0.0f, 1.0f + m11 - m00 - m22)) * 2.0f;
        if (scale <= 0.000001f) return {};
        result.w = (m02 - m20) / scale;
        result.x = (m01 + m10) / scale;
        result.y = 0.25f * scale;
        result.z = (m12 + m21) / scale;
    } else {
        const float scale = std::sqrt(
            std::max(0.0f, 1.0f + m22 - m00 - m11)) * 2.0f;
        if (scale <= 0.000001f) return {};
        result.w = (m10 - m01) / scale;
        result.x = (m02 + m20) / scale;
        result.y = (m12 + m21) / scale;
        result.z = 0.25f * scale;
    }
    return normalize(result);
}

inline void set_orientation(DockingTransform* transform,
                            Quaternion orientation) noexcept {
    if (!transform) return;
    orientation = normalize(orientation);
    const float xx = orientation.x * orientation.x;
    const float yy = orientation.y * orientation.y;
    const float zz = orientation.z * orientation.z;
    const float xy = orientation.x * orientation.y;
    const float xz = orientation.x * orientation.z;
    const float yz = orientation.y * orientation.z;
    const float xw = orientation.x * orientation.w;
    const float yw = orientation.y * orientation.w;
    const float zw = orientation.z * orientation.w;

    transform->values[0] = 1.0f - 2.0f * (yy + zz);
    transform->values[1] = 2.0f * (xy + zw);
    transform->values[2] = 2.0f * (xz - yw);
    transform->values[3] = 2.0f * (xy - zw);
    transform->values[4] = 1.0f - 2.0f * (xx + zz);
    transform->values[5] = 2.0f * (yz + xw);
    transform->values[6] = 2.0f * (xz + yw);
    transform->values[7] = 2.0f * (yz - xw);
    transform->values[8] = 1.0f - 2.0f * (xx + yy);
}

}  // namespace detail

inline DockingTransform interpolate_docking_transform(
    const DockingTransform& origin, const DockingTransform& destination,
    float progress) noexcept {
    progress = std::clamp(progress, 0.0f, 1.0f);
    // Smoothstep gives a gentle departure and arrival without extending the
    // synchronized transition beyond its fixed simulation-step budget.
    const float blend = progress * progress * (3.0f - 2.0f * progress);
    detail::Quaternion from = detail::orientation_quaternion(origin);
    detail::Quaternion to = detail::orientation_quaternion(destination);
    const float dot = from.x * to.x + from.y * to.y +
        from.z * to.z + from.w * to.w;
    if (dot < 0.0f) {
        to.x = -to.x;
        to.y = -to.y;
        to.z = -to.z;
        to.w = -to.w;
    }
    detail::Quaternion orientation{
        from.x + (to.x - from.x) * blend,
        from.y + (to.y - from.y) * blend,
        from.z + (to.z - from.z) * blend,
        from.w + (to.w - from.w) * blend,
    };

    DockingTransform result{};
    detail::set_orientation(&result, orientation);
    for (int index = 9; index < 12; ++index) {
        result.values[index] = origin.values[index] +
            (destination.values[index] - origin.values[index]) * blend;
    }
    return result;
}

}  // namespace a2fo::refit
