#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Synchronous CPU-only SOD adapter. Callback storage expires on return; copy
// what is needed. No allocator, engine object, device or input ownership crosses.
#ifndef A2FO_N11_VERTEX_DEFINED
#define A2FO_N11_VERTEX_DEFINED
typedef struct A2FO_N11_Vertex { float position[3], normal[3], uv[2]; } A2FO_N11_Vertex;
#endif
typedef struct A2FO_N11_Part {
    const A2FO_N11_Vertex* vertices;
    uint32_t vertex_count;
    const uint32_t* indices;
    uint32_t index_count;
    float local_to_model[16];
    const uint8_t* texture_name;
    uint32_t texture_name_length;
    float colour[4];
    float emissive[3];
    uint32_t flags; // native11 DrawFlags
} A2FO_N11_Part;
typedef int (*A2FO_N11_PartSink)(void*,const A2FO_N11_Part*);
// Static-model policy: pristine authored base pose (damage subtrees hidden),
// highest-detail LOD per geometry
// group, diffuse texture and outward reconstructed normals. SOD triangles are
// reversed to native11 clockwise fronts; node transforms stay authored.
// Sprite/emitter/light nodes
// and animated/bump channels are not submitted by this CPU mesh adapter.
// 1 success, 0 invalid/unsupported data or callback rejection; no exceptions
// unwind across the boundary. The caller provides bounded complete file bytes.
int a2fo_native11_parse_sod(const uint8_t*,uint32_t,A2FO_N11_PartSink,void*);
// Full authored mesh groups, including damage subtrees and every LOD level.
// Same borrowed callback lifetime and validation limits as the static parser.
int a2fo_native11_parse_sod_all(const uint8_t*,uint32_t,A2FO_N11_PartSink,void*);
// Explicit native-geometry path. Transform original owned XYZ triplets before
// reconstructing normals; callback count is points, not floats. All callbacks
// are synchronous; source file bytes remain unchanged. Missing transform fails.
typedef int (*A2FO_N11_TransformPositions)(void*,float*,uint32_t);
int a2fo_native11_parse_sod_native_geometry(const uint8_t*,uint32_t,
    A2FO_N11_TransformPositions,void*,A2FO_N11_PartSink,void*);
typedef int (*A2FO_N11_ImageSink)(void*,const uint8_t*,uint32_t,uint32_t,uint32_t,uint64_t);
int a2fo_native11_decode_image(const uint8_t*,uint32_t,A2FO_N11_ImageSink,void*);
// CPU DDS view. Payload and mip descriptors are borrowed until the synchronous
// callback returns. Format IDs: 1 BGRA8, 2 RGBA8, 3 BGRA8 sRGB, 4 RGBA8 sRGB,
// 5/6 BC1 UNORM/sRGB, 7/8 BC2, 9/10 BC3, 11 BC4 UNORM,
// 12 BC4 SNORM, 13 BC5 UNORM, 14 BC5 SNORM, 15/16 BC7 UNORM/sRGB.
// Alpha IDs: 0 unknown, 1 straight, 2 premultiplied, 3 opaque, 4 custom.
typedef struct A2FO_N11_TextureMip {
    uint32_t width, height, row_pitch, slice_pitch;
    uint64_t offset, byte_count;
} A2FO_N11_TextureMip;
typedef struct A2FO_N11_Texture {
    uint32_t format, alpha_mode, width, height, mip_count, reserved;
    uint64_t payload_bytes;
    const uint8_t* payload;
    const A2FO_N11_TextureMip* mips;
} A2FO_N11_Texture;
typedef int (*A2FO_N11_TextureSink)(void*,const A2FO_N11_Texture*);
// 1 success, 0 invalid/unsupported DDS or callback rejection.
int a2fo_native11_parse_dds(const uint8_t*,uint32_t,A2FO_N11_TextureSink,void*);
// Generic CPU texture entry point. DDS retains supported authored mips and BC
// blocks; TGA and validated legacy single-mip raw DDS emit one BGRA8 mip.
int a2fo_native11_parse_texture(const uint8_t*,uint32_t,A2FO_N11_TextureSink,void*);
#ifdef __cplusplus
}
#endif
