#pragma once

namespace a2fo::renderer::shaders {

// The VS source is prefixed with HAS_DIFFUSE, HAS_SPECULAR and HAS_TEXCOORD
// definitions for each supported transformed FVF layout.
constexpr const char* ui_vertex = R"hlsl(
cbuffer DrawConstants : register(b0) {
    float4 viewport;
    uint4 color_args;
    uint4 alpha_args;
    uint4 control;
    float4 blend_factor;
};
struct Input {
    float4 position : POSITION;
#if HAS_DIFFUSE
    float4 diffuse : COLOR0;
#endif
#if HAS_SPECULAR
    float4 specular : COLOR1;
#endif
#if HAS_TEXCOORD
    float2 uv : TEXCOORD0;
#endif
};
struct Output {
    float4 position : SV_Position;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 uv : TEXCOORD0;
};
Output VSMain(Input input) {
    Output output;
    float w = 1.0 / input.position.w;
    float2 ndc = float2(
        2.0 * (input.position.x + 0.5 - viewport.x) / viewport.z - 1.0,
        1.0 - 2.0 * (input.position.y + 0.5 - viewport.y) / viewport.w);
    output.position = float4(ndc * w, input.position.z * w, w);
#if HAS_DIFFUSE
    output.diffuse = input.diffuse.bgra;
#else
    output.diffuse = float4(1, 1, 1, 1);
#endif
#if HAS_SPECULAR
    output.specular = input.specular.bgra;
#else
    output.specular = float4(0, 0, 0, 0);
#endif
#if HAS_TEXCOORD
    output.uv = input.uv;
#else
    output.uv = float2(0, 0);
#endif
    return output;
}
)hlsl";

constexpr const char* ui_pixel = R"hlsl(
cbuffer DrawConstants : register(b0) {
    float4 viewport;
    uint4 color_args;
    uint4 alpha_args;
    uint4 control;
    float4 blend_factor;
};
Texture2D stage0 : register(t0);
SamplerState sample0 : register(s0);
struct Input {
    float4 position : SV_Position;
    float4 diffuse : COLOR0;
    float4 specular : COLOR1;
    float2 uv : TEXCOORD0;
};
float4 argument(uint code, float4 diffuse, float4 texel) {
    float4 value = (code & 15u) == 2u ? texel : diffuse;
    if ((code & 16u) != 0u) value = 1.0 - value;
    if ((code & 32u) != 0u) value = value.aaaa;
    return value;
}
float4 combine(uint op, float4 a, float4 b) {
    if (op == 2u) return a;
    if (op == 3u) return b;
    if (op == 4u) return a * b;
    return a + b;
}
float4 PSMain(Input input) : SV_Target {
    float4 texel = control.x != 0u ? stage0.Sample(sample0, input.uv) : float4(1, 1, 1, 1);
    float4 a = argument(color_args.y, input.diffuse, texel);
    float4 b = argument(color_args.z, input.diffuse, texel);
    float4 result = color_args.x == 1u ? input.diffuse : combine(color_args.x, a, b);
    if (color_args.w != 0u) result.rgb = saturate(result.rgb + input.specular.rgb);
    if (alpha_args.x != 1u) {
        float4 aa = argument(alpha_args.y, input.diffuse, texel);
        float4 ab = argument(alpha_args.z, input.diffuse, texel);
        result.a = combine(alpha_args.x, aa, ab).a;
    }
    if (control.y != 0u) {
        uint value = (uint)floor(saturate(result.a) * 255.0 + 0.5);
        uint reference = control.z;
        uint function = control.w;
        bool passesAlpha = function == 8u ||
            (function == 2u && value < reference) ||
            (function == 3u && value == reference) ||
            (function == 4u && value <= reference) ||
            (function == 5u && value > reference) ||
            (function == 6u && value != reference) ||
            (function == 7u && value >= reference);
        clip(passesAlpha ? 1.0 : -1.0);
    }
    return result;
}
)hlsl";

} // namespace a2fo::renderer::shaders
