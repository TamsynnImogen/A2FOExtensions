#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::m3d
#define A2FO_ModuleInit A2FOBuiltinM3D_Init
#define A2FO_ModuleShutdown A2FOBuiltinM3D_Shutdown
#include "../modules/A2FOM3D/module.cpp"

extern "C" bool A2FOBuiltinM3D_Ready() noexcept {
    return g_ready;
}
