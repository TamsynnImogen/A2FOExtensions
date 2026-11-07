#pragma once
#include <cstdint>
extern "C" {
typedef std::uint64_t (*A2foRsModuleMenuStep)(void*,std::uint32_t,std::uint64_t);
std::uint64_t a2fo_rs_stage9_module_menu_flow(std::uint32_t,A2foRsModuleMenuStep,void*,const std::uint8_t*);
}
