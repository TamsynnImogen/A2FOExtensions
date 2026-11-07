#include "team_color_policy.hpp"
#include "../../rust/config_ffi.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <string>
namespace a1compat {
namespace {
uint8_t parse_float(void *p, const uint8_t *data, size_t size, size_t *consumed,
                    float *value) noexcept {
    auto &error = *static_cast<std::exception_ptr *>(p);
    if (error)
        return 0;
    try {
        std::string text(reinterpret_cast<const char *>(data), size);
        errno = 0;
        char *end = nullptr;
        const float parsed = std::strtof(text.c_str(), &end);
        if (end == text.c_str() || errno == ERANGE || !std::isfinite(parsed))
            return 0;
        *consumed = static_cast<size_t>(end - text.c_str());
        *value = parsed;
        return 1;
    } catch (...) {
        error = std::current_exception();
        return 0;
    }
}
} // namespace
TeamColorMergeResult merge_team_color_odf(std::string_view contents,
                                          TeamColorPalettePolicy &palette) {
    A2foRsColor colors[kPlayerTeamColorCount]{};
    for (std::size_t i = 0; i < kPlayerTeamColorCount; ++i) {
        colors[i] = {palette.colors[i].red, palette.colors[i].green, palette.colors[i].blue,
                     static_cast<uint8_t>(palette.present[i])};
    }
    A2foRsColorReport report{};
    std::exception_ptr error;
    a2fo_rs_config_team({reinterpret_cast<const uint8_t *>(contents.data()), contents.size()},
                        colors, &report, parse_float, &error);
    if (error)
        std::rethrow_exception(error);
    for (std::size_t i = 0; i < kPlayerTeamColorCount; ++i) {
        palette.colors[i] = {colors[i].red, colors[i].green, colors[i].blue};
        palette.present[i] = colors[i].present != 0;
    }
    return {report.indexed, report.legacy};
}
} // namespace a1compat
