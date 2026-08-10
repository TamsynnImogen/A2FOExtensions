/*
 * Host-testable parser for the small subset of ODF syntax used by recursive
 * map-editor menus. Runtime object resolution remains in module.cpp.
 */

#include "edit_menu_odf.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string_view>

namespace a2fo::edit_menu {
namespace {

bool identifier_character(char value) noexcept {
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_';
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return value;
}

void trim(std::string* value) {
    if (!value) return;
    std::size_t begin = 0;
    while (begin < value->size() &&
           std::isspace(static_cast<unsigned char>((*value)[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value->size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>((*value)[end - 1])) != 0) {
        --end;
    }
    *value = value->substr(begin, end - begin);
}

std::string strip_comments(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    bool in_string = false;
    bool escaped = false;
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
        if (!in_string && current == '/' && next == '/') {
            output.append("  ");
            ++index;
            line_comment = true;
            continue;
        }
        if (!in_string && current == '/' && next == '*') {
            output.append("  ");
            ++index;
            block_comment = true;
            continue;
        }

        output.push_back(current);
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (current == '\\') {
                escaped = true;
            } else if (current == '"') {
                in_string = false;
            }
        } else if (current == '"') {
            in_string = true;
        }
    }
    return output;
}

bool parse_value(const std::string& line, std::size_t equals,
                 std::string* output) {
    if (!output || equals >= line.size()) return false;
    std::size_t position = equals + 1;
    while (position < line.size() &&
           std::isspace(static_cast<unsigned char>(line[position])) != 0) {
        ++position;
    }
    if (position >= line.size()) return false;

    if (line[position] != '"') {
        std::string value = line.substr(position);
        trim(&value);
        if (!value.empty() && value.back() == ';') {
            value.pop_back();
            trim(&value);
        }
        *output = std::move(value);
        return !output->empty();
    }

    ++position;
    std::string value;
    bool escaped = false;
    for (; position < line.size(); ++position) {
        const char current = line[position];
        if (escaped) {
            switch (current) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                default: value.push_back(current); break;
            }
            escaped = false;
        } else if (current == '\\') {
            escaped = true;
        } else if (current == '"') {
            *output = std::move(value);
            return true;
        } else {
            value.push_back(current);
        }
    }
    return false;
}

bool numbered_command(const std::string& identifier,
                      std::string_view prefix,
                      std::size_t* index) noexcept {
    if (!index || identifier.size() <= prefix.size() ||
        identifier.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    std::size_t number = 0;
    for (std::size_t position = prefix.size();
         position < identifier.size(); ++position) {
        const char value = identifier[position];
        if (value < '0' || value > '9') return false;
        number = number * 10 + static_cast<std::size_t>(value - '0');
        if (number > kEntryCount) return false;
    }
    if (number == 0) return false;
    *index = number - 1;
    return true;
}

bool parse_bool(const std::string& value, bool* output) {
    if (!output) return false;
    const std::string lowered = lower_ascii(value);
    if (lowered == "true" || lowered == "yes" || lowered == "on") {
        *output = true;
        return true;
    }
    if (lowered == "false" || lowered == "no" || lowered == "off") {
        *output = false;
        return true;
    }
    char* end = nullptr;
    const long parsed = std::strtol(lowered.c_str(), &end, 10);
    if (end == lowered.c_str() || *end != '\0') return false;
    *output = parsed != 0;
    return true;
}

}  // namespace

bool MenuNode::is_submenu() const noexcept {
    return std::any_of(build_items.begin(), build_items.end(),
                       [](const std::string& value) {
                           return !value.empty();
                       });
}

std::string normalize_odf_name(const std::string& value) {
    std::string normalized = value;
    trim(&normalized);
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    const std::size_t slash = normalized.find_last_of('\\');
    if (slash != std::string::npos) normalized.erase(0, slash + 1);
    normalized = lower_ascii(std::move(normalized));
    if (!normalized.empty() &&
        (normalized.size() < 4 ||
         normalized.compare(normalized.size() - 4, 4, ".odf") != 0)) {
        normalized += ".odf";
    }
    return normalized;
}

bool parse_menu_node(const std::string& contents,
                     const std::string& source_name,
                     MenuNode* output,
                     std::string* error) {
    if (error) error->clear();
    if (!output) {
        if (error) *error = "missing output node";
        return false;
    }

    MenuNode parsed{};
    parsed.source_name = normalize_odf_name(source_name);
    const std::string source = strip_comments(contents);
    std::size_t begin = 0;
    while (begin <= source.size()) {
        const std::size_t newline = source.find_first_of("\r\n", begin);
        const std::size_t end = newline == std::string::npos
            ? source.size() : newline;
        const std::string line = source.substr(begin, end - begin);
        const std::size_t equals = line.find('=');
        if (equals != std::string::npos) {
            std::size_t identifier_end = equals;
            while (identifier_end != 0 &&
                   std::isspace(static_cast<unsigned char>(
                       line[identifier_end - 1])) != 0) {
                --identifier_end;
            }
            std::size_t identifier_begin = identifier_end;
            while (identifier_begin != 0 &&
                   identifier_character(line[identifier_begin - 1])) {
                --identifier_begin;
            }
            const std::string identifier = lower_ascii(line.substr(
                identifier_begin, identifier_end - identifier_begin));
            std::string value;
            if (parse_value(line, equals, &value)) {
                std::size_t index = 0;
                if (identifier == "menutitle") {
                    parsed.title = value;
                } else if (numbered_command(
                               identifier, "builditem", &index)) {
                    parsed.build_items[index] = normalize_odf_name(value);
                } else if (numbered_command(identifier, "item", &index)) {
                    parsed.items[index] = value;
                } else if (identifier == "forcetoneutral") {
                    bool enabled = false;
                    if (parse_bool(value, &enabled)) {
                        parsed.force_to_neutral = enabled;
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

    if (parsed.source_name.empty()) {
        if (error) *error = "empty ODF filename";
        return false;
    }
    *output = std::move(parsed);
    return true;
}

}  // namespace a2fo::edit_menu
