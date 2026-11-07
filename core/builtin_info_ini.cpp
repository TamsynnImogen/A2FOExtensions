// Preserve the standalone SDK target while sharing its implementation.
#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::info_ini
#define A2FO_ModuleInit A2FOBuiltinInfoIni_Init
#include "../modules/A2FOInfoIni/module.cpp"
