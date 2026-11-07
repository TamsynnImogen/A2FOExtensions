#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foStage10SquadronsStep=std::uintptr_t (*)(void*,std::uint32_t,std::uintptr_t);
void a2fo_rs_stage10_squadrons_bar(void*,A2foStage10SquadronsStep,std::uint32_t colour);
std::uint8_t a2fo_rs_stage10_squadrons_scan(void*,A2foStage10SquadronsStep);
void a2fo_rs_stage10_squadrons_snapshot(void*,A2foStage10SquadronsStep);
std::uint8_t a2fo_rs_stage10_squadrons_cost(const std::uint8_t*,std::size_t,std::int32_t*);
std::uint8_t a2fo_rs_stage10_squadrons_project(std::int32_t,std::uint32_t);
std::uint8_t a2fo_rs_stage10_squadrons_active(std::int32_t);
void a2fo_rs_stage10_squadrons_values(float*,const float*);
void a2fo_rs_stage10_squadrons_values_copy(float*,const float*);
std::uint32_t a2fo_rs_stage10_squadrons_weight(std::uint32_t);
void a2fo_rs_stage10_squadrons_colour(float*,float*,const float*,std::uint32_t);
void a2fo_rs_stage10_squadrons_divide(float*,const float*);
void a2fo_rs_stage10_squadrons_axis(float*,std::uint32_t,std::uint32_t);
std::uint8_t a2fo_rs_stage10_squadrons_near(std::uint32_t,std::uint32_t);
void a2fo_rs_stage10_squadrons_retry(float*,const float*);
std::uint8_t a2fo_rs_stage10_squadrons_retry_pending(std::uint32_t);
}
