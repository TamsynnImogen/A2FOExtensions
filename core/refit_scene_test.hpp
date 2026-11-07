#pragma once
#include <windows.h>
#include <string>
namespace a2fo::refit_scene_test {
using Log=void(*)(const char*);
// Inert outside the explicitly tagged, isolated headless diagnostic child.
void initialize(HMODULE,const std::string&,Log) noexcept;
void before_world(void* camera,unsigned main) noexcept;
// 0 normal, 1 untextured white, 2 untextured black. Requires the existing
// isolated scene admission, an extra environment marker, explicit child INI
// mode, a successfully created diagnostic scene and current profile ownership.
unsigned material_control() noexcept;
}
