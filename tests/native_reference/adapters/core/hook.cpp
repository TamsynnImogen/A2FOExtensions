/* C++ API facade; Rust owns Win32 memory operations and checked patches. */
#include "hook.hpp"
#include "../rust/native_hooks_ffi.h"
namespace a2fo {
bool patch_jump(void *target, void *replacement, const std::uint8_t *expected, std::size_t length) {
    return a2fo_rs_windows_hook_patch(target, replacement, expected, length, 0) != 0;
}
bool patch_call(void *target, void *replacement, const std::uint8_t *expected, std::size_t length) {
    return a2fo_rs_windows_hook_patch(target, replacement, expected, length, 1) != 0;
}
bool patch_bytes(void *target, const std::uint8_t *replacement, const std::uint8_t *expected,
                 std::size_t length) {
    return a2fo_rs_windows_hook_patch(target, replacement, expected, length, 2) != 0;
}
bool install_inline_hook(void *target, void *replacement, std::size_t length,
                         const std::uint8_t *expected, InlineHook &hook) {
    A2foRsInlineHook parsed{};
    if (!a2fo_rs_windows_hook_install(target, replacement, length, expected, &parsed))
        return false;
    hook.target = parsed.target;
    hook.gateway = parsed.gateway;
    hook.length = parsed.length;
    return true;
}
} // namespace a2fo
