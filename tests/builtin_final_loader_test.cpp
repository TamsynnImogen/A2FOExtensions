// Real loader and policy path with stubbed builtin initializers. Each mode
// runs in a fresh process because builtin preparation is process-local.
#include "../core/module_loader.hpp"
#include "../core/module_policy.hpp"
#include "../core/builtin_modules.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept;

namespace startup_fixture {
constexpr unsigned kCount = static_cast<unsigned>(a2fo::BuiltinFeature::count) -
                            static_cast<unsigned>(a2fo::BuiltinFeature::cheats);
extern std::array<unsigned, kCount> init_calls;
extern std::array<unsigned, kCount> shutdown_calls;
extern std::vector<std::string> init_order;
extern bool enforce_dependencies;
extern bool dependency_error;
extern bool producer_ready;
extern unsigned old_init_calls;
}

namespace {
using namespace a2fo;
constexpr unsigned kCount = startup_fixture::kCount;
static_assert(kCount == 24, "expected 24 startup modules");
std::vector<std::string> began;
std::vector<std::string> finished;
std::vector<std::string> logs;

void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

BuiltinFeature feature(unsigned index) {
    return static_cast<BuiltinFeature>(static_cast<unsigned>(BuiltinFeature::cheats) + index);
}

void begin(const std::string& path) { began.push_back(path); }
void finish(const std::string& path, bool accepted) {
    check(accepted, "initializer rejected " + path);
    const std::string name = path.substr(std::string("builtin:").size());
    BuiltinFeature feature;
    if (path.compare(0, 8, "builtin:") != 0 ||
        !find_builtin_feature(name, feature) ||
        (builtin_requires_restart(feature) && !A2FOBuiltinModule_StartupReady(name.c_str()))) {
        throw std::runtime_error("readiness absent at observer finish: " + path);
    }
    finished.push_back(path);
}
void log_line(const std::string& line) { logs.push_back(line); }

struct TempRoot {
    std::string path;
    TempRoot() {
        char directory[MAX_PATH]{};
        char candidate[MAX_PATH]{};
        check(GetTempPathA(MAX_PATH, directory) != 0, "GetTempPathA");
        check(GetTempFileNameA(directory, "bfl", 0, candidate) != 0,
              "GetTempFileNameA");
        check(DeleteFileA(candidate) != 0 && CreateDirectoryA(candidate, nullptr) != 0,
              "create private Data root");
        path = candidate;
    }
    ~TempRoot() {
        DeleteFileA((path + "\\info.ini").c_str());
        RemoveDirectoryA((path + "\\modules").c_str());
        RemoveDirectoryA(path.c_str());
    }
};

void run(const std::string& mode) {
    const bool reject_pack = mode == "reject";
    TempRoot root;
    {
        std::ofstream info(root.path + "\\info.ini", std::ios::binary);
        check(static_cast<bool>(info), "open private info.ini");
        info << "[modules]\n";
        unsigned active = 0;
        for (unsigned index = 0; index < kCount; ++index) {
            const auto item = feature(index);
            const std::string name = canonical_builtin_name(item);
            if (item == BuiltinFeature::feature_pack) {
                info << (reject_pack ? "reject0 = " : "required0 = ") << name << '\n';
            } else {
                info << "active" << active++ << " = " << name << '\n';
            }
        }
        check(static_cast<bool>(info), "write private info.ini");
    }
    startup_fixture::enforce_dependencies = !reject_pack;
    startup_fixture::producer_ready = mode == "producer_ready";
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    const std::vector<std::string> roots{root.path};
    LoadedModules loaded;
    const ModuleRegistrationObserver observer{begin, finish};
    check(load_native_modules_from_roots(roots, api, loaded, log_line, observer),
          "real loader rejected fixture");
    check(!startup_fixture::dependency_error, "dependent initializer saw missing startup readiness");

    std::vector<std::string> expected;
    for (unsigned index = 0; index < kCount; ++index) {
        const auto item = feature(index);
        const std::string name = canonical_builtin_name(item);
        const bool selected = !(reject_pack && item == BuiltinFeature::feature_pack);
        check(startup_fixture::init_calls[index] == (selected ? 1u : 0u),
              "startup init count " + name);
        check(A2FOBuiltinModule_StartupReady(name.c_str()) == selected,
              "pre-finish readiness " + name);
        if (selected) expected.push_back(name);
    }
    std::sort(expected.begin(), expected.end(), [](const std::string& a, const std::string& b) {
        return module_name_key(a) < module_name_key(b);
    });
    check(startup_fixture::init_order == expected, "startup initializer canonical order");
    check(loaded.size() == expected.size(), "one lifecycle record per selected startup module");
    for (std::size_t index = 0; index < loaded.size(); ++index) {
        check(loaded[index].path == "builtin:" + expected[index] &&
              loaded[index].handle == nullptr && loaded[index].shutdown != nullptr,
              "ordered core-owned lifecycle record");
    }
    check(began.size() == expected.size() +
              static_cast<unsigned>(BuiltinFeature::cheats) && finished == began,
          "observer brackets prepared live and selected startup builtins");
    check(startup_fixture::old_init_calls == static_cast<unsigned>(BuiltinFeature::cheats),
          "all live modules prepare while inactive");
    check(producer_dispatch_available(loaded) == (mode == "producer_ready"),
          "Producer capability follows ready provider only");

    finish_builtin_module_preparation(true, true, true, true, true, true, true, true);
    ModulePolicy changed = evaluate_module_policy(roots, discover_installed_modules(root.path));
    check(changed.valid && changed.managed, "private managed policy evaluated");
    for (auto& entry : changed.entries) {
        BuiltinFeature item;
        if (find_builtin_feature(entry.name, item) && builtin_requires_restart(item)) {
            entry.state = reject_pack ? ModulePolicyState::active : ModulePolicyState::rejected;
        }
    }
    std::string error;
    check(apply_builtin_module_policy(roots, changed, error) && error.empty(),
          "post-finish selection save");
    for (unsigned index = 0; index < kCount; ++index) {
        const auto item = feature(index);
        const bool selected = !(reject_pack && item == BuiltinFeature::feature_pack);
        bool initial = !selected;
        check(builtin_module_initial_request(item, initial) == selected &&
              (!selected || initial), "fixed initial selection");
        check(builtin_module_enabled(item) == selected &&
              A2FOBuiltinModule_StartupReady(canonical_builtin_name(item)) == selected,
              "startup state fixed after policy save");
    }
    unload_native_modules(loaded, log_line);
    check(loaded.empty(), "loader lifecycle released");
    for (unsigned index = 0; index < kCount; ++index) {
        const bool selected = !(reject_pack && feature(index) == BuiltinFeature::feature_pack);
        check(startup_fixture::shutdown_calls[index] == (selected ? 1u : 0u),
              "core-owned shutdown count");
    }
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "usage: builtin_final_loader_test producer_fail|producer_ready|reject");
        const std::string mode = argv[1];
        check(mode == "producer_fail" || mode == "producer_ready" || mode == "reject",
              "unknown mode");
        run(mode);
        std::printf("builtin final loader %s: passed\n", mode.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "builtin final loader: %s\n", error.what());
        return 1;
    }
}
