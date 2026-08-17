#pragma once

#include "../../sdk/include/a2fo_module_api.h"

#include <cstdint>

// The added resources continue Armada II's native 0..5 resource indices.
enum A2FO_ResourceIndex : std::uint32_t {
    A2FO_RESOURCE_TRITANIUM = 6,
    A2FO_RESOURCE_SUPPLY = 7,
    A2FO_RESOURCE_CREDITS = 8,
    A2FO_RESOURCE_COLLECTIVE_CONNECTIONS = 9,
    A2FO_RESOURCE_COUNT = 10,
};

enum A2FO_ResourcePresentation : std::uint32_t {
    A2FO_RESOURCE_PRESENTATION_RES = 0,
    A2FO_RESOURCE_PRESENTATION_TOOLTIP = 1,
    A2FO_RESOURCE_PRESENTATION_VERBOSE_TOOLTIP = 2,
    A2FO_RESOURCE_PRESENTATION_ICON = 3,
    A2FO_RESOURCE_PRESENTATION_COUNT = 4,
};

using A2FOResourcesGetFn = std::int64_t (A2FO_CALL*)(
    void* team, std::uint32_t resource);
using A2FOResourcesSetFn = bool (A2FO_CALL*)(
    void* team, std::uint32_t resource, std::int64_t amount);
using A2FOResourcesAddFn = bool (A2FO_CALL*)(
    void* team, std::uint32_t resource, std::int64_t amount);
using A2FOResourcesGetCostFn = std::int32_t (A2FO_CALL*)(
    void* object_class, std::uint32_t resource);
using A2FOResourcesGetPresentationTextFn = const char* (A2FO_CALL*)(
    void* team, std::uint32_t resource, std::uint32_t presentation);
