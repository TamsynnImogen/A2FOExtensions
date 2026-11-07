#include "object_editor_state.hpp"
#include "../../rust/lead_ffi.h"
#include <cerrno>
#include <cstddef>
namespace a2fo::object_editor {
static_assert(sizeof(SavedState) == 1076 && offsetof(SavedState, current) == 20 &&
                  offsetof(SavedState, maximum) == 36 && offsetof(SavedState, captain) == 52 &&
                  offsetof(SavedState, registry) == 564,
              "Rust editor byte offsets must match saved ABI");
static_assert(ERANGE == 34, "Review CRT errno mapping for this platform");
bool valid_state(const SavedState &state) noexcept {
    return a2fo_rs_lead_editor_valid(reinterpret_cast<const std::uint8_t *>(&state),
                                     sizeof(state)) != 0;
}
bool parse_shield_number(const char *text, float *value) noexcept {
    return a2fo_rs_lead_editor_number(text, value) != 0;
}
PrefixResult decode_prefix(std::string_view bytes, bool binary, bool tagged, SavedState *state,
                           std::size_t *consumed) noexcept {
    return static_cast<PrefixResult>(a2fo_rs_lead_editor_decode(
        reinterpret_cast<const std::uint8_t *>(bytes.data()), bytes.size(), binary, tagged,
        reinterpret_cast<std::uint8_t *>(state), consumed));
}
} // namespace a2fo::object_editor
