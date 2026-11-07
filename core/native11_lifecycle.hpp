#pragma once
#include <windows.h>
#include <cstdint>
namespace a2fo::native11_lifecycle {
using Log=void(*)(const char*);
inline constexpr unsigned model=1,texture=2;
// Startup serialized, exact image only. Hooks observe destruction before the
// native body and then tail-forward its original entry; no replay or replacement
// of native destruction. Trampolines and core hooks have process lifetime.
bool initialize(HMODULE,bool models,bool textures,Log,HMODULE fleet_ops=nullptr) noexcept;
void disable() noexcept;
// Unrequested domains retain legacy diagnostic behavior. Requested domains
// require active owned hooks and intact original-entry trampolines.
bool healthy(unsigned domains) noexcept;
} // namespace a2fo::native11_lifecycle
extern "C" {
extern std::uintptr_t a2fo_native11_model_destroy_original;
extern std::uintptr_t a2fo_native11_texture_destroy_original;
void a2fo_native11_model_destroy_tap();
void a2fo_native11_texture_destroy_tap();
void a2fo_native11_observe_destruction(unsigned kind,void* object) noexcept;
}
