// Separate translation unit preserves module-local symbols and standalone SDK builds.
#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::wreckage
#define A2FO_ModuleInit A2FOBuiltinWreckage_Init
#define A2FO_ModuleShutdown A2FOBuiltinWreckage_Shutdown
#include "../modules/A2FOWreckage/module.cpp"
