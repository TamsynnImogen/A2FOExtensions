#pragma once

#include "../../sdk/include/a2fo_module_api.h"
#include <cstdint>

// Read-only, already-resolved squad costs for resource slots 6..9. False for
// ordinary classes or unresolved/invalid squads. Never loads ODFs or recurses
// into Resources, so payment/refund/UI all consume the same cached values.
using A2FOSquadronsGetAdditionalCostsFn = bool (A2FO_CALL*)(
    void* object_class, std::int32_t* costs, std::uint32_t count);
