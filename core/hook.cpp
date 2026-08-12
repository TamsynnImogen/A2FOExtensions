/*
 * File: core/hook.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Low-level patch primitives for inline hooks, patched jumps/calls, and opcode-safe byte replacement.
 */

#include "hook.hpp"

#include <windows.h>

#include <cstring>
#include <limits>

namespace a2fo {
namespace {

// Encode a five-byte x86 near CALL/JMP and pad any remaining whole
// instructions with NOPs. Callers must validate that `length` ends on an
// instruction boundary before using this helper.
bool write_relative_branch(void* source, void* destination, std::size_t length,
                           std::uint8_t opcode) {
    if (length < 5) {
        return false;
    }
    const auto from = reinterpret_cast<std::intptr_t>(source);
    const auto to = reinterpret_cast<std::intptr_t>(destination);
    const auto displacement = to - (from + 5);
    if (displacement < std::numeric_limits<std::int32_t>::min() ||
        displacement > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(source, length, PAGE_EXECUTE_READWRITE, &old_protection)) {
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(source);
    bytes[0] = opcode;
    const auto relative = static_cast<std::int32_t>(displacement);
    std::memcpy(bytes + 1, &relative, sizeof(relative));
    for (std::size_t index = 5; index < length; ++index) {
        bytes[index] = 0x90;
    }
    FlushInstructionCache(GetCurrentProcess(), source, length);
    DWORD ignored = 0;
    VirtualProtect(source, length, old_protection, &ignored);
    return true;
}

bool write_relative_jump(void* source, void* destination, std::size_t length) {
    return write_relative_branch(source, destination, length, 0xe9);
}

}  // namespace


bool patch_jump(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length) {
    if (!target || !replacement || !expected ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    return write_relative_jump(target, replacement, length);
}

bool patch_call(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length) {
    if (!target || !replacement || !expected || length < 5 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }
    return write_relative_branch(target, replacement, length, 0xe8);
}

bool patch_bytes(void* target, const std::uint8_t* replacement,
                 const std::uint8_t* expected, std::size_t length) {
    if (!target || !replacement || !expected || length == 0 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }

    DWORD old_protection = 0;
    if (!VirtualProtect(target, length, PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        return false;
    }
    std::memcpy(target, replacement, length);
    FlushInstructionCache(GetCurrentProcess(), target, length);
    DWORD ignored = 0;
    VirtualProtect(target, length, old_protection, &ignored);
    return true;
}

bool install_inline_hook(void* target, void* replacement, std::size_t length,
                         const std::uint8_t* expected, InlineHook& hook) {
    if (!target || !replacement || !expected || length < 5 ||
        std::memcmp(target, expected, length) != 0) {
        return false;
    }

    // The gateway contains the displaced prologue followed by a jump back to
    // target+length. Current hook sites were selected so these copied bytes do
    // not contain relative branches or instruction-pointer-relative operands.
    auto* gateway = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, length + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!gateway) {
        return false;
    }
    std::memcpy(gateway, target, length);
    if (!write_relative_jump(gateway + length,
                             static_cast<std::uint8_t*>(target) + length, 5) ||
        !write_relative_jump(target, replacement, length)) {
        VirtualFree(gateway, 0, MEM_RELEASE);
        return false;
    }

    hook.target = target;
    hook.gateway = gateway;
    hook.length = length;
    return true;
}

}  // namespace a2fo
