#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::swarm_system
#define A2FO_ModuleInit A2FOBuiltinSwarmSystem_Init
#define A2FO_ModuleShutdown A2FOBuiltinSwarmSystem_Shutdown
#include "../modules/A2FOSwarmSystem/module.cpp"

extern "C" bool A2FOBuiltinSwarmSystem_Ready() noexcept {
    return g_runtime_ready;
}
