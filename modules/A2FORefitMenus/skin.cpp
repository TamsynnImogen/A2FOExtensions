#include "skin.hpp"
#include "../../rust/ui_ffi.h"
namespace a2fo::refit_menu::skin {
Style classify(std::string_view path) {
    const auto bits =
        a2fo_rs_ui_style(reinterpret_cast<const std::uint8_t *>(path.data()), path.size());
    return {static_cast<Role>(bits & 3), (bits & 4) != 0, (bits & 8) != 0, (bits & 16) != 0};
}
bool apply(std::uint32_t *pixels, std::size_t stride_pixels, int width, int height,
           Style style) noexcept {
    return a2fo_rs_ui_apply(pixels, stride_pixels, width, height,
                            {static_cast<std::uint32_t>(style.role),
                             static_cast<std::uint8_t>(style.hot),
                             static_cast<std::uint8_t>(style.pressed),
                             static_cast<std::uint8_t>(style.disabled)}) != 0;
}
} // namespace a2fo::refit_menu::skin
