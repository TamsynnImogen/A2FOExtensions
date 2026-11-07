#pragma once

namespace a2fo::shadows::shader {
inline constexpr char vertex[] = R"HLSL(
float4 clip_rows[4] : register(c0);
float4 shadow_rows[4] : register(c4);
struct Input { float3 position : POSITION0; };
struct Output { float4 position : POSITION0; float4 shadow : TEXCOORD0; };
Output main(Input input) {
    Output output;
    float4 p = float4(input.position, 1.0);
    output.position = float4(dot(p, clip_rows[0]), dot(p, clip_rows[1]),
                             dot(p, clip_rows[2]), dot(p, clip_rows[3]));
    output.shadow = float4(dot(p, shadow_rows[0]), dot(p, shadow_rows[1]),
                           dot(p, shadow_rows[2]), dot(p, shadow_rows[3]));
    return output;
}
)HLSL";
inline constexpr char depth_pixel[] = R"HLSL(
float4 main(float4 shadow : TEXCOORD0) : COLOR0 {
    float depth = shadow.z / shadow.w;
    return float4(depth, depth, depth, 1.0);
}
)HLSL";
#ifdef A2FO_SHADOW_TEST_PROBE
inline constexpr char overlay_pixel[] = R"HLSL(
float4 main(float4 shadow : TEXCOORD0) : COLOR0 {
    return float4(shadow.xyz / shadow.w, 1.0);
}
)HLSL";
#else
inline constexpr char overlay_pixel[] = R"HLSL(
sampler2D shadow_map : register(s0);
float4 options : register(c0); // strength, bias, inverse resolution, unused
float4 main(float4 shadow : TEXCOORD0) : COLOR0 {
    float3 uvz = shadow.xyz / shadow.w;
    // Compare against the receiver plane at each PCF tap. Comparing every
    // neighbour with the centre depth makes sloped surfaces shadow themselves.
    float3 dx = ddx(uvz), dy = ddy(uvz);
    float determinant = dx.x * dy.y - dx.y * dy.x;
    float2 gradient = float2(0.0, 0.0);
    if (abs(determinant) > 1.0e-10)
        gradient = clamp(float2(dx.z * dy.y - dy.z * dx.y,
                                dy.z * dx.x - dx.z * dy.x) / determinant,
                         -100.0, 100.0);
    float lit = 0.0;
    // D3D9 raster centres are integer pixel positions; texture centres are
    // half a texel in. Avoid sampling the previous texel on an exact boundary.
    float2 lookup_base = uvz.xy + 0.5 * options.z;
    [unroll] for (int y = -1; y <= 1; ++y)
        [unroll] for (int x = -1; x <= 1; ++x) {
            float2 offset = float2(x, y) * options.z;
            float2 uv = lookup_base + offset;
            float depth = tex2D(shadow_map, uv).r;
            // Evaluate the receiver at the sampled texel's raster centre,
            // including its sub-texel displacement from the original point.
            float2 texel = clamp(floor(uv / options.z) * options.z,
                                 0.0, 1.0 - options.z);
            float receiver_depth = uvz.z + dot(gradient, texel - uvz.xy);
            lit += (receiver_depth - options.y <= depth) ? 1.0 : 0.0;
        }
    float alpha = options.x * (1.0 - lit / 9.0);
    return float4(0.0, 0.0, 0.0, alpha);
}
)HLSL";
#endif
} // namespace a2fo::shadows::shader
