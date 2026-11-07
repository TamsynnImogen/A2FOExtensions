#ifndef A2FO_NATIVE11_PRIMITIVES_API_H
#define A2FO_NATIVE11_PRIMITIVES_API_H
#include "a2fo_native11_geometry_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_PRIMITIVES 7u
#define A2FO_N11_CAP_PRIMITIVES 128u
#define A2FO_N11_PRIMITIVE_LOAD_COLOUR 1u
#define A2FO_N11_PRIMITIVE_LOAD_DEPTH 2u
#define A2FO_N11_SURFACE_SHADOW_ONLY 1u
#define A2FO_N11_SURFACE_NO_CAST 2u
#define A2FO_N11_SURFACE_NO_RECEIVE 4u
#define A2FO_N11_SURFACE_NO_FOG 8u
#pragma pack(push,8)
#ifndef A2FO_N11_VERTEX_DEFINED
#define A2FO_N11_VERTEX_DEFINED
typedef struct A2FO_N11_Vertex {
    float position[3],normal[3],uv[2];
} A2FO_N11_Vertex;
#endif
typedef struct A2FO_N11_PrimitiveMesh {
    uint64_t identity;
    uint32_t vertex_count,index_count;
    const A2FO_N11_Vertex* vertices;
    const uint32_t* indices;
} A2FO_N11_PrimitiveMesh;
typedef struct A2FO_N11_PrimitiveTexture {
    uint64_t identity;
    uint32_t width,height;
    const uint8_t* bgra; /* tightly packed top-down, straight alpha */
    uint32_t byte_count; /* exactly width*height*4 */
} A2FO_N11_PrimitiveTexture;
typedef struct A2FO_N11_PrimitiveDraw {
    uint64_t mesh,texture; /* host identities, texture zero is implicit white */
    float local_to_world[16],colour[4],emissive[3];
    uint32_t flags; /* two-sided=1, cutout=2, alpha=4, additive=8 */
    uint32_t layer,depth_mode,sampling,options;
    /* depth: automatic=0, read-only=1, disabled=2.
     * sampling: linear-clamp=0, linear-wrap=1, point-clamp=2, point-wrap=3.
     * options: SHADOW_ONLY/NO_CAST/NO_RECEIVE/NO_FOG above. */
    float alpha_cutoff,uv_transform[6]; /* cutoff 0..1; affine row-major 2x3 */
    uint32_t reserved;
} A2FO_N11_PrimitiveDraw;
typedef struct A2FO_N11_PrimitiveFrame {
    uint32_t struct_size,flags; /* LOAD_COLOUR/LOAD_DEPTH, default clears both */
    uint64_t generation,source_epoch;
    float world_to_clip[16],viewport[4],ambient[3],clear_colour[4];
    uint32_t ordering; /* capture=0; layer, opaque-first, alpha back-to-front=1 */
    uint32_t shadow_mask,shadow_resolution;
    float shadow_bias;
    uint32_t shadow_filter; /* bilinear=0, hard=1, PCF3=2 */
    uint32_t fog_enabled;
    float fog_colour[3],fog_start,fog_end,camera_position[3];
    uint32_t mesh_count,texture_count,draw_count,light_count;
    const A2FO_N11_PrimitiveMesh* meshes;
    const A2FO_N11_PrimitiveTexture* textures;
    const A2FO_N11_PrimitiveDraw* draws;
    const A2FO_N11_Light* lights;
    uint32_t reserved;
} A2FO_N11_PrimitiveFrame;
/* The host supplies a complete immutable resource manifest on EVERY call.
 * Same source_epoch requires identical mesh/texture ID sets and content;
 * transforms, surfaces, light/view settings may change without GPU uploads.
 * generation strictly increases on successful primitive submissions. Changing
 * resource bytes or IDs requires a greater source_epoch. A device recreation
 * reuploads identical resources. Inputs expire on return and are copied before
 * use. No graphics object, engine pointer or legacy render state crosses ABI.
 * Bounds: 2048 meshes, 256 textures, 8192 draws, 8 lights, 1M cumulative vertices,
 * 3M cumulative indices and 128 MiB payload. A failed candidate upload or input
 * validation retains the last successful cache/image. Device loss can invalidate
 * target pixels; recover the target before requesting retained-target passes.
 * LOAD requires a previously initialized target. Raw geometry is host-owned
 * scene evidence; this service does not establish game visibility/completeness.
 * All API1-6 calls remain available and retain their previous default behavior.
 */
typedef struct A2FO_N11_API7 {
    A2FO_N11_API6 base; /* i686 size=68, version=7 */
    int32_t (A2FO_N11_CALL *submit_primitives)(A2FO_N11_Session,const A2FO_N11_PrimitiveFrame*);
} A2FO_N11_API7;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
