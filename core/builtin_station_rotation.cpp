#define A2FO_BUILTIN_FEATURE a2fo::BuiltinFeature::station_rotation
#define A2FO_ModuleInit A2FOBuiltinStationRotation_Init
#define A2FO_ModuleShutdown A2FOBuiltinStationRotation_Shutdown
#include "../modules/A2FOStationRotation/module.cpp"

extern "C" bool A2FOBuiltinStationRotation_Ready() noexcept {
    return g_ready || (g_armada && a2fo::supported_armada::uses_native_station_rotation(a2fo::supported_armada::identify(reinterpret_cast<HMODULE>(g_armada))));
}
