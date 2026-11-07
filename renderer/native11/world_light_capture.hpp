#pragma once
#include "light_class_store.hpp"
#include "scene_capture.hpp"
#include "world_lighting.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace a2fo::native11 {
struct WorldLightReadProfile { std::uint32_t object_list_pointer=0; };
using PinLightClass=std::shared_ptr<const LightClassRecord>(*)(void*,std::uint64_t);
struct WorldMember {
    std::uint32_t entity=0,instance=0,object_class=0;
    std::uint64_t class_version=0;
};
struct WorldLightCapture {
    std::vector<PlacedLight> sources;
    // Parallel to sources; owners keep class configuration alive after a store
    // replacement or clear. No engine memory is retained.
    std::vector<std::shared_ptr<const LightClassRecord>> class_owners;
    // Checked GameObject membership only; other native draw domains are separate.
    std::vector<WorldMember> members;
};

// Externally serialized. Read and Pin callbacks must not throw or mutate the
// world during one pass. A second pass rejects observed changes before publish.
// The supplied profile is the only address source; no game address is baked in.
bool capture_world_lights(WorldLightReadProfile profile,void* read_context,Read read,
    void* class_context,PinLightClass pin,WorldLightCapture& output,
    std::string& error) noexcept;
} // namespace a2fo::native11
