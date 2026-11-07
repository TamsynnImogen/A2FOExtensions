#ifndef A2FO_NATIVE11_GEOMETRY_API_H
#define A2FO_NATIVE11_GEOMETRY_API_H
#include "a2fo_native11_world_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_GEOMETRY 6u
#define A2FO_N11_CAP_MESH_SHARING 64u
#pragma pack(push,8)
typedef struct A2FO_N11_MeshSharing {
    uint32_t struct_size,enabled,reserved[2]; /* enabled 0 or 1; reserved zero */
} A2FO_N11_MeshSharing;
/* Opt-in exact vertex/index content reuse across material views. Default off,
 * including for older tables. Draw materials, textures, transforms and current
 * part visibility stay independent. Source/device epoch and policy changes
 * invalidate the geometry pool. Policy changes rebuild scene caches only after
 * a replacement owner is ready; identical configuration is a no-op. Other
 * session policies are preserved. Counts from usage describe actual uploads.
 */
typedef struct A2FO_N11_API6 {
    A2FO_N11_API5 base; /* i686 size=64, version=6 */
    int32_t (A2FO_N11_CALL *configure_mesh_sharing)(A2FO_N11_Session,const A2FO_N11_MeshSharing*);
} A2FO_N11_API6;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
