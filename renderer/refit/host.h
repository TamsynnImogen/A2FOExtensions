#ifndef A2FO_REFIT_HOST_H
#define A2FO_REFIT_HOST_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* v1 uses cdecl and natural x86 layout. All pointed-to spans must be valid,
 * non-overlapping, naturally aligned caller memory for the duration of a call.
 * Uploads and frame inputs are copied; no caller pointers survive a call.
 * One owner thread, no reentry. The host owns Display and its HWND. Keep both
 * helpers loaded for the process lifetime after initializing this first ABI.
 * Bad parameters, a missing device or a wrong owner are rejected before GPU
 * attachment and may be corrected. Once native GPU attachment is attempted,
 * initialize is one-shot even if it returns DEVICE; restart the process before
 * another attempt. Keep the existing renderer authoritative on that failure.
 * Validation rejection leaves the previous frame usable. A DEVICE failure may
 * have recorded part of a frame and invalidates readback until a later success.
 * This is a scene interface, not permission to execute stock GOG game hooks. */
enum { A2FO_REFIT_ABI=1, A2FO_REFIT_OK=0, A2FO_REFIT_INVALID=-1,
 A2FO_REFIT_STATE=-2, A2FO_REFIT_DEVICE=-3, A2FO_REFIT_CONTENT=-4,
 A2FO_REFIT_LIMIT=-5, A2FO_REFIT_STALE=-6 };
enum { A2FO_REFIT_HDR=1, A2FO_REFIT_MSAA4=2, A2FO_REFIT_BLOOM=4 };
enum { A2FO_REFIT_TEAM=1, A2FO_REFIT_UNLIT=2 };
enum { A2FO_REFIT_DIRECTIONAL=1, A2FO_REFIT_POINT=2 };
enum { A2FO_REFIT_CAP_SOD=1, A2FO_REFIT_CAP_POSE=2,
 A2FO_REFIT_CAP_TEXTURE=4, A2FO_REFIT_CAP_LIGHTS=8,
 A2FO_REFIT_CAP_READBACK=16, A2FO_REFIT_CAP_POST=32 };
typedef struct A2FORefitConfig {
 uint32_t size; void *display_module; uint32_t flags, source_epoch;
} A2FORefitConfig;
typedef struct A2FORefitModel {
 uint32_t size, id, revision; const uint8_t *bytes; uint32_t byte_count;
} A2FORefitModel;
typedef struct A2FORefitTexture {
 uint32_t size; const char *name; uint32_t name_length, revision, width, height;
 const uint8_t *bgra; uint32_t byte_count;
} A2FORefitTexture;
/* Row-vector Matrix34: right[3], up[3], front[3], translation[3]. */
typedef struct A2FORefitNode { float world[12]; uint32_t shown; } A2FORefitNode;
typedef struct A2FORefitObject {
 uint32_t size, model_id, model_revision, flags;
 float world[12]; const A2FORefitNode *nodes; uint32_t node_count;
 float alpha, team[3], ambient[3], self_illum, animation_time;
 uint32_t animation_flags, lod;
 /* nodes NULL/count0: evaluate SOD animation at animation_time, force lod.
  * Otherwise supply exactly every SOD node's authoritative world pose. */
} A2FORefitObject;
typedef struct A2FORefitLight {
 uint32_t kind; float position[3], direction[3], colour[3], intensity, start, range;
} A2FORefitLight;
typedef struct A2FORefitFrame {
 uint32_t size, source_epoch, serial, x, y, width, height;
 float view[16], projection[16], eye[3], time;
 const A2FORefitObject *objects; uint32_t object_count;
 const A2FORefitLight *lights; uint32_t light_count;
} A2FORefitFrame;
typedef struct A2FORefitReadback {
 uint32_t size, source_epoch, serial, width, height;
 uint8_t *bgra; uint32_t byte_count;
 /* Full native target, tightly packed BGRA8. Output untouched on rejection. */
} A2FORefitReadback;
typedef struct A2FORefitStatus {
 uint32_t size, backend, source_epoch, last_serial, models, textures, device_epoch;
} A2FORefitStatus;
typedef struct A2FORefitApi {
 uint32_t size, abi_version, capabilities;
 int32_t (*initialize)(const A2FORefitConfig*);
 int32_t (*upload_model)(const A2FORefitModel*, uint32_t *node_count);
 int32_t (*upload_texture)(const A2FORefitTexture*);
 int32_t (*render)(const A2FORefitFrame*);
 int32_t (*readback)(const A2FORefitReadback*);
 int32_t (*reset_content)(uint32_t new_source_epoch);
 int32_t (*status)(A2FORefitStatus*);
 int32_t (*shutdown)(void);
} A2FORefitApi;
typedef const A2FORefitApi* (*A2FORefitGetInterface)(uint32_t version);
/* A2FORefitDisplay_Configure_v1: backend 1 Vulkan, 2 D3D12, 3 auto;
 * supplied HWND belongs to current process/owner thread. flags bit0 validation.
 * CreateDevice uses this same HWND; Display never changes its visibility/focus.
 * Display facade COM entry points retain stdcall, unlike the host C ABI. */
typedef int32_t (*A2FORefitDisplayConfigure)(uint32_t, void*, uint32_t, uint32_t, uint32_t);
/* Optional A2FORefitDisplay_Passthrough_v1(enabled), owner thread only.
 * Draw existing FO D3D8 fixed-function submissions on the native backend;
 * this does not load Refit UI or consult stock game call-stack categories.
 * Off by default. Host explicitly selects the scene/UI ownership boundary. */
typedef int32_t (*A2FORefitDisplayPassthrough)(uint32_t enabled);
/* Optional A2FORefitDisplay_Prepare_v1(backend,w,h,flags): configure the factory
 * before its game HWND exists. The first valid owner-thread CreateDevice binds
 * the HWND. This function neither admits an executable nor patches game code;
 * A2FO's separately validated startup bridge must own those decisions. */
typedef int32_t (*A2FORefitDisplayPrepare)(uint32_t, uint32_t, uint32_t, uint32_t);
/* ABI v2 adds owned per-group submissions. ABI v1 and all its sizes stay
 * unchanged. Obtain v2 through the same exported GetInterface(2), then cast to
 * A2FORefitApi2. The first eight function pointers have the v1 signatures.
 * Prepare copies all inputs and validates resources without issuing a draw.
 * Draw consumes exactly that prepared (epoch,serial) once, on the owner thread.
 * This is rendering capability only: the caller separately owns native draw
 * suppression. Preflight rejection must preserve original drawing. A DEVICE
 * failure during draw may have partially recorded colour/depth writes: there
 * is no GPU rollback guarantee, and callers must retire that attempted frame
 * or explicitly fall back; they must not claim an atomic replacement. */
enum { A2FO_REFIT_ABI2=2, A2FO_REFIT_CAP_PARTS=64,
       A2FO_REFIT_CAP_DIRECT=128, A2FO_REFIT_CAP_ENCODED_TEXTURE=256,
       A2FO_REFIT_FRAME_DIRECT=1 };
typedef struct A2FORefitPart2 {
 uint32_t size, model_id, model_revision, node_index, group_index, flags;
 float world[12];
 const char *texture_name; uint32_t texture_name_length, texture_revision;
 float alpha, team[3], ambient[3], self_illum;
 /* node_index/group_index identify the authored SOD mesh and face group.
  * Texture identity/revision is explicit per instance, not the authored name.
  * Empty texture span + revision0 selects untextured white.
  * DIRECT currently admits opaque/cutout groups with alpha exactly1 only. */
} A2FORefitPart2;
typedef struct A2FORefitFrame2 {
 uint32_t size, source_epoch, serial, x, y, width, height, flags;
 float view[16], projection[16], eye[3], time;
 const A2FORefitPart2 *parts; uint32_t part_count;
 const A2FORefitLight *lights; uint32_t light_count;
 /* DIRECT is required in v2: load current engine colour/depth unchanged,
  * draw supplied opaque groups, keep existing sky/effects/HUD ordering.
  * This path performs no post processing, colour clear or HUD depth clear. */
} A2FORefitFrame2;
typedef int32_t (*A2FORefitTransformPositions2)(void *context, float *xyz, uint32_t count);
typedef struct A2FORefitModel2 {
 A2FORefitModel base;
 void *context; A2FORefitTransformPositions2 transform_positions;
 /* base.size is sizeof(A2FORefitModel2). Callback synchronously transforms
  * each mutable owned XYZ mesh before normal reconstruction, exactly once.
  * count is XYZ triples, contiguous stride3. No callback/pointer survives upload.
  * The host owns captured loader scale/arithmetic policy; callback must not
  * reenter this API, read mutable engine state, or unwind across the C ABI. */
} A2FORefitModel2;
typedef struct A2FORefitEncodedTexture2 {
 uint32_t size; const char *name; uint32_t name_length, revision;
 const uint8_t *bytes; uint32_t byte_count, flags;
 /* Original DDS or TGA bytes. flags must be0 for this ABI revision.
  * Dimensions and decoded storage are validated before GPU allocation. */
} A2FORefitEncodedTexture2;
typedef struct A2FORefitApi2 {
 uint32_t size, abi_version, capabilities;
 int32_t (*initialize)(const A2FORefitConfig*);
 int32_t (*upload_model)(const A2FORefitModel*, uint32_t *node_count);
 int32_t (*upload_texture)(const A2FORefitTexture*);
 int32_t (*render)(const A2FORefitFrame*);
 int32_t (*readback)(const A2FORefitReadback*);
 int32_t (*reset_content)(uint32_t new_source_epoch);
 int32_t (*status)(A2FORefitStatus*);
 int32_t (*shutdown)(void);
 int32_t (*prepare_parts)(const A2FORefitFrame2*);
 int32_t (*draw_prepared)(uint32_t source_epoch, uint32_t serial);
 int32_t (*upload_encoded_texture)(const A2FORefitEncodedTexture2*);
 int32_t (*upload_model_transformed)(const A2FORefitModel2*, uint32_t *node_count);
} A2FORefitApi2;
/* ABI v3 (Renderer GetInterface(3)) adds host spotlights without touching any
 * ABI1/2 record. A2FORefitApi3 is the ABI2 table plus prepare_parts3; capability
 * A2FO_REFIT_CAP_SPOTS marks it. A frame3 is a complete ABI2 frame whose size field
 * equals sizeof(A2FORefitFrame3), followed by up to A2FO_REFIT_MAX_SPOTS spots.
 * Spot angles are FULL cone angles in radians, 0 < inner <= outer < pi: full colour
 * inside the inner cone, fading to none at the outer one (hard edge if equal), with
 * point-light start/range distance falloff; range > 0. Any invalid record rejects the
 * whole frame (A2FO_REFIT_INVALID; more than A2FO_REFIT_MAX_SPOTS is A2FO_REFIT_LIMIT,
 * as for over-long light spans) transactionally and retires the pending packet. */
enum { A2FO_REFIT_ABI3=3, A2FO_REFIT_CAP_SPOTS=1024, A2FO_REFIT_MAX_SPOTS=4 };
typedef struct A2FORefitSpot3 {
 float position[3], direction[3], colour[3], intensity, start, range, inner, outer;
} A2FORefitSpot3;
/* Optional ABI3 per-part material: Blinn-Phong specular colour (>= 0) and power
 * (0..1024, 0 = no highlight). material_count is 0 or exactly base.part_count. */
typedef struct A2FORefitPartMaterial3 { float specular[3], power; } A2FORefitPartMaterial3;
typedef struct A2FORefitFrame3 {
 A2FORefitFrame2 base; const A2FORefitSpot3 *spots; uint32_t spot_count;
 const A2FORefitPartMaterial3 *materials; uint32_t material_count;
} A2FORefitFrame3;
/* Specular maps (A2FO_REFIT_CAP_SPECULAR_MAPS, served through prepare_parts3): a frame
 * whose base.size equals sizeof(A2FORefitFrame3Maps) carries a full frame3 plus none,
 * or exactly base.part_count, map records. A map names a texture already uploaded with
 * upload_encoded_texture (name + current revision, as part textures); its RGB masks the
 * Blinn-Phong highlight at the part's diffuse UVs. A null name with zero length and zero
 * revision means no map (unmasked). A missing texture is A2FO_REFIT_CONTENT, an old
 * revision A2FO_REFIT_STALE. Renderers without the capability reject this size with
 * A2FO_REFIT_INVALID, so a host must check the capability first. */
enum { A2FO_REFIT_CAP_SPECULAR_MAPS=2048 };
typedef struct A2FORefitPartMap3 {
 const char *specular_name; uint32_t specular_name_length, specular_revision;
} A2FORefitPartMap3;
typedef struct A2FORefitFrame3Maps {
 A2FORefitFrame3 frame; const A2FORefitPartMap3 *maps; uint32_t map_count;
} A2FORefitFrame3Maps;
typedef struct A2FORefitApi3 {
 uint32_t size, abi_version, capabilities;
 int32_t (*initialize)(const A2FORefitConfig*);
 int32_t (*upload_model)(const A2FORefitModel*, uint32_t *node_count);
 int32_t (*upload_texture)(const A2FORefitTexture*);
 int32_t (*render)(const A2FORefitFrame*);
 int32_t (*readback)(const A2FORefitReadback*);
 int32_t (*reset_content)(uint32_t new_source_epoch);
 int32_t (*status)(A2FORefitStatus*);
 int32_t (*shutdown)(void);
 int32_t (*prepare_parts)(const A2FORefitFrame2*);
 int32_t (*draw_prepared)(uint32_t source_epoch, uint32_t serial);
 int32_t (*upload_encoded_texture)(const A2FORefitEncodedTexture2*);
 int32_t (*upload_model_transformed)(const A2FORefitModel2*, uint32_t *node_count);
 int32_t (*prepare_parts3)(const A2FORefitFrame3*);
} A2FORefitApi3;
/* Optional Display hook for a scoped opaque replacement. The callback is
 * invoked only on the Display owner thread at eligible back-buffer draws,
 * BEFORE acquiring a native backend command/context. Return1 only after the
 * replacement draw succeeded; 0 retains the original draw. Scope callbacks
 * must be restored by the host at the original instance-render return.
 * No input/window operation or stock call-stack inference is performed. */
typedef int32_t (*A2FORefitReplaceOpaqueCallback2)(void*, const uint32_t viewport[4]);
typedef int32_t (*A2FORefitDisplayReplaceOpaque2)(A2FORefitReplaceOpaqueCallback2, void*);
/* Read-only owner-thread draw diagnostics, captured before any foreign draw
 * callback. args excludes this; pointers are telemetry values, never ownership.
 * LastDraw writes the complete record only on success. */
typedef struct A2FORefitDraw2 {
 uint32_t size, slot, caller, vertex_format, viewport[4];
 uint32_t alpha_test, alpha_ref, alpha_func, arg_count, args[8], sequence;
} A2FORefitDraw2;
typedef int32_t (*A2FORefitDisplayLastDraw2)(A2FORefitDraw2*);
/* Additional x86 call-entry telemetry, separate from the stable Draw2 record.
 * sequence must match Draw2 before a consumer interprets caller_esi. */
typedef struct A2FORefitCaller2 {uint32_t size,sequence,caller_esi;} A2FORefitCaller2;
typedef int32_t (*A2FORefitDisplayLastCaller2)(A2FORefitCaller2*);
/* Screen-coordinate adjustment from the engine's RHW facade to scene draws.
 * Display applies the legacy pixel-centre correction to RHW vertices; native
 * scene viewports may already include it. Add this difference to scene screen
 * x/y when matching that RHW draw. Owner-thread query, transactional output. */
typedef struct A2FORefitRaster2 {uint32_t size;float rhw_to_scene_offset;} A2FORefitRaster2;
typedef int32_t (*A2FORefitDisplayRaster2)(A2FORefitRaster2*);
/* Separate explicit mesh submission scope. Only the exact legacy world layout
 * 0x1c4 with an opaque depth-writing base pass can enter this callback. The
 * consumer MUST correlate caller/arguments with its checked native mesh/group
 * scope; registering it does not attest geometry ownership. Generic opaque
 * registration continues to exclude RHW/HUD vertices. Registrations are
 * mutually exclusive and output records are borrowed for this call only. */
typedef int32_t (*A2FORefitReplaceMeshCallback2)(void*,const A2FORefitDraw2*);
typedef int32_t (*A2FORefitDisplayReplaceMesh2)(A2FORefitReplaceMeshCallback2,void*);
/* Separate opt-in export A2FORefitDisplay_ReplaceMeshRhw_v3. Same Draw2 wire
 * record; accepts exactly 0x144 (28-byte) and 0x1c4 (32-byte) RHW layouts.
 * ABI2 registration remains 0x1c4-only. All ownership and opaque-state
 * checks still apply; clear/unregister restores the non-extended mode. */
typedef int32_t (*A2FORefitDisplayReplaceMeshRhw3)(A2FORefitReplaceMeshCallback2,void*);
/* Owner-thread ready query. STATE during a window handoff or before a live
 * facade device exists; callers must defer first renderer attachment then. */
typedef int32_t (*A2FORefitDisplayReady2)(void);
#ifdef __cplusplus
}
#endif
#endif
