// Portable lifecycle test. Run each mode in a fresh process because builtin
// preparation is intentionally fixed for the lifetime of the process.
#include "../core/builtin_modules.hpp"
#include "../core/builtin_startup_modules.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" bool A2FO_CALL
A2FOBuiltinModule_StartupReady(const char* name) noexcept;

namespace startup_fixture {
constexpr unsigned kCount =
    static_cast<unsigned>(a2fo::BuiltinFeature::count) -
    static_cast<unsigned>(a2fo::BuiltinFeature::cheats);
extern std::array<unsigned, kCount> init_calls;
extern std::array<unsigned, kCount> shutdown_calls;
extern int fail_index;
extern unsigned old_init_calls;
}

namespace {
using namespace a2fo;
constexpr unsigned kCount = startup_fixture::kCount;
static_assert(kCount == 24, "startup fixture must cover the current 24 modules");
const std::vector<std::string> kRoots{"C:/Fleet Ops/Data", "C:/Fleet Ops/Data/Mods/Selected"};

void check(bool value, const std::string& why) {
    if (!value) throw std::runtime_error(why);
}

BuiltinFeature feature(unsigned index) {
    return static_cast<BuiltinFeature>(static_cast<unsigned>(BuiltinFeature::cheats) + index);
}

ModulePolicy policy(bool selected, bool include_unavailable_live = false) {
    ModulePolicy result;
    result.managed = true;
    for (unsigned index = 0; index < kCount; ++index) {
        ModulePolicyEntry entry;
        entry.name = canonical_builtin_name(feature(index));
        entry.state = selected ? ModulePolicyState::active : ModulePolicyState::rejected;
        result.entries.push_back(entry);
    }
    if (include_unavailable_live) {
        ModulePolicyEntry entry;
        entry.name = canonical_builtin_name(BuiltinFeature::build_tooltips);
        entry.state = ModulePolicyState::active;
        result.entries.push_back(entry);
    }
    return result;
}

void verify_state(bool initially_on, int failed) {
    for (unsigned index = 0; index < kCount; ++index) {
        const auto item = feature(index);
        const bool available = !initially_on || static_cast<int>(index) != failed;
        check(builtin_requires_restart(item), "restart classification");
        bool startup = !initially_on;
        check(builtin_module_initial_request(item, startup) && startup == initially_on,
              "initial snapshot ignores saved menu choices");
        check(startup_fixture::init_calls[index] == (initially_on ? 1u : 0u),
              "startup init count for " + std::string(canonical_builtin_name(item)));
        check(startup_fixture::shutdown_calls[index] == 0, "no policy shutdown");
        check(builtin_module_available(item) == available, "fixed availability");
        check(A2FOBuiltinModule_StartupReady(canonical_builtin_name(item)) ==
                  (initially_on && available), "startup readiness is fixed");
        const std::string dll_name =
            std::string(canonical_builtin_name(item)) + ".dll";
        check(A2FOBuiltinModule_StartupReady(dll_name.c_str()) ==
                  (initially_on && available), "DLL-name startup readiness is fixed");
        check(builtin_module_enabled(item) == (initially_on && available), "fixed activation");
        check((builtin_module_activation(item) != 0) == (initially_on && available),
              "fixed activation generation");
    }
}

void status_all(const char* expected, int failed = -1) {
    for (unsigned index = 0; index < kCount; ++index) {
        const char* own = static_cast<int>(index) == failed ? "Unavailable" : expected;
        check(builtin_module_status(canonical_builtin_name(feature(index)), kRoots) == own,
              "status for " + std::string(canonical_builtin_name(feature(index))));
    }
}

void apply(const ModulePolicy& selection, bool expected_success) {
    std::string error = "stale";
    check(apply_builtin_module_policy(kRoots, selection, error) == expected_success,
          "policy result");
    check(error.empty() == expected_success, "policy explanation");
}

void run(const std::string& mode, int failed) {
    const bool initially_on = mode != "off";
    startup_fixture::fail_index = failed;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    set_builtin_module_roots(kRoots);
    check(builtin_modules_match_roots({"c:/fleet ops/data/", "c:/fleet ops/data/mods/selected"}),
          "equivalent root identity");
    check(!builtin_modules_match_roots({"C:/Fleet Ops/Data", "C:/Other"}), "foreign roots");
    check(!prepare_builtin_module(BuiltinFeature::count, api, true), "invalid feature rejected");
    check(!initialize_startup_builtin(BuiltinFeature::build_tooltips, api), "non-startup init rejected");
    check(startup_builtin_shutdown(BuiltinFeature::build_tooltips) == nullptr,
          "non-startup shutdown absent");
    check(!A2FOBuiltinModule_StartupReady(nullptr), "null readiness name rejected");
    check(!A2FOBuiltinModule_StartupReady("A2FOUnknown.dll"),
          "unknown readiness name rejected");
    check(!A2FOBuiltinModule_StartupReady("A2FOBuildTooltips.dll"),
          "live module readiness rejected");

    for (unsigned index = 0; index < kCount; ++index) {
        const auto item = feature(index);
        check(!A2FOBuiltinModule_StartupReady(canonical_builtin_name(item)),
              "unprepared startup readiness is false");
        const std::string dll_name =
            std::string(canonical_builtin_name(item)) + ".dll";
        check(!A2FOBuiltinModule_StartupReady(dll_name.c_str()),
              "unprepared DLL-name readiness is false");
        check(builtin_module_status(canonical_builtin_name(item), kRoots) == "Starting",
              "unprepared status");
        check(prepare_builtin_module(item, api, initially_on) ==
                  !(initially_on && static_cast<int>(index) == failed),
              "initial preparation result");
        check(prepare_builtin_module(item, api, !initially_on) ==
                  !(initially_on && static_cast<int>(index) == failed),
              "preparation is one shot");
        check(A2FOBuiltinModule_StartupReady(canonical_builtin_name(item)) ==
                  (initially_on && static_cast<int>(index) != failed),
              "pre-finish readiness follows accepted initialization");
        check(A2FOBuiltinModule_StartupReady(dll_name.c_str()) ==
                  (initially_on && static_cast<int>(index) != failed),
              "pre-finish DLL-name readiness follows accepted initialization");
    }
    check(startup_fixture::old_init_calls == 0, "no unrelated old module initialized");
    finish_builtin_module_preparation(true, true, true, true, true, true, true, true);
    verify_state(initially_on, failed);
    status_all(initially_on ? "On (restart required to change)" :
                              "Off (restart required to change)", failed);

    const auto desired = policy(!initially_on);
    check(builtin_module_status(canonical_builtin_name(feature(0)), {"C:/Other"}) ==
              "On mod launch", "foreign root status");
    std::string error;
    check(apply_builtin_module_policy({"C:/Other"}, desired, error) && error.empty(),
          "foreign policy ignored");
    verify_state(initially_on, failed);
    status_all(initially_on ? "On (restart required to change)" :
                              "Off (restart required to change)", failed);

    ModulePolicy invalid = desired;
    invalid.valid = false;
    apply(invalid, false);
    verify_state(initially_on, failed);
    status_all(initially_on ? "On (restart required to change)" :
                              "Off (restart required to change)", failed);

    // An unprepared live module makes the entire save invalid, including the
    // startup requests. No old-module stub is entered by this operation.
    apply(policy(!initially_on, true), false);
    verify_state(initially_on, failed);
    status_all(initially_on ? "On (restart required to change)" :
                              "Off (restart required to change)", failed);

    apply(desired, true);
    verify_state(initially_on, failed);
    status_all(initially_on ? "Restart to turn off" : "Restart to turn on");
    apply(desired, true);
    verify_state(initially_on, failed);
    status_all(initially_on ? "Restart to turn off" : "Restart to turn on");

    apply(policy(initially_on), true);
    verify_state(initially_on, failed);
    status_all(initially_on ? "On (restart required to change)" :
                              "Off (restart required to change)", failed);
    check(startup_fixture::old_init_calls == 0, "no old module init during policy saves");

    // Verify each startup shutdown table slot points at its corresponding
    // entry point. Calling these test stubs is separate from policy teardown.
    for (unsigned index = 0; index < kCount; ++index) {
        auto shutdown = startup_builtin_shutdown(feature(index));
        check(shutdown != nullptr, "startup shutdown registered");
        shutdown();
        for (unsigned observed = 0; observed < kCount; ++observed)
            check(startup_fixture::shutdown_calls[observed] == (observed <= index ? 1u : 0u),
                  "startup shutdown table identity");
    }
}
}

int main(int argc, char** argv) {
    try {
        check(argc >= 2, "usage: builtin_startup_policy_test off|on|fail [failure-index]");
        const std::string mode = argv[1];
        check(mode == "off" || mode == "on" || mode == "fail", "unknown mode");
        int failed = -1;
        if (mode == "fail") {
            check(argc == 3, "fail mode requires index 0..23");
            failed = std::atoi(argv[2]);
            check(failed >= 0 && failed < static_cast<int>(kCount), "failure index range");
        }
        run(mode, failed);
        std::printf("builtin startup policy %s%s: passed\n", mode.c_str(),
                    mode == "fail" ? argv[2] : "");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "builtin startup policy: %s\n", error.what());
        return 1;
    }
}
