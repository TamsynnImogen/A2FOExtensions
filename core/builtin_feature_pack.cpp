// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinFeaturePack_Init
#define A2FO_ModuleShutdown A2FOBuiltinFeaturePack_Shutdown
#include "../modules/A2FOFeaturePack/odf_recursive.cpp"

// FeaturePack has no standalone shutdown export; hooks remain resident.
extern "C" void A2FO_CALL A2FOBuiltinFeaturePack_Shutdown() {}
