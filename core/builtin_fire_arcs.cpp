#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::fire_arcs
#define A2FO_ModuleInit A2FOBuiltinFireArcs_Init
#define A2FO_ModuleShutdown A2FOBuiltinFireArcs_Shutdown
#include "../modules/A2FOFireArcs/module.cpp"

extern "C" bool A2FOBuiltinFireArcs_Ready() noexcept {
    return g_runtime_ready;
}
