#include "load_button_bounds.hpp"
#include "../../rust/ui_ffi.h"
namespace a2fo::instant_action_settings {
namespace {
A2foRsBounds pack(Bounds b) { return {b.left, b.top, b.right, b.bottom}; }
} // namespace
EffectiveBounds effective_load_bounds(const Bounds &load, const Bounds &save) noexcept {
    const auto r = a2fo_rs_ui_load_bounds(pack(load), pack(save));
    return {{r.bounds.left, r.bounds.top, r.bounds.right, r.bounds.bottom},
            r.valid != 0,
            r.repaired != 0};
}
bool contains(const Bounds &b, std::int32_t x, std::int32_t y) noexcept {
    return a2fo_rs_ui_contains(pack(b), x, y) != 0;
}
bool is_load_settings_caption(const char *caption) noexcept {
    if (!caption)
        return false;
    std::size_t n = 0;
    while (caption[n])
        ++n;
    return a2fo_rs_ui_caption(reinterpret_cast<const std::uint8_t *>(caption), n) != 0;
}
} // namespace a2fo::instant_action_settings
