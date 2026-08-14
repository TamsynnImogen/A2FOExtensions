/*
 * File: core/module_policy.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Per-mod native-module policy parsing, evaluation, and persistence.
 */

#include "module_policy.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
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

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return trim(std::move(value));
}

bool indexed_key(const std::string& key, const char* prefix) {
    const std::string lower = lower_ascii(trim(key));
    const std::string wanted(prefix);
    if (lower.size() <= wanted.size() ||
        lower.compare(0, wanted.size(), wanted) != 0) {
        return false;
    }
    return std::all_of(
        lower.begin() + static_cast<std::ptrdiff_t>(wanted.size()), lower.end(),
        [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

void append_unique(std::vector<std::string>& values,
                   std::set<std::string>& keys,
                   const std::string& value) {
    const std::string name = normalize_module_name(value);
    const std::string key = module_name_key(name);
    if (!key.empty() && keys.insert(key).second) values.push_back(name);
}

std::vector<std::string> split_lines(const std::string& contents) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < contents.size()) {
        const std::size_t newline = contents.find('\n', start);
        std::size_t end = newline == std::string::npos ? contents.size() : newline;
        if (end > start && contents[end - 1] == '\r') --end;
        lines.push_back(contents.substr(start, end - start));
        if (newline == std::string::npos) return lines;
        start = newline + 1;
    }
    if (contents.empty() || contents.back() == '\n') lines.emplace_back();
    return lines;
}

bool section_line(const std::string& line, std::string& name) {
    const std::string cleaned = trim(line);
    if (cleaned.size() < 2 || cleaned.front() != '[' ||
        cleaned.back() != ']') {
        return false;
    }
    name = lower_ascii(trim(cleaned.substr(1, cleaned.size() - 2)));
    return true;
}

bool active_assignment(const std::string& line) {
    std::string cleaned = trim(line);
    if (cleaned.empty() || cleaned.front() == ';' || cleaned.front() == '#') {
        return false;
    }
    const std::size_t equals = cleaned.find('=');
    return equals != std::string::npos &&
           indexed_key(cleaned.substr(0, equals), "active");
}

#if defined(_WIN32)
std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool has_dll_extension(const std::string& name) {
    return name.size() >= 4 &&
           _stricmp(name.c_str() + name.size() - 4, ".dll") == 0;
}

std::string read_text_file(const std::string& path, bool& opened) {
    std::ifstream input(path, std::ios::binary);
    opened = static_cast<bool>(input);
    if (!input) return {};
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}
#endif

}  // namespace

std::string normalize_module_name(const std::string& value) {
    std::string name = unquote(value);
    if (name.size() >= 4 &&
        lower_ascii(name.substr(name.size() - 4)) == ".dll") {
        name.resize(name.size() - 4);
        name = trim(std::move(name));
    }
    if (name.empty() || name == "." || name == ".." ||
        name.find('\\') != std::string::npos ||
        name.find('/') != std::string::npos ||
        name.find(':') != std::string::npos ||
        name.find('"') != std::string::npos ||
        name.find('\'') != std::string::npos) {
        return {};
    }
    return name;
}

std::string module_name_key(const std::string& value) {
    return lower_ascii(normalize_module_name(value));
}

ModuleRules module_rules_from_info(const std::string& contents) {
    ModuleRules result;
    std::set<std::string> required_keys;
    std::set<std::string> rejected_keys;
    std::set<std::string> active_keys;
    bool in_modules = false;
    std::istringstream stream(contents);
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(stream, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string cleaned = trim(line);
        if (cleaned.empty() || cleaned.front() == ';' ||
            cleaned.front() == '#') {
            continue;
        }
        std::string section;
        if (section_line(cleaned, section)) {
            in_modules = section == "modules";
            if (in_modules) result.has_section = true;
            continue;
        }
        if (!in_modules) continue;
        const std::size_t equals = cleaned.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = cleaned.substr(0, equals);
        const std::string value = unquote(cleaned.substr(equals + 1));
        if (!indexed_key(key, "required") &&
            !indexed_key(key, "reject") &&
            !indexed_key(key, "active")) {
            continue;
        }
        if (value.empty()) continue;
        const std::string name = normalize_module_name(value);
        if (name.empty()) {
            result.diagnostics.push_back(
                "Invalid module reference on info.ini line " +
                std::to_string(line_number) + ": " + value);
            continue;
        }
        if (indexed_key(key, "required")) {
            append_unique(result.required, required_keys, name);
        } else if (indexed_key(key, "reject")) {
            append_unique(result.rejected, rejected_keys, name);
        } else {
            append_unique(result.active, active_keys, name);
        }
    }
    return result;
}

ModulePolicy evaluate_module_rules(
    const std::vector<ModuleRules>& ordered_rules,
    const std::vector<InstalledModule>& installed) {
    ModulePolicy result;
    std::map<std::string, std::string> required;
    std::map<std::string, std::string> rejected;
    std::map<std::string, std::string> active;

    for (const ModuleRules& rules : ordered_rules) {
        result.managed = result.managed || rules.has_section;
        result.diagnostics.insert(result.diagnostics.end(),
                                  rules.diagnostics.begin(),
                                  rules.diagnostics.end());
        for (const std::string& name : rules.required) {
            required.emplace(module_name_key(name), name);
        }
        for (const std::string& name : rules.rejected) {
            rejected.emplace(module_name_key(name), name);
        }
        if (rules.has_section) {
            active.clear();
            for (const std::string& name : rules.active) {
                active.emplace(module_name_key(name), name);
            }
        }
    }

    std::map<std::string, InstalledModule> inventory;
    for (const InstalledModule& module : installed) {
        const std::string key = module_name_key(module.name.empty()
                                                    ? module.filename
                                                    : module.name);
        if (key.empty()) continue;
        inventory.emplace(key, module);
    }

    std::set<std::string> emitted;
    for (const auto& item : inventory) {
        const std::string& key = item.first;
        const InstalledModule& module = item.second;
        ModulePolicyEntry entry;
        entry.key = key;
        entry.name = module.name.empty()
                         ? normalize_module_name(module.filename)
                         : module.name;
        entry.filename = module.filename;
        entry.path = module.path;
        entry.installed = true;
        const bool is_required = required.count(key) != 0;
        const bool is_rejected = rejected.count(key) != 0;
        if (is_required && is_rejected) {
            entry.state = ModulePolicyState::conflict;
            result.valid = false;
            result.diagnostics.push_back(
                "Module is both required and rejected: " + entry.name);
        } else if (is_rejected) {
            entry.state = ModulePolicyState::rejected;
        } else if (is_required) {
            entry.state = ModulePolicyState::required;
        } else if (!result.managed || active.count(key) != 0) {
            entry.state = ModulePolicyState::active;
        } else {
            entry.state = ModulePolicyState::inactive;
        }
        emitted.insert(key);
        result.entries.push_back(std::move(entry));
    }

    for (const auto& item : required) {
        if (emitted.count(item.first) != 0) continue;
        ModulePolicyEntry entry;
        entry.key = item.first;
        entry.name = item.second;
        entry.state = rejected.count(item.first) != 0
                          ? ModulePolicyState::conflict
                          : ModulePolicyState::missing_required;
        result.valid = false;
        if (entry.state == ModulePolicyState::conflict) {
            result.diagnostics.push_back(
                "Missing module is both required and rejected: " + entry.name);
        } else {
            result.diagnostics.push_back(
                "Required module is not installed: " + entry.name);
        }
        emitted.insert(item.first);
        result.entries.push_back(std::move(entry));
    }

    for (const auto& item : active) {
        if (emitted.count(item.first) != 0 ||
            rejected.count(item.first) != 0) {
            continue;
        }
        ModulePolicyEntry entry;
        entry.key = item.first;
        entry.name = item.second;
        entry.state = ModulePolicyState::missing_active;
        result.entries.push_back(std::move(entry));
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [](const ModulePolicyEntry& left,
                 const ModulePolicyEntry& right) {
                  return left.key < right.key;
              });
    return result;
}

std::string rewrite_active_modules(
    const std::string& contents,
    const std::vector<std::string>& active_modules) {
    const std::string newline =
        contents.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::vector<std::string> lines = split_lines(contents);
    std::vector<std::string> output;
    output.reserve(lines.size() + active_modules.size() + 3);
    bool found_modules = false;
    bool in_modules = false;
    bool inserted = false;

    auto insert_active = [&]() {
        if (inserted) return;
        std::set<std::string> keys;
        std::size_t index = 0;
        for (const std::string& raw : active_modules) {
            const std::string name = normalize_module_name(raw);
            const std::string key = module_name_key(name);
            if (key.empty() || !keys.insert(key).second) continue;
            output.push_back("active" + std::to_string(index++) +
                             " = \"" + name + "\"");
        }
        inserted = true;
    };

    for (const std::string& line : lines) {
        std::string section;
        if (section_line(line, section)) {
            if (in_modules) insert_active();
            in_modules = section == "modules";
            if (in_modules) found_modules = true;
            output.push_back(line);
            continue;
        }
        if (in_modules && active_assignment(line)) continue;
        output.push_back(line);
    }
    if (in_modules) insert_active();
    if (!found_modules) {
        while (!output.empty() && output.back().empty()) output.pop_back();
        if (!output.empty()) output.emplace_back();
        output.push_back("[modules]");
        insert_active();
    }

    std::ostringstream joined;
    for (std::size_t index = 0; index < output.size(); ++index) {
        joined << output[index];
        if (index + 1 < output.size()) joined << newline;
    }
    if (!output.empty() && (contents.empty() ||
                            contents.back() == '\n')) {
        joined << newline;
    }
    return joined.str();
}

#if defined(_WIN32)
std::vector<InstalledModule> discover_installed_modules(
    const std::string& data_root,
    std::vector<std::string>* diagnostics) {
    const std::string directory = join_path(data_root, "modules");
    CreateDirectoryA(directory.c_str(), nullptr);
    std::vector<InstalledModule> result;
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(join_path(directory, "*.dll").c_str(),
                                   &data);
    if (search == INVALID_HANDLE_VALUE) return result;
    std::set<std::string> keys;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            !has_dll_extension(data.cFileName)) {
            continue;
        }
        InstalledModule module;
        module.filename = data.cFileName;
        module.name = normalize_module_name(module.filename);
        module.path = join_path(directory, module.filename);
        const std::string key = module_name_key(module.name);
        if (key.empty() || !keys.insert(key).second) {
            if (diagnostics) {
                diagnostics->push_back(
                    "Duplicate or invalid global module filename ignored: " +
                    module.filename);
            }
            continue;
        }
        result.push_back(std::move(module));
    } while (FindNextFileA(search, &data));
    FindClose(search);
    std::sort(result.begin(), result.end(),
              [](const InstalledModule& left, const InstalledModule& right) {
                  return module_name_key(left.name) <
                         module_name_key(right.name);
              });
    return result;
}

ModulePolicy evaluate_module_policy(
    const std::vector<std::string>& ordered_roots,
    const std::vector<InstalledModule>& installed) {
    std::vector<ModuleRules> rules;
    rules.reserve(ordered_roots.size());
    for (const std::string& root : ordered_roots) {
        bool opened = false;
        const std::string path = join_path(root, "info.ini");
        const std::string contents = read_text_file(path, opened);
        ModuleRules parsed = module_rules_from_info(contents);
        if (!opened) {
            parsed.diagnostics.push_back(
                "Could not read module policy: " + path);
        }
        rules.push_back(std::move(parsed));
    }
    return evaluate_module_rules(rules, installed);
}

bool save_active_module_selection(
    const std::string& info_path,
    const std::vector<std::string>& active_modules,
    std::string& error) {
    bool opened = false;
    const std::string contents = read_text_file(info_path, opened);
    if (!opened) {
        error = "Could not read " + info_path;
        return false;
    }
    const std::string rewritten =
        rewrite_active_modules(contents, active_modules);
    const std::string temporary = info_path + ".a2fo.tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            error = "Could not create " + temporary;
            return false;
        }
        output.write(rewritten.data(),
                     static_cast<std::streamsize>(rewritten.size()));
        output.flush();
        if (!output) {
            error = "Could not write " + temporary;
            output.close();
            DeleteFileA(temporary.c_str());
            return false;
        }
    }
    if (!MoveFileExA(temporary.c_str(), info_path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD code = GetLastError();
        DeleteFileA(temporary.c_str());
        error = "Could not replace " + info_path + " (error " +
                std::to_string(code) + ")";
        return false;
    }
    return true;
}
#endif

}  // namespace a2fo
