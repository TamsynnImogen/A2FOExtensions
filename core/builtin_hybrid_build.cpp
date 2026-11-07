// Startup-only consolidation: keep the original native admission and callbacks.
#define A2FO_ModuleInit A2FOBuiltinHybridBuild_Init
#define A2FO_ModuleShutdown A2FOBuiltinHybridBuild_Shutdown
#include "../modules/A2FOHybridBuild/module.cpp"

// The standalone module has no shutdown entrypoint. Hooks remain resident.
extern "C" void A2FO_CALL A2FOBuiltinHybridBuild_Shutdown() {}
