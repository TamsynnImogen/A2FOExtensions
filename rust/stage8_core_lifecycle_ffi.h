#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
typedef std::uint64_t(*A2foRsCoreLifecycleStep)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
std::uint64_t a2fo_rs_stage8_core_initialize(A2foRsCoreLifecycleStep,void*);
std::uint64_t a2fo_rs_stage8_core_worker(A2foRsCoreLifecycleStep,void*);
}
