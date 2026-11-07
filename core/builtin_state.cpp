// Native API adapters. Policy and atomic transitions are implemented in Rust.
#include "builtin_state.hpp"
#include "../rust/builtin_state_ffi.h"

namespace a2fo {
static_assert(static_cast<int>(BuiltinFeature::count) == 48,
              "Update the Rust identity table when adding a builtin");
static_assert(static_cast<int>(BuiltinFeature::cheats) == 24,
              "Update the Rust startup selection boundary");

bool builtin_requires_restart(BuiltinFeature feature) noexcept {
    return a2fo_rs_builtin_requires_restart(static_cast<int>(feature)) != 0;
}
const char* canonical_builtin_name(BuiltinFeature feature) noexcept {
    return a2fo_rs_builtin_name(static_cast<int>(feature));
}
bool find_builtin_feature(const std::string& name, BuiltinFeature& feature) noexcept {
    const auto found = a2fo_rs_builtin_find(
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size());
    if (found < 0) return false;
    feature = static_cast<BuiltinFeature>(found);
    return true;
}
bool same_module_roots(const std::vector<std::string>& active,
                       const std::vector<std::string>& selected) noexcept {
    if (active.empty() || active.size() != selected.size()) return false;
    for (std::size_t index = 0; index < active.size(); ++index) {
        if (!a2fo_rs_builtin_same_root(
            reinterpret_cast<const std::uint8_t*>(active[index].data()), active[index].size(),
            reinterpret_cast<const std::uint8_t*>(selected[index].data()), selected[index].size())) {
            return false;
        }
    }
    return true;
}
void BuiltinState::prepared(bool available, bool requested) noexcept {
    a2fo_rs_builtin_prepare(&flags_, available, requested);
}
bool BuiltinState::request(bool enabled) noexcept {
    return a2fo_rs_builtin_request(&flags_, enabled) != 0;
}
bool BuiltinState::enabled() const noexcept { return activation() != 0; }
std::uint64_t BuiltinState::activation() const noexcept {
    return a2fo_rs_builtin_activation(&flags_);
}
bool BuiltinState::available() const noexcept {
    return (a2fo_rs_builtin_flags(&flags_) & 2) != 0;
}
bool BuiltinState::is_prepared() const noexcept {
    return (a2fo_rs_builtin_flags(&flags_) & 1) != 0;
}
bool BuiltinState::requested() const noexcept {
    return (a2fo_rs_builtin_flags(&flags_) & 4) != 0;
}
} // namespace a2fo
