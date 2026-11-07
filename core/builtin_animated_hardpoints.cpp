#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::animated_hardpoints
#define A2FO_ModuleInit A2FOBuiltinAnimatedHardpoints_Init
#define A2FO_ModuleShutdown A2FOBuiltinAnimatedHardpoints_Shutdown
#include "../modules/A2FOAnimatedHardpoints/module.cpp"

extern "C" bool A2FOBuiltinAnimatedHardpoints_Ready() noexcept {
    return g_runtime_ready;
}
