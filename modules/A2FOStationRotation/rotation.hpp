#pragma once

#include <cstdint>

namespace a2fo::station_rotation {

constexpr std::uint32_t kBuildCommand = 0x19;
// Reserved A2FO commands in the native typed-class + position packet (0x0c).
// These are decoded before Armada installs an AiCommand on an object.
constexpr std::uint32_t kFirstRotatedBuildCommand = 0xb1;
constexpr std::uint32_t kRotationParameterTag = 0x52393000; // "R90", low byte = turns

constexpr unsigned decode_wire_turns(std::uint32_t command) noexcept {
    return command >= kFirstRotatedBuildCommand &&
                   command < kFirstRotatedBuildCommand + 3
        ? command - kFirstRotatedBuildCommand + 1 : 0;
}

constexpr std::uint32_t encode_build(unsigned turns) noexcept {
    return (turns & 3) ? kFirstRotatedBuildCommand + (turns & 3) - 1
                       : kBuildCommand;
}

constexpr std::uint32_t rotation_parameter(unsigned turns) noexcept {
    return (turns & 3) ? kRotationParameterTag | (turns & 3) : 0;
}

constexpr unsigned parameter_turns(std::uint32_t parameter) noexcept {
    return (parameter & ~3u) == kRotationParameterTag ? parameter & 3 : 0;
}

// Armada Matrix34 stores right, up, forward, translation, in that order.
// Assign exact cardinal bases: no cumulative trigonometric/rounding drift.
inline void set_yaw(float* matrix, unsigned turns) noexcept {
    constexpr float bases[4][9] = {
        { 1, 0, 0, 0, 1, 0,  0, 0, 1},
        { 0, 0,-1, 0, 1, 0,  1, 0, 0},
        {-1, 0, 0, 0, 1, 0,  0, 0,-1},
        { 0, 0, 1, 0, 1, 0, -1, 0, 0},
    };
    for (unsigned i = 0; i != 9; ++i) matrix[i] = bases[turns & 3][i];
}

struct PlacementControl {
    std::uintptr_t target = 0;
    unsigned turns = 0;
    bool r_down = false;

    void observe(std::uintptr_t next_target) noexcept {
        if (target != next_target) {
            target = next_target;
            turns = 0;
        }
    }

    // One turn per fresh press. Track held keys across chat, focus and target
    // changes, so returning to placement cannot manufacture another press.
    // Return true while R is reserved, including held frames, to prevent Repair.
    bool key(std::uintptr_t next_target, bool allowed, bool down, bool reverse) noexcept {
        const bool pressed = down && !r_down;
        r_down = down;
        observe(next_target);
        if (!target || !allowed) return false;
        if (pressed) turns = (turns + (reverse ? 3u : 1u)) & 3;
        return down;
    }
};

} // namespace a2fo::station_rotation
