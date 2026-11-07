#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::odf_variants
#define A2FO_ModuleInit A2FOBuiltinODFVariants_Init
#define A2FO_ModuleShutdown A2FOBuiltinODFVariants_Shutdown
#include "../modules/A2FOODFVariants/module.cpp"

extern "C" bool A2FOBuiltinODFVariants_Ready() noexcept {
    return g_runtime_alive;
}
