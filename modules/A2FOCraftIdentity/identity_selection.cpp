#include "identity_selection.hpp"

namespace a2fo::craft_identity {
bool aligned_identity_index(
    std::int32_t craft_name_index,
    std::size_t entry_count,
    std::size_t* output) noexcept {
    if (output) *output = 0;
    if (craft_name_index < 0) return false;
    const auto index = static_cast<std::size_t>(craft_name_index);
    if (index >= entry_count) return false;
    if (output) *output = index;
    return true;
}

}  // namespace a2fo::craft_identity
