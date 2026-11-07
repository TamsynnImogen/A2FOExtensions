// Console-only resolver fixture. Each mode runs in a fresh process because
// Windows retains the loaded module identity for the process lifetime.
#include "../core/builtin_animated_hardpoints.cpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace a2fo {
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept { return 0; }
}

namespace {
void check(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}

using FixtureInt = int (__cdecl*)() noexcept;
FixtureInt fixture_export(HMODULE module, const char* name) {
    FARPROC address = GetProcAddress(module, name);
    FixtureInt result = nullptr;
    static_assert(sizeof(result) == sizeof(address));
    std::memcpy(&result, &address, sizeof(result));
    check(result != nullptr, "fixture query export");
    return result;
}

void run(const std::string& mode) {
    check(mode == "core" || mode == "fallback" || mode == "incomplete",
          "usage: builtin_animation_bridge_native_test.exe core|fallback|incomplete");
    HMODULE core = nullptr;
    if (mode != "fallback") {
        const char* path = mode == "incomplete"
            ? "build/fixtures/builtin-animation/missing/A2FOExtensions.dll"
            : "build/fixtures/builtin-animation/core/A2FOExtensions.dll";
        core = LoadLibraryA(path);
        check(core != nullptr && GetModuleHandleA("A2FOExtensions.dll") == core,
              "load fake core provider");
    }
    HMODULE standalone = LoadLibraryA(
        "build/fixtures/builtin-animation/standalone/A2FOAnimations.dll");
    check(standalone != nullptr &&
          GetModuleHandleA("A2FOAnimations.dll") == standalone,
          "load fake standalone provider");

    resolve_animation_controller();
    const HMODULE expected = mode == "core" ? core : standalone;
    FARPROC evaluate = GetProcAddress(expected, "A2FO_AnimationsEvaluate");
    FARPROC controlled = GetProcAddress(expected, "A2FO_AnimationsControlled");
    A2FO_AnimationsEvaluateFn expected_evaluate = nullptr;
    A2FO_AnimationsControlledFn expected_controlled = nullptr;
    static_assert(sizeof(expected_evaluate) == sizeof(evaluate));
    static_assert(sizeof(expected_controlled) == sizeof(controlled));
    std::memcpy(&expected_evaluate, &evaluate, sizeof(evaluate));
    std::memcpy(&expected_controlled, &controlled, sizeof(controlled));
    check(g_controlled_evaluate == expected_evaluate &&
          g_controlled_instance == expected_controlled,
          "both bridge functions belong to selected provider");

    const FixtureInt selected_calls = fixture_export(expected, "FixtureCallCount");
    const FixtureInt other_calls = fixture_export(
        mode == "core" ? standalone : (core ? core : standalone),
        "FixtureCallCount");
    check(selected_calls() == 0, "selected provider starts untouched");
    check(g_controlled_evaluate(nullptr, nullptr, nullptr) == (mode == "core"),
          "evaluation executes selected provider");
    check(g_controlled_instance(nullptr) == (mode == "core"),
          "control query executes selected provider");
    check(selected_calls() == 2, "both selected exports executed");
    if (mode != "fallback")
        check(other_calls() == 0, "unselected provider remains untouched");
    std::puts("builtin animation bridge provider selection: PASS");
}
}  // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "expected one mode");
        run(argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
