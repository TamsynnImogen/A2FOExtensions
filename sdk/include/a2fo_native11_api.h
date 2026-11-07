#ifndef A2FO_NATIVE11_API_H
#define A2FO_NATIVE11_API_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#define A2FO_N11_CALL __cdecl
#else
#define A2FO_N11_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif

/* Version 1 is an offscreen, pristine static-scene service. It cannot present
 * or replace the game renderer. No D3D8/9 object or state crosses this ABI.
 * All functions return signed 32-bit HRESULT values. Borrowed inputs and sink
 * bytes expire when the synchronous call returns; callbacks must not throw.
 * Every session call, including destruction, belongs to the first successful
 * create thread. Reentrant calls and other threads are refused. Keep the DLL
 * and host callback/context alive until every session has been destroyed.
 * Only readable/writable caller memory is accepted; this is not a fault barrier.
 */
#pragma pack(push,8)
#define A2FO_N11_API_VERSION 1u
#define A2FO_N11_API_VERSION_SHADOWS 2u
#define A2FO_N11_API_VERSION_TEXTURE_BINDINGS 3u
#define A2FO_N11_API_VERSION_PART_STATES 4u
#define A2FO_N11_CAP_OFFSCREEN_STATIC 1u
#define A2FO_N11_CAP_OFFSCREEN_SHADOWS 2u
#define A2FO_N11_CAP_MATERIAL_RELIGHT 4u
#define A2FO_N11_CAP_TEXTURE_BINDINGS 8u
#define A2FO_N11_CAP_PART_STATES 16u
#define A2FO_N11_FRAME_REQUIRE_PART_STATES 1u
typedef uint64_t A2FO_N11_Session;
typedef int32_t (A2FO_N11_CALL *A2FO_N11_BytesSink)(void*,const uint8_t*,uint32_t);
/* kind: 1 model, 2 texture. Exact opaque identifiers, no terminating NUL.
 * read_asset returns 1 only after exactly one successful sink call. The sink
 * copies bytes immediately and enforces max_bytes. Resolve against one stable
 * source epoch; do not allocate/free memory across the DLL boundary. */
typedef int32_t (A2FO_N11_CALL *A2FO_N11_ReadAsset)(void*,uint32_t kind,
    const uint8_t* identifier,uint32_t length,uint32_t max_bytes,
    A2FO_N11_BytesSink,void* sink_context);
typedef void (A2FO_N11_CALL *A2FO_N11_Log)(void*,const char*,uint32_t length);
typedef struct A2FO_N11_Host {
    uint32_t struct_size,flags,width,height; /* flags must be zero */
    void* context;
    A2FO_N11_ReadAsset read_asset;
    A2FO_N11_Log log; /* optional */
} A2FO_N11_Host;
typedef struct A2FO_N11_Object {
    float local_to_world[16]; /* row major, column vector */
    const uint8_t* model_identifier; /* exact printable ASCII, 1..259 bytes */
    uint32_t model_identifier_length,reserved; /* reserved must be zero */
} A2FO_N11_Object;
typedef struct A2FO_N11_Light {
    uint32_t type; /* 1 directional, 2 point, 3 cone/spot */
    float position[3],direction[3],colour[3]; /* direction is ray travel */
    float intensity,falloff_start,falloff_range;
    float cone_inner_degrees,cone_outer_degrees; /* full angles */
} A2FO_N11_Light;
typedef struct A2FO_N11_Frame {
    /* Required-state frames fail unless a part-state resolver is configured. */
    uint32_t struct_size,flags; /* zero or A2FO_N11_FRAME_REQUIRE_PART_STATES */
    uint64_t generation,source_epoch; /* both nonzero; epoch tracks geometry/texture sources */
    float world_to_clip[16],viewport[4],ambient[3];
    uint32_t object_count,light_count; /* <=2048 / <=8 */
    const A2FO_N11_Object* objects;
    const A2FO_N11_Light* lights;
} A2FO_N11_Frame;
typedef struct A2FO_N11_Readback {
    uint32_t struct_size,width,height,pitch;
    uint64_t required_bytes;
    uint8_t* bgra;
    uint64_t capacity;
} A2FO_N11_Readback;
typedef struct A2FO_N11_Usage {
    uint32_t struct_size,reserved;
    uint64_t device_epoch,mesh_bytes,texture_bytes; /* logical uploads, not VRAM */
    uint32_t meshes,textures; /* includes implicit white texture */
} A2FO_N11_Usage;
typedef struct A2FO_N11_API {
    uint32_t struct_size,version,capabilities,reserved;
    int32_t (A2FO_N11_CALL *create)(const A2FO_N11_Host*,A2FO_N11_Session*);
    int32_t (A2FO_N11_CALL *destroy)(A2FO_N11_Session);
    int32_t (A2FO_N11_CALL *submit)(A2FO_N11_Session,const A2FO_N11_Frame*);
    int32_t (A2FO_N11_CALL *resize)(A2FO_N11_Session,uint32_t,uint32_t);
    /* NULL bgra and zero capacity queries metadata (S_FALSE, no GPU read).
     * Metadata is also returned with an insufficient-buffer failure.
     * Dimensions describe the last successful create/resize. After target
     * creation fails, pixel reads fail until a successful resize retry.
     * Pixels change only on successful readback; rows are top-down BGRA8. */
    int32_t (A2FO_N11_CALL *readback)(A2FO_N11_Session,A2FO_N11_Readback*);
    int32_t (A2FO_N11_CALL *usage)(A2FO_N11_Session,A2FO_N11_Usage*);
} A2FO_N11_API;
typedef struct A2FO_N11_Shadows {
    uint32_t struct_size,light_mask,resolution;
    float depth_bias; /* 0..0.1 in normalized light depth */
} A2FO_N11_Shadows;
typedef struct A2FO_N11_Materials {
    uint32_t struct_size,relight_fullbright; /* explicit art policy, 0 or 1 */
    float retained_emission; /* 0..1; remaining fullbright moves to diffuse */
    uint32_t reserved;
} A2FO_N11_Materials;
/* Version 2 retains all version-1 function signatures in its prefix. It adds
 * persistent per-session shadow settings. Default mask=0; each bit selects the
 * corresponding Frame light. Resolution is a power of two, 128..2048.
 * Directional and spot lights use one depth slice, point lights use six.
 * The submit call checks the actual slice count against a 64 MiB depth budget.
 * Generic alpha/additive surfaces do not cast solid shadows; cutouts do.
 * World/caster completeness and light provenance belong to the host. */
typedef struct A2FO_N11_API2 {
    A2FO_N11_API base; /* struct_size=sizeof(API2), version=2 */
    int32_t (A2FO_N11_CALL *configure_shadows)(A2FO_N11_Session,const A2FO_N11_Shadows*);
    /* Optional appended capability. Rebuilds the scene's asset cache; the host
     * must be able to read its assets again. A new policy is committed only
     * after the replacement scene owner has been created successfully.
     * This relights opaque/cutout fullbright material; alpha/additive glow is
     * preserved. Authored emission masks are needed to relight a hull while
     * retaining windows/glow embedded in that same opaque texture. */
    int32_t (A2FO_N11_CALL *configure_materials)(A2FO_N11_Session,const A2FO_N11_Materials*);
} A2FO_N11_API2;
/* The earlier version-2 shadow-only prefix remains queryable. In that case
 * struct_size is this value and MATERIAL_RELIGHT is absent from capabilities. */
#define A2FO_N11_API2_SHADOWS_SIZE ((uint32_t)offsetof(A2FO_N11_API2,configure_materials))
/* Version 3 adds explicit host texture selection per emitted static SOD part.
 * model_identifier is an opaque CONTENT VIEW identity, not just a filename.
 * Give two views different identifiers when they select different materials,
 * even when read_asset returns identical SOD bytes for both. part_index is the
 * zero-based emitted-part ordinal of the shared pristine static policy,
 * including untextured parts. It is not an engine mesh pointer or node index.
 * The callback runs once per textured part on a cold model load; untextured
 * parts keep implicit white. Resolve must return 1 after exactly one successful
 * sink call with a printable ASCII texture ID of 1..max_identifier_bytes bytes.
 * read_asset(kind=2) then reads that exact ID. No fallback follows a failed
 * binding. Borrowed IDs expire when the callback returns; sinks copy at once.
 * This resolves diffuse textures only, not live node visibility, animation,
 * colour keys or other material properties. The host owns selection provenance.
 */
typedef int32_t (A2FO_N11_CALL *A2FO_N11_ResolveTexture)(void*,
    const uint8_t* model_identifier,uint32_t model_identifier_length,
    uint32_t part_index,const uint8_t* authored_texture,uint32_t authored_length,
    uint32_t max_identifier_bytes,A2FO_N11_BytesSink,void* sink_context);
typedef struct A2FO_N11_TextureBindings {
    uint32_t struct_size,reserved;
    void* context;
    A2FO_N11_ResolveTexture resolve; /* NULL + NULL context resets authored mode */
} A2FO_N11_TextureBindings;
typedef struct A2FO_N11_API3 {
    A2FO_N11_API2 base; /* base.base.struct_size=sizeof(API3), version=3 */
    /* Copies callback/context, not pointed-to state. Keep them alive through
     * reset or session destruction. Rebuilds asset cache transactionally and
     * preserves shadow/relight settings. Identical configuration is a no-op.
     * Advance Frame.source_epoch when selected IDs or source bytes change;
     * cached models never poll the callback. Invalid input leaves policy/cache
     * intact. Callback reentry and foreign-thread calls are refused. */
    int32_t (A2FO_N11_CALL *configure_texture_bindings)(A2FO_N11_Session,const A2FO_N11_TextureBindings*);
} A2FO_N11_API3;
/* Version 4 resolves every emitted static-policy part on every submission,
 * including cached models, hidden parts, and parts using implicit white.
 * part_index is the original emitted ordinal and does not change when earlier
 * parts are hidden. The callback returns 1 after exactly one successful sink
 * call. The sink copies the state immediately. Borrowed identifiers, authored
 * matrix, and state expire with the synchronous callback. No callback may
 * throw, reenter the service, or call it from another thread.
 * The resolved local_to_model REPLACES the authored transform; it is not a
 * delta. Matrix is finite affine row-major/column-vector; visible matrices
 * must be nonsingular. Visibility is 0 or 1. Hidden parts neither draw nor
 * cast shadows. Failed/invalid state refuses the entire frame before render.
 * Pose/visibility changes need no source_epoch advance; geometry, texture
 * selection, or source-byte changes do. Keep callback/context alive until
 * reset or session destruction. NULL resolve and NULL context reset authored
 * state. Identical configuration is a no-op. */
typedef struct A2FO_N11_PartState {
    uint32_t struct_size,visible;
    float local_to_model[16];
    uint32_t reserved;
} A2FO_N11_PartState;
typedef int32_t (A2FO_N11_CALL *A2FO_N11_PartStateSink)(void*,const A2FO_N11_PartState*);
typedef int32_t (A2FO_N11_CALL *A2FO_N11_ResolvePartState)(void*,
    const uint8_t* model_identifier,uint32_t model_identifier_length,
    uint32_t part_index,const float authored_local_to_model[16],
    A2FO_N11_PartStateSink,void* sink_context);
typedef struct A2FO_N11_PartStates {
    uint32_t struct_size,reserved;
    void* context;
    A2FO_N11_ResolvePartState resolve;
} A2FO_N11_PartStates;
typedef struct A2FO_N11_API4 {
    A2FO_N11_API3 base; /* base.base.base.struct_size=sizeof(API4), version=4 */
    int32_t (A2FO_N11_CALL *configure_part_states)(A2FO_N11_Session,const A2FO_N11_PartStates*);
} A2FO_N11_API4;
typedef int32_t (A2FO_N11_CALL *A2FO_N11_GetInterface)(uint32_t version,
    uint32_t output_size,void*);
/* Export: A2FO_Native11_GetInterface. Unsupported versions/sizes leave output
 * untouched. The table is caller owned and valid only while the DLL is loaded. */
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
