#include "a2fo_module_api.h"

#include <cassert>
#include <cstddef>

int main() {
    A2FO_ModuleApi api{};
    api.struct_size = A2FO_MODULE_API_V4_BASE_SIZE;
    assert(!A2FO_MODULE_API_HAS(&api, api_revision));
    api.struct_size = sizeof(api);
    assert(A2FO_MODULE_API_HAS(&api, api_revision));
    assert(A2FO_MODULE_API_HAS(&api, capabilities));
    assert(A2FO_MODULE_API_HAS(&api, register_object_destroyed_handler));
    assert(offsetof(A2FO_ModuleApi, api_revision) >=
           A2FO_MODULE_API_V4_BASE_SIZE);
    assert(A2FO_MODULE_API_VERSION == 4u);
    assert(A2FO_MODULE_API_REVISION >= 1u);
}
