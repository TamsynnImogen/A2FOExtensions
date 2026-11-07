#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
typedef std::uint64_t(*A2foRsCoreClassloadStep)(void*,std::uint32_t,std::uintptr_t);
std::uint8_t a2fo_rs_stage8_core_classload_admit(const std::uint8_t*,std::uint8_t*,std::uint32_t);
std::uint64_t a2fo_rs_stage8_core_classload(std::uint32_t,A2foRsCoreClassloadStep,void*);
}
