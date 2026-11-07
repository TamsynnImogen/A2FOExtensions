#pragma once

#include <cstddef>
#include <cstdint>

namespace a2fo::renderer::shaders {

enum class TokenParseStatus {
    complete,
    truncated,
    unsupported,
    invalid,
};

struct TokenParseResult {
    TokenParseStatus status;
    // Nonzero only on complete; includes the terminating DWORD.
    std::size_t consumed_words;
};

// Read only inside [words, words + word_count). Trailing words after END are
// ignored. These routines find stream boundaries; they do not validate shader
// semantics or whether a device accepts the stream.
TokenParseResult parse_d3d8_vertex_declaration(const std::uint32_t* words,
                                                std::size_t word_count) noexcept;
TokenParseResult parse_d3d8_shader_bytecode(const std::uint32_t* words,
                                             std::size_t word_count) noexcept;

} // namespace a2fo::renderer::shaders
