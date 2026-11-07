// Built-in Refit policy and native bridge fixture. The reference EXE is mapped
// as data only; its entry point, game loop, and input paths are never called.
#define main refit_standalone_fixture_main
#include <algorithm>
#include "refit_module_smoke.cpp"
#undef main
#include "../core/builtin_modules.hpp"
#include "../core/module_loader.hpp"
#include "../core/module_policy.hpp"
#include <utility>

extern "C" {
bool A2FO_CALL A2FOBuiltinCameraControls_QueryStatus(a2fo::refit::RefitStatus*);
bool A2FO_CALL A2FOBuiltinViewDistance_QueryStatus(a2fo::refit::RefitStatus*);
bool A2FOBuiltinCameraControls_Ready() noexcept;
bool A2FOBuiltinViewDistance_Ready() noexcept;
}

namespace {
using namespace a2fo;
using namespace a2fo::refit;
void* camera_bridge = nullptr;
void* view_bridge = nullptr;
unsigned camera_patches = 0, view_patches = 0;
bool fail_camera_patch = false, fail_view_patch = false;

bool A2FO_CALL builtin_patch(void* target, void* replacement,
                             const std::uint8_t* expected, std::size_t size) {
    const auto site = reinterpret_cast<std::uint8_t*>(engine);
    if (target == site + rb::pan_context_rva + rb::pan_patch_offset) {
        ++camera_patches;
        camera_bridge = replacement;
        if (fail_camera_patch) return false;
    } else if (target == site + rb::view_context_rva) {
        ++view_patches;
        view_bridge = replacement;
        if (fail_view_patch) return false;
    } else {
        require(false, "unexpected Refit patch site");
    }
    return a2fo_rs_windows_patch_call(target, replacement, expected, size);
}

void setting(bool pan, const char* enabled, const char* value) {
    const std::string path = root + (pan ? "\\A2FOCameraControls.ini" : "\\A2FOViewDistance.ini");
    const char* section = pan ? "CameraControls" : "ViewDistance";
    require(WritePrivateProfileStringA(section, "Enabled", enabled, path.c_str()) != 0,
            "write Refit enable");
    require(WritePrivateProfileStringA(section, pan ? "PanSpeed" : "MinimumDistance",
                                       value, path.c_str()) != 0, "write Refit value");
}

ModulePolicy selection(bool camera, bool view) {
    ModulePolicy policy;
    policy.managed = true;
    for (const auto& item : {std::pair<const char*, bool>{"A2FOCameraControls", camera},
                             {"A2FOViewDistance", view}}) {
        ModulePolicyEntry entry;
        entry.name = item.first;
        entry.state = item.second ? ModulePolicyState::active : ModulePolicyState::inactive;
        policy.entries.push_back(entry);
    }
    return policy;
}

void select(bool camera, bool view) {
    std::string error;
    require(apply_builtin_module_policy({root}, selection(camera, view), error),
            error.empty() ? "apply Refit policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::camera_controls) == camera &&
            builtin_module_enabled(BuiltinFeature::view_distance) == view,
            "independent Refit gates");
}

RefitStatus status(bool pan) {
    RefitStatus out{sizeof(RefitStatus), 0, 0, 0, 0, 0};
    require((pan ? A2FOBuiltinCameraControls_QueryStatus(&out)
                 : A2FOBuiltinViewDistance_QueryStatus(&out)), "Refit status query");
    require(out.version == 1, "Refit status version");
    return out;
}

void execute(bool pan, bool enabled) {
    bridge = pan ? camera_bridge : view_bridge;
    require(bridge != nullptr, "captured bridge");
    refit_target = fixture(pan);
    refit_pan_fixture = pan;
    refit_control_word = 0x37f;
    Capture out{};
    if (pan) {
        refit_parsed_value = float_to_bits(0.005f);
        set_word(rb::fastscroll_rva, float_to_bits(9.0f));
        run(out, true, 0);
        float actual = 0;
        const auto bits = word(rb::fastscroll_rva);
        std::memcpy(&actual, &bits, sizeof(actual));
        require(enabled ? actual > 0.01249f && actual < 0.01251f
                        : bits == refit_parsed_value, "camera bridge gated native parse");
        if (enabled) for (const std::uint16_t precision : {0u, 0x200u, 0x300u})
            for (const std::uint16_t rounding : {0u, 0x400u, 0x800u, 0xc00u}) {
                refit_control_word = static_cast<std::uint16_t>(0x7f | precision | rounding);
                run(out, true, 0);
                const auto result = word(rb::fastscroll_rva);
                std::memcpy(&actual, &result, sizeof(actual));
                require(actual > 0.01249f && actual < 0.01251f,
                        "camera scale across x87 control words");
            }
    } else {
        const auto below = float_to_bits(2800.0f);
        set_word(rb::far_plane_rva, below);
        set_word(rb::cull_rva, below);
        set_word(rb::restore_cull_rva, 123);
        run(out, false, enabled ? float_to_bits(10000.0f) : below);
        require(word(rb::far_plane_rva) == (enabled ? float_to_bits(10000.0f) : below) &&
                word(rb::cull_rva) == (enabled ? float_to_bits(10000.0f) : below),
                "view bridge gated native parse");
        if (enabled) {
            const std::array<std::uint32_t, 10> values{{
                0, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u,
                0x7f812345u, float_to_bits(2800.0f), float_to_bits(20000.0f),
                float_to_bits(10000.0f), 0x3f800000u}};
            for (const auto far_bits : values) for (const auto cull : values) {
                set_word(rb::far_plane_rva, far_bits);
                set_word(rb::cull_rva, cull);
                set_word(rb::restore_cull_rva, 123);
                const auto expected_far = raise_positive_finite_bits(far_bits, float_to_bits(10000.0f));
                const auto expected_cull = raise_positive_finite_bits(cull, float_to_bits(10000.0f));
                run(out, false, expected_cull);
                require(word(rb::far_plane_rva) == expected_far &&
                        word(rb::cull_rva) == expected_cull &&
                        word(rb::restore_cull_rva) == expected_cull,
                        "view special FP values");
            }
        }
    }
    require(status(pan).parser_visits > 0, "native parser visit recorded");
    require(VirtualFree(refit_target, 0, MEM_RELEASE) != 0, "release private bridge fixture");
    refit_target = nullptr;
}

void write_text(const std::string& path, const char* contents) {
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "create loader fixture file");
    DWORD written = 0;
    const auto size = static_cast<DWORD>(std::strlen(contents));
    require(WriteFile(file, contents, size, &written, nullptr) && written == size,
            "write loader fixture file");
    CloseHandle(file);
}
void log_line(const std::string&) {}

void run_case(const char* exe, const std::string& mode) {
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    require(GetTempPathA(sizeof(temporary), temporary) != 0, "temporary path");
    require(GetTempFileNameA(temporary, "brf", 0, folder) != 0, "temporary name");
    require(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "private root");
    root = folder;
    const bool loader_mode = mode == "required" || mode == "legacy" || mode == "rejected";
    const bool missing = mode == "missing";
    const bool disabled = mode == "disabled";
    const bool noop = mode == "noop";
    const bool invalid = mode == "invalid";
    const bool inactive_config = missing || disabled || noop;
    if (!missing) {
        setting(true, disabled ? "0" : "1", invalid ? "NaN" : noop ? "1" : "2.5");
        setting(false, disabled ? "0" : "1", invalid ? "NaN" : noop ? "0" : "10000");
    }
    engine = LoadLibraryExA(exe, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    require(engine != nullptr, "map reference EXE without entry point");
    relocate_fixture();
    if (mode == "identity") mutate(0);
    if (mode == "relocation") remove_relocation(rb::pan_context_rva + 19);
    if (mode == "foreign") {
        mutate(rb::pan_context_rva + rb::pan_patch_offset);
        mutate(rb::view_context_rva);
    }
    if (mode == "context") {
        mutate(rb::pan_context_rva);
        mutate(rb::view_context_rva + 20);
    }
    fail_camera_patch = mode == "patch-fail";
    fail_view_patch = mode == "partial";
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger; api.armada_module = image; api.root_directory = directory;
    api.patch_call = builtin_patch;
    if (mode == "api") api.struct_size = offsetof(A2FO_ModuleApi, patch_call);
    set_builtin_module_roots({root});
    const bool rejected = mode == "rejected";
    if (loader_mode) {
        require(CreateDirectoryA((root + "\\modules").c_str(), nullptr) != 0,
                "loader module directory");
        write_text(root + "\\modules\\A2FOCameraControls.dll", "stale");
        write_text(root + "\\modules\\A2FOViewDistance.dll", "stale");
        write_text(root + "\\info.ini", mode == "legacy" ? "[mod]\nName=Fixture\n" :
            mode == "required" ? "[modules]\nrequired1=A2FOCameraControls\nrequired2=A2FOViewDistance\n" :
                                 "[modules]\nreject1=A2FOCameraControls\nreject2=A2FOViewDistance\n");
        require(discover_installed_modules(root).size() ==
                static_cast<std::size_t>(BuiltinFeature::count),
                "built-in inventory deduplicates stale Refit DLLs");
        LoadedModules loaded;
        require(load_native_modules_from_roots({root}, api, loaded, &log_line),
                "loader prepares Refit built-ins");
        require(std::all_of(loaded.begin(), loaded.end(), [](const LoadedModule& item) { return item.handle == nullptr; }), "stale standalone Refit DLLs never load");
    } else {
        require(prepare_builtin_module(BuiltinFeature::camera_controls, api, false) ==
                (mode != "api" && !invalid && mode != "identity" &&
                 mode != "relocation" && mode != "foreign" &&
                 mode != "context" && !fail_camera_patch),
                "camera preparation result");
        require(prepare_builtin_module(BuiltinFeature::view_distance, api, false) ==
                (mode != "api" && !invalid && mode != "identity" &&
                 mode != "foreign" && mode != "context" && !fail_view_patch),
                "view preparation result");
    }
    require(!status(true).armed && !status(false).armed,
            "prepared bridges remain neutral before policy publication");
    finish_builtin_module_preparation(false, false, false, false, false);
    if (loader_mode) {
        require(builtin_module_enabled(BuiltinFeature::camera_controls) == !rejected &&
                builtin_module_enabled(BuiltinFeature::view_distance) == !rejected,
                "required/legacy initially requested; rejected excluded");
    }
    const bool camera_ready = !rejected && mode != "api" && !invalid &&
        mode != "identity" && mode != "relocation" && mode != "foreign" &&
        mode != "context" && !fail_camera_patch;
    const bool view_ready = !rejected && mode != "api" && !invalid &&
        mode != "identity" && mode != "foreign" && mode != "context" && !fail_view_patch;
    require(builtin_module_available(BuiltinFeature::camera_controls) == camera_ready &&
            builtin_module_available(BuiltinFeature::view_distance) == view_ready,
            "Refit availability follows preparation");
    require(camera_patches == ((camera_ready || fail_camera_patch) && !inactive_config ? 1u : 0u) &&
            view_patches == ((!inactive_config && (view_ready || fail_view_patch) &&
                              mode != "api") ? 1u : 0u), "native patch attempts");
    require(status(true).installed == (camera_ready && camera_patches ? 1u : 0u) &&
            status(false).installed == (view_ready && view_patches ? 1u : 0u),
            "Refit installation status");
    if (camera_ready && view_ready) {
        require(prepare_builtin_module(BuiltinFeature::camera_controls, api, false) &&
                prepare_builtin_module(BuiltinFeature::view_distance, api, false) &&
                camera_patches == (inactive_config ? 0u : 1u) &&
                view_patches == (inactive_config ? 0u : 1u), "repeated preparation never repatches");
        if (inactive_config) {
            select(true, true);
            require(!status(true).installed && !status(false).installed,
                    "valid passive configuration remains unhooked");
        } else {
            select(false, false);
            execute(true, false); execute(false, false);
            select(true, false);
            execute(true, true); execute(false, false);
            select(false, true);
            execute(true, false); execute(false, true);
            select(true, true);
            execute(true, true); execute(false, true);
            const auto saved_far = word(rb::far_plane_rva);
            select(false, false);
            require(word(rb::far_plane_rva) == saved_far,
                    "policy toggle does not roll back engine value");
            execute(true, false); execute(false, false);
            select(true, true);
            execute(true, true); execute(false, true);
            require(camera_patches == 1 && view_patches == 1, "toggle never repatches");
        }
        std::string error;
        require(apply_builtin_module_policy({root + "\\other-mod"}, selection(false, false), error) &&
                builtin_module_enabled(BuiltinFeature::camera_controls) &&
                builtin_module_enabled(BuiltinFeature::view_distance),
                "another mod root cannot change running Refit gates");
    } else {
        std::string error;
        require(!apply_builtin_module_policy({root}, selection(true, true), error) && !error.empty() &&
                !builtin_module_enabled(BuiltinFeature::camera_controls) &&
                !builtin_module_enabled(BuiltinFeature::view_distance),
                "unavailable policy activation is atomic");
    }
    if (!missing) {
        require(DeleteFileA((root + "\\A2FOCameraControls.ini").c_str()) &&
                DeleteFileA((root + "\\A2FOViewDistance.ini").c_str()), "remove private INIs");
    }
    if (loader_mode) {
        require(DeleteFileA((root + "\\modules\\A2FOCameraControls.dll").c_str()) &&
                DeleteFileA((root + "\\modules\\A2FOViewDistance.dll").c_str()) &&
                DeleteFileA((root + "\\info.ini").c_str()) &&
                RemoveDirectoryA((root + "\\modules").c_str()), "remove loader fixtures");
    }
    require(RemoveDirectoryA(root.c_str()) != 0, "remove private root");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: builtin_refit_native_test.exe ENGINE_EXE MODE\n");
        return 2;
    }
    try {
        run_case(argv[1], argv[2]);
        std::printf("builtin Refit %s: PASS\n", argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "builtin Refit %s: FAIL: %s\n%s", argv[2], error.what(), diagnostic.c_str());
        return 1;
    }
}
