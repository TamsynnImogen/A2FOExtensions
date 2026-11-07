// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinA1Compat_Init
#define A2FO_ModuleShutdown A2FOBuiltinA1Compat_Shutdown
#include "../modules/A1Compat/module.cpp"
