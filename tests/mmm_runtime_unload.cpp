// Exercise the real DLL CRT finalizer path with the fake mission framework.
#include <windows.h>
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    HMODULE module = LoadLibraryA(argv[1]);
    if (!module) return 3;
    auto address = GetProcAddress(module, "MmmFixtureRunScenario");
    int (__cdecl* run)(const char*) = nullptr;
    static_assert(sizeof(run) == sizeof(address));
    std::memcpy(&run, &address, sizeof(run));
    if (!run || run(argv[2]) != 0) return 4;
    if (!FreeLibrary(module)) return 5;
    std::puts("UNLOADED");
    return 0;
}
