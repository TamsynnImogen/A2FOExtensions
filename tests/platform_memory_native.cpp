// Console-only Win32 memory contract fixture. Owns every address it touches.
#include "../rust/native_hooks_ffi.h"
#include "../sdk/include/a2fo_module_api.h"
#include <type_traits>
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void require(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", label);
        std::exit(1);
    }
}

DWORD protection(const void* p) {
    MEMORY_BASIC_INFORMATION info{};
    require(VirtualQuery(p, &info, sizeof(info)) == sizeof(info), "VirtualQuery");
    return info.Protect;
}

std::int32_t displacement(const std::uint8_t* p) {
    std::int32_t value = 0;
    std::memcpy(&value, p + 1, sizeof(value));
    return value;
}

void check_branch(const std::uint8_t* site, const void* destination,
                  std::uint8_t opcode, std::size_t length) {
    require(site[0] == opcode, "branch opcode");
    const auto actual = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(site) + 5 + displacement(site));
    const auto wanted = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(destination));
    require(actual == wanted, "branch rel32 destination");
    for (std::size_t i = 5; i < length; ++i)
        require(site[i] == 0x90, "branch NOP fill");
}
} // namespace

int main() {
    static_assert(std::is_same_v<decltype(A2FO_ModuleApi::patch_jump),decltype(&a2fo_rs_windows_patch_jump)>);
    static_assert(std::is_same_v<decltype(A2FO_ModuleApi::patch_call),decltype(&a2fo_rs_windows_patch_call)>);
    static_assert(std::is_same_v<decltype(A2FO_ModuleApi::patch_bytes),decltype(&a2fo_rs_windows_patch_bytes)>);
    static_assert(sizeof(A2foRsInlineHook)==12 && offsetof(A2foRsInlineHook,gateway)==4 && offsetof(A2foRsInlineHook,length)==8);
    static_assert(sizeof(void*) == 4, "fixture exercises x86 rel32 hooks");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    auto* page = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    require(page != nullptr, "VirtualAlloc page");

    // Checked byte replacement restores the original executable protection.
    constexpr std::uint8_t original[] = {0xB8, 37, 0, 0, 0, 0xC3};
    constexpr std::uint8_t replacement[] = {0xB8, 99, 0, 0, 0, 0xC3};
    std::memcpy(page, original, sizeof(original));
    DWORD old = 0;
    require(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old), "protect executable");
    require(a2fo_rs_windows_patch_bytes(page, replacement, original, sizeof(original)),
            "patch_bytes success");
    require(std::memcmp(page, replacement, sizeof(replacement)) == 0,
            "patch_bytes exact write");
    require(protection(page) == PAGE_EXECUTE_READ, "patch_bytes protection restored");
    require(!a2fo_rs_windows_patch_bytes(page, original, original, sizeof(original)),
            "patch_bytes mismatch rejected");
    require(!a2fo_rs_windows_patch_bytes(page, nullptr, replacement, sizeof(replacement)),
            "patch_bytes null replacement rejected");
    require(!a2fo_rs_windows_patch_bytes(page, original, replacement, 0),
            "patch_bytes zero length rejected");
    require(std::memcmp(page, replacement, sizeof(replacement)) == 0,
            "invalid patch_bytes left target untouched");
    require(a2fo_rs_windows_patch_bytes(page, original, replacement, sizeof(original)),
            "patch_bytes restore");
    require(protection(page) == PAGE_EXECUTE_READ, "restored page protection");
    std::puts("bytes:exact,checked,restored,execute_read");

    // Whole synthetic instruction spans; destination is inside the same page.
    constexpr std::uint8_t branch_original[] = {0x90, 0x90, 0x90, 0x90,
                                                 0x90, 0x90, 0x90, 0x90};
    constexpr std::uint8_t call_original[] = {0x90, 0x90, 0x90, 0x90,
                                               0x90, 0x90, 0x90};
    auto* jump = page + 32;
    auto* call = page + 48;
    auto* destination = page + 80;
    require(VirtualProtect(page, 4096, PAGE_READWRITE, &old), "prepare branch sites");
    std::memcpy(jump, branch_original, sizeof(branch_original));
    std::memcpy(call, call_original, sizeof(call_original));
    require(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old), "seal branch sites");
    require(a2fo_rs_windows_patch_jump(jump, destination, branch_original,
                             sizeof(branch_original)), "patch_jump success");
    check_branch(jump, destination, 0xE9, sizeof(branch_original));
    require(a2fo_rs_windows_patch_call(call, destination, call_original,
                             sizeof(call_original)), "patch_call success");
    check_branch(call, destination, 0xE8, sizeof(call_original));
    require(protection(page) == PAGE_EXECUTE_READ, "branch protection restored");
    std::uint8_t jump_written[sizeof(branch_original)]{};
    std::uint8_t call_written[sizeof(call_original)]{};
    std::memcpy(jump_written, jump, sizeof(jump_written));
    std::memcpy(call_written, call, sizeof(call_written));
    require(!a2fo_rs_windows_patch_jump(jump, destination, branch_original,
                              sizeof(branch_original)), "jump mismatch rejected");
    require(!a2fo_rs_windows_patch_call(call, destination, call_written, 4),
            "short call rejected");
    require(!a2fo_rs_windows_patch_jump(jump, nullptr, jump_written,
                              sizeof(jump_written)), "null jump destination rejected");
    require(std::memcmp(jump, jump_written, sizeof(jump_written)) == 0 &&
            std::memcmp(call, call_written, sizeof(call_written)) == 0,
            "invalid branches left targets untouched");
    std::puts("branches:jump_e9,call_e8,rel32,nop_fill,checked");

    // The displaced MOV EAX,37 is five bytes; target+5 is its RET.
    using Function = int (__cdecl*)();
    require(reinterpret_cast<Function>(page)() == 37, "original function result");
    auto* hook_replacement = page + 96;
    require(VirtualProtect(page, 4096, PAGE_READWRITE, &old), "prepare replacement");
    std::memcpy(hook_replacement, replacement, sizeof(replacement));
    require(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old), "seal replacement");
    A2foRsInlineHook untouched{page + 1, page + 2, 123};
    constexpr std::uint8_t wrong[] = {0xB8, 38, 0, 0, 0};
    require(!a2fo_rs_windows_hook_install(page, hook_replacement, 5, wrong, &(untouched)),
            "hook mismatch rejected");
    require(!a2fo_rs_windows_hook_install(page, hook_replacement, 4, original, &(untouched)),
            "short hook rejected");
    require(!a2fo_rs_windows_hook_install(page, nullptr, 5, original, &(untouched)),
            "null hook replacement rejected");
    require(untouched.target == page + 1 && untouched.gateway == page + 2 &&
            untouched.length == 123 && std::memcmp(page, original, 5) == 0,
            "invalid hook left target/output untouched");
    A2foRsInlineHook hook{};
    require(a2fo_rs_windows_hook_install(page, hook_replacement, 5, original, &(hook)),
            "install_inline_hook success");
    require(hook.target == page && hook.gateway != nullptr && hook.length == 5,
            "hook output fields");
    check_branch(page, hook_replacement, 0xE9, 5);
    require(reinterpret_cast<Function>(page)() == 99, "hooked function result");
    require(reinterpret_cast<Function>(hook.gateway)() == 37, "gateway function result");
    std::uint8_t hook_written[5]{};
    std::memcpy(hook_written, page, 5);
    require(a2fo_rs_windows_patch_bytes(page, original, hook_written, 5), "restore hooked target");
    require(reinterpret_cast<Function>(page)() == 37, "restored function result");
    require(VirtualFree(hook.gateway, 0, MEM_RELEASE), "free inactive gateway");
    std::puts("hook:original=37,replacement=99,gateway=37,restored=37");

    // A read-only mapping cannot be upgraded to executable writable memory.
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READONLY, 0, 4096, nullptr);
    require(mapping != nullptr, "CreateFileMappingA read-only");
    auto* readonly = static_cast<std::uint8_t*>(MapViewOfFile(
        mapping, FILE_MAP_READ, 0, 0, 4096));
    require(readonly != nullptr, "MapViewOfFile read-only");
    require(protection(readonly) == PAGE_READONLY, "mapping initial protection");
    constexpr std::uint8_t zeros[] = {0, 0, 0, 0, 0};
    constexpr std::uint8_t ones[] = {1, 1, 1, 1, 1};
    A2foRsInlineHook mapping_out{page + 1, page + 2, 123};
    SetLastError(0);
    const bool upgraded = a2fo_rs_windows_patch_bytes(readonly, ones, zeros, sizeof(zeros));
    const DWORD failure_error = GetLastError();
    if (upgraded) {
        std::fprintf(stderr, "FAIL unexpected read-only mapping upgrade\n");
        return 2;
    }
    require(failure_error != 0, "mapping failure last-error");
    SetLastError(0);
    const bool hooked_mapping = a2fo_rs_windows_hook_install(
        readonly, hook_replacement, sizeof(zeros), zeros, &(mapping_out));
    const DWORD hook_failure_error = GetLastError();
    if (hooked_mapping) {
        std::fprintf(stderr, "FAIL unexpected read-only mapping hook\n");
        return 2;
    }
    require(hook_failure_error != 0, "mapping hook failure last-error");
    require(std::memcmp(readonly, zeros, sizeof(zeros)) == 0,
            "mapping bytes unchanged");
    require(mapping_out.target == page + 1 && mapping_out.gateway == page + 2 &&
            mapping_out.length == 123, "mapping output sentinel unchanged");
    require(protection(readonly) == PAGE_READONLY, "mapping protection unchanged");
    std::printf("mapping:rejected,patch_error=%lu,hook_error=%lu,bytes=0,out=sentinel\n",
                static_cast<unsigned long>(failure_error),
                static_cast<unsigned long>(hook_failure_error));
    require(UnmapViewOfFile(readonly), "UnmapViewOfFile");
    require(CloseHandle(mapping), "CloseHandle mapping");
    require(VirtualFree(page, 0, MEM_RELEASE), "VirtualFree page");
    std::puts("PASS platform_memory_native");
}
