#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::breeders
#define A2FO_ModuleInit A2FOBuiltinBreeders_Init
#define A2FO_ModuleShutdown A2FOBuiltinBreeders_Shutdown
#include "../modules/A2FOBreeders/module.cpp"

extern "C" bool A2FOBuiltinBreeders_Ready() noexcept {
    return g_runtime_ready;
}
