// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinEnergySystems_Init
#define A2FO_ModuleShutdown A2FOBuiltinEnergySystems_Shutdown
#include "../modules/A2FOEnergySystems/module.cpp"
