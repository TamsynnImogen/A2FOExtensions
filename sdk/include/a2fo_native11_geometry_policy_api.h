#ifndef A2FO_NATIVE11_GEOMETRY_POLICY_API_H
#define A2FO_NATIVE11_GEOMETRY_POLICY_API_H
#include "a2fo_native11_model_scope_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_NATIVE_GEOMETRY 10u
#define A2FO_N11_CAP_NATIVE_GEOMETRY 1024u
#pragma pack(push,8)
typedef struct A2FO_N11_GeometryPolicy {
    uint32_t struct_size,reserved,parser_scalar_bits,native_x87_control;
} A2FO_N11_GeometryPolicy;
/* Synchronous, exactly one sink call. Policy is immutable for this model ID
 * within source_epoch. x87 encoding is bit16 | native control word; unknown 0
 * is rejected. Host certifies original LOAD arithmetic and positive uniform
 * scalar. Draw-time arithmetic and inferred scalar 1 are not substitutes.
 * Original bytes and raw positions remain private to the service. */
typedef int32_t (A2FO_N11_CALL *A2FO_N11_GeometryPolicySink)(void*,const A2FO_N11_GeometryPolicy*);
typedef int32_t (A2FO_N11_CALL *A2FO_N11_ResolveGeometryPolicy)(void*,const uint8_t*,uint32_t,
    A2FO_N11_GeometryPolicySink,void*);
typedef struct A2FO_N11_GeometryPolicies {
    uint32_t struct_size,reserved;
    void* context;
    A2FO_N11_ResolveGeometryPolicy resolve;
} A2FO_N11_GeometryPolicies;
/* Appended API; every prefix entry retains API1-9 behavior. Native geometry
 * requires complete world/all-part state and a policy resolver. Policy applies
 * to original mesh positions BEFORE normal reconstruction, never node matrices.
 * Switching native/legacy geometry scope or callbacks clears cached resources.
 * This does not establish world completeness or class/material authority. */
typedef struct A2FO_N11_API10 {
    A2FO_N11_API9 base;
    int32_t (A2FO_N11_CALL *configure_geometry_policies)(A2FO_N11_Session,const A2FO_N11_GeometryPolicies*);
    int32_t (A2FO_N11_CALL *submit_world_native_geometry)(A2FO_N11_Session,const A2FO_N11_Frame*);
} A2FO_N11_API10;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
