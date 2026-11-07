/*
 * File: core/hook.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Low-level patch primitives for inline hooks, patched jumps/calls, and opcode-safe byte replacement.
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo {

// Description of one installed prologue hook. `gateway` executes the displaced
// bytes and returns to target+length, allowing the replacement to call through
// to the original implementation.
struct InlineHook {
    void* target = nullptr;
    void* gateway = nullptr;
    std::size_t length = 0;
};

// Installs a checked prologue hook. `expected` must contain exactly `length`
// bytes from the supported binary and the copied instructions must be safely
// relocatable as a block.
bool install_inline_hook(void* target, void* replacement, std::size_t length,
                         const std::uint8_t* expected, InlineHook& hook);

// Checked in-place branch patches for existing whole-instruction sites.
bool patch_jump(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length);
bool patch_call(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length);

// Replaces a checked, fixed-size byte range without imposing branch opcode or
// instruction-length requirements. Suitable for data and vtable entries.
bool patch_bytes(void* target, const std::uint8_t* replacement,
                 const std::uint8_t* expected, std::size_t length);

}  // namespace a2fo
