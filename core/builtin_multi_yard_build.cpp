// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinMultiYardBuild_Init
#define A2FO_ModuleShutdown A2FOBuiltinMultiYardBuild_Shutdown
#include "../modules/A2FOMultiYardBuild/module.cpp"
