// Exercise the actual cached InfoIni consumers with synthetic native gateways.
// Including the core gives this executable access to private hook entrypoints;
// DllMain and A2FO_Initialize are never invoked. No game or GUI is launched.
#include "../core/dllmain.cpp"
#include <array>
#include <stdexcept>

namespace {
unsigned native_directory_calls = 0, native_settings_calls = 0;
unsigned native_configuration_calls = 0, native_save_calls = 0;
constexpr char native_path[] = "C:\\NativeSettings";
constexpr char override_path[] = "C:\\FixtureSettings";

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void __attribute__((regparm(2))) fixture_directory(void*, void* output) {
    ++native_directory_calls;
    *static_cast<const char**>(output) = native_path;
}
void __attribute__((regparm(2))) fixture_assign(void* output, void* value) {
    *static_cast<const char**>(output) = static_cast<const char*>(value);
}
void* __attribute__((regparm(1))) fixture_settings(void* settings) {
    ++native_settings_calls;
    return settings;
}
void __attribute__((regparm(1))) fixture_save(void*) { ++native_save_calls; }
void* __attribute__((fastcall)) fixture_configuration(void* configuration, void*) {
    ++native_configuration_calls;
    *reinterpret_cast<int*>(static_cast<std::uint8_t*>(configuration) + 4) = 5;
    return reinterpret_cast<void*>(1);
}
void thunk(std::uintptr_t rva, void* function) {
    // push immediate; ret preserves the native argument registers.
    std::uint8_t code[]{0x68, 0, 0, 0, 0, 0xc3};
    std::memcpy(code + 1, &function, sizeof(function));
    std::memcpy(at(g_fleet_ops, rva), code, sizeof(code));
}
void run() {
    InitializeCriticalSection(&g_state_lock);
    g_state_lock_ready = true;
    g_deferred_initialization_finished = 1;
    g_root_directory = "C:\\A2FO_Cache_Fixture";
    g_extension_roots = {g_root_directory};
    // Model successful typed ParameterDB hook preparation; this fixture only
    // exercises registration maps and never invokes those native hooks.
    g_classlabel_alias_hook_ready = true;
    g_module_api = make_module_api();
    for (bool commit : {false, true}) {
        const std::string owner = commit ? "fixture:commit" : "fixture:rollback";
        const char* alias = commit ? "fixture_breeder_commit" : "fixture_breeder_rollback";
        begin_module_registration(owner);
        require(api_register_classlabel_alias("A2FOBreeders",alias,"shipyard"),
                "prepare schema alias in actual registration transaction");
        A2FO_ClasslabelOdfDefault field{"breederClass","1"};
        require(api_register_classlabel_odf_defaults("A2FOBreeders",alias,&field,1),
                "prepare ODF schema in actual registration transaction");
        finish_module_registration(owner,commit);
        require((g_classlabel_aliases.count(alias)!=0)==commit &&
                (g_classlabel_odf_defaults.count(alias)!=0)==commit,
                "actual core rolls failed aliases/defaults back and retains committed schemas");
    }

    begin_module_registration("builtin:A2FOInfoIni");
    const bool prepared = a2fo::prepare_builtin_module(
        a2fo::BuiltinFeature::info_ini, g_module_api, false);
    finish_module_registration("builtin:A2FOInfoIni", prepared);
    require(prepared, "prepare actual InfoIni provider");
    a2fo::set_builtin_module_roots(g_extension_roots);
    a2fo::finish_builtin_module_preparation(true, true, true, true, true);

    // Model values already cached during an earlier enabled lookup.
    g_mod_defaults_state = 2;
    g_has_default_game_speed = true;
    g_default_game_speed = 3;
    g_delphi_settings_directory = override_path;
    g_mod_user_directory_hook.gateway = reinterpret_cast<void*>(&fixture_directory);
    g_fo_settings_get_instance_hook.gateway = reinterpret_cast<void*>(&fixture_settings);
    g_game_configuration_new_hook.gateway = reinterpret_cast<void*>(&fixture_configuration);
    g_game_configuration_load_profile_hook.gateway = reinterpret_cast<void*>(&fixture_configuration);
    const auto bytes = std::max(kFoSettingsSaveRva, kDelphiLStrAsgRva) + 16;
    g_fleet_ops = static_cast<HMODULE>(VirtualAlloc(
        nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    require(g_fleet_ops != nullptr, "allocate synthetic FleetOps gateways");
    thunk(kFoSettingsSaveRva, reinterpret_cast<void*>(&fixture_save));
    thunk(kDelphiLStrAsgRva, reinterpret_cast<void*>(&fixture_assign));
    FlushInstructionCache(GetCurrentProcess(), g_fleet_ops, bytes);

    for (bool enabled : {false, true, false, true}) {
        a2fo::ModulePolicy policy;
        policy.valid = true;
        a2fo::ModulePolicyEntry entry;
        entry.name = "A2FOInfoIni";
        entry.state = enabled ? a2fo::ModulePolicyState::active : a2fo::ModulePolicyState::inactive;
        policy.entries.push_back(entry);
        std::string error;
        require(a2fo::apply_builtin_module_policy(g_extension_roots, policy, error),
                "apply live cache-fixture policy");
        require(default_user_profile_game_speed_hook() == (enabled ? 3 : 5),
                "cached user-profile speed respects live gate");
        const char* directory = nullptr;
        mod_user_directory_hook(nullptr, &directory);
        require(directory && std::strcmp(directory, enabled ? override_path : native_path) == 0,
                "cached directory hook preserves native path while disabled");
        char output[128] = "sentinel";
        require(api_get_settings_directory(output, sizeof(output)) == enabled,
                "shared settings-directory API respects live gate");
        require(enabled ? std::strcmp(output, override_path) == 0 : output[0] == '\0',
                "shared API exposes no stale override while disabled");
        std::array<int, 8> configuration{};
        game_configuration_new_hook(configuration.data(), nullptr);
        require(configuration[1] == (enabled ? 3 : 5), "cached new-configuration speed gate");
        g_fo_settings_first_run = 1;
        require(game_configuration_load_profile_hook(configuration.data(), nullptr),
                "native profile result preserved");
        require(configuration[1] == (enabled ? 3 : 5), "cached profile-load speed gate");
        alignas(4) std::array<std::uint8_t, 0x80> settings{};
        settings[0x70] = 1;
        g_fo_settings_default_applied = 0;
        require(fo_settings_get_instance_hook(settings.data()) == settings.data(),
                "native settings instance preserved");
        int speed = 0;
        std::memcpy(&speed, settings.data() + 8, sizeof(speed));
        require(speed == (enabled ? 3 : 0), "cached first-run Settings.xml speed gate");
    }
    require(native_directory_calls == 4 && native_settings_calls == 4 &&
                native_configuration_calls == 8 && native_save_calls == 2,
            "native calls preserved; only enabled defaults saved");
}
}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        run();
        std::puts("builtin cached core defaults: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
