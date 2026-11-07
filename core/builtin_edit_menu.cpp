#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::edit_menu
#define A2FO_ModuleInit A2FOBuiltinEditMenu_Init
#define A2FO_ModuleShutdown A2FOBuiltinEditMenu_Shutdown
#include "../modules/A2FOEditMenu/module.cpp"

extern "C" bool A2FOBuiltinEditMenu_Ready() noexcept {
    return g_runtime_ready;
}
