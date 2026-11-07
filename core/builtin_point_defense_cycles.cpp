// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinPointDefenseCycles_Init
#define A2FO_ModuleShutdown A2FOBuiltinPointDefenseCycles_Shutdown
#include "../modules/A2FOPointDefenseCycles/module.cpp"
