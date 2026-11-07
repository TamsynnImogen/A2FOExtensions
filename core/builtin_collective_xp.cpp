// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinCollectiveXP_Init
#define A2FO_ModuleShutdown A2FOBuiltinCollectiveXP_Shutdown
#include "../modules/A2FOCollectiveXP/module.cpp"
