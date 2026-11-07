#ifndef A2FO_NATIVE11_WORLD_API_H
#define A2FO_NATIVE11_WORLD_API_H
#include "a2fo_native11_api.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Version 5 adds a separate world submit function; older submit functions
 * retain their original validation. No existing record changes size.
 * World submission requires COMPLETE_CASTERS, an explicit host assertion that
 * every relevant caster, including offscreen geometry, is represented.
 * Object.reserved becomes participation flags ONLY for submit_world:
 * SHADOW_ONLY geometry enters light depth/bounds but never camera colour/depth.
 * It still consumes the same object/part budgets and resolves current part
 * state. A part hidden by its state callback neither draws nor casts.
 * The service cannot establish world completeness or native visibility itself.
 */
#define A2FO_N11_API_VERSION_WORLD 5u
#define A2FO_N11_CAP_WORLD_CASTERS 32u
#define A2FO_N11_FRAME_COMPLETE_CASTERS 2u
#define A2FO_N11_OBJECT_SHADOW_ONLY 1u
#pragma pack(push,8)
typedef struct A2FO_N11_API5 {
    A2FO_N11_API4 base; /* base.base.base.base.struct_size=60 on i686, version=5 */
    int32_t (A2FO_N11_CALL *submit_world)(A2FO_N11_Session,const A2FO_N11_Frame*);
} A2FO_N11_API5;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
