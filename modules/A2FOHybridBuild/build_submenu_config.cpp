/*
 * Host-testable parser for live Producer build-submenu commands.
 */

#include "build_submenu_config.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace a2fo::build_submenu {
namespace {

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
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

bool identifier_character(char value) noexcept {
    const unsigned char byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_';
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
            value.push_back(current);
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

bool parse_index(std::string_view text, std::size_t* value) noexcept {
    if (!value || text.empty()) return false;
    std::size_t parsed = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') return false;
        parsed = parsed * 10 + static_cast<std::size_t>(ch - '0');
        if (parsed >= kBuildItemCount) return false;
    }
    *value = parsed;
    return true;
}

bool parse_command(const std::string& identifier, std::size_t* parent,
                   std::size_t* child) noexcept {
    constexpr std::string_view prefix = "builditem";
    constexpr std::string_view infix = "refit";
    if (identifier.size() <= prefix.size() + infix.size() ||
        identifier.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const std::size_t split = identifier.find(infix, prefix.size());
    if (split == std::string::npos || split == prefix.size() ||
        split + infix.size() == identifier.size()) {
        return false;
    }
    return parse_index(
               std::string_view(identifier).substr(
                   prefix.size(), split - prefix.size()),
               parent) &&
        parse_index(std::string_view(identifier).substr(
                        split + infix.size()),
                    child);
}

}  // namespace

bool Page::empty() const noexcept {
    return std::all_of(children.begin(), children.end(),
                       [](const std::string& child) {
                           return child.empty();
                       });
}

bool Config::empty() const noexcept {
    return std::all_of(pages.begin(), pages.end(),
                       [](const Page& page) { return page.empty(); });
}

bool parse_config(const std::string& contents, Config* output,
                  std::string* error) {
    if (error) error->clear();
    if (!output) {
        if (error) *error = "missing output config";
        return false;
    }
    Config parsed{};
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
            std::size_t parent = 0;
            std::size_t child = 0;
            std::string value;
            if (parse_command(identifier, &parent, &child) &&
                parse_value(line, equals, &value)) {
                parsed.pages[parent].children[child] =
                    normalize_object_name(value);
            }
        }
        if (newline == std::string::npos) break;
        begin = newline + 1;
        if (begin < source.size() && source[begin] == '\n' &&
            source[newline] == '\r') {
            ++begin;
        }
    }
    *output = std::move(parsed);
    return true;
}

std::string normalize_object_name(const std::string& value) {
    std::string normalized = value;
    trim(&normalized);
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    const std::size_t slash = normalized.find_last_of('\\');
    if (slash != std::string::npos) normalized.erase(0, slash + 1);
    normalized = lower_ascii(std::move(normalized));
    if (normalized.size() > 4 &&
        normalized.compare(normalized.size() - 4, 4, ".odf") == 0) {
        normalized.resize(normalized.size() - 4);
    }
    return normalized;
}

}  // namespace a2fo::build_submenu
