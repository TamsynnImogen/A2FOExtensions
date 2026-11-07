#ifndef A2FO_NATIVE11_MODEL_SCOPE_API_H
#define A2FO_NATIVE11_MODEL_SCOPE_API_H
#include "a2fo_native11_presentation_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_ALL_MODEL_PARTS 9u
#define A2FO_N11_CAP_ALL_MODEL_PARTS 512u
/* Unlike the pristine highest-LOD APIs, this entry emits every SOD mesh group,
 * including damage groups and all LODs, in authored node/group order. The host
 * must supply COMPLETE_CASTERS and REQUIRE_PART_STATES and resolve every part,
 * including hidden ones. A hidden part's visibility is selected by the host;
 * this capability does not discover native class state or world completeness.
 * Other entry points in the API1-8 prefix retain their original model scope.
 * Switching scope invalidates cached model resources and presentation eligibility.
 */
#pragma pack(push,8)
typedef struct A2FO_N11_API9 {
    A2FO_N11_API8 base;
    int32_t (A2FO_N11_CALL *submit_world_all_parts)(A2FO_N11_Session,const A2FO_N11_Frame*);
} A2FO_N11_API9;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
