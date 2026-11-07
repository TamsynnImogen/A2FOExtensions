#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsA1PhysicsOp)(void*, std::uint32_t, std::size_t);
std::uint8_t a2fo_rs_stage8_a1_physics_database(void*, A2foRsA1PhysicsOp);
std::uint8_t a2fo_rs_stage8_a1_physics_profile(void*, A2foRsA1PhysicsOp);
std::uint8_t a2fo_rs_stage8_a1_physics_profile_name(std::uint32_t);
}
