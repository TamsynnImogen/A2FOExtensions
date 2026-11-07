// Isolated native checks for Rust-owned module records and the real loader unload path.
#include "../core/module_loader.hpp"
#include "../core/module_policy.hpp"
#include "../core/builtin_modules.hpp"
#include "../core/builtin_startup_modules.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
[[noreturn]] void unexpected(const char* name) {
    std::fprintf(stderr, "unexpected loader fixture path: %s\n", name);
    std::abort();
}
std::vector<std::string> trace;
a2fo::LoadedModules* clearing_owner = nullptr;
unsigned shutdown_calls = 0;
void log_line(const std::string& message) { trace.push_back("log:" + message); }
void A2FO_CALL shutdown_first() { ++shutdown_calls; trace.push_back("shutdown:first"); }
void A2FO_CALL shutdown_second() { ++shutdown_calls; trace.push_back("shutdown:second"); }
void A2FO_CALL shutdown_clear_throw() {
    ++shutdown_calls;
    trace.push_back("shutdown:clear-throw");
    require(clearing_owner != nullptr, "clearing owner");
    clearing_owner->clear();
    throw std::runtime_error("injected shutdown failure");
}
void copy_move_and_bytes() {
    const std::string bytes("Data\\modules\\X\0\xff.dll", 20);
    a2fo::LoadedModules original;
    original.push_back({reinterpret_cast<HMODULE>(0xf1234567u), nullptr, bytes});
    require(original.size() == 1, "module push");
    const auto view = original.view(0);
    require(view.handle == 0xf1234567u && view.path_size == bytes.size() &&
            std::memcmp(view.path, bytes.data(), bytes.size()) == 0 &&
            view.path[bytes.size()] == 0, "full pointer and NUL path bytes");
    a2fo::LoadedModules copy(original);
    original.clear();
    require(original.empty() && copy.size() == 1 && copy[0].path == bytes,
            "independent clone after clear");
    a2fo::LoadedModules moved(std::move(copy));
    require(copy.empty() && moved.size() == 1 && moved[0].path == bytes,
            "move constructor");
    a2fo::LoadedModules assigned;
    assigned = moved;
    require(assigned.size() == 1 && assigned[0].path == bytes,
            "copy assignment");
    a2fo::LoadedModules moved_assigned;
    moved_assigned = std::move(assigned);
    require(assigned.empty() && moved_assigned.size() == 1 &&
            moved_assigned[0].path == bytes, "move assignment");
    require(moved_assigned.view(1).path == nullptr, "out-of-range view");
    bool rejected = false;
    try { (void)moved_assigned[1]; }
    catch (const std::out_of_range&) { rejected = true; }
    require(rejected, "out-of-range snapshot");
    std::puts("records:clone-move-byte-path:PASS");
}
void explicit_shutdown() {
    trace.clear();
    shutdown_calls = 0;
    {
        a2fo::LoadedModules record;
        record.push_back({nullptr, shutdown_first, "first"});
    }
    require(shutdown_calls == 0, "destructor must not invoke shutdown");
    a2fo::LoadedModules loaded;
    loaded.push_back({nullptr, shutdown_first, "first"});
    loaded.push_back({nullptr, shutdown_second, "second"});
    a2fo::unload_native_modules(loaded, log_line);
    require(loaded.empty() && shutdown_calls == 2 && trace.size() == 2 &&
            trace[0] == "shutdown:second" && trace[1] == "shutdown:first",
            "explicit reverse shutdown");
    std::puts("records:explicit-reverse-shutdown:PASS");
}
void clearing_throw() {
    trace.clear();
    shutdown_calls = 0;
    a2fo::LoadedModules loaded;
    const std::string path("C:\\fixture\\\xff.dll", 16);
    loaded.push_back({nullptr, shutdown_clear_throw, path});
    clearing_owner = &loaded;
    a2fo::unload_native_modules(loaded, log_line);
    clearing_owner = nullptr;
    require(loaded.empty() && shutdown_calls == 1 && trace.size() == 2 &&
            trace[0] == "shutdown:clear-throw" &&
            trace[1] == "log:Module loader: module threw during shutdown: " + path,
            "reentrant clear retains diagnostic bytes and one callback");
    std::puts("records:clear-throw-diagnostic:PASS");
}
bool middle_saw_empty = false;
unsigned recursive_unloads = 0;
void A2FO_CALL probe_shutdown_first() { ++shutdown_calls; trace.push_back("probe:first"); }
void A2FO_CALL probe_shutdown_last() { ++shutdown_calls; trace.push_back("probe:last"); }
void A2FO_CALL probe_shutdown_middle() {
    ++shutdown_calls;
    trace.push_back("probe:middle");
    middle_saw_empty = clearing_owner && clearing_owner->empty();
    if (clearing_owner) {
        clearing_owner->clear();
        ++recursive_unloads;
        a2fo::unload_native_modules(*clearing_owner, log_line);
    }
    throw std::runtime_error("probe middle shutdown failure");
}
void throw_range_log(const std::string& message) {
    trace.push_back("log:" + message);
    throw std::range_error("injected logger failure");
}
void probe_reentrant_cleanup(const char* probe_path, bool throwing_logger) {
    require(probe_path && *probe_path, "nonempty probe path");
    require(GetModuleHandleA(probe_path) == nullptr, "probe absent before case");
    trace.clear();
    shutdown_calls = 0;
    recursive_unloads = 0;
    middle_saw_empty = false;
    const std::string diagnostic = std::string(probe_path) + "\\diagnostic\xff";
    a2fo::LoadedModules loaded;
    HMODULE first = LoadLibraryA(probe_path);
    require(first != nullptr, "first probe LoadLibraryA");
    loaded.push_back({first, probe_shutdown_first, diagnostic});
    HMODULE second = LoadLibraryA(probe_path);
    require(second == first, "second probe reference shares HMODULE");
    loaded.push_back({second, probe_shutdown_middle, diagnostic});
    HMODULE third = LoadLibraryA(probe_path);
    require(third == first, "third probe reference shares HMODULE");
    loaded.push_back({third, probe_shutdown_last, diagnostic});
    clearing_owner = &loaded;
    bool caught_range_error = false;
    try {
        a2fo::unload_native_modules(loaded, throwing_logger ? throw_range_log : log_line);
    } catch (const std::range_error& error) {
        caught_range_error = std::strcmp(error.what(), "injected logger failure") == 0;
    }
    clearing_owner = nullptr;
    require(caught_range_error == throwing_logger, "typed logger exception after cleanup");
    require(middle_saw_empty && recursive_unloads == 1 && loaded.empty(),
            "detached public owner and one recursive unload");
    require(shutdown_calls == 3 && trace.size() == 4 &&
            trace[0] == "probe:last" && trace[1] == "probe:middle" &&
            trace[2] == "log:Module loader: module threw during shutdown: " + diagnostic &&
            trace[3] == "probe:first", "reverse callbacks and exact diagnostic bytes");
    require(GetModuleHandleA(probe_path) == nullptr, "all three probe references released");
    std::puts(throwing_logger ? "records:probe-throwing-logger:PASS" :
                                "records:probe-reentrant-cleanup:PASS");
}
} // namespace

// The real unload path does not invoke discovery, admission, or built-in startup.
namespace a2fo {
std::vector<InstalledModule> discover_installed_modules(const std::string&,
                                                        std::vector<std::string>*) {
    unexpected("discover_installed_modules");
}
ModulePolicy evaluate_module_policy(const std::vector<std::string>&,
                                    const std::vector<InstalledModule>&) {
    unexpected("evaluate_module_policy");
}
void set_builtin_module_roots(const std::vector<std::string>&) {
    unexpected("set_builtin_module_roots");
}
bool find_builtin_feature(const std::string&, BuiltinFeature&) noexcept {
    unexpected("find_builtin_feature");
}
bool prepare_builtin_module(BuiltinFeature, const A2FO_ModuleApi&, bool) {
    unexpected("prepare_builtin_module");
}
bool builtin_requires_restart(BuiltinFeature) noexcept {
    unexpected("builtin_requires_restart");
}
A2FO_ModuleShutdownFn startup_builtin_shutdown(BuiltinFeature) noexcept {
    unexpected("startup_builtin_shutdown");
}
} // namespace a2fo
extern "C" bool A2FO_CALL A2FOBuiltinModule_StartupReady(const char*) noexcept {
    unexpected("A2FOBuiltinModule_StartupReady");
}
extern "C" bool A2FO_CALL A2FO_ProducerEventsReady() {
    unexpected("A2FO_ProducerEventsReady");
}

int main(int argc, char** argv) {
    try {
        copy_move_and_bytes();
        explicit_shutdown();
        clearing_throw();
        if (argc > 1) {
            require(argc == 2, "one probe path argument");
            probe_reentrant_cleanup(argv[1], false);
            probe_reentrant_cleanup(argv[1], true);
        }
        std::puts("rust-runtime-ownership-native:PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "rust-runtime-ownership-native: %s\n", error.what());
        return 1;
    }
}
