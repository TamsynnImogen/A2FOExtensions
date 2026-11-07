// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinCraftIdentity_Init
#define A2FO_ModuleShutdown A2FOBuiltinCraftIdentity_Shutdown
#include "../modules/A2FOCraftIdentity/module.cpp"
