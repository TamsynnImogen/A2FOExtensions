#pragma once
#include "../../sdk/include/a2fo_module_api.h"
#include <windows.h>

namespace a2fo::shell_display::native_intro {
// Only the synchronous native TBinkIntro.play call owns this presentation
// scope. Other NextBinkFrame callers retain their original output.
struct MovieState { void* bink{}; HWND window{}; bool enabled{}; };
struct FrameBuffer;
class Scope {
    MovieState previous_;
    FrameBuffer* previous_buffer_{};
    FrameBuffer* buffer_{};
public:
    Scope(void* bink, HWND window, bool enabled) noexcept;
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
bool initialize(const A2FO_ModuleApi*, HMODULE armada, HMODULE fleetops) noexcept;
void shutdown() noexcept;
}
