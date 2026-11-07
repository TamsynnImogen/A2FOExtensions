#pragma once
#include "craft_view_policy.hpp"
#include "scene_capture.hpp"
#include <cstdint>
#include <string>

namespace a2fo::native11 {
struct CraftViewReadProfile {
    std::uint32_t engine_pointer=0,game_pointer=0,mode_pointer=0;
    std::uint32_t skip_near_pointer=0,max_distance_pointer=0;
    std::uint32_t instance_vtable=0,database_vtable=0;
    std::uint32_t near_fade_pointer=0; // three mutable native floats: min, max, amount
};
struct CapturedCraftView {
    CraftViewInput input{};
    CraftViewResult result{};
    std::uint32_t engine=0,game=0,settings=0,camera=0;
    std::uint32_t entity=0,instance=0,database=0,native_x87_control=0;
};
// Identity-only input for checked roster callers; no renderable placement or
// render-selection assertion is manufactured to sample distance/fade policy.
struct CraftViewSubject {
    std::uint32_t entity=0,instance=0,database=0;
    std::uint32_t instance_vtable=0,database_vtable=0;
    std::uint64_t model_serial=0;
};
bool capture_craft_view(const CraftViewSubject&,std::uint32_t camera,
    std::uint32_t native_x87_control,CraftViewReadProfile,
    void* read_context,Read,CapturedCraftView& output,std::string& error) noexcept;
// Reads the caller-certified addresses twice and evaluates fresh policy input.
// The caller separately binds model owner, class, world membership and frame.
bool capture_craft_view(const Object&,std::uint32_t camera,
    std::uint32_t native_x87_control,CraftViewReadProfile,
    void* read_context,Read,CapturedCraftView& output,std::string& error) noexcept;
} // namespace a2fo::native11
