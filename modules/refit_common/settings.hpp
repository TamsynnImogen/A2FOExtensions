#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace a2fo::refit {

inline constexpr float kDefaultPanSpeed = 2.5f;
inline constexpr float kMinimumPanSpeed = 0.25f;
inline constexpr float kMaximumPanSpeed = 10.0f;
inline constexpr float kMinimumViewDistance = 0.0f;
inline constexpr float kMaximumViewDistance = 1000000.0f;

inline bool ascii_space(char ch) noexcept {
    return ch == ' ' || ch == '\t' || ch == '\n' ||
           ch == '\r' || ch == '\f' || ch == '\v';
}

inline std::string_view trim_ascii(std::string_view text) noexcept {
    while (!text.empty() && ascii_space(text.front())) text.remove_prefix(1);
    while (!text.empty() && ascii_space(text.back())) text.remove_suffix(1);
    return text;
}

inline std::optional<float> parse_number(std::string_view text,
                                         float minimum, float maximum) {
    text = trim_ascii(text);
    if (text.empty() || !std::isfinite(minimum) ||
        !std::isfinite(maximum) || minimum > maximum) return std::nullopt;

    std::istringstream stream{std::string(text)};
    stream.imbue(std::locale::classic());
    stream >> std::noskipws;
    float value = 0.0f;
    stream >> value;
    if (!stream || stream.peek() != std::char_traits<char>::eof() ||
        !std::isfinite(value) || value < minimum || value > maximum) {
        return std::nullopt;
    }
    return value;
}

inline std::optional<bool> parse_enabled(std::string_view text) noexcept {
    text = trim_ascii(text);
    if (text == "0") return false;
    if (text == "1") return true;
    return std::nullopt;
}

struct CameraSettings {
    bool enabled = false;
    float pan_speed = kDefaultPanSpeed;
};

struct ViewSettings {
    bool enabled = false;
    float minimum_distance = kMinimumViewDistance;
};

inline std::optional<CameraSettings> parse_camera_settings(
    std::string_view enabled, std::string_view pan) {
    const auto parsed_enabled = parse_enabled(enabled);
    const auto parsed_pan = parse_number(pan, kMinimumPanSpeed, kMaximumPanSpeed);
    if (!parsed_enabled || !parsed_pan) return std::nullopt;
    return CameraSettings{*parsed_enabled, *parsed_pan};
}

inline std::optional<ViewSettings> parse_view_settings(
    std::string_view enabled, std::string_view distance) {
    const auto parsed_enabled = parse_enabled(enabled);
    const auto parsed_distance = parse_number(
        distance, kMinimumViewDistance, kMaximumViewDistance);
    if (!parsed_enabled || !parsed_distance) return std::nullopt;
    return ViewSettings{*parsed_enabled, *parsed_distance};
}

inline std::uint32_t float_to_bits(float value) noexcept {
    static_assert(sizeof(float) == sizeof(std::uint32_t));
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// Nonnegative finite IEEE-754 values preserve their numeric ordering as bits.
inline std::uint32_t raise_positive_finite_bits(std::uint32_t current,
                                                std::uint32_t requested) noexcept {
    constexpr std::uint32_t sign = 0x80000000u;
    constexpr std::uint32_t exponent = 0x7f800000u;
    if ((current & sign) != 0 || (current & exponent) == exponent ||
        (requested & sign) != 0 || (requested & exponent) == exponent ||
        requested == 0) return current;
    return requested > current ? requested : current;
}

}  // namespace a2fo::refit
