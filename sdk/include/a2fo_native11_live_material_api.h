#ifndef A2FO_NATIVE11_LIVE_MATERIAL_API_H
#define A2FO_NATIVE11_LIVE_MATERIAL_API_H
#include "a2fo_native11_geometry_policy_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_LIVE_MATERIALS 11u
#define A2FO_N11_CAP_LIVE_MATERIALS 2048u
#pragma pack(push,8)
/* Immutable authored seed, not the last frame's material. IDs refer to exact
 * SOD node/group identity, independently of filtered/legacy part ordering. */
typedef struct A2FO_N11_MaterialSeed {
    uint32_t struct_size,part_index,node_index,group_index;
    const uint8_t* authored_texture;
    uint32_t authored_texture_length;
    float colour[4],emissive[3];
    uint32_t flags;
} A2FO_N11_MaterialSeed;
/* Exactly one synchronous sink; identifier storage expires on callback return.
 * Empty identifier explicitly selects white, including previously textured
 * parts. Nonempty identifiers may texture previously untextured parts.
 * All fields replace the seed; no implicit previous-frame state is used.
 * Texture bytes are immutable for each ID inside source_epoch. */
typedef struct A2FO_N11_LiveMaterial {
    uint32_t struct_size,flags,sampler,depth_mode;
    float colour[4],emissive[3],alpha_cutoff,uv_transform[6];
    const uint8_t* texture_identifier;
    uint32_t texture_identifier_length;
} A2FO_N11_LiveMaterial;
typedef int32_t (A2FO_N11_CALL *A2FO_N11_LiveMaterialSink)(void*,const A2FO_N11_LiveMaterial*);
typedef int32_t (A2FO_N11_CALL *A2FO_N11_ResolveLiveMaterial)(void*,const uint8_t*,uint32_t,
    const A2FO_N11_MaterialSeed*,A2FO_N11_LiveMaterialSink,void*);
typedef struct A2FO_N11_LiveMaterials {
    uint32_t struct_size,reserved;
    void* context;
    A2FO_N11_ResolveLiveMaterial resolve;
} A2FO_N11_LiveMaterials;
/* API1-10 prefixes retain their original behavior. Preparation validates every
 * part and uploads resources without clearing, drawing or presenting. Preview
 * renders observed all-part native geometry without claiming complete casters;
 * shadows and presentation remain unavailable through this entry point.
 * Full-world callers still use the existing complete-caster submission API. */
typedef struct A2FO_N11_API11 {
    A2FO_N11_API10 base;
    int32_t (A2FO_N11_CALL *configure_live_materials)(A2FO_N11_Session,const A2FO_N11_LiveMaterials*);
    int32_t (A2FO_N11_CALL *prepare_live_materials)(A2FO_N11_Session,const A2FO_N11_Frame*);
    int32_t (A2FO_N11_CALL *submit_live_material_preview)(A2FO_N11_Session,const A2FO_N11_Frame*);
} A2FO_N11_API11;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
