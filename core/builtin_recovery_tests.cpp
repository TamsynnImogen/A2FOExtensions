#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::recovery_tests
#define A2FO_ModuleInit A2FOBuiltinRecoveryTests_Init
#define A2FO_ModuleShutdown A2FOBuiltinRecoveryTests_Shutdown
#include "../modules/A2FORecoveryTests/module.cpp"

extern "C" bool A2FOBuiltinRecoveryTests_Ready() noexcept {
    return !config.enabled || ((!config.engine_context || armed[0]) && (!config.node_transform || armed[1]));
}
