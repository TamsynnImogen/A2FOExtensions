// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinRefitYards_Init
#define A2FO_ModuleShutdown A2FOBuiltinRefitYards_Shutdown
#include "../modules/A2FORefitYards/module.cpp"
