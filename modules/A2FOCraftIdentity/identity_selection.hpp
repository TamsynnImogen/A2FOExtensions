#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo::craft_identity {

// Maps an identity list to Fleet Operations' already-selected
// possibleCraftNames entry. Returning false rather than wrapping preserves the
// mod author's one-to-one row alignment when a companion list is too short.
bool aligned_identity_index(
    std::int32_t craft_name_index,
    std::size_t entry_count,
    std::size_t* output) noexcept;

}  // namespace a2fo::craft_identity
