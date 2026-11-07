#pragma once

// Adapted from Armada II Refit render3d/shaders/hulls.hlsl at
// 37d60079169b086dba4216fe55e05cd6015d66c0 (MIT licence).
// Copyright (c) 2026 Cedric Haase.
// This prototype uses the Refit travelling hull bands, bend, shimmer, and rim.
// Its wave axis is local Y; a production integration may select the longest
// local bounds axis without changing the renderer's public interface.
namespace a2fo::cloak::shader {
inline constexpr char vertex[] = R"HLSL(
float4 clip_rows[4] : register(c0);
float4 effect : register(c4); // visibility, time, ripple, max extent
float4 bounds : register(c5); // centre.xyz, inverse max extent
float4 light_dir[8] : register(c6);
float4 light_col[8] : register(c14);
float4 ambient : register(c22);
struct VI { float3 pos:POSITION0; float3 normal:NORMAL0; float2 uv:TEXCOORD0;
            float3 tx:TEXCOORD1; float3 ty:TEXCOORD2; float3 tz:TEXCOORD3; };
struct VO { float4 pos:POSITION0; float2 uv:TEXCOORD0;
            float3 local:TEXCOORD1; float2 screen_normal:TEXCOORD2;
            float3 normal:TEXCOORD3; float4 lit:COLOR0; };
VO main(VI i) {
    VO o;
    float3 q = (i.pos - bounds.xyz) * bounds.w;
    float env = 4.0 * effect.x * (1.0 - effect.x);
    float phase = q.y * 8.0 - effect.y * 1.3;
    float4 p = float4(i.pos, 1.0);
    float4 clip = float4(dot(p, clip_rows[0]), dot(p, clip_rows[1]),
                         dot(p, clip_rows[2]), dot(p, clip_rows[3]));
    float2 screen = float2(dot(i.normal, clip_rows[0].xyz),
                           -dot(i.normal, clip_rows[1].xyz));
    // Refit sway changes projected silhouette only; native depth stays intact.
    clip.xy += screen * (sin(phase) * effect.z * effect.w * env);
    o.pos = clip;
    o.uv = i.uv;
    o.local = q;
    o.screen_normal = screen * (0.5 * effect.w / max(abs(clip.w), 1e-3));
    o.normal = normalize(i.normal);
    float3 lit = ambient.rgb;
    [unroll] for (int k = 0; k < 8; ++k) {
        float3 tangent_light = float3(dot(i.tx, light_dir[k].xyz),
                                      dot(i.ty, light_dir[k].xyz),
                                      dot(i.tz, light_dir[k].xyz));
        float inv_len = rsqrt(max(dot(tangent_light, tangent_light), 1e-12));
        lit += light_col[k].rgb * max(tangent_light.z * inv_len, 0.0);
    }
    o.lit = float4(saturate(lit), 1.0);
    return o;
}
)HLSL";
inline constexpr char pixel[] = R"HLSL(
sampler2D hull_tex : register(s0);
sampler2D scene_tex : register(s1);
float4 effect : register(c0); // visibility, time, bend, ripple
float4 style : register(c1); // rim, front, floor, shimmer
float4 view : register(c2); // viewport width, height, inverse width, inverse height
float4 origin : register(c3); // viewport top left, inverse maximum extent, unused
struct PI { float2 uv:TEXCOORD0; float3 local:TEXCOORD1;
            float2 screen_normal:TEXCOORD2; float3 normal:TEXCOORD3;
            float4 lit:COLOR0; };
float4 main(PI i, float2 pixel:VPOS) : COLOR0 {
    float3 q = i.local;
    float vis = saturate(effect.x);
    float env = 4.0 * vis * (1.0 - vis);
    float phase = q.y * 8.0 - effect.y * 1.3;
    float2 wob = float2(sin(q.y * 5.3 + effect.y * 1.1),
                        sin(q.z * 4.7 + effect.y * 0.9 + 1.7));
    float2 ng = i.screen_normal + wob * effect.w * (0.4 + 0.6 * env)
                           * length(i.screen_normal);
    ng += i.screen_normal * cos(phase) * env * effect.w;
    float2 shimmer = float2(sin(q.x * 2.3 + q.y * 1.1 - effect.y * 0.53)
                             + 0.6 * sin(q.z * 3.7 - q.x * 1.9 + effect.y * 0.31 + 2.1),
                             sin(q.y * 2.9 - q.z * 1.3 + effect.y * 0.41 + 0.9)
                             + 0.6 * sin(q.x * 3.3 + q.z * 2.3 - effect.y * 0.67 + 4.0));
    float2 uv = (pixel - origin.xy + 0.5) * view.zw;
    float2 off = ng * effect.z + shimmer * style.w * (1.0 - vis)
                             * length(i.screen_normal);
    float3 glass = tex2D(scene_tex, saturate(uv + off)).rgb;
    float facing = saturate(abs(normalize(i.normal).z));
    glass += style.x * pow(1.0 - facing, 4.0);
    float3 hull = tex2D(hull_tex, i.uv).rgb * i.lit.rgb;
    float band = 0.2 + 0.6 * (0.5 + 0.5 * sin(phase))
               + 0.06 * sin(q.x * 7.1 - q.z * 5.3 + 2.0);
    float edge_pos = vis * 1.5 - 0.25;
    float keep = lerp(style.z, 1.0, smoothstep(band - 0.25, band + 0.25, edge_pos));
    float edge = (1.0 - saturate(abs(edge_pos - band) / 0.04))
               * step(0.001, vis) * step(vis, 0.999);
    return float4(lerp(glass, hull, keep) + edge * style.y, 1.0);
}
)HLSL";
} // namespace a2fo::cloak::shader
