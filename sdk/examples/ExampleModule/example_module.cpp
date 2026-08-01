#include "../../include/a2fo_module_api.h"

namespace {
const A2FO_ModuleApi* g_api = nullptr;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log) {
        return false;
    }
    g_api = api;
    g_api->log("ExampleModule", "Example native module initialized");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    if (g_api && g_api->log) {
        g_api->log("ExampleModule", "Example native module shutting down");
    }
    g_api = nullptr;
}
