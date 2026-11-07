#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace a2fo::renderer::shaders {
struct Vs11Translation {
    std::string hlsl;
    std::uint16_t input_mask = 0;
    // oPos, oD0, oD1, oT0..oT7; final slot reserved. Only written channels
    // are defined legacy outputs. A future pixel facade must check these.
    std::array<std::uint8_t,12> output_masks{};
    std::size_t instruction_count = 0;
};
// Compile the downstream pixel entry using this same complete interface.
// D3D11 stage signatures must agree on registers, including preceding fields
// a particular pixel shader does not use.
std::string vs11_output_interface();
// Original, deliberately bounded translator for the ten opcode families in
// captured FO programs, accepting VS1.0 and VS1.1 versions. This
// is not a complete shader-model 1 translator. Refuses
// unsupported instructions/addressing/modifiers and undefined temporary reads.
// Constants: b0 = float4[96]; b1 = float2 inverse viewport dimensions + padding.
// IA inputs: TEXCOORD0..15, four components with native declaration defaults.
// Shader entry VSMain; output VSOutput carries position/diffuse/specular/tex0..7.
// No COM/state changes or GPU work. On failure output is cleared.
bool translate_vs11(const std::uint32_t*,std::size_t,Vs11Translation&,std::string& error);
}
