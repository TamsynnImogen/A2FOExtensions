// Startup-only Missions integration. The facade uses Rust mission handoff state
// and retains the native framework behind its exception firewall.
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinMissions_Init
#define A2FO_ModuleShutdown A2FOBuiltinMissions_Shutdown
#include "../modules/A2FOMissions/module.cpp"
