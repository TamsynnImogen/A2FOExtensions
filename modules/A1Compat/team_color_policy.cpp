#include "team_color_policy.hpp"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace a1compat {
namespace {

constexpr std::array<const char*, 13> kLegacyPlayerColorNames{{
    "white", "red", "blue", "green", "yellow", "purple", "cyan",
    "brown", "orange", "pink", "magenta", "gray", "black",
}};

struct TeamColorLayer {
    std::array<TeamColorRgb, kPlayerTeamColorCount> indexed{};
    std::array<bool, kPlayerTeamColorCount> indexed_present{};
    std::array<TeamColorRgb, kLegacyPlayerColorNames.size()> legacy{};
    std::array<bool, kLegacyPlayerColorNames.size()> legacy_present{};
};

std::string strip_comments(std::string_view input) {
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

void trim(std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    value = value.substr(begin, end - begin);
}

std::string lower_ascii(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool parse_rgb(std::string value, TeamColorRgb& color) {
    trim(value);
    const char* cursor = value.c_str();
    float components[3]{};
    for (float& component : components) {
        while (*cursor != '\0' &&
               std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        errno = 0;
        char* end = nullptr;
        component = std::strtof(cursor, &end);
        if (end == cursor || errno == ERANGE || !std::isfinite(component)) {
            return false;
        }
        cursor = end;
        if (*cursor == 'f' || *cursor == 'F') ++cursor;
    }
    while (*cursor != '\0' &&
           std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    if (*cursor == ';') {
        ++cursor;
        while (*cursor != '\0' &&
               std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
    }
    if (*cursor != '\0') return false;
    color = TeamColorRgb{components[0], components[1], components[2]};
    return true;
}

int indexed_player_slot(const std::string& key) {
    constexpr std::size_t kPrefixLength = 7;
    if (key.size() != kPrefixLength + 2 ||
        key.compare(0, kPrefixLength, "mpcolor") != 0 ||
        !std::isdigit(static_cast<unsigned char>(key[kPrefixLength])) ||
        !std::isdigit(static_cast<unsigned char>(key[kPrefixLength + 1]))) {
        return -1;
    }
    const int number = (key[kPrefixLength] - '0') * 10 +
        (key[kPrefixLength + 1] - '0');
    return number >= 1 && number <=
            static_cast<int>(kPlayerTeamColorCount)
        ? number - 1 : -1;
}

int legacy_player_slot(const std::string& key) {
    for (std::size_t index = 0; index < kLegacyPlayerColorNames.size();
         ++index) {
        if (key == kLegacyPlayerColorNames[index]) {
            return static_cast<int>(index);
        }
    }
    // Accept the alternate spelling without changing A1's native gray slot.
    return key == "grey" ? 11 : -1;
}

TeamColorLayer parse_layer(std::string_view contents) {
    TeamColorLayer layer;
    const std::string source = strip_comments(contents);
    std::size_t begin = 0;
    while (begin <= source.size()) {
        const std::size_t newline = source.find_first_of("\r\n", begin);
        const std::size_t end = newline == std::string::npos
            ? source.size() : newline;
        std::string line = source.substr(begin, end - begin);
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            std::string key = line.substr(0, equals);
            std::string value = line.substr(equals + 1);
            trim(key);
            key = lower_ascii(key);
            TeamColorRgb color{};
            if (parse_rgb(value, color)) {
                const int indexed = indexed_player_slot(key);
                if (indexed >= 0) {
                    layer.indexed[static_cast<std::size_t>(indexed)] = color;
                    layer.indexed_present[
                        static_cast<std::size_t>(indexed)] = true;
                } else {
                    const int legacy = legacy_player_slot(key);
                    if (legacy >= 0) {
                        layer.legacy[static_cast<std::size_t>(legacy)] = color;
                        layer.legacy_present[
                            static_cast<std::size_t>(legacy)] = true;
                    }
                }
            }
        }
        if (newline == std::string::npos) break;
        begin = newline + 1;
        if (begin < source.size() && source[newline] == '\r' &&
            source[begin] == '\n') {
            ++begin;
        }
    }
    return layer;
}

}  // namespace

TeamColorMergeResult merge_team_color_odf(
    std::string_view contents, TeamColorPalettePolicy& palette) {
    const TeamColorLayer layer = parse_layer(contents);
    TeamColorMergeResult result{};
    for (std::size_t index = 0; index < kPlayerTeamColorCount; ++index) {
        if (layer.indexed_present[index]) {
            palette.colors[index] = layer.indexed[index];
            palette.present[index] = true;
            ++result.indexed_values;
        } else if (index < layer.legacy_present.size() &&
                   layer.legacy_present[index]) {
            palette.colors[index] = layer.legacy[index];
            palette.present[index] = true;
            ++result.legacy_aliases;
        }
    }
    return result;
}

}  // namespace a1compat
