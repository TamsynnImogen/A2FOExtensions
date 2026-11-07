#pragma once
#include <windows.h>
#include "../renderer/native11/sprite_observation.hpp"
#include <memory>
#include <vector>
namespace a2fo::native11_sprites {
using Log=void(*)(const char*);
using RefreshOwnerCheck=bool(*)(HMODULE) noexcept;
// World viewport and native size are copied correlation metadata; this trace
// does not certify a complete UI render target or all native draw routes.
struct Trace {
    std::uint32_t camera=0,device=0,source_ticket=0;
    float world_viewport[4]{},native_target_size[2]{};
    std::uint64_t generation=0;
    std::vector<std::shared_ptr<const native11::SpriteObservation>> sprites;
};
// Startup-only opt-in CPU tracing. Native sprite drawing and EndScene each
// execute once. This observes one route; it does not certify complete UI.
bool initialize(HMODULE image,HMODULE fleet_ops,bool requested,Log,RefreshOwnerCheck=nullptr) noexcept;
void disable() noexcept;
bool healthy() noexcept;
// Start after a completed checked main world, before its outer HUD passes.
// No absence is inferred for sprites/fonts/minimap drawn through other routes.
void begin_ui(const native11::Frame&,std::uint32_t source_ticket) noexcept;
void abort_ui() noexcept;
std::shared_ptr<const Trace> latest(std::string& error) noexcept;
} // namespace a2fo::native11_sprites
extern "C" {
void a2fo_native11_sprite_dispatch_tap();
void a2fo_native11_sprite_end_scene_tap();
void a2fo_native11_sprite_fleet_end_scene_tap();
extern std::uintptr_t a2fo_native11_sprite_end_scene_original;
void a2fo_native11_observe_sprite(void* device,const std::uint32_t* arguments,
    std::uint32_t native_x87_control,void* dispatch_table) noexcept;
void a2fo_native11_end_sprite_ui(void* wrapper,void* native_device,void* dispatch_table) noexcept;
}
