#pragma once
#include <cstdint>

// Optional diagnostic export, not an extension of the core SDK ABI. Pass
// sizeof(RefitStatus); no callbacks, input events or game actions occur.
namespace a2fo::refit {
struct RefitStatus {
    std::uint32_t struct_size;
    std::uint32_t version;
    std::uint32_t installed;
    // Armed is not proof that a native parser visit/effect has occurred.
    std::uint32_t armed;
    std::uint32_t parser_visits;
    float configured_value;
};
}
