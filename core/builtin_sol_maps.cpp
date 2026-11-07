// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinSOLMaps_Init
#define A2FO_ModuleShutdown A2FOBuiltinSOLMaps_Shutdown
#define DllMain A2FOBuiltinSOLMaps_DllMain
#include "../modules/A2FOSOLMaps/module.cpp"
