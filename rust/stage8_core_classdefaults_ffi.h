#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreClassdefaultStep)(void*,std::uint32_t);
std::uint8_t a2fo_rs_stage8_core_classdefault(std::uint32_t,A2foRsCoreClassdefaultStep,void*);
}
