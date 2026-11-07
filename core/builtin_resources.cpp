// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinResources_Init
#define A2FO_ModuleShutdown A2FOBuiltinResources_Shutdown
#include "../modules/A2FOResources/module.cpp"
