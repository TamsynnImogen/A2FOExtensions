#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
namespace a2fo::refit_scene {
using Log=void(*)(const char*);
// CPU listener only, at the admitted outside-loader-lock workspace boundary. No device or
// helper loading occurs until the first owned, posed instance submission.
bool initialize(HMODULE,const std::string& root,Log) noexcept;
// Existing checked world wrapper only; end runs before CPU capture publication.
// A ticket is private to this synchronous view and never a completed-frame ID.
std::uint64_t begin_view(void* camera,bool capture_owned) noexcept;
void end_view(void* camera,std::uint64_t ticket) noexcept;
}
