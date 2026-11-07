#include "d3d8_tokens.hpp"
#include "../../rust/lead_ffi.h"
namespace a2fo::renderer::shaders {
namespace {
TokenParseResult parse(const std::uint32_t *words, std::size_t count, std::uint8_t kind) noexcept {
    std::uint32_t status = 3;
    std::size_t used = 0;
    a2fo_rs_lead_tokens(words, count, kind, &status, &used);
    return {static_cast<TokenParseStatus>(status), used};
}
} // namespace
TokenParseResult parse_d3d8_vertex_declaration(const std::uint32_t *w, std::size_t n) noexcept {
    return parse(w, n, 0);
}
TokenParseResult parse_d3d8_shader_bytecode(const std::uint32_t *w, std::size_t n) noexcept {
    return parse(w, n, 1);
}
} // namespace a2fo::renderer::shaders
