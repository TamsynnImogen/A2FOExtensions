#pragma once
#include <cstdint>
#include <cstddef>
extern "C" {
typedef std::uint64_t (*A2foRsRefitStep)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
std::uint64_t a2fo_rs_stage8_refit_flow(std::uint32_t,A2foRsRefitStep,void*);
void a2fo_rs_stage8_refit_update(A2foRsRefitStep,void*);
std::uint8_t a2fo_rs_stage8_refit_cancel(std::uint8_t,A2foRsRefitStep,void*);
void a2fo_rs_stage8_refit_finished(A2foRsRefitStep,void*);
std::uint8_t a2fo_rs_stage8_refit_removed(std::uint8_t,A2foRsRefitStep,void*);
}
