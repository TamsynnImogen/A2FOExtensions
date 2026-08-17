#include "load_button_bounds.hpp"

#include <cstdint>
#include <limits>

namespace a2fo::instant_action_settings {
namespace {

constexpr std::int32_t kMaximumButtonWidth = 1024;
constexpr std::int32_t kMaximumButtonHeight = 512;
constexpr std::int32_t kMaximumCoordinateMagnitude = 16384;
constexpr std::int32_t kNativeVerticalGap = 6;

bool plausible_coordinate(std::int32_t value) noexcept {
    return value >= -kMaximumCoordinateMagnitude &&
        value <= kMaximumCoordinateMagnitude;
}

bool valid_bounds(const Bounds& bounds) noexcept {
    if (!plausible_coordinate(bounds.left) ||
        !plausible_coordinate(bounds.top) ||
        !plausible_coordinate(bounds.right) ||
        !plausible_coordinate(bounds.bottom) ||
        bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
        return false;
    }
    const std::int32_t width = bounds.right - bounds.left;
    const std::int32_t height = bounds.bottom - bounds.top;
    return width <= kMaximumButtonWidth && height <= kMaximumButtonHeight;
}

bool checked_add(std::int32_t left, std::int32_t right,
                 std::int32_t* result) noexcept {
    if (!result) return false;
    const std::int64_t value = static_cast<std::int64_t>(left) + right;
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }
    *result = static_cast<std::int32_t>(value);
    return true;
}

bool expected_load_origin(const Bounds& load, const Bounds& save) noexcept {
    std::int32_t expected_top = 0;
    return checked_add(save.bottom, kNativeVerticalGap, &expected_top) &&
        load.left == save.left && load.top == expected_top;
}

}  // namespace

EffectiveBounds effective_load_bounds(const Bounds& load,
                                      const Bounds& save) noexcept {
    const bool load_valid = valid_bounds(load);
    const bool save_valid = valid_bounds(save);
    if (load_valid && !save_valid) return {load, true, false};
    if (!save_valid) return {};

    const std::int32_t save_width = save.right - save.left;
    const std::int32_t save_height = save.bottom - save.top;
    if (load_valid && expected_load_origin(load, save) &&
        load.right - load.left == save_width &&
        load.bottom - load.top == save_height) {
        return {load, true, false};
    }

    Bounds repaired = load;

    // Fleet Operations' form gives Save and Load identical dimensions. Some
    // versions of its ShellBitmap wrapper retain the Load origin but lose the
    // image-derived right/bottom values.
    if (!expected_load_origin(repaired, save)) {
        repaired.left = save.left;
        if (!checked_add(save.bottom, kNativeVerticalGap, &repaired.top)) {
            return {};
        }
    }
    if (!checked_add(repaired.left, save_width, &repaired.right) ||
        !checked_add(repaired.top, save_height, &repaired.bottom) ||
        !valid_bounds(repaired)) {
        return {};
    }
    return {repaired, true, true};
}

bool contains(const Bounds& bounds, std::int32_t x,
              std::int32_t y) noexcept {
    return valid_bounds(bounds) && x >= bounds.left && x < bounds.right &&
        y >= bounds.top && y < bounds.bottom;
}

bool is_load_settings_caption(const char* caption) noexcept {
    if (!caption) return false;
    constexpr char expected[] = "loadsettings";
    std::size_t expected_index = 0;
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(caption);
         *cursor != '\0'; ++cursor) {
        unsigned char value = *cursor;
        if (value == '&' || value == ' ' || value == '\t' || value == '_') {
            continue;
        }
        if (value >= 'A' && value <= 'Z') value = value - 'A' + 'a';
        if (expected_index >= sizeof(expected) - 1 ||
            value != static_cast<unsigned char>(expected[expected_index])) {
            return false;
        }
        ++expected_index;
    }
    return expected_index == sizeof(expected) - 1;
}

}  // namespace a2fo::instant_action_settings
