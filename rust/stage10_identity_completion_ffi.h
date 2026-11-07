#ifndef A2FO_STAGE10_IDENTITY_COMPLETION_FFI_H
#define A2FO_STAGE10_IDENTITY_COMPLETION_FFI_H

#include <cstddef>
#include <cstdint>

extern "C" {

using A2foIdentityCompletionStep = std::uint32_t (*)(
    void* context, std::uint32_t operation, std::size_t index);
using A2foIdentityIsSpace = bool (*)(std::uint8_t value);

struct A2foIdentityTrimSpan {
    std::size_t begin;
    std::size_t end;
};

bool a2fo_rs_stage10_identity_first_match(
    std::size_t count, A2foIdentityCompletionStep step, void* context);
bool a2fo_rs_stage10_identity_trim(
    const std::uint8_t* bytes, std::size_t length,
    A2foIdentityIsSpace is_space, A2foIdentityTrimSpan* span);
bool a2fo_rs_stage10_identity_display_tail(
    const std::uint8_t* bytes, std::size_t length, long parsed,
    A2foIdentityIsSpace is_space);
bool a2fo_rs_stage10_identity_any(
    std::size_t count, A2foIdentityCompletionStep step, void* context);
bool a2fo_rs_stage10_identity_each_visible(
    std::size_t count, A2foIdentityCompletionStep step, void* context);

}

#endif
