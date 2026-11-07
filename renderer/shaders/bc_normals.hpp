#pragma once
namespace a2fo::renderer::shaders {
// BC5 stores X/Y only. Preserve the existing tangent-space handedness: +Z
// hemisphere, no green flip. Only normal-map material consumers should call
// this; raw two-channel data must remain raw. No lighting style is introduced.
constexpr const char* bc5_normal_functions = R"hlsl(
float3 a2fo_reconstruct_bc5(float2 xy) {
    float z = sqrt(saturate(1.0 - dot(xy,xy)));
    return normalize(float3(xy,z));
}
float3 a2fo_decode_bc5_unorm(float2 encoded) {
    return a2fo_reconstruct_bc5(encoded * 2.0 - 1.0);
}
float3 a2fo_decode_bc5_snorm(float2 encoded) {
    return a2fo_reconstruct_bc5(encoded);
}
)hlsl";
}
