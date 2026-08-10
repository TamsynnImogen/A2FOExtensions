/*
 * File: core/lua_host.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Lua host bootstrap and API binding layer for module scripts and optional runtime state exposed to Lua.
 */

#include "lua_host.hpp"

#include <windows.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace a2fo {
namespace {

constexpr std::uint64_t kMaximumScriptBytes = 1024u * 1024u;
constexpr unsigned kHookInstructionInterval = 1000;
constexpr unsigned kMaximumInstructionTicks = 1000;
constexpr lua_Integer kLuaApiVersion = 1;
constexpr lua_Integer kLuaApiRevision = 2;
constexpr lua_Integer kMaximumUpgradePodTier = 16;

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

bool has_lua_extension(const std::string& name) {
    return name.size() >= 4 &&
           _stricmp(name.c_str() + name.size() - 4, ".lua") == 0;
}

void host_log(LuaHost* host, const std::string& message) {
    if (host && host->log) host->log(message);
}

LuaHost* host_from_state(lua_State* state) {
    if (!state) return nullptr;
    return *static_cast<LuaHost**>(lua_getextraspace(state));
}

void* lua_allocator(void* user_data, void* pointer,
                    std::size_t old_size, std::size_t new_size) {
    auto* budget = static_cast<LuaMemoryBudget*>(user_data);
    const std::size_t accounted_old_size = pointer ? old_size : 0;
    if (new_size == 0) {
        std::free(pointer);
        if (accounted_old_size <= budget->used) {
            budget->used -= accounted_old_size;
        } else {
            budget->used = 0;
        }
        return nullptr;
    }
    if (new_size > accounted_old_size &&
        new_size - accounted_old_size > budget->limit - budget->used) {
        return nullptr;
    }
    void* resized = std::realloc(pointer, new_size);
    if (!resized) return nullptr;
    budget->used = budget->used -
        std::min(budget->used, accounted_old_size) + new_size;
    return resized;
}

int lua_a2fo_log(lua_State* state) {
    LuaHost* host = host_from_state(state);
    const char* message = luaL_checkstring(state, 1);
    host_log(host, std::string("[Lua] ") + (message ? message : ""));
    return 0;
}

int lua_require_api(lua_State* state) {
    const lua_Integer required_version = luaL_checkinteger(state, 1);
    const lua_Integer required_revision = luaL_optinteger(state, 2, 0);
    if (required_version != kLuaApiVersion) {
        return luaL_error(
            state, "A2FO Lua API major %lld required; host provides %lld",
            static_cast<long long>(required_version),
            static_cast<long long>(kLuaApiVersion));
    }
    if (required_revision > kLuaApiRevision) {
        return luaL_error(
            state, "A2FO Lua API revision %lld required; host provides %lld",
            static_cast<long long>(required_revision),
            static_cast<long long>(kLuaApiRevision));
    }
    lua_pushboolean(state, 1);
    return 1;
}

int lua_has_capability(lua_State* state) {
    const char* capability = luaL_checkstring(state, 1);
    const bool available = capability &&
        (std::strcmp(capability, "declared_destroyed_odf_fields") == 0 ||
         std::strcmp(capability, "configurable_upgrade_pods") == 0);
    lua_pushboolean(state, available ? 1 : 0);
    return 1;
}

int lua_configure_upgrade_pods(lua_State* state) {
    LuaHost* host = host_from_state(state);
    luaL_checktype(state, 1, LUA_TTABLE);
    if (lua_gettop(state) != 1) {
        return luaL_error(state,
                          "configure_upgrade_pods expects one table");
    }
    if (!host || host->current_script.empty()) {
        return luaL_error(
            state,
            "upgrade-pod policy can only be configured while a script is loading");
    }
    if (host->upgrade_pod_policy_registered) {
        return luaL_error(state, "upgrade-pod policy is already owned by %s",
                          host->upgrade_pod_policy_owner.c_str());
    }

    lua_getfield(state, 1, "maximum_tier");
    if (!lua_isinteger(state, -1)) {
        lua_pop(state, 1);
        return luaL_error(state, "maximum_tier must be an integer");
    }
    const lua_Integer maximum_tier = lua_tointeger(state, -1);
    lua_pop(state, 1);
    if (maximum_tier < 3 || maximum_tier > kMaximumUpgradePodTier) {
        return luaL_error(state,
                          "maximum_tier must be between 3 and %lld",
                          static_cast<long long>(kMaximumUpgradePodTier));
    }

    host->upgrade_pod_maximum_tier =
        static_cast<std::uint32_t>(maximum_tier);
    host->upgrade_pod_policy_registered = true;
    host->upgrade_pod_policy_owner = host->current_script;
    host_log(host, "Lua upgrade-pod maximum tier: " +
                       std::to_string(maximum_tier) + " by " +
                       host->current_script);
    return 0;
}

int lua_on_classlabel(lua_State* state) {
    LuaHost* host = host_from_state(state);
    luaL_checktype(state, 1, LUA_TFUNCTION);
    if (!host || host->current_script.empty()) {
        return luaL_error(state,
                          "classlabel callbacks can only be registered while "
                          "a script is loading");
    }
    const std::string owner = host->current_script;
    lua_pushvalue(state, 1);
    const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    try {
        LuaCallback callback;
        callback.reference = reference;
        callback.owner = owner;
        host->classlabel_callbacks.push_back(std::move(callback));
    } catch (...) {
        luaL_unref(state, LUA_REGISTRYINDEX, reference);
        return luaL_error(state, "could not retain classlabel callback");
    }
    host_log(host, "Lua callback registered: classlabel by " + owner);
    return 0;
}

int lua_on_evolver_cocoon(lua_State* state) {
    LuaHost* host = host_from_state(state);
    luaL_checktype(state, 1, LUA_TFUNCTION);
    if (!host || host->current_script.empty()) {
        return luaL_error(state,
                          "Evolver cocoon callbacks can only be registered "
                          "while a script is loading");
    }
    if (host->evolver_cocoon_callback.reference != LUA_NOREF) {
        return luaL_error(
            state, "Evolver cocoon callback is already owned by %s",
            host->evolver_cocoon_callback.owner.c_str());
    }
    const std::string owner = host->current_script;
    lua_pushvalue(state, 1);
    const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    try {
        LuaCallback callback;
        callback.reference = reference;
        callback.owner = owner;
        host->evolver_cocoon_callback = std::move(callback);
    } catch (...) {
        luaL_unref(state, LUA_REGISTRYINDEX, reference);
        return luaL_error(state, "could not retain Evolver cocoon callback");
    }
    host_log(host, "Lua callback registered: evolver_cocoon by " + owner);
    return 0;
}

int lua_on_object_destroyed(lua_State* state) {
    LuaHost* host = host_from_state(state);
    const int argument_count = lua_gettop(state);
    const bool legacy_registration =
        argument_count == 1 && lua_isfunction(state, 1);
    const int callback_index = legacy_registration ? 1 : 2;
    if (!legacy_registration) {
        luaL_checktype(state, 1, LUA_TTABLE);
        luaL_checktype(state, 2, LUA_TFUNCTION);
        if (argument_count != 2) {
            return luaL_error(
                state,
                "on_object_destroyed expects fields and a callback");
        }
    }
    if (!host || host->current_script.empty()) {
        return luaL_error(state,
                          "object-destroyed callbacks can only be registered "
                          "while a script is loading");
    }

    std::set<std::string> unique_fields;
    if (legacy_registration) {
        // Compatibility for scripts written before declarations existed.
        unique_fields.insert("wreckage");
        unique_fields.insert("wreckagechance");
        host_log(host, "Lua host: deprecated one-argument "
                       "on_object_destroyed registration in " +
                       host->current_script);
    } else {
        const lua_Unsigned count = lua_rawlen(state, 1);
        if (count > 64) {
            return luaL_error(state,
                              "at most 64 destroyed-object ODF fields may be "
                              "declared");
        }
        for (lua_Unsigned index = 1; index <= count; ++index) {
            lua_rawgeti(state, 1, static_cast<lua_Integer>(index));
            std::size_t length = 0;
            const char* field = luaL_checklstring(state, -1, &length);
            if (length == 0 || length > 127 ||
                std::memchr(field, '\0', length)) {
                lua_pop(state, 1);
                return luaL_error(
                    state, "ODF field names must be 1-127 bytes of text");
            }
            for (std::size_t offset = 0; offset < length; ++offset) {
                const unsigned char ch =
                    static_cast<unsigned char>(field[offset]);
                if (!std::isalnum(ch) && ch != '_' && ch != '-' &&
                    ch != '.') {
                    lua_pop(state, 1);
                    return luaL_error(
                        state, "ODF field names must be identifiers");
                }
            }
            unique_fields.insert(
                lower_ascii(std::string(field, length)));
            lua_pop(state, 1);
        }
    }

    const std::string owner = host->current_script;
    lua_pushvalue(state, callback_index);
    const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    try {
        LuaCallback callback;
        callback.reference = reference;
        callback.owner = owner;
        callback.required_odf_fields.assign(
            unique_fields.begin(), unique_fields.end());
        host->object_destroyed_callbacks.push_back(std::move(callback));
    } catch (...) {
        luaL_unref(state, LUA_REGISTRYINDEX, reference);
        return luaL_error(state,
                          "could not retain object-destroyed callback");
    }
    host_log(host, "Lua callback registered: object_destroyed by " + owner);
    return 0;
}

int lua_odf_get_string(lua_State* state) {
    LuaHost* host = host_from_state(state);
    luaL_checktype(state, 1, LUA_TTABLE);
    std::size_t key_length = 0;
    const char* key = luaL_checklstring(state, 2, &key_length);
    const std::uint64_t generation = static_cast<std::uint64_t>(
        lua_tointeger(state, lua_upvalueindex(1)));
    if (!host || !host->active_odf_view ||
        generation == 0 || generation != host->active_odf_generation) {
        return luaL_error(state,
                          "ODF views are valid only during an engine callback");
    }
    if (key_length == 0 || key_length > 127 ||
        std::strlen(key) != key_length) {
        return luaL_error(state, "ODF key must be a 1-127 byte text string");
    }

    const bool has_default = !lua_isnoneornil(state, 3);
    std::size_t default_length = 0;
    const char* default_value = has_default
        ? luaL_checklstring(state, 3, &default_length) : "";
    if (default_length > 4095 ||
        (has_default && std::strlen(default_value) != default_length)) {
        return luaL_error(state,
                          "ODF default must be at most 4095 bytes of text");
    }

    if (host->active_parameter_db && host->engine.parameter_db_get_string) {
        std::array<char, 4096> output{};
        const bool found = host->engine.parameter_db_get_string(
            host->active_parameter_db, key, output.data(),
            static_cast<std::uint32_t>(output.size()), default_value);
        output.back() = '\0';
        if (!found && !has_default) {
            lua_pushnil(state);
            return 1;
        }
        const char* end = static_cast<const char*>(
            std::memchr(output.data(), '\0', output.size()));
        if (!end) {
            return luaL_error(state, "ODF string result was not terminated");
        }
        lua_pushlstring(state, output.data(),
                        static_cast<std::size_t>(end - output.data()));
        return 1;
    }

    if (host->active_odf_snapshot) {
        const std::string normalized_key = lower_ascii(std::string(key, key_length));
        const auto found = host->active_odf_snapshot->find(normalized_key);
        if (found != host->active_odf_snapshot->end()) {
            lua_pushlstring(state, found->second.data(), found->second.size());
        } else if (has_default) {
            lua_pushlstring(state, default_value, default_length);
        } else {
            lua_pushnil(state);
        }
        return 1;
    }

    return luaL_error(state,
                      "ODF view has no active engine data or snapshot");
}

std::uint32_t mix_random_seed(std::uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

int lua_event_roll_percent(lua_State* state) {
    luaL_checktype(state, 1, LUA_TTABLE);
    const lua_Number chance = luaL_checknumber(state, 2);
    if (!std::isfinite(static_cast<double>(chance)) ||
        chance < 0.0 || chance > 100.0) {
        return luaL_error(state,
                          "roll_percent chance must be between 0 and 100");
    }
    if (chance <= 0.0) {
        lua_pushboolean(state, 0);
        return 1;
    }
    if (chance >= 100.0) {
        lua_pushboolean(state, 1);
        return 1;
    }
    const std::uint32_t seed = static_cast<std::uint32_t>(
        lua_tointeger(state, lua_upvalueindex(1)));
    const double roll = static_cast<double>(mix_random_seed(seed)) *
        (100.0 / 4294967296.0);
    lua_pushboolean(state, roll < static_cast<double>(chance));
    return 1;
}

int traceback(lua_State* state) {
    const char* message = lua_tostring(state, 1);
    if (message) {
        luaL_traceback(state, state, message, 1);
    } else if (!lua_isnoneornil(state, 1)) {
        lua_pushliteral(state, "Lua error object is not a string");
    }
    return 1;
}

void instruction_hook(lua_State* state, lua_Debug*) {
    LuaHost* host = host_from_state(state);
    if (!host) return;
    ++host->instruction_ticks;
    if (host->instruction_ticks > kMaximumInstructionTicks) {
        luaL_error(state, "script instruction limit exceeded");
    }
}

void open_safe_libraries(lua_State* state) {
    const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8},
        {nullptr, nullptr},
    };
    for (const luaL_Reg* library = libraries; library->func; ++library) {
        luaL_requiref(state, library->name, library->func, 1);
        lua_pop(state, 1);
    }

    // Base-library file loaders bypass extension-root overlay rules. Scripts
    // use only the files selected and loaded by this host.
    lua_pushnil(state);
    lua_setglobal(state, "dofile");
    lua_pushnil(state);
    lua_setglobal(state, "loadfile");
}

void install_a2fo_table(lua_State* state,
                        const std::vector<std::string>& roots) {
    lua_newtable(state);
    lua_pushinteger(state, kLuaApiVersion);
    lua_setfield(state, -2, "api_version");
    lua_pushinteger(state, kLuaApiRevision);
    lua_setfield(state, -2, "api_revision");
    lua_pushliteral(state, "5.4.8");
    lua_setfield(state, -2, "lua_version");
    lua_pushcfunction(state, &lua_require_api);
    lua_setfield(state, -2, "require_api");
    lua_pushcfunction(state, &lua_has_capability);
    lua_setfield(state, -2, "has_capability");
    lua_newtable(state);
    lua_pushboolean(state, 1);
    lua_setfield(state, -2, "declared_destroyed_odf_fields");
    lua_pushboolean(state, 1);
    lua_setfield(state, -2, "configurable_upgrade_pods");
    lua_setfield(state, -2, "capabilities");
    lua_pushcfunction(state, &lua_a2fo_log);
    lua_setfield(state, -2, "log");
    lua_pushcfunction(state, &lua_on_classlabel);
    lua_setfield(state, -2, "on_classlabel");
    lua_pushcfunction(state, &lua_on_evolver_cocoon);
    lua_setfield(state, -2, "on_evolver_cocoon");
    lua_pushcfunction(state, &lua_on_object_destroyed);
    lua_setfield(state, -2, "on_object_destroyed");
    lua_pushcfunction(state, &lua_configure_upgrade_pods);
    lua_setfield(state, -2, "configure_upgrade_pods");

    lua_createtable(state, static_cast<int>(roots.size()), 0);
    for (std::size_t index = 0; index < roots.size(); ++index) {
        lua_pushlstring(state, roots[index].data(), roots[index].size());
        lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(state, -2, "extension_roots");
    lua_setglobal(state, "a2fo");
}

std::map<std::string, std::string> select_scripts(
    const std::vector<std::string>& roots, LuaHost* host) {
    std::map<std::string, std::string> selected;
    if (roots.empty()) return selected;
    CreateDirectoryA(join_path(roots.front(), "scripts").c_str(), nullptr);

    for (const std::string& root : roots) {
        const std::string directory = join_path(root, "scripts");
        WIN32_FIND_DATAA data{};
        HANDLE search = FindFirstFileA(
            join_path(directory, "*.lua").c_str(), &data);
        if (search == INVALID_HANDLE_VALUE) continue;
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
                !has_lua_extension(data.cFileName)) {
                continue;
            }
            const std::string path = join_path(directory, data.cFileName);
            const std::string key = lower_ascii(data.cFileName);
            const auto previous = selected.find(key);
            if (previous != selected.end()) {
                host_log(host, "Lua host: " + path + " overrides " +
                                  previous->second);
            }
            selected[key] = path;
        } while (FindNextFileA(search, &data));
        FindClose(search);
    }
    return selected;
}

bool read_script(const std::string& path, std::vector<char>& bytes,
                 std::string& error) {
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = "open failed (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        static_cast<std::uint64_t>(size.QuadPart) > kMaximumScriptBytes) {
        CloseHandle(file);
        error = "file is larger than the 1 MiB script limit";
        return false;
    }
    bytes.resize(static_cast<std::size_t>(size.QuadPart));
    std::size_t total = 0;
    while (total < bytes.size()) {
        DWORD read = 0;
        const DWORD wanted = static_cast<DWORD>(bytes.size() - total);
        if (!ReadFile(file, bytes.data() + total, wanted, &read, nullptr) ||
            read == 0) {
            CloseHandle(file);
            error = "read failed (error " +
                    std::to_string(GetLastError()) + ")";
            return false;
        }
        total += read;
    }
    CloseHandle(file);
    return true;
}

bool run_script(LuaHost& host, const std::string& path) {
    std::vector<char> bytes;
    std::string error;
    if (!read_script(path, bytes, error)) {
        host_log(&host, "Lua host: " + error + ": " + path);
        return false;
    }

    const std::size_t classlabel_count = host.classlabel_callbacks.size();
    const std::size_t object_destroyed_count =
        host.object_destroyed_callbacks.size();
    const LuaCallback cocoon_before = host.evolver_cocoon_callback;
    const std::uint32_t upgrade_pod_maximum_tier_before =
        host.upgrade_pod_maximum_tier;
    const bool upgrade_pod_policy_registered_before =
        host.upgrade_pod_policy_registered;
    const std::string upgrade_pod_policy_owner_before =
        host.upgrade_pod_policy_owner;
    host.current_script = path;
    lua_State* state = host.state;
    lua_settop(state, 0);
    lua_pushcfunction(state, &traceback);
    const int message_handler = lua_gettop(state);
    const std::string chunk_name = "@" + path;
    int status = luaL_loadbufferx(
        state, bytes.empty() ? "" : bytes.data(), bytes.size(),
        chunk_name.c_str(), "t");
    if (status == LUA_OK) {
        host.instruction_ticks = 0;
        lua_sethook(state, &instruction_hook, LUA_MASKCOUNT,
                    kHookInstructionInterval);
        status = lua_pcall(state, 0, 0, message_handler);
        lua_sethook(state, nullptr, 0, 0);
    }
    if (status != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        host_log(&host, "Lua host: script failed: " + path + ": " +
                           (message ? message : "unknown Lua error"));
        while (host.classlabel_callbacks.size() > classlabel_count) {
            luaL_unref(state, LUA_REGISTRYINDEX,
                       host.classlabel_callbacks.back().reference);
            host.classlabel_callbacks.pop_back();
        }
        while (host.object_destroyed_callbacks.size() >
               object_destroyed_count) {
            luaL_unref(state, LUA_REGISTRYINDEX,
                       host.object_destroyed_callbacks.back().reference);
            host.object_destroyed_callbacks.pop_back();
        }
        if (host.evolver_cocoon_callback.reference !=
            cocoon_before.reference) {
            luaL_unref(state, LUA_REGISTRYINDEX,
                       host.evolver_cocoon_callback.reference);
            host.evolver_cocoon_callback = cocoon_before;
        }
        host.upgrade_pod_maximum_tier =
            upgrade_pod_maximum_tier_before;
        host.upgrade_pod_policy_registered =
            upgrade_pod_policy_registered_before;
        host.upgrade_pod_policy_owner =
            upgrade_pod_policy_owner_before;
        host_log(&host, "Lua host: rolled back registrations from " + path);
        lua_settop(state, 0);
        host.current_script.clear();
        return false;
    }
    lua_settop(state, 0);
    host.current_script.clear();
    ++host.loaded_script_count;
    host_log(&host, "Lua host: loaded " + path);
    return true;
}

void push_odf_view(lua_State* state, std::uint64_t generation) {
    lua_newtable(state);
    lua_pushinteger(state, static_cast<lua_Integer>(generation));
    lua_pushcclosure(state, &lua_odf_get_string, 1);
    lua_setfield(state, -2, "get_string");
}

std::uint64_t begin_odf_view(LuaHost& host, void* parameter_db) {
    ++host.next_odf_generation;
    if (host.next_odf_generation == 0) ++host.next_odf_generation;
    host.active_parameter_db = parameter_db;
    host.active_odf_snapshot = nullptr;
    host.active_odf_generation = host.next_odf_generation;
    host.active_odf_view = true;
    return host.active_odf_generation;
}

std::uint64_t begin_odf_view(LuaHost& host,
                             const LuaOdfSnapshot& snapshot) {
    ++host.next_odf_generation;
    if (host.next_odf_generation == 0) ++host.next_odf_generation;
    host.active_parameter_db = nullptr;
    host.active_odf_snapshot = &snapshot;
    host.active_odf_generation = host.next_odf_generation;
    host.active_odf_view = true;
    return host.active_odf_generation;
}

void end_odf_view(LuaHost& host) {
    host.active_odf_view = false;
    host.active_odf_generation = 0;
    host.active_parameter_db = nullptr;
    host.active_odf_snapshot = nullptr;
}

bool valid_classlabel(const char* value, std::size_t length) {
    if (!value || length == 0 || length > 63) return false;
    for (std::size_t index = 0; index < length; ++index) {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (!std::isalnum(ch) && ch != '_' && ch != '-') return false;
    }
    return true;
}

bool valid_odf_name(const char* value, std::size_t length) {
    if (!value || length == 0 || length > 255) return false;
    for (std::size_t index = 0; index < length; ++index) {
        const unsigned char ch = static_cast<unsigned char>(value[index]);
        if (!std::isalnum(ch) && ch != '_' && ch != '-' && ch != '.') {
            return false;
        }
    }
    return true;
}

bool optional_boolean_field(lua_State* state, const char* name,
                            bool default_value, bool& output,
                            std::string& error) {
    lua_pushstring(state, name);
    lua_rawget(state, -2);
    if (lua_isnil(state, -1)) {
        output = default_value;
        lua_pop(state, 1);
        return true;
    }
    if (!lua_isboolean(state, -1)) {
        error = std::string(name) + " must be a boolean when supplied";
        lua_pop(state, 1);
        return false;
    }
    output = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    return true;
}

void disable_callback(LuaHost& host, LuaCallback& callback,
                      const std::string& event,
                      const char* reason) {
    callback.enabled = false;
    host_log(&host, "Lua callback disabled: " + event + " by " +
                       callback.owner + ": " +
                       (reason ? reason : "unknown Lua error"));
}

}  // namespace

bool transform_classlabel(LuaHost& host,
                          void* parameter_db,
                          const std::string& classlabel,
                          std::string& replacement) {
    replacement.clear();
    if (!host.state || !parameter_db || host.classlabel_callbacks.empty()) {
        return false;
    }

    std::string current = classlabel;
    for (LuaCallback& callback : host.classlabel_callbacks) {
        if (!callback.enabled || callback.reference == LUA_NOREF) continue;

        lua_State* state = host.state;
        lua_settop(state, 0);
        lua_pushcfunction(state, &traceback);
        const int message_handler = lua_gettop(state);
        lua_rawgeti(state, LUA_REGISTRYINDEX, callback.reference);
        if (!lua_isfunction(state, -1)) {
            disable_callback(host, callback, "classlabel",
                             "registry reference is not a function");
            lua_settop(state, 0);
            continue;
        }
        lua_pushlstring(state, current.data(), current.size());
        const std::uint64_t odf_generation =
            begin_odf_view(host, parameter_db);
        push_odf_view(state, odf_generation);

        host.instruction_ticks = 0;
        lua_sethook(state, &instruction_hook, LUA_MASKCOUNT,
                    kHookInstructionInterval);
        const int status = lua_pcall(state, 2, 1, message_handler);
        lua_sethook(state, nullptr, 0, 0);
        end_odf_view(host);

        if (status != LUA_OK) {
            disable_callback(host, callback, "classlabel",
                             lua_tostring(state, -1));
            lua_settop(state, 0);
            continue;
        }
        if (lua_isnil(state, -1)) {
            lua_settop(state, 0);
            continue;
        }
        if (lua_type(state, -1) != LUA_TSTRING) {
            disable_callback(host, callback, "classlabel",
                             "callback must return a classlabel string or nil");
            lua_settop(state, 0);
            continue;
        }

        std::size_t length = 0;
        const char* value = lua_tolstring(state, -1, &length);
        if (!valid_classlabel(value, length)) {
            disable_callback(host, callback, "classlabel",
                             "returned classlabel is not a valid identifier");
            lua_settop(state, 0);
            continue;
        }
        current.assign(value, length);
        lua_settop(state, 0);
    }

    if (current == classlabel) return false;
    replacement = std::move(current);
    return true;
}

bool resolve_evolver_cocoon(LuaHost& host,
                            void* parameter_db,
                            std::string& cocoon) {
    cocoon.clear();
    LuaCallback& callback = host.evolver_cocoon_callback;
    if (!host.state || !parameter_db || !callback.enabled ||
        callback.reference == LUA_NOREF) {
        return false;
    }

    lua_State* state = host.state;
    lua_settop(state, 0);
    lua_pushcfunction(state, &traceback);
    const int message_handler = lua_gettop(state);
    lua_rawgeti(state, LUA_REGISTRYINDEX, callback.reference);
    if (!lua_isfunction(state, -1)) {
        disable_callback(host, callback, "evolver_cocoon",
                         "registry reference is not a function");
        lua_settop(state, 0);
        return false;
    }
    const std::uint64_t odf_generation = begin_odf_view(host, parameter_db);
    push_odf_view(state, odf_generation);

    host.instruction_ticks = 0;
    lua_sethook(state, &instruction_hook, LUA_MASKCOUNT,
                kHookInstructionInterval);
    const int status = lua_pcall(state, 1, 1, message_handler);
    lua_sethook(state, nullptr, 0, 0);
    end_odf_view(host);

    if (status != LUA_OK) {
        disable_callback(host, callback, "evolver_cocoon",
                         lua_tostring(state, -1));
        lua_settop(state, 0);
        return false;
    }
    if (lua_isnil(state, -1)) {
        lua_settop(state, 0);
        return false;
    }
    if (lua_type(state, -1) != LUA_TSTRING) {
        disable_callback(host, callback, "evolver_cocoon",
                         "callback must return a SOD name string or nil");
        lua_settop(state, 0);
        return false;
    }

    std::size_t length = 0;
    const char* value = lua_tolstring(state, -1, &length);
    if (!value || length > 1023 || std::memchr(value, '\0', length)) {
        disable_callback(host, callback, "evolver_cocoon",
                         "returned SOD name is too long or contains NUL");
        lua_settop(state, 0);
        return false;
    }
    cocoon.assign(value, length);
    lua_settop(state, 0);
    return !cocoon.empty();
}

bool resolve_object_destroyed(LuaHost& host,
                              const LuaObjectDestroyedEvent& event,
                              LuaObjectReplacement& replacement) {
    replacement = LuaObjectReplacement{};
    if (!host.state || host.object_destroyed_callbacks.empty()) {
        return false;
    }

    for (LuaCallback& callback : host.object_destroyed_callbacks) {
        if (!callback.enabled || callback.reference == LUA_NOREF) continue;

        lua_State* state = host.state;
        lua_settop(state, 0);
        lua_pushcfunction(state, &traceback);
        const int message_handler = lua_gettop(state);
        lua_rawgeti(state, LUA_REGISTRYINDEX, callback.reference);
        if (!lua_isfunction(state, -1)) {
            disable_callback(host, callback, "object_destroyed",
                             "registry reference is not a function");
            lua_settop(state, 0);
            continue;
        }

        const std::uint64_t odf_generation =
            begin_odf_view(host, event.odf);
        lua_newtable(state);
        push_odf_view(state, odf_generation);
        lua_setfield(state, -2, "odf");
        lua_pushinteger(state, static_cast<lua_Integer>(event.random_seed));
        lua_pushcclosure(state, &lua_event_roll_percent, 1);
        lua_setfield(state, -2, "roll_percent");

        host.instruction_ticks = 0;
        lua_sethook(state, &instruction_hook, LUA_MASKCOUNT,
                    kHookInstructionInterval);
        const int status = lua_pcall(state, 1, 1, message_handler);
        lua_sethook(state, nullptr, 0, 0);
        end_odf_view(host);

        if (status != LUA_OK) {
            disable_callback(host, callback, "object_destroyed",
                             lua_tostring(state, -1));
            lua_settop(state, 0);
            continue;
        }
        if (lua_isnil(state, -1)) {
            lua_settop(state, 0);
            continue;
        }
        if (!lua_istable(state, -1)) {
            disable_callback(host, callback, "object_destroyed",
                             "callback must return a replacement table or nil");
            lua_settop(state, 0);
            continue;
        }

        lua_pushliteral(state, "odf");
        lua_rawget(state, -2);
        if (lua_type(state, -1) != LUA_TSTRING) {
            disable_callback(host, callback, "object_destroyed",
                             "replacement.odf must be an ODF name string");
            lua_settop(state, 0);
            continue;
        }
        std::size_t odf_length = 0;
        const char* odf = lua_tolstring(state, -1, &odf_length);
        if (!valid_odf_name(odf, odf_length)) {
            disable_callback(host, callback, "object_destroyed",
                             "replacement.odf is not a valid ODF basename");
            lua_settop(state, 0);
            continue;
        }
        std::string odf_name(odf, odf_length);
        lua_pop(state, 1);

        bool inherit_position = true;
        bool inherit_rotation = true;
        std::string field_error;
        if (!optional_boolean_field(state, "inherit_position", true,
                                    inherit_position, field_error) ||
            !optional_boolean_field(state, "inherit_rotation", true,
                                    inherit_rotation, field_error)) {
            disable_callback(host, callback, "object_destroyed",
                             field_error.c_str());
            lua_settop(state, 0);
            continue;
        }

        LuaReplacementOwner owner = LuaReplacementOwner::Neutral;
        lua_pushliteral(state, "owner");
        lua_rawget(state, -2);
        if (!lua_isnil(state, -1)) {
            if (lua_type(state, -1) != LUA_TSTRING) {
                lua_pop(state, 1);
                disable_callback(host, callback, "object_destroyed",
                                 "replacement.owner must be a string");
                lua_settop(state, 0);
                continue;
            }
            std::size_t owner_length = 0;
            const char* owner_value =
                lua_tolstring(state, -1, &owner_length);
            const std::string normalized_owner =
                lower_ascii(std::string(owner_value, owner_length));
            if (normalized_owner == "neutral") {
                owner = LuaReplacementOwner::Neutral;
            } else if (normalized_owner == "original") {
                owner = LuaReplacementOwner::Original;
            } else {
                lua_pop(state, 1);
                disable_callback(host, callback, "object_destroyed",
                                 "replacement.owner must be neutral or original");
                lua_settop(state, 0);
                continue;
            }
        }
        lua_pop(state, 1);

        replacement.odf = std::move(odf_name);
        replacement.inherit_position = inherit_position;
        replacement.inherit_rotation = inherit_rotation;
        replacement.owner = owner;
        lua_settop(state, 0);
        return true;
    }
    return false;
}

std::vector<std::string> object_destroyed_odf_fields(const LuaHost& host) {
    std::set<std::string> fields{"basename"};
    for (const LuaCallback& callback : host.object_destroyed_callbacks) {
        fields.insert(callback.required_odf_fields.begin(),
                      callback.required_odf_fields.end());
    }
    return {fields.begin(), fields.end()};
}

bool initialize_lua_host(const std::vector<std::string>& roots,
                         LuaHost& host,
                         LuaLogFunction log,
                         const LuaEngineApi& engine) {
    if (host.state) return true;
    host.log = log;
    host.engine = engine;
    host.state = lua_newstate(&lua_allocator, &host.memory);
    if (!host.state) {
        host_log(&host, "Lua host: could not create the Lua state");
        return false;
    }
    *static_cast<LuaHost**>(lua_getextraspace(host.state)) = &host;
    open_safe_libraries(host.state);
    install_a2fo_table(host.state, roots);

    const std::map<std::string, std::string> scripts =
        select_scripts(roots, &host);
    if (scripts.empty()) {
        host_log(&host, "Lua host: no scripts found");
        return true;
    }
    bool all_ok = true;
    for (const auto& script : scripts) {
        if (!run_script(host, script.second)) all_ok = false;
    }
    host_log(&host, "Lua host: " +
                      std::to_string(host.loaded_script_count) +
                      " scripts loaded");
    return all_ok;
}

}  // namespace a2fo
