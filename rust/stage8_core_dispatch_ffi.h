#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreDispatchOp)(void*, std::uint32_t);
void a2fo_rs_stage8_core_dispatch_weapon_class(void*, A2foRsCoreDispatchOp);
std::uint8_t a2fo_rs_stage8_core_dispatch_weapon_trigger(
    void*, A2foRsCoreDispatchOp, std::uint32_t, std::uint32_t);
void a2fo_rs_stage8_core_dispatch_craft(void*, A2foRsCoreDispatchOp);
}
