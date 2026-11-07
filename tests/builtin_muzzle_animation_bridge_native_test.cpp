// Console-only check of the built-in MuzzleFlashes animation notification.
#include "../core/builtin_muzzle_flashes.cpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace a2fo {
bool builtin_module_enabled(BuiltinFeature) noexcept { return true; }
}

namespace {
void check(bool okay, const char* message) {
    if (!okay) throw std::runtime_error(message);
}

using FixtureInt = int (__cdecl*)() noexcept;
FixtureInt call_count(HMODULE module) {
    FARPROC address = GetProcAddress(module, "FixtureCallCount");
    FixtureInt result = nullptr;
    static_assert(sizeof(result) == sizeof(address));
    std::memcpy(&result, &address, sizeof(result));
    check(result != nullptr, "fixture count export");
    return result;
}

void run(const std::string& mode) {
    check(mode == "core" || mode == "fallback",
          "usage: builtin_muzzle_animation_bridge_native_test.exe core|fallback");
    HMODULE core = mode == "core"
        ? LoadLibraryA("build/fixtures/builtin-animation/core/A2FOExtensions.dll")
        : nullptr;
    if (mode == "core") check(core != nullptr, "load fake core provider");
    HMODULE standalone = LoadLibraryA(
        "build/fixtures/builtin-animation/standalone/A2FOAnimations.dll");
    check(standalone != nullptr, "load fake standalone provider");
    notify_animation(nullptr);
    HMODULE selected = mode == "core" ? core : standalone;
    FARPROC expected = GetProcAddress(selected, "A2FO_AnimationsWeaponFired");
    A2FO_AnimationsWeaponFiredFn expected_fired = nullptr;
    static_assert(sizeof(expected_fired) == sizeof(expected));
    std::memcpy(&expected_fired, &expected, sizeof(expected));
    check(g_animation_fired == expected_fired, "notification uses selected provider");
    check(call_count(selected)() == 1, "selected notification executed");
    if (core) check(call_count(standalone)() == 0,
                    "standalone notification untouched when core present");
    std::puts("builtin muzzle animation bridge provider selection: PASS");
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
