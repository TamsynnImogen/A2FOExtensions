#include "runtime_config.hpp"

#include <cctype>
#include <string>

namespace a2fo::fire_arcs {
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
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') ||
           (value >= '0' && value <= '9') || value == '_';
}

std::string assignment_identifier(
    std::string_view statement, std::size_t equals) {
    std::size_t end = equals;
    while (end != 0 && !identifier_character(statement[end - 1])) --end;
    std::size_t begin = end;
    while (begin != 0 && identifier_character(statement[begin - 1])) --begin;
    return std::string(statement.substr(begin, end - begin));
}

bool parse_boolean_literal(
    std::string_view statement, std::size_t equals, bool* enabled) noexcept {
    if (!enabled) return false;
    std::size_t cursor = equals + 1;
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    if (cursor >= statement.size() ||
        (statement[cursor] != '0' && statement[cursor] != '1')) {
        return false;
    }
    const bool parsed = statement[cursor] == '1';
    ++cursor;
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    if (cursor != statement.size()) return false;
    *enabled = parsed;
    return true;
}

}  // namespace

FireArcSettingStatus parse_firearc_setting(
    std::string_view source, bool* enabled) {
    if (!enabled) return FireArcSettingStatus::invalid;
    const std::string stripped = strip_c_comments(source);
    FireArcSettingStatus status = FireArcSettingStatus::absent;
    bool candidate = *enabled;
    std::size_t begin = 0;
    while (begin < stripped.size()) {
        const std::size_t semicolon = stripped.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? stripped.size() : semicolon;
        const std::string_view statement(stripped.data() + begin,
                                         end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string_view::npos &&
            assignment_identifier(statement, equals) == "firearc") {
            bool parsed = candidate;
            if (parse_boolean_literal(statement, equals, &parsed)) {
                candidate = parsed;
                status = FireArcSettingStatus::valid;
            } else {
                status = FireArcSettingStatus::invalid;
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }
    if (status == FireArcSettingStatus::valid) *enabled = candidate;
    return status;
}

}  // namespace a2fo::fire_arcs
