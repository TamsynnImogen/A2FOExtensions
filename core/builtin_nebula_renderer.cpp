// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinNebulaRenderer_Init
#define A2FO_ModuleShutdown A2FOBuiltinNebulaRenderer_Shutdown
#include "../modules/A2FONebulaRenderer/module.cpp"
