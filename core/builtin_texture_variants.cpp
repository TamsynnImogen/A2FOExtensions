// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinTextureVariants_Init
#define A2FO_ModuleShutdown A2FOBuiltinTextureVariants_Shutdown
#include "../modules/A2FOTextureVariants/module.cpp"
