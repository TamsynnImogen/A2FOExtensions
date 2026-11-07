// Headless fake-host load of the actual Rust-owned DLL. No game image or hooks.
#include "../rust/runtime_globals_ffi.h"

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
std::string executable_directory() {
    char path[MAX_PATH]{};
    const DWORD size = GetModuleFileNameA(nullptr, path, MAX_PATH);
    require(size != 0 && size < MAX_PATH, "host executable path");
    std::string result(path, size);
    const auto slash = result.find_last_of("\\/");
    require(slash != std::string::npos, "host executable directory");
    return result.substr(0, slash);
}
std::uintptr_t parse_rva(const char* text) {
    char* end = nullptr;
    const unsigned long number = std::strtoul(text, &end, 16);
    require(text != end && *end == 0 && number != 0, "private function RVA");
    return static_cast<std::uintptr_t>(number);
}
std::string read_log(const std::string& directory) {
    std::ifstream file(directory + "\\A2FOExtensions.log", std::ios::binary);
    require(static_cast<bool>(file), "DLL startup log");
    return {std::istreambuf_iterator<char>{file}, {}};
}
void run(const char* dll, const char* rva_text) {
    const auto directory = executable_directory();
    HMODULE module = LoadLibraryA(dll);
    require(module != nullptr, "LoadLibraryA Rust-owned DLL");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    require(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0,
            "loaded DLL DOS header");
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    require(nt->Signature == IMAGE_NT_SIGNATURE &&
            nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC,
            "loaded DLL PE32 header");
    const std::uintptr_t rva = parse_rva(rva_text);
    require(rva < nt->OptionalHeader.SizeOfImage &&
            rva + 16 < nt->OptionalHeader.SizeOfImage, "private function RVA bounds");
    require(GetProcAddress(module, "a2fo_rs_runtime_globals") == nullptr,
            "private runtime globals must remain outside export table");
    using Globals = A2foRuntimeGlobals* (__cdecl*)();
    const auto globals = reinterpret_cast<Globals>(
        reinterpret_cast<std::uintptr_t>(module) + rva);
    A2foRuntimeGlobals* state = globals();
    require(state != nullptr && (reinterpret_cast<std::uintptr_t>(state) & 3u) == 0,
            "private runtime globals pointer");
    const DWORD started = GetTickCount();
    while (InterlockedCompareExchange(&state->deferred_finished, 0, 0) != 1) {
        require(GetTickCount() - started < 15000, "deferred worker timeout");
        Sleep(10);
    }
    require(InterlockedCompareExchange(&state->initialize_state, 0, 0) == 0 &&
            state->initialize_thread_id == 0 && state->log != INVALID_HANDLE_VALUE,
            "worker finished with failed fake-host init and live log");
    const std::string log = read_log(directory);
    const auto attach = log.find("A2FOExtensions initialization started");
    const auto mismatch = log.find("ArmadaL.exe version mismatch");
    const auto deferred = log.find("A2FOExtensions deferred initialization started");
    const auto no_hooks = log.find("No hooks installed");
    require(attach != std::string::npos && mismatch != std::string::npos &&
            deferred != std::string::npos && no_hooks != std::string::npos &&
            attach < mismatch && mismatch < deferred && deferred < no_hooks,
            "Rust DLL startup and fake-host failure log order");
    using Initialize = bool (__cdecl*)();
    const FARPROC exported_initialize = GetProcAddress(module, "A2FO_Initialize");
    static_assert(sizeof(Initialize) == sizeof(exported_initialize),
                  "Win32 exported function pointer width");
    Initialize initialize = nullptr;
    std::memcpy(&initialize, &exported_initialize, sizeof(initialize));
    require(initialize != nullptr && !initialize(),
            "A2FO_Initialize remains false after worker finishes");
    require(InterlockedCompareExchange(&state->initialize_state, 0, 0) == 0 &&
            state->initialize_thread_id == 0,
            "repeat failed initialization leaves lifecycle idle");
    std::puts("rust-owned-dll:attach-worker-log-order:PASS");
    std::puts("rust-owned-dll:private-globals-and-retry:PASS");
    // The original DLL lifetime is process scoped. Leave the loaded image in place.
}
} // namespace

int main(int argc, char** argv) {
    try {
        require(argc == 3, "usage: rust_owned_dll_load.exe DLL PRIVATE_GLOBALS_RVA_HEX");
        run(argv[1], argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "rust-owned-dll-load: %s\n", error.what());
        return 1;
    }
}
