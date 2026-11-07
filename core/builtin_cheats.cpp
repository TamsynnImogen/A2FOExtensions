// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinCheats_Init
#define A2FO_ModuleShutdown A2FOBuiltinCheats_Shutdown
#include "../modules/A2FOCheats/module.cpp"

// The standalone module has no shutdown entrypoint. Hooks remain resident.
extern "C" void A2FO_CALL A2FOBuiltinCheats_Shutdown() {}
