#pragma once

#include "entity_registry.hpp"

#include <cstdint>

namespace armadal::engine_context {

// Recovered ArmadaL 0x0062B710:
//   index = object[0xC0]
//   entry = object[index * 4 + 0xCC]
//   return entry[0x28]
inline std::uint32_t lookup(std::uint32_t object,
                            const entity_registry::Memory& memory) {
    const std::uint32_t index = memory.read(object + 0xc0u);
    const std::uint32_t entry = memory.read(object + index * 4u + 0xccu);
    return memory.read(entry + 0x28u);
}

} // namespace armadal::engine_context
