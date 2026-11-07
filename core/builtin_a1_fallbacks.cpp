#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::a1_fallbacks
#define A2FO_ModuleInit A2FOBuiltinA1Fallbacks_Init
#define A2FO_ModuleShutdown A2FOBuiltinA1Fallbacks_Shutdown
#include "../modules/A1Fallbacks/module.cpp"

extern "C" bool A2FOBuiltinA1Fallbacks_Ready() noexcept {
    return g_runtime_ready;
}
