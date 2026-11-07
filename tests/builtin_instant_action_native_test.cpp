// Synthetic i386 native fixture for the built-in Instant Action Settings module.
// No game entry point, window, input message, or GUI device is used.
#include "../core/builtin_instant_action_settings.cpp"
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace a2fo;
std::uint8_t* image = nullptr;
std::uint8_t* fleet_image = nullptr;
std::string root;
std::string diagnostics;
unsigned hooks = 0, native_loads = 0, native_reads = 0;
unsigned native_openings = 0, native_races = 0, native_dialogs = 0;
unsigned fail_hook = 0;
bool native_result = false;
bool toggle_during_load = false;
bool toggle_during_open = false, nested_load = false, run_scoped = true;
bool directory_available = true, missing_race = false;
unsigned load_depth = 0;
bool malformed = false;
std::uint32_t requested_size = kSetupDetailsSize;
const char* requested_profile = "Settings.prf";
bool redirect_observed = false;
bool native_arguments_ok = true, scoped_ok = true;
std::string opened_path;
std::array<std::uint8_t, kSetupDetailsSize> blob{};
char* reader_cursor_after = nullptr;
constexpr char native_profile[] = "Settings.prf";
constexpr char expected_profile[] = "C:\\FixtureSettings\\Settings.prf";

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class T> void put(std::uint8_t* base, std::size_t offset, T value) {
    std::memcpy(base + offset, &value, sizeof(value));
}
void put_bytes(std::size_t offset, const std::uint8_t* bytes, std::size_t count) {
    std::memcpy(image + offset, bytes, count);
}
void A2FO_CALL logger(const char*, const char* text) {
    if (text) diagnostics += std::string(text) + '\n';
}
void* A2FO_CALL armada_module() { return image; }
void* A2FO_CALL fleetops_module() { return fleet_image; }
bool A2FO_CALL settings_directory(char* output, std::uint32_t capacity) {
    constexpr char directory[] = "C:\\FixtureSettings";
    if (!directory_available || capacity < sizeof(directory)) return false;
    std::memcpy(output, directory, sizeof(directory));
    return true;
}

void policy(bool enabled) {
    ModulePolicy choice;
    choice.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOInstantActionSettings";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    choice.entries.push_back(entry);
    std::string error;
    check(apply_builtin_module_policy({root}, choice, error),
          error.empty() ? "apply Instant Action policy" : error.c_str());
    check(builtin_module_enabled(BuiltinFeature::instant_action_settings) == enabled,
          "Instant Action live gate");
}

bool __cdecl native_read(void*, void* destination, std::uint32_t size) {
    ++native_reads;
    std::memset(destination, 0x5a, size);
    return native_result;
}
extern "C" std::uintptr_t __cdecl native_open(void* reader, void* path_string,
                                                std::uintptr_t options) {
    ++native_openings;
    native_arguments_ok &= reader != nullptr && options == 0x1234;
    char* path = nullptr;
    std::memcpy(&path, static_cast<std::uint8_t*>(path_string) + 4, sizeof(path));
    opened_path = path ? path : "";
    redirect_observed = opened_path == expected_profile;
    if (toggle_during_open) { toggle_during_open = false; policy(false); }
    return 0x12345678;
}
extern "C" __attribute__((naked)) void native_open_bridge() {
    __asm__ volatile("pushl 8(%esp)\npushl 8(%esp)\npushl %ecx\n"
                     "call _native_open\naddl $12,%esp\nret $8");
}
void* __cdecl native_race(std::int32_t) {
    ++native_races;
    return missing_race ? nullptr : reinterpret_cast<void*>(0x12340000);
}
INT_PTR CALLBACK native_dialog(HWND, UINT message, WPARAM, LPARAM) {
    ++native_dialogs;
    check(message == WM_NULL, "dialog fixture only uses WM_NULL");
    return 43;
}

void scoped_checks();
void __cdecl native_load(void* setup) {
    ++native_loads;
    native_arguments_ok &= setup != nullptr;
    if (toggle_during_load) {
        policy(false);
        policy(true);
    }
    if (nested_load && !load_depth) {
        ++load_depth;
        const auto saved_activation = g_active_load_activation;
        load_settings_hook(reinterpret_cast<void*>(0x98760000));
        scoped_ok &= g_active_load_game_setup == setup &&
            g_active_load_activation == saved_activation && active_load_scope();
        --load_depth;
    }
    if (run_scoped) scoped_checks();
    else scoped_ok &= !active_load_scope();
}

bool A2FO_CALL install(void* target, void* replacement, std::size_t size,
                       const std::uint8_t* expected, A2FO_InlineHook* hook) {
    check(target && replacement && hook && expected &&
          std::memcmp(target, expected, size) == 0,
          "Instant Action hook signature");
    ++hooks;
    if (hooks == fail_hook) return false;
    void* gateway = nullptr;
    switch (hooks) {
        case 1:
            check(target == image + kRaceByIdRva, "race hook target");
            check(replacement == reinterpret_cast<void*>(&race_by_id_hook), "race replacement");
            gateway = reinterpret_cast<void*>(&native_race); break;
        case 2:
            check(target == image + kLoadSettingsRva, "LoadSettings hook target");
            check(replacement == reinterpret_cast<void*>(&load_settings_hook), "load replacement");
            gateway = reinterpret_cast<void*>(&native_load); break;
        case 3:
            check(target == image + kReadBlobRva, "ReadBlob hook target");
            check(replacement == reinterpret_cast<void*>(&read_blob_hook), "blob replacement");
            gateway = reinterpret_cast<void*>(&native_read); break;
        case 4:
            check(target == image + kFileReaderConstructorRva, "FileReader hook target");
            check(replacement == reinterpret_cast<void*>(&a2fo_instant_action_settings_file_reader_bridge), "reader replacement");
            gateway = reinterpret_cast<void*>(&native_open_bridge); break;
        case 5:
            check(target == image + kMultiplayerSetupDlgProcRva, "dialog hook target");
            check(replacement == reinterpret_cast<void*>(&multiplayer_setup_dialog_hook), "dialog replacement");
            gateway = reinterpret_cast<void*>(&native_dialog); break;
        default: check(false, "unexpected extra Instant Action hook");
    }
    *hook = {target, gateway, static_cast<std::uint32_t>(size)};
    return true;
}

void seed(bool bad_identity, bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet_image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(image && fleet_image, "synthetic image allocation");
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = a2fo::supported_armada::kCanonicalTimestamp;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = bad_identity ? 0x400000 :
        a2fo::supported_armada::kCanonicalImageSize;
    auto* fleet_dos = reinterpret_cast<IMAGE_DOS_HEADER*>(fleet_image);
    fleet_dos->e_magic = IMAGE_DOS_SIGNATURE;
    fleet_dos->e_lfanew = 0x80;
    auto* fleet_nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(fleet_image + 0x80);
    fleet_nt->Signature = IMAGE_NT_SIGNATURE;
    fleet_nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    fleet_nt->OptionalHeader.SizeOfImage = 0x1000;
    for (const auto site : {kMultiplayerSetupDlgProcRva, kLoadSettingsRva,
                            kFileReaderConstructorRva})
        put_bytes(site, kExpectedFunctionPrologue.data(), kExpectedFunctionPrologue.size());
    put_bytes(kReadBlobRva, kExpectedReadBlob.data(), kExpectedReadBlob.size());
    put_bytes(kRaceByIdRva, kExpectedRaceById.data(), kExpectedRaceById.size());
    put_bytes(kGetGameSetupRva, kExpectedGetGameSetup.data(), kExpectedGetGameSetup.size());
    put_bytes(kGameSetupIsHostRva, kExpectedGameSetupIsHost.data(), kExpectedGameSetupIsHost.size());
    if (bad_signature) image[kReadBlobRva] ^= 1;
}

std::string details_line() {
    static constexpr char hex[] = "0123456789abcdef";
    std::string line = "setupDetails = ";
    for (std::size_t i = 0; i < blob.size(); ++i) {
        blob[i] = static_cast<std::uint8_t>(i);
        line += hex[blob[i] >> 4];
        line += hex[blob[i] & 15];
    }
    line += "\r\nnext = value\r\n";
    return line;
}

void scoped_checks() {
    std::string line = details_line();
    if (malformed) line[line.find('=') + 2] = 'G';
    std::array<std::uint8_t, 0x60> reader{};
    char* start = const_cast<char*>(line.data());
    char* end = start + line.size();
    put(reader.data(), 0x54, start);
    put(reader.data(), 0x58, end);
    std::array<std::uint8_t, kSetupDetailsSize> output{};
    const auto before = native_reads;
    const bool result = read_blob_hook(reader.data(), output.data(), requested_size);
    std::memcpy(&reader_cursor_after, reader.data() + 0x54, sizeof(reader_cursor_after));
    const bool decoded = native_reads == before;
    scoped_ok &= result == (decoded ? true : native_result);
    scoped_ok &= decoded ? output == blob && reader_cursor_after == start + line.find("next")
                         : output[0] == 0x5a && reader_cursor_after == start;
    std::array<std::uint8_t, 16> path_string{};
    char* path = const_cast<char*>(requested_profile);
    put(path_string.data(), 4, path);
    scoped_ok &= a2fo_instant_action_settings_call_thiscall_2(
        reinterpret_cast<void*>(&a2fo_instant_action_settings_file_reader_bridge),
        reader.data(), path_string.data(), 0x1234) == 0x12345678;
    char* restored = nullptr;
    std::memcpy(&restored, path_string.data() + 4, sizeof(restored));
    scoped_ok &= restored == path;
}

void exception_checks(bool enabled) {
    std::array<std::uint8_t, 0x1a0> object{};
    std::array<std::uint8_t, 32> entries{};
    auto* table = entries.data();
    put(object.data(), 0x190, table);
    put(object.data(), 0x19c, std::uint32_t{2});
    char* bad = reinterpret_cast<char*>(1);
    char valid[] = "retained";
    char* good = valid;
    put(entries.data(), 4, bad); put(entries.data(), 20, good);
    EXCEPTION_RECORD record{}; CONTEXT context{};
    record.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    record.ExceptionAddress = image + kShellMultipleConstructorScanRvaA;
    context.Ebx = reinterpret_cast<DWORD>(object.data()); context.Edi = 0x12345678;
    EXCEPTION_POINTERS pointers{&record, &context};
    check(shell_multiple_exception_handler(&pointers) ==
              (enabled ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH),
          "exact-site exception gate");
    char* first = nullptr; char* second = nullptr;
    std::memcpy(&first, entries.data()+4, sizeof(first));
    std::memcpy(&second, entries.data()+20, sizeof(second));
    check(first == (enabled ? nullptr : bad) && second == good &&
          context.Edi == (enabled ? reinterpret_cast<DWORD>(native_empty_string()) : 0x12345678),
          "exception repairs only enabled corrupt label");
    record.ExceptionAddress = image + 0x100;
    context.Edi = 0x11223344;
    check(shell_multiple_exception_handler(&pointers) == EXCEPTION_CONTINUE_SEARCH &&
          context.Edi == 0x11223344, "unrelated exception left untouched");
    record.ExceptionAddress = image + kShellMultipleDestructorReadRva;
    put(entries.data(), 4, bad); context.Esi = reinterpret_cast<DWORD>(entries.data());
    context.Eip = 0x55667788;
    check(shell_multiple_exception_handler(&pointers) ==
          (enabled ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH),
          "destructor guard gate");
    std::memcpy(&first, entries.data()+4, sizeof(first));
    check(first == (enabled ? nullptr : bad) && context.Eip == (enabled ?
          reinterpret_cast<DWORD>(image+kShellMultipleDestructorContinueRva) : 0x55667788),
          "destructor continuation and label retained when off");
}

void race_checks(bool enabled) {
    // Private cdecl caller: PUSH argument; CALL hook; ADD ESP,4; RET. Its CALL
    // return address is exactly the documented starting-unit lookup site.
    auto* code = image + kStartingUnitsRaceLookupReturnRva - 9;
    const std::uint8_t bytes[]{0xff,0x74,0x24,0x04,0xe8,0,0,0,0,0x83,0xc4,0x04,0xc3};
    std::memcpy(code, bytes, sizeof(bytes));
    const auto displacement = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(&race_by_id_hook) -
        reinterpret_cast<std::uintptr_t>(code+9));
    std::memcpy(code+5, &displacement, sizeof(displacement));
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(bytes));
    missing_race = true;
    check(race_by_id_hook(7) == nullptr, "unrelated missing race stays null");
    check(reinterpret_cast<RaceById>(code)(7) ==
          (enabled ? g_empty_starting_race_table.data() : nullptr),
          "only enabled scoped missing-race lookup gets empty table");
    missing_race = false;
    check(reinterpret_cast<RaceById>(code)(7) == reinterpret_cast<void*>(0x12340000),
          "valid scoped race remains native");
}

void write_file(const std::string& path, const char* contents) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "private policy file");
    DWORD written{};
    const auto length = static_cast<DWORD>(std::strlen(contents));
    check(WriteFile(file, contents, length, &written, nullptr) && written == length,
          "write private policy"); CloseHandle(file);
}

void run(const std::string& mode) {
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    check(GetTempPathA(sizeof(temporary), temporary) != 0, "temporary directory");
    check(GetTempFileNameA(temporary, "ias", 0, folder) != 0, "temporary name");
    check(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "private root");
    root = folder;
    seed(mode == "identity", mode == "signature");
    if (mode == "fleet") fleet_image[0] = 0;
    if (mode.size() == 5 && mode.substr(0, 4) == "fail")
        fail_hook = static_cast<unsigned>(mode[4] - '0');
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger;
    api.armada_module = armada_module;
    api.fleetops_module = fleetops_module;
    api.install_inline_hook = install;
    api.get_settings_directory = settings_directory;
    if (mode == "api") api.struct_size = A2FO_MODULE_API_V4_BASE_SIZE - 1;
    set_builtin_module_roots({root});
    const bool loader = mode == "required" || mode == "rejected" || mode == "legacy";
    const bool preflight_fail = mode == "identity" || mode == "signature" ||
                               mode == "fleet" || mode == "api";
    const bool ready = !preflight_fail && !fail_hook && mode != "rejected";
    if (loader) {
        check(CreateDirectoryA((root+"\\modules").c_str(), nullptr) ||
              GetLastError() == ERROR_ALREADY_EXISTS, "private modules directory");
        write_file(root+"\\modules\\A2FOInstantActionSettings.dll", "stale");
        write_file(root+"\\info.ini", mode == "legacy" ? "[mod]\nName=Fixture\n" :
            mode == "required" ? "[modules]\nrequired1=A2FOInstantActionSettings\n" :
                                 "[modules]\nreject1=A2FOInstantActionSettings\n");
        check(discover_installed_modules(root).size() == static_cast<std::size_t>(BuiltinFeature::count),
              "stale Instant Action DLL deduplicated");
        LoadedModules loaded;
        check(load_native_modules_from_roots({root}, api, loaded, [](const std::string&){}),
              "actual module loader");
        check(std::all_of(loaded.begin(), loaded.end(), [](const LoadedModule& item) { return item.handle == nullptr; }), "stale DLL never loaded");
    } else check(prepare_builtin_module(BuiltinFeature::instant_action_settings, api, false) == ready,
                 "Instant Action preparation outcome");
    check(feature_activation() == 0, "preparation remains disarmed before publication");
    finish_builtin_module_preparation(false, false, false, false, false);
    check(builtin_module_available(BuiltinFeature::instant_action_settings) == ready,
          "Instant Action availability");
    check(hooks == (preflight_fail || mode == "rejected" ? 0u :
                    fail_hook ? fail_hook : 5u), "hook install count");
    if (loader) check(builtin_module_enabled(BuiltinFeature::instant_action_settings) == ready,
                      "required/legacy on and rejected off at startup");
    if (ready) {
        check(prepare_builtin_module(BuiltinFeature::instant_action_settings, api, false) &&
              hooks == 5, "idempotent preparation");
        for (bool enabled : {false, true, false, true}) {
            policy(enabled);
            native_reads = native_openings = 0;
            native_result = true;
            void* setup = reinterpret_cast<void*>(0x12340000);
            load_settings_hook(setup);
            check(native_loads != 0 && native_reads == (enabled ? 0u : 1u) &&
                  native_openings == 1 && redirect_observed == enabled &&
                  native_arguments_ok && scoped_ok,
                  "load scoped decode and redirect gate");
            check(multiplayer_setup_dialog_hook(nullptr, WM_NULL, 0, 0) == 43,
                  "WM_NULL preserves native dialog result");
            check(race_by_id_hook(7) == reinterpret_cast<void*>(0x12340000),
                  "race lookup preserves native result");
            exception_checks(enabled);
            race_checks(enabled);
        }
        check(native_dialogs == 4 && native_races >= 4 && hooks == 5,
              "native callbacks preserved without repatching");
        policy(true);
        for (const auto variant : {0,1,2,3}) {
            requested_size = variant == 0 ? 33 : kSetupDetailsSize;
            malformed = variant == 1;
            directory_available = variant != 2;
            requested_profile = variant == 3 ? "Unrelated.prf" : native_profile;
            native_result = false; native_reads = 0;
            load_settings_hook(reinterpret_cast<void*>(1));
            check(scoped_ok && native_reads == (variant <= 1 ? 1u : 0u) &&
                  redirect_observed == (variant <= 1), "scoped native fallback and unrelated path");
        }
        requested_size = kSetupDetailsSize; malformed = false;
        directory_available = true; requested_profile = native_profile;
        native_result = false; native_reads = 0;
        scoped_checks();
        check(scoped_ok && native_reads == 1 && !redirect_observed,
              "enabled unscoped consumers preserve native false result and path");
        native_result = true; native_reads = 0;
        toggle_during_load = true;
        load_settings_hook(reinterpret_cast<void*>(1));
        check(scoped_ok && native_reads == 1 && !redirect_observed && feature_activation(),
              "off/on cannot revive pending load");
        toggle_during_load = false; native_reads = 0;
        nested_load = true;
        load_settings_hook(reinterpret_cast<void*>(1));
        check(scoped_ok && native_reads == 0 && redirect_observed &&
              !g_active_load_game_setup && !g_active_load_activation,
              "fresh nested load restores outer then clears context");
        nested_load = false;
        toggle_during_open = true;
        load_settings_hook(reinterpret_cast<void*>(1));
        check(scoped_ok && !feature_activation(), "in-flight redirected path restored after disable");
        policy(true);
        std::string error;
        ModulePolicy other;
        check(apply_builtin_module_policy({root+"\\Other"}, other, error) && feature_activation(),
              "another mod cannot change running gate");
        check(hooks == 5, "all policy changes keep original hook installation");
    } else {
        ModulePolicy choice;
        ModulePolicyEntry entry;
        entry.name = "A2FOInstantActionSettings";
        entry.state = ModulePolicyState::active;
        choice.entries.push_back(entry);
        std::string error;
        check(!apply_builtin_module_policy({root}, choice, error) && !error.empty(),
              "unavailable activation refused");
        exception_checks(false);
        if (g_race_by_id_hook.gateway) race_checks(false);
        native_result = false;
        if (g_read_blob_hook.gateway) {
            std::array<std::uint8_t, 16> output{};
            check(!read_blob_hook(nullptr, output.data(), output.size()) && output[0] == 0x5a,
                  "partial blob hook forwards native false result");
        }
        if (g_load_settings_hook.gateway) {
            run_scoped = false; load_settings_hook(reinterpret_cast<void*>(1));
            check(scoped_ok && !g_active_load_game_setup, "partial load hook never tracks scope");
        }
        if (g_file_reader_hook.gateway) {
            std::array<std::uint8_t, 16> path{};
            char* name = const_cast<char*>(native_profile); put(path.data(), 4, name);
            check(a2fo_instant_action_settings_call_thiscall_2(
                reinterpret_cast<void*>(&a2fo_instant_action_settings_file_reader_bridge),
                path.data(), path.data(), 0x1234) == 0x12345678 &&
                opened_path == native_profile, "partial file hook preserves native path");
        }
    }
    if (loader) {
        check(DeleteFileA((root+"\\modules\\A2FOInstantActionSettings.dll").c_str()) &&
              DeleteFileA((root+"\\info.ini").c_str()) && RemoveDirectoryA((root+"\\modules").c_str()),
              "remove private loader files");
    }
    check(RemoveDirectoryA(root.c_str()) != 0, "remove private root");
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: builtin_instant_action_native_test.exe MODE\n");
        return 2;
    }
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    try {
        run(argv[1]);
        std::printf("builtin Instant Action %s: PASS\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "builtin Instant Action %s: FAIL: %s\n%s",
                     argv[1], error.what(), diagnostics.c_str());
        return 1;
    }
}
