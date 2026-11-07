#pragma once
#include <string>
#include <windows.h>
namespace a2fo::refit {
using StartupLog = void (*)(const std::string &);
using StartupSourcesReady = void (*)() noexcept;
// Early, opt-in child-mod factory interception only. No helper loading/GPU work
// under DllMain. Exact files and mode code are checked on the factory's thread.
// Returns true when this module handled a Refit test request (including
// refusal).
bool install_startup(HMODULE executable, const std::string &root,
                     StartupLog,StartupSourcesReady=nullptr) noexcept;
// An armed factory route reserves this explicit test's graphics path, including
// a later pre-device fallback. Suspend legacy DXVK-only material/draw hooks for
// that attempt; the base original renderer is the startup fallback. A normal
// backend restart restores those effects. Rejected/unpatched requests do not
// reserve the path.
bool reserves_graphics_route() noexcept;
// Owner-thread only, after successful outside-loader-lock factory admission.
// Output remains untouched on refusal. No library is loaded by this accessor.
bool live_host_context(HMODULE& display,std::string& helper,unsigned& backend) noexcept;
} // namespace a2fo::refit
