#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo {

struct InlineHook {
    void* target = nullptr;
    void* gateway = nullptr;
    std::size_t length = 0;
};

bool install_inline_hook(void* target, void* replacement, std::size_t length,
                         const std::uint8_t* expected, InlineHook& hook);
bool patch_jump(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length);
bool patch_call(void* target, void* replacement, const std::uint8_t* expected,
                std::size_t length);

}  // namespace a2fo
