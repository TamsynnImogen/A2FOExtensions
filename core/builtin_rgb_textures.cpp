// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinRGBTextures_Init
#define A2FO_ModuleShutdown A2FOBuiltinRGBTextures_Shutdown
#include "../modules/A2FORGBTextures/module.cpp"
#include "native11_rgb_owner.hpp"
#include "../renderer/native11/open_read_owner.hpp"

bool a2fo_builtin_rgb_owns_open_read(HMODULE image) noexcept {
    if(!image||image!=g_armada||g_open_read_hook.target!=at(g_armada,kOpenReadRva))return false;
    const auto reader=+[](void*,std::uint32_t address,void* output,std::size_t size)->bool {
        SIZE_T count=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&count)&&count==size;
    };
    const a2fo::native11::OpenReadOwner owner{
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_open_read_hook.target)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_open_read_hook.gateway)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function_address(&open_read_hook))),g_open_read_hook.length};
    return a2fo::native11::owns_open_read_gateway(nullptr,reader,owner);
}
