#pragma once
#include <windows.h>
#include <cstdint>
namespace a2fo::refit_mesh_capture {
using Observe=void(*)(void* mesh,std::uint32_t pass,void* material,std::uint32_t route);
using Log=void(*)(const char*);
using Scope=void(*)(bool begin,void* mesh,std::uint32_t pass,void* material,
    std::uint32_t route,bool qualified);
// Observational taps at the checked per-mesh calls, before legacy batching or
// the MeshVB primitive loop. Original functions always execute exactly once.
bool initialize(HMODULE,Observe,Log,Scope=nullptr) noexcept;
bool owns_calls() noexcept;
bool draw_source_current() noexcept;
}
