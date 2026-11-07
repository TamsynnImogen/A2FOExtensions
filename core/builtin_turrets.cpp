// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinTurrets_Init
#define A2FO_ModuleShutdown A2FOBuiltinTurrets_Shutdown
#include "../modules/A2FOTurrets/module.cpp"
