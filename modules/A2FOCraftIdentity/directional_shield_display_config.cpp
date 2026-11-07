#include "directional_shield_display_config.hpp"
#include "../../rust/gameplay_ffi.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

namespace a2fo::craft_identity {
namespace {
A2foRsShieldConfig pod(const DirectionalShieldDisplayConfig &config) noexcept {
    return {config.display_mode,
            {config.facing_positions[0], config.facing_positions[1], config.facing_positions[2],
             config.facing_positions[3]},
            static_cast<std::uint8_t>(config.position_mapping_configured)};
}

} // namespace

bool format_directional_shield_value(DirectionalShieldValueDisplayMode mode, float current,
                                     float maximum, char *output,
                                     std::size_t output_size) noexcept {
    if (!output || output_size == 0)
        return false;
    output[0] = '\0';
    double selected = 0, selected_maximum = 0;
    if (!a2fo_rs_gameplay_shield_value(static_cast<std::uint32_t>(mode), current, maximum,
                                       &selected, &selected_maximum))
        return false;
    int written = -1;
    if (mode == DirectionalShieldValueDisplayMode::percent) {
        const double rounded_percent = selected;
        written = std::snprintf(output, output_size, "%.0f", rounded_percent);
    } else if (mode == DirectionalShieldValueDisplayMode::amount) {
        const double rounded_current = selected;
        const double rounded_maximum = selected_maximum;
        written = std::snprintf(output, output_size, "%.0f/%.0f", rounded_current, rounded_maximum);
    }
    if (written < 0 || static_cast<std::size_t>(written) >= output_size) {
        output[0] = '\0';
        return false;
    }
    return written > 0;
}

DirectionalShieldDisplayParseReport
parse_directional_shield_display_config(std::string_view source,
                                        DirectionalShieldDisplayConfig *config) {
    A2foRsShieldConfig native = config ? pod(*config) : A2foRsShieldConfig{};
    A2foRsShieldReport parsed{};
    a2fo_rs_gameplay_parse_shield_config(reinterpret_cast<const std::uint8_t *>(source.data()),
                                         source.size(), config ? &native : nullptr, &parsed);
    if (config) {
        config->display_mode = native.display_mode;
        for (std::size_t i = 0; i < 4; ++i)
            config->facing_positions[i] = native.positions[i];
        config->position_mapping_configured = native.configured != 0;
    }
    DirectionalShieldDisplayParseReport report{};
    report.valid_assignments = parsed.valid;
    report.invalid_assignments = parsed.invalid;
    report.display_mode_found = parsed.display_found != 0;
    for (std::size_t i = 0; i < 4; ++i)
        report.position_found[i] = parsed.position_found[i] != 0;
    return report;
}

bool valid_directional_shield_position_mapping(
    const DirectionalShieldDisplayConfig &config) noexcept {
    const auto native = pod(config);
    return a2fo_rs_gameplay_valid_shield_mapping(&native) != 0;
}

bool remap_directional_shield_positions(const DirectionalShieldDisplayConfig &config,
                                        const std::array<RectangleF, 4> &input,
                                        std::array<RectangleF, 4> *output,
                                        std::array<std::size_t, 4> *source_indices) noexcept {
    if (!output)
        return false;
    const auto native = pod(config);
    A2foRsRect rectangles[4]{};
    for (std::size_t i = 0; i < 4; ++i)
        rectangles[i] = {input[i].x, input[i].y, input[i].width, input[i].height};
    std::array<RectangleF, 4> remapped{};
    std::array<std::size_t, 4> remapped_indices{};
    if (!a2fo_rs_gameplay_remap_shield(&native, rectangles, remapped_indices.data()))
        return false;
    for (std::size_t facing = 0; facing < remapped.size(); ++facing) {
        remapped[facing] = input[remapped_indices[facing]];
    }
    *output = remapped;
    if (source_indices)
        *source_indices = remapped_indices;
    return true;
}

void scale_directional_shield_default_positions(std::array<RectangleF, 4> *positions,
                                                const std::array<bool, 4> &authored_positions,
                                                float graphic_width,
                                                float graphic_height) noexcept {
    if (!positions)
        return;
    A2foRsRect rectangles[4]{};
    std::uint8_t authored[4]{};
    for (std::size_t i = 0; i < 4; ++i) {
        rectangles[i] = {(*positions)[i].x, (*positions)[i].y, (*positions)[i].width,
                         (*positions)[i].height};
        authored[i] = authored_positions[i];
    }
    a2fo_rs_gameplay_scale_shield(rectangles, authored, graphic_width, graphic_height);
    for (std::size_t i = 0; i < 4; ++i)
        (*positions)[i] = {rectangles[i].x, rectangles[i].y, rectangles[i].width,
                           rectangles[i].height};
}

} // namespace a2fo::craft_identity
