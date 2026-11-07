// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinDirectionalShields_Init
#define A2FO_ModuleShutdown A2FOBuiltinDirectionalShields_Shutdown
#include "../modules/A2FODirectionalShields/module.cpp"
