#include "../renderer/shaders/d3d8_tokens.hpp"
#include "../core/amd_dot3_compat.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using a2fo::renderer::shaders::TokenParseStatus;
using a2fo::renderer::shaders::parse_d3d8_shader_bytecode;
using a2fo::renderer::shaders::parse_d3d8_vertex_declaration;

namespace {

void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

template<std::size_t N>
void declaration(const std::uint32_t (&words)[N], TokenParseStatus status,
                 std::size_t consumed = 0) {
    const auto result = parse_d3d8_vertex_declaration(words, N);
    check(result.status == status && result.consumed_words == consumed, "declaration result");
}

template<std::size_t N>
void shader(const std::uint32_t (&words)[N], TokenParseStatus status,
            std::size_t consumed = 0) {
    const auto result = parse_d3d8_shader_bytecode(words, N);
    check(result.status == status && result.consumed_words == consumed, "shader result");
}

} // namespace

int main() {
    const auto& stock = a2fo::amd_dot3::kStockDeclaration;
    const auto stock_result = parse_d3d8_vertex_declaration(stock.data(), stock.size());
    check(stock_result.status == TokenParseStatus::complete &&
          stock_result.consumed_words == stock.size(), "real stock declaration");
    const std::uint32_t decl_payload[] = {
        0x88000000u, // CONSTMEM: 4 vectors, 16 raw DWORDs
        0xffffffffu, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0xa2000000u, // EXT: two raw DWORDs
        0xffffffffu, 0,
        0xffffffffu, 0x40020000u,
    };
    declaration(decl_payload, TokenParseStatus::complete, 21);
    for (std::size_t count = 0; count < 21; ++count)
        check(parse_d3d8_vertex_declaration(decl_payload, count).status ==
              TokenParseStatus::truncated, "every declaration prefix incomplete");
    check(parse_d3d8_vertex_declaration(decl_payload, 16).status ==
          TokenParseStatus::truncated, "truncated CONSTMEM");
    check(parse_d3d8_vertex_declaration(decl_payload, 18).status ==
          TokenParseStatus::truncated, "truncated EXT");
    const std::uint32_t reserved_decl[] = {0xc0000000u};
    declaration(reserved_decl, TokenParseStatus::unsupported);
    const std::uint32_t malformed_end[] = {0xe0000000u};
    declaration(malformed_end, TokenParseStatus::invalid);

    const std::uint32_t vertex[] = {
        0xfffe0101u,        // vs_1_1
        0x00000051u,        // def c0, four immediate DWORDs
        0xa0000000u, 0x0000ffffu, 0, 0, 0,
        0x00000001u,        // mov oPos, v0
        0xc0000000u, 0x90e40000u,
        0x0000ffffu, 0,
    };
    shader(vertex, TokenParseStatus::complete, 11);
    for (std::size_t count = 0; count < 11; ++count)
        check(parse_d3d8_shader_bytecode(vertex, count).status ==
              TokenParseStatus::truncated, "every vertex shader prefix incomplete");
    check(parse_d3d8_shader_bytecode(vertex, 6).status ==
          TokenParseStatus::truncated, "truncated DEF immediate");
    check(parse_d3d8_shader_bytecode(vertex, 10).status ==
          TokenParseStatus::truncated, "missing shader END");

    const std::uint32_t pixel_13[] = {
        0xffff0103u, 0x00000042u, 0xb0000000u, // tex t0 (one operand)
        0x00000002u, 0x80000000u, 0x80000000u, 0x80000000u,
        0x0000ffffu,
    };
    shader(pixel_13, TokenParseStatus::complete, 8);
    const std::uint32_t pixel_14[] = {
        0xffff0104u, 0x00000042u, 0x80000000u, 0xb0000000u, // texld dst, src
        0x0000fffdu,                              // phase
        0x00000057u, 0x80000005u,                // texdepth dst
        0x0000ffffu,
    };
    shader(pixel_14, TokenParseStatus::complete, 8);
    const std::uint32_t bem[] = {
        0xffff0104u, 0x00000059u, 0x80000000u, 0x80000001u,
        0x80000002u, 0x0000ffffu,
    };
    shader(bem, TokenParseStatus::complete, 6);
    const std::uint32_t undocumented_diff[] = {0xffff0101u, 75u, 0x0000ffffu};
    shader(undocumented_diff, TokenParseStatus::unsupported);
    const std::uint32_t comment[] = {
        0xffff0101u, 0x0002fffeu, 0x0000ffffu, 0xffffffffu,
        0x0000ffffu,
    };
    shader(comment, TokenParseStatus::complete, 5);
    check(parse_d3d8_shader_bytecode(comment, 3).status ==
          TokenParseStatus::truncated, "truncated comment payload");
    const std::uint32_t unsupported_op[] = {0xfffe0101u, 0x00001234u, 0x0000ffffu};
    shader(unsupported_op, TokenParseStatus::unsupported);
    const std::uint32_t unsupported_version[] = {0xfffe0200u, 0x0000ffffu};
    shader(unsupported_version, TokenParseStatus::unsupported);
    const std::uint32_t wrong_end[] = {0xfffe0101u, 0x8000ffffu};
    shader(wrong_end, TokenParseStatus::invalid);
    check(parse_d3d8_shader_bytecode(nullptr, 0).status == TokenParseStatus::invalid,
          "null stream rejected");
    check(parse_d3d8_shader_bytecode(vertex,
          std::numeric_limits<std::size_t>::max()).status == TokenParseStatus::invalid,
          "overflowing span rejected");
    std::puts("PASS: bounded D3D8 declaration and shader token boundaries");
}
