#include "extension_roots.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace a2fo {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(
        value.begin(), value.end(),
        [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (first == value.end()) return {};
    const auto last = std::find_if_not(
        value.rbegin(), value.rend(),
        [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    return std::string(first, last);
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

bool ascii_equal(char left, char right) {
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
}

bool switch_at(const std::string& text, std::size_t position) {
    if (position + 4 > text.size() ||
        (text[position] != '/' && text[position] != '-')) {
        return false;
    }
    if (!ascii_equal(text[position + 1], 'm') ||
        !ascii_equal(text[position + 2], 'o') ||
        !ascii_equal(text[position + 3], 'd')) {
        return false;
    }
    if (position != 0) {
        const unsigned char before =
            static_cast<unsigned char>(text[position - 1]);
        if (!std::isspace(before) && text[position - 1] != '"' &&
            text[position - 1] != '\'') {
            return false;
        }
    }
    if (position + 4 == text.size()) return true;
    const unsigned char after =
        static_cast<unsigned char>(text[position + 4]);
    return std::isspace(after) || text[position + 4] == '=' ||
           text[position + 4] == ':' || text[position + 4] == '"' ||
           text[position + 4] == '\\';
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return trim(std::move(value));
}

std::string mod_value_from_info(const std::string& contents,
                                const std::string& requested_key) {
    const std::string wanted = lower_ascii(requested_key);
    std::istringstream lines(contents);
    std::string line;
    bool in_mod_section = false;
    while (std::getline(lines, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(std::move(line));
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            in_mod_section =
                lower_ascii(trim(line.substr(1, line.size() - 2))) == "mod";
            continue;
        }
        if (!in_mod_section) continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        if (lower_ascii(trim(line.substr(0, equals))) == wanted) {
            return unquote(line.substr(equals + 1));
        }
    }
    return {};
}

bool absolute_windows_path(const std::string& path) {
    const bool drive_path =
        path.size() >= 3 &&
        std::isalpha(static_cast<unsigned char>(path[0])) != 0 &&
        path[1] == ':' && (path[2] == '\\' || path[2] == '/');
    const bool unc_path =
        path.size() >= 2 && (path[0] == '\\' || path[0] == '/') &&
        (path[1] == '\\' || path[1] == '/');
    return drive_path || unc_path;
}

std::string normalize_windows_directory(std::string path) {
    for (char& ch : path) {
        if (ch == '/') ch = '\\';
    }
    if (!path.empty() && path.back() != '\\') path.push_back('\\');
    return path;
}

std::size_t find_mods_component(const std::string& path) {
    const std::string lower = lower_ascii(path);
    const std::string component = "\\mods\\";
    return lower.rfind(component);
}

#if defined(_WIN32)
std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool directory_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::string read_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}
#endif

}  // namespace

std::string active_mod_from_command_line(const std::string& command_line) {
    for (std::size_t index = 0; index < command_line.size(); ++index) {
        if (!switch_at(command_line, index)) continue;

        std::size_t cursor = index + 4;
        while (cursor < command_line.size() &&
               std::isspace(static_cast<unsigned char>(command_line[cursor]))) {
            ++cursor;
        }
        if (cursor < command_line.size() &&
            (command_line[cursor] == '=' || command_line[cursor] == ':')) {
            ++cursor;
            while (cursor < command_line.size() &&
                   std::isspace(
                       static_cast<unsigned char>(command_line[cursor]))) {
                ++cursor;
            }
        }

        bool escaped_quotes = false;
        char quote = 0;
        if (cursor + 1 < command_line.size() &&
            command_line[cursor] == '\\' &&
            command_line[cursor + 1] == '"') {
            escaped_quotes = true;
            quote = '"';
            cursor += 2;
        } else if (cursor < command_line.size() &&
                   (command_line[cursor] == '"' ||
                    command_line[cursor] == '\'')) {
            quote = command_line[cursor++];
        }

        const std::size_t start = cursor;
        if (quote != 0) {
            while (cursor < command_line.size()) {
                if (escaped_quotes && cursor + 1 < command_line.size() &&
                    command_line[cursor] == '\\' &&
                    command_line[cursor + 1] == quote) {
                    return trim(command_line.substr(start, cursor - start));
                }
                if (!escaped_quotes && command_line[cursor] == quote) {
                    return trim(command_line.substr(start, cursor - start));
                }
                ++cursor;
            }
        } else {
            while (cursor < command_line.size() &&
                   !std::isspace(
                       static_cast<unsigned char>(command_line[cursor])) &&
                   command_line[cursor] != '"') {
                ++cursor;
            }
        }
        return trim(command_line.substr(start, cursor - start));
    }
    return {};
}

std::string parent_mod_from_info(const std::string& contents) {
    return mod_value_from_info(contents, "ParentMod");
}

FleetOpsInfoDefaults fleet_ops_defaults_from_info(
    const std::string& contents) {
    FleetOpsInfoDefaults result;
    result.settings_directory =
        mod_value_from_info(contents, "SettingsDirectory");

    const std::string speed =
        mod_value_from_info(contents, "DefaultGameSpeed");
    if (speed.empty()) return result;

    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(speed.c_str(), &end, 10);
    if (errno == 0 && end && *end == '\0' && parsed >= 1 && parsed <= 6) {
        result.has_default_game_speed = true;
        result.default_game_speed = static_cast<int>(parsed);
    }
    return result;
}

std::string resolve_fleet_ops_settings_directory(
    const std::string& normal_mod_directory,
    const std::string& configured_directory,
    const std::string& fallback_config_root,
    const std::string& shared_settings_root) {
    std::string configured = trim(configured_directory);
    if (configured.empty()) return {};
    configured = normalize_windows_directory(std::move(configured));
    if (absolute_windows_path(configured)) return configured;

    const bool bare_name =
        configured.find('\\') == configured.size() - 1;
    if (bare_name && !trim(shared_settings_root).empty()) {
        std::string root = normalize_windows_directory(
            trim(shared_settings_root));
        if (!absolute_windows_path(root)) {
            root = resolve_fleet_ops_settings_directory(
                normal_mod_directory, root, fallback_config_root);
        }
        if (!root.empty()) {
            return normalize_windows_directory(
                root + "mods\\" + configured);
        }
    }

    std::string base = normalize_windows_directory(normal_mod_directory);
    const std::size_t mods = find_mods_component(base);
    if (mods != std::string::npos) {
        base.resize(mods + 1);
    } else {
        base = normalize_windows_directory(fallback_config_root);
    }
    if (base.empty()) return {};

    if (bare_name) configured.insert(0, "mods\\");
    return normalize_windows_directory(base + configured);
}

bool safe_mod_directory_name(const std::string& name) {
    const std::string value = trim(name);
    if (value.empty() || value == "." || value == "..") return false;
    for (unsigned char ch : value) {
        if (ch < 0x20 || ch == '/' || ch == '\\' || ch == ':' || ch == '*'
            || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|') {
            return false;
        }
    }
    return true;
}

#if defined(_WIN32)
ExtensionRootDiscovery discover_extension_roots(
    const std::string& data_root,
    const std::string& command_line) {
    ExtensionRootDiscovery result;
    result.roots.push_back(data_root);
    result.active_mod = active_mod_from_command_line(command_line);
    if (result.active_mod.empty()) {
        result.diagnostics.push_back(
            "Extension roots: no /mod argument; using Data only");
        return result;
    }
    if (!safe_mod_directory_name(result.active_mod)) {
        result.diagnostics.push_back(
            "Extension roots: rejected unsafe mod name");
        result.active_mod.clear();
        return result;
    }

    std::vector<std::string> chain;
    std::set<std::string> seen;
    std::string current = result.active_mod;
    constexpr std::size_t kMaximumParentDepth = 32;
    for (std::size_t depth = 0; depth < kMaximumParentDepth; ++depth) {
        const std::string key = lower_ascii(current);
        if (!seen.insert(key).second) {
            result.diagnostics.push_back(
                "Extension roots: ParentMod cycle stopped at " + current);
            break;
        }
        const std::string root =
            join_path(join_path(data_root, "Mods"), current);
        if (!directory_exists(root)) {
            result.diagnostics.push_back(
                "Extension roots: mod directory not found: " + root);
            break;
        }
        chain.push_back(root);

        const std::string parent = parent_mod_from_info(
            read_text_file(join_path(root, "info.ini")));
        if (parent.empty()) break;
        if (!safe_mod_directory_name(parent)) {
            result.diagnostics.push_back(
                "Extension roots: rejected unsafe ParentMod for " + current);
            break;
        }
        current = parent;
    }
    if (chain.size() == kMaximumParentDepth) {
        result.diagnostics.push_back(
            "Extension roots: ParentMod depth limit reached");
    }

    std::reverse(chain.begin(), chain.end());
    result.roots.insert(result.roots.end(), chain.begin(), chain.end());
    return result;
}
#endif

}  // namespace a2fo
