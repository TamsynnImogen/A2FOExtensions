#include "upgrade_pod_config.hpp"

#include <cctype>
#include <string>

namespace a2fo::upgrade_pods {
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
    return std::string(statement.substr(begin, end - begin));
}

bool parse_integer_literal(
    std::string_view statement, std::size_t equals,
    std::uint32_t* output) noexcept {
    if (!output) return false;
    std::size_t cursor = equals + 1;
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    if (cursor >= statement.size() || !std::isdigit(
            static_cast<unsigned char>(statement[cursor]))) {
        return false;
    }
    std::uint32_t value = 0;
    while (cursor < statement.size() && std::isdigit(
               static_cast<unsigned char>(statement[cursor]))) {
        value = value * 10u + static_cast<std::uint32_t>(
            statement[cursor] - '0');
        if (value > 1000u) return false;
        ++cursor;
    }
    while (cursor < statement.size() && std::isspace(
               static_cast<unsigned char>(statement[cursor]))) {
        ++cursor;
    }
    if (cursor != statement.size()) return false;
    *output = value;
    return true;
}

}  // namespace

MaximumTierSettingStatus parse_maximum_tier_setting(
    std::string_view source, std::uint32_t* maximum_tier) {
    if (!maximum_tier) return MaximumTierSettingStatus::invalid;
    const std::string stripped = strip_c_comments(source);
    MaximumTierSettingStatus status = MaximumTierSettingStatus::absent;
    std::uint32_t candidate = *maximum_tier;
    std::size_t begin = 0;
    while (begin < stripped.size()) {
        const std::size_t semicolon = stripped.find(';', begin);
        const std::size_t end = semicolon == std::string::npos
            ? stripped.size() : semicolon;
        const std::string_view statement(
            stripped.data() + begin, end - begin);
        const std::size_t equals = statement.find('=');
        if (equals != std::string_view::npos &&
            assignment_identifier(statement, equals) ==
                "upgradePodMaximumTier") {
            std::uint32_t parsed = candidate;
            if (parse_integer_literal(statement, equals, &parsed) &&
                parsed >= 3u && parsed <= 16u) {
                candidate = parsed;
                status = MaximumTierSettingStatus::valid;
            } else {
                status = MaximumTierSettingStatus::invalid;
            }
        }
        if (semicolon == std::string::npos) break;
        begin = semicolon + 1;
    }
    if (status == MaximumTierSettingStatus::valid) {
        *maximum_tier = candidate;
    }
    return status;
}

}  // namespace a2fo::upgrade_pods
