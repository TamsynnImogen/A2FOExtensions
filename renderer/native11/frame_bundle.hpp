#pragma once

#include "primitive_scene.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {

enum class FrameDomain : std::uint32_t {
    background=1, terrain=2, objects=4, effects=8,
    video=16, hud=32, menu=64
};
constexpr std::uint32_t all_frame_domains=127;

struct FrameIdentity {
    std::uint64_t view=0,generation=0,source_epoch=0,source_ticket=0;
};
struct FramePass {
    FrameDomain domain{};
    std::shared_ptr<const PrimitiveFrame> frame;
};
struct FrameBundle {
    FrameIdentity identity{};
    std::uint32_t width=0,height=0;
    std::uint32_t absent_domains=0;
    bool complete_geometry=false;
    std::vector<FramePass> passes;
};

// Structural admission only. The host supplies the complete-geometry claim;
// this check does not establish that all game producers have been observed.
bool validate_frame_bundle(const FrameBundle&,std::string& error) noexcept;

} // namespace a2fo::native11
