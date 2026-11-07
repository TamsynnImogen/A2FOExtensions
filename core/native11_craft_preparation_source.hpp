#pragma once
#include <windows.h>
#include <cstdint>
namespace a2fo::native11_craft_preparation::source {
struct Proof {
    std::uint32_t image=0,fleet=0,gateway=0;
    bool fleet_base_wrapper=false;
};
inline bool same(const Proof& a,const Proof& b) noexcept {
    return a.image==b.image&&a.fleet==b.fleet&&a.gateway==b.gateway&&a.fleet_base_wrapper==b.fleet_base_wrapper;
}
// One-time file identity and module pinning. Does not install hooks, invoke
// game functions, or qualify entity display/material/FOW behavior.
bool qualify(HMODULE image,HMODULE fleet) noexcept;
// Checks original base setup or the exact Fleet post-original cache wrapper.
// No body other than the known entry redirect is normalized for comparison.
// Returns only a base flags/hull source proof, never a whole-class proof.
bool current(HMODULE image,HMODULE fleet,Proof&) noexcept;
}
