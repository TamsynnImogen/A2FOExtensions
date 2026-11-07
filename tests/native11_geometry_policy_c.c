#include "../sdk/include/a2fo_native11_geometry_policy_api.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==4,"i686 C fixture");
_Static_assert(sizeof(A2FO_N11_GeometryPolicy)==16,"policy ABI");
_Static_assert(sizeof(A2FO_N11_GeometryPolicies)==16,"resolver ABI");
_Static_assert(sizeof(A2FO_N11_API)==40&&sizeof(A2FO_N11_API9)==80,"API prefixes");
_Static_assert(sizeof(A2FO_N11_API10)==88,"API10 table");
_Static_assert(offsetof(A2FO_N11_API10,configure_geometry_policies)==80,"config offset");
_Static_assert(offsetof(A2FO_N11_API10,submit_world_native_geometry)==84,"submit offset");
int32_t native11_c_query10(A2FO_N11_GetInterface get,A2FO_N11_API10* out){
    return get(A2FO_N11_API_VERSION_NATIVE_GEOMETRY,sizeof(*out),out);
}
int32_t native11_c_submit_geometry(const A2FO_N11_API10* api,A2FO_N11_Session session,
    const A2FO_N11_Frame* frame){return api->submit_world_native_geometry(session,frame);}
