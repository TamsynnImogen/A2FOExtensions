#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint64_t(*A2foRsCoreSettingsStep)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
std::uint64_t a2fo_rs_stage8_core_query_defaults(A2foRsCoreSettingsStep,void*);
std::uint64_t a2fo_rs_stage8_core_ensure_defaults(A2foRsCoreSettingsStep,void*);
std::uint64_t a2fo_rs_stage8_core_resolve_directory(A2foRsCoreSettingsStep,void*);
}
