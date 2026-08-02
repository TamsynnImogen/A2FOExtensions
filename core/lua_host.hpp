#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct lua_State;

namespace a2fo {

using LuaLogFunction = void (*)(const std::string& message);
using LuaParameterDbGetStringFunction = bool (*)(
    void* parameter_db,
    const char* key,
    char* output,
    std::uint32_t output_size,
    const char* default_value);

struct LuaEngineApi {
    LuaParameterDbGetStringFunction parameter_db_get_string = nullptr;
};

struct LuaMemoryBudget {
    std::size_t used = 0;
    std::size_t limit = 32u * 1024u * 1024u;
};

struct LuaCallback {
    // LUA_NOREF without making the public header depend on lua.h.
    int reference = -2;
    std::string owner;
    bool enabled = true;
    std::vector<std::string> required_odf_fields;
};

using LuaOdfSnapshot = std::map<std::string, std::string>;

struct LuaObjectDestroyedEvent {
    LuaOdfSnapshot odf;
    std::uint32_t random_seed = 0;
};

enum class LuaReplacementOwner {
    Neutral,
    Original,
};

struct LuaObjectReplacement {
    std::string odf;
    bool inherit_position = true;
    bool inherit_rotation = true;
    LuaReplacementOwner owner = LuaReplacementOwner::Neutral;
};

struct LuaHost {
    lua_State* state = nullptr;
    LuaMemoryBudget memory;
    LuaLogFunction log = nullptr;
    LuaEngineApi engine;
    std::string current_script;
    std::vector<LuaCallback> classlabel_callbacks;
    LuaCallback evolver_cocoon_callback;
    std::vector<LuaCallback> object_destroyed_callbacks;
    void* active_parameter_db = nullptr;
    const LuaOdfSnapshot* active_odf_snapshot = nullptr;
    bool active_odf_view = false;
    std::uint64_t next_odf_generation = 0;
    std::uint64_t active_odf_generation = 0;
    std::size_t loaded_script_count = 0;
    unsigned instruction_ticks = 0;
    std::uint32_t upgrade_pod_maximum_tier = 3;
    bool upgrade_pod_policy_registered = false;
    std::string upgrade_pod_policy_owner;
};

// Loads text-only .lua files from <root>\scripts. Roots use the same
// low-to-high precedence order as native modules, and matching basenames are
// overlaid before execution. Script failures are logged and isolated.
bool initialize_lua_host(const std::vector<std::string>& roots,
                         LuaHost& host,
                         LuaLogFunction log,
                         const LuaEngineApi& engine);

// Executes registered callbacks synchronously. The caller must serialize
// access to the LuaHost. The temporary ODF view is valid only for the duration
// of the callback and exposes bounded semantic reads, never engine pointers.
bool transform_classlabel(LuaHost& host,
                          void* parameter_db,
                          const std::string& classlabel,
                          std::string& replacement);
bool resolve_evolver_cocoon(LuaHost& host,
                            void* parameter_db,
                            std::string& cocoon);
bool resolve_object_destroyed(LuaHost& host,
                              const LuaObjectDestroyedEvent& event,
                              LuaObjectReplacement& replacement);

// Returns the normalized union declared by successfully loaded destruction
// callbacks. `basename` is always present in destroyed-object snapshots.
std::vector<std::string> object_destroyed_odf_fields(const LuaHost& host);

}  // namespace a2fo
