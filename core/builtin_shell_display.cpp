// Preserve original startup-only module code, signatures and callbacks.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinShellDisplay_Init
#define A2FO_ModuleShutdown A2FOBuiltinShellDisplay_Shutdown
#include "../modules/A2FOShellDisplay/module.cpp"
