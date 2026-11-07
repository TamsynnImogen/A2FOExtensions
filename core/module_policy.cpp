/*
 * File: core/module_policy.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Native module policy adapters and Win32 discovery/persistence.
 */
#include "module_policy.hpp"
#include "builtin_state.hpp"
#include "../rust/module_policy_ffi.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace a2fo {
namespace {
A2foPolicyBytes view(const std::string& value) {
    return {reinterpret_cast<const uint8_t*>(value.data()), value.size()};
}
std::string copy(A2foPolicyBytes value) {
    return value.size ? std::string(reinterpret_cast<const char*>(value.data), value.size)
                      : std::string();
}
struct CallbackError { std::exception_ptr error; };
struct StringResult : CallbackError { std::string value; };
void receive_string(void* context, A2foPolicyBytes value) noexcept {
    auto& result = *static_cast<StringResult*>(context);
    if (result.error) return;
    try { result.value = copy(value); }
    catch (...) { result.error = std::current_exception(); }
}
struct RuleResult : CallbackError { ModuleRules value; };
void receive_rule(void* context, uint32_t kind, A2foPolicyBytes value) noexcept {
    auto& result = *static_cast<RuleResult*>(context);
    if (result.error) return;
    try {
        switch (kind) {
        case 0: result.value.has_section = true; break;
        case 1: result.value.required.push_back(copy(value)); break;
        case 2: result.value.rejected.push_back(copy(value)); break;
        case 3: result.value.active.push_back(copy(value)); break;
        case 4: result.value.diagnostics.push_back(copy(value)); break;
        default: throw std::runtime_error("Invalid Rust policy rule kind");
        }
    } catch (...) { result.error = std::current_exception(); }
}
struct PolicyResult : CallbackError { ModulePolicy value; };
void receive_meta(void* context, uint8_t managed, uint8_t valid) noexcept {
    auto& result = *static_cast<PolicyResult*>(context);
    if (result.error) return;
    result.value.managed = managed != 0;
    result.value.valid = valid != 0;
}
void receive_entry(void* context, const A2foPolicyEntry* value) noexcept {
    auto& result = *static_cast<PolicyResult*>(context);
    if (result.error) return;
    try {
        if (!value || value->state > static_cast<uint32_t>(ModulePolicyState::missing_required))
            throw std::runtime_error("Invalid Rust policy state");
        ModulePolicyEntry entry;
        entry.key = copy(value->key);
        entry.name = copy(value->name);
        entry.filename = copy(value->filename);
        entry.path = copy(value->path);
        entry.state = static_cast<ModulePolicyState>(value->state);
        entry.installed = value->installed != 0;
        result.value.entries.push_back(std::move(entry));
    } catch (...) { result.error = std::current_exception(); }
}
void receive_diagnostic(void* context, A2foPolicyBytes value) noexcept {
    auto& result = *static_cast<PolicyResult*>(context);
    if (result.error) return;
    try { result.value.diagnostics.push_back(copy(value)); }
    catch (...) { result.error = std::current_exception(); }
}
void check(uint8_t success, const CallbackError& result) {
    if (result.error) std::rethrow_exception(result.error);
    if (!success) throw std::invalid_argument("Invalid Rust policy input");
}
std::vector<A2foPolicyBytes> views(const std::vector<std::string>& strings) {
    std::vector<A2foPolicyBytes> result;
    result.reserve(strings.size());
    for (const auto& item : strings) result.push_back(view(item));
    return result;
}
A2foPolicyByteArray array(const std::vector<A2foPolicyBytes>& values) {
    return {values.data(), values.size()};
}
#if defined(_WIN32)
std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}
bool has_dll_extension(const std::string& name) {
    return name.size() >= 4 && _stricmp(name.c_str() + name.size() - 4, ".dll") == 0;
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
} // namespace

std::string normalize_module_name(const std::string& value) {
    StringResult result;
    check(a2fo_rs_policy_normalize(view(value), &result, receive_string), result);
    return result.value;
}
std::string module_name_key(const std::string& value) {
    StringResult result;
    check(a2fo_rs_policy_key(view(value), &result, receive_string), result);
    return result.value;
}
ModuleRules module_rules_from_info(const std::string& contents) {
    RuleResult result;
    check(a2fo_rs_policy_parse(view(contents), &result, receive_rule), result);
    return result.value;
}
ModulePolicy evaluate_module_rules(const std::vector<ModuleRules>& ordered_rules,
                                   const std::vector<InstalledModule>& installed) {
    struct RuleViews {
        std::vector<A2foPolicyBytes> required, rejected, active, diagnostics;
    };
    std::vector<RuleViews> storage;
    storage.reserve(ordered_rules.size());
    for (const auto& rule : ordered_rules)
        storage.push_back({views(rule.required), views(rule.rejected),
                           views(rule.active), views(rule.diagnostics)});
    std::vector<A2foPolicyRule> rule_inputs;
    rule_inputs.reserve(ordered_rules.size());
    for (std::size_t i = 0; i < ordered_rules.size(); ++i)
        rule_inputs.push_back({static_cast<uint8_t>(ordered_rules[i].has_section),
                               array(storage[i].required), array(storage[i].rejected),
                               array(storage[i].active), array(storage[i].diagnostics)});
    std::vector<A2foPolicyInstalled> module_inputs;
    module_inputs.reserve(installed.size());
    for (const auto& module : installed)
        module_inputs.push_back({view(module.name), view(module.filename), view(module.path)});
    PolicyResult result;
    check(a2fo_rs_policy_evaluate(rule_inputs.data(), rule_inputs.size(),
                                  module_inputs.data(), module_inputs.size(), &result,
                                  receive_meta, receive_entry, receive_diagnostic), result);
    return result.value;
}
std::string rewrite_active_modules(const std::string& contents,
                                   const std::vector<std::string>& active_modules) {
    auto names = views(active_modules);
    StringResult result;
    check(a2fo_rs_policy_rewrite(view(contents), array(names), &result, receive_string), result);
    return result.value;
}

#if defined(_WIN32)
std::vector<InstalledModule> discover_installed_modules(
    const std::string& data_root,
    std::vector<std::string>* diagnostics) {
    const std::string directory = join_path(data_root, "modules");
    CreateDirectoryA(directory.c_str(), nullptr);
    std::vector<InstalledModule> result;
    std::set<std::string> keys;
    // Built-ins are present even when no Data/modules directory or DLL exists.
    // Reserve their names first so an old standalone copy cannot run twice.
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(BuiltinFeature::count); ++index) {
        const std::string name = canonical_builtin_name(static_cast<BuiltinFeature>(index));
        result.push_back({name, name + ".dll", "builtin:" + name});
        keys.insert(module_name_key(name));
    }
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(join_path(directory, "*.dll").c_str(),
                                   &data);
    if (search == INVALID_HANDLE_VALUE) return result;
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
                    "Built-in, duplicate or invalid global module filename ignored: " +
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
