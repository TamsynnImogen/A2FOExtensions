#pragma once
#include <windows.h>
#include "../renderer/native11/world_light_capture.hpp"
namespace a2fo::native11_world_lights {
using Log=void(*)(const char*);
struct Snapshot {
    std::uint32_t camera=0;
    std::uint64_t frame_generation=0,class_generation=0;
    std::uint64_t source_lifecycle=0;
    native11::WorldLightCapture captured;
    native11::LightingFrame lighting;
};
// Uses read-only checked layouts. It creates no engine lights or native calls.
bool initialize(HMODULE,bool requested,Log) noexcept;
bool requested() noexcept;
bool enabled() noexcept;
void disable() noexcept;
// Read-only source validity, including lifecycle and class generation. Does not
// turn a historical immutable snapshot into a fresh world-state observation.
bool current(const Snapshot&) noexcept;
bool sample(std::uint32_t camera,std::uint64_t frame_generation,const float ambient[3],
    std::shared_ptr<const Snapshot>& output,std::string& error) noexcept;
}
