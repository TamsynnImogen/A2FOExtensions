/*
 * Pure, host-testable row-selection helper for A2FOCraftIdentity.
 *
 * The runtime deliberately shares Fleet Operations' possibleCraftNames index
 * instead of selecting captain and registry strings independently.
 */

#include "identity_selection.hpp"
#include "../../rust/gameplay_ffi.h"

namespace a2fo::craft_identity {
bool aligned_identity_index(std::int32_t craft_name_index, std::size_t entry_count,
                            std::size_t *output) noexcept {
    return a2fo_rs_gameplay_identity_index(craft_name_index, entry_count, output) != 0;
}

} // namespace a2fo::craft_identity
