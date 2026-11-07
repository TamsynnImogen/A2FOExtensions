#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace a2fo::refit_menu::skin {

enum class Role { none, background, widget };

struct Style {
    Role role = Role::none;
    bool hot = false;
    bool pressed = false;
    bool disabled = false;
};

// Accepts a relative path beneath bitmaps/, optionally prefixed by bitmaps/.
Style classify(std::string_view path);

// Returns true when a recognized style was applied. Invalid inputs are untouched.
bool apply(std::uint32_t* pixels, std::size_t stride_pixels,
           int width, int height, Style style) noexcept;

} // namespace a2fo::refit_menu::skin
