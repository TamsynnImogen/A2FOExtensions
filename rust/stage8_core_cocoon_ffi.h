#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreCocoonStep)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
std::uint64_t a2fo_rs_stage8_core_select_cocoon(A2foRsCoreCocoonStep,void*);
}
