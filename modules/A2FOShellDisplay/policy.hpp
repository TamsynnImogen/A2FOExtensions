#pragma once
#include <cstdint>

namespace a2fo::shell_display {
struct Mode { std::int32_t width, height, depth; };
inline bool valid_mode(const Mode& mode) noexcept {
    return mode.width >= 640 && mode.width <= 16384 &&
           mode.height >= 480 && mode.height <= 16384 &&
           (mode.depth == 16 || mode.depth == 32);
}
inline bool applies(int display, int mode, bool editor) noexcept {
    return display == 0 && mode == 3 && !editor;
}
inline int centered_origin(int start, int end, int size) noexcept {
    return start + ((end - start) > size ? ((end - start) - size) / 2 : 0);
}
} // namespace a2fo::shell_display
