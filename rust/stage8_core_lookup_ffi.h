#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreLookupStep)(void*,std::uint32_t);
std::uint64_t a2fo_rs_stage8_core_lookup(std::uint32_t,A2foRsCoreLookupStep,void*);
}
