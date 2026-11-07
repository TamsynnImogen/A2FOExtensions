#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::always_show_shields
#define A2FO_ModuleInit A2FOBuiltinAlwaysShowShields_Init
#define A2FO_ModuleShutdown A2FOBuiltinAlwaysShowShields_Shutdown
#include "../modules/A2FOAlwaysShowShields/module.cpp"

extern "C" bool A2FOBuiltinAlwaysShowShields_Ready() noexcept {
    return g_runtime_ready;
}
