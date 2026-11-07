#pragma once
#include <windows.h>
#include "../renderer/native11/material_snapshot.hpp"
namespace a2fo::native11_materials {
using Log=void(*)(const char*);
// Startup-installed, process-pinned render listener. Invoked on the capture
// owner without holding capture/cache locks. The original render is bracketed
// even when observation is refused; listener owns any nested scope cleanup.
struct Listener {
    void (*begin)(void*,void*,void*) noexcept=nullptr;
    void (*parts)(void*,void*,const native11::Object&,
        const std::shared_ptr<const native11::MaterialObservation>&) noexcept=nullptr;
    void (*end)(void*) noexcept=nullptr;
    void* context=nullptr;
};
bool set_listener(Listener) noexcept;
// Startup-only opt-in. Observer runs after craft selection, without replacing
// a draw or invoking any extra engine function.
bool initialize(HMODULE,bool requested,Log) noexcept;
void disable() noexcept;
void begin(void* camera) noexcept;
void abort() noexcept;
void observe_nodes(void* instance,void* camera,void* database,unsigned native_x87_control=0) noexcept;
bool bind_frame(const native11::Frame&,std::shared_ptr<const native11::BoundMaterialCapture>&,
    std::string& error) noexcept;
bool current(const std::shared_ptr<const native11::BoundMaterialCapture>&) noexcept;
}
extern "C" std::uintptr_t a2fo_native11_material_draw_original;
extern "C" void a2fo_native11_material_draw_tap();
extern "C" void a2fo_native11_observe_materials(void* instance,void* camera) noexcept;
extern "C" void a2fo_native11_material_render_end() noexcept;
