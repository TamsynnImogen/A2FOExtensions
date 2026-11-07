#pragma once
#include <windows.h>
namespace a2fo::native11_nodes {
using Log=void(*)(const char*);
// Startup-only opt-in; both observed branches are after native animation.
bool initialize(HMODULE,bool requested,Log) noexcept;
void disable() noexcept;
bool enabled() noexcept;
bool requested() noexcept; // remains required after a failed/partial admission
bool owns_calls() noexcept;
}
extern "C" void a2fo_native11_node_branch_a();
extern "C" void a2fo_native11_node_branch_b();
extern "C" void a2fo_native11_observe_nodes(void* instance,void* camera,void* database,
    unsigned native_x87_control=0) noexcept;
