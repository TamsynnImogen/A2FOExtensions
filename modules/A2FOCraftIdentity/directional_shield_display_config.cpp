#include "directional_shield_display_config.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <string>

namespace a2fo::craft_identity {
namespace {

std::string strip_c_comments(std::string_view input) {
    std::string output;
    output.reserve(input.size());
    bool line_comment = false;
    bool block_comment = false;
    for (std::size_t index = 0; index < input.size(); ++index) {
        const char current = input[index];
        const char next = index + 1 < input.size() ? input[index + 1] : '\0';
        if (line_comment) {
            if (current == '\n' || current == '\r') {
                line_comment = false;
                output.push_back(current);
            } else {
                output.push_back(' ');
            }
            continue;
        }
        if (block_comment) {
            if (current == '*' && next == '/') {
                output.append("  ");
                ++index;
                block_comment = false;
            } else {
                output.push_back(
                    current == '\n' || current == '\r' ? current : ' ');
            }
            continue;
        }
        if (current == '/' && next == '/') {
            output.append("  ");
            ++index;
            line_comment = true;
        } else if (current == '/' && next == '*') {
            output.append("  ");
            ++index;
            block_comment = true;
        } else {
            output.push_back(current);
        }
    }
    return output;
}

bool identifier_character(char value) noexcept {
    return std::isalnum(static_cast<unsigned char>(value)) != 0 ||
        value == '_';
}

std::string assignment_identifier(
    std::string_view statement, std::size_t equals) {
    std::size_t end = equals;
    while (end != 0 && !identifier_character(statement[end - 1])) --end;
    std::size_t begin = end;
    while (begin != 0 && identifier_character(statement[begin - 1])) --begin;
    std::string identifier(statement.substr(begin, end - begin));
    std::transform(
        identifier.begin(), identifier.end(), identifier.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return identifier;
}

bool parse_integer_literal(
    std::string_view statement, std::size_t equals, int* result) noexcept {
    if (!result) return false;
    std::size_t cursor = equals + 1;
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    int sign = 1;
    if (cursor < statement.size() &&
        (statement[cursor] == '+' || statement[cursor] == '-')) {
        if (statement[cursor] == '-') sign = -1;
        ++cursor;
    }
    if (cursor >= statement.size() || !std::isdigit(
            static_cast<unsigned char>(statement[cursor]))) {
        return false;
    }
    int value = 0;
    while (cursor < statement.size() && std::isdigit(
               static_cast<unsigned char>(statement[cursor]))) {
        value = value * 10 + (statement[cursor] - '0');
        if (value > 1000) return false;
        ++cursor;
    }
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    if (cursor != statement.size()) return false;
    *result = sign * value;
    return true;
}

int facing_for_identifier(const std::string& identifier) noexcept {
    if (identifier == "directionalshieldforwardposition") return 0;
    if (identifier == "directionalshieldaftposition") return 1;
    if (identifier == "directionalshieldportposition") return 2;
    if (identifier == "directionalshieldstarboardposition") return 3;
    return -1;
}

bool usable_rectangle(const RectangleF& rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y) &&
        std::isfinite(rectangle.width) && std::isfinite(rectangle.height) &&
        rectangle.width > 0.0f && rectangle.height > 0.0f;
}

}  // namespace

DirectionalShieldDisplayParseReport parse_directional_shield_display_config(
    std::string_view source, DirectionalShieldDisplayConfig* config) {
    DirectionalShieldDisplayParseReport report{};
    if (!config) {
        report.invalid_assignments = 1;
        return report;
    }

    const std::string stripped = strip_c_comments(source);
    std::size_t begin = 0;
    while (begin < stripped.size()) {
        const std::size_t semicolon = stripped.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? stripped.size() : semicolon;
        const std::string_view statement(
            stripped.data() + begin, end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string_view::npos) {
            const std::string identifier =
                assignment_identifier(statement, equals);
            const bool display_mode =
                identifier == "directionalshielddisplaymode";
            const int facing = facing_for_identifier(identifier);
            if (display_mode || facing >= 0) {
                int value = 0;
                const bool parsed = parse_integer_literal(
                    statement, equals, &value);
                const bool in_range = display_mode
                    ? value >= 1 && value <= 2
                    : value >= 0 && value <= 3;
                if (!parsed || !in_range) {
                    ++report.invalid_assignments;
                } else if (display_mode) {
                    config->display_mode = value;
                    report.display_mode_found = true;
                    ++report.valid_assignments;
                } else {
                    const std::size_t index =
                        static_cast<std::size_t>(facing);
                    config->facing_positions[index] = value;
                    config->position_mapping_configured = true;
                    report.position_found[index] = true;
                    ++report.valid_assignments;
                }
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }
    return report;
}

bool valid_directional_shield_position_mapping(
    const DirectionalShieldDisplayConfig& config) noexcept {
    std::array<bool, 4> used{};
    for (const int position : config.facing_positions) {
        if (position < 0 || position >= static_cast<int>(used.size()) ||
            used[static_cast<std::size_t>(position)]) {
            return false;
        }
        used[static_cast<std::size_t>(position)] = true;
    }
    return true;
}

bool remap_directional_shield_positions(
    const DirectionalShieldDisplayConfig& config,
    const std::array<RectangleF, 4>& input,
    std::array<RectangleF, 4>* output) noexcept {
    if (!output || !config.position_mapping_configured ||
        !valid_directional_shield_position_mapping(config)) {
        return false;
    }

    std::array<std::size_t, 2> horizontal{};
    std::array<std::size_t, 2> vertical{};
    std::size_t horizontal_count = 0;
    std::size_t vertical_count = 0;
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (!usable_rectangle(input[index])) return false;
        if (input[index].width >= input[index].height) {
            if (horizontal_count >= horizontal.size()) return false;
            horizontal[horizontal_count++] = index;
        } else {
            if (vertical_count >= vertical.size()) return false;
            vertical[vertical_count++] = index;
        }
    }
    if (horizontal_count != 2 || vertical_count != 2) return false;
    if (input[horizontal[0]].y + input[horizontal[0]].height * 0.5f >
        input[horizontal[1]].y + input[horizontal[1]].height * 0.5f) {
        std::swap(horizontal[0], horizontal[1]);
    }
    if (input[vertical[0]].x + input[vertical[0]].width * 0.5f >
        input[vertical[1]].x + input[vertical[1]].width * 0.5f) {
        std::swap(vertical[0], vertical[1]);
    }

    // Position codes: north, east, south, west.
    const std::array<std::size_t, 4> slot_indices{{
        horizontal[0], vertical[1], horizontal[1], vertical[0]}};
    std::array<RectangleF, 4> remapped{};
    for (std::size_t facing = 0; facing < remapped.size(); ++facing) {
        const std::size_t position = static_cast<std::size_t>(
            config.facing_positions[facing]);
        remapped[facing] = input[slot_indices[position]];
    }
    *output = remapped;
    return true;
}

}  // namespace a2fo::craft_identity
