#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
using A2foStage10UiStep=std::uintptr_t (*)(void*,std::uint32_t,std::uintptr_t);
std::uintptr_t a2fo_rs_stage10_ui_effect_tree(void*,A2foStage10UiStep,std::int32_t);
std::uint8_t a2fo_rs_stage10_ui_descendant(void*,A2foStage10UiStep,std::uintptr_t);
void a2fo_rs_stage10_ui_map_points(void*,A2foStage10UiStep,std::size_t);
std::uintptr_t a2fo_rs_stage10_ui_lease(void*,A2foStage10UiStep);
}
