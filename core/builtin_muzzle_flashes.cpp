#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::muzzle_flashes
#define A2FO_ModuleInit A2FOBuiltinMuzzleFlashes_Init
#define A2FO_ModuleShutdown A2FOBuiltinMuzzleFlashes_Shutdown
#include "../modules/A2FOMuzzleFlashes/module.cpp"

extern "C" bool A2FOBuiltinMuzzleFlashes_Ready() noexcept {
    return g_ready;
}
