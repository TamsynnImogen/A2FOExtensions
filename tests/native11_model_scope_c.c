#include "../sdk/include/a2fo_native11_model_scope_api.h"

_Static_assert(sizeof(A2FO_N11_API9)==80,"API9 i686 table size");
_Static_assert(offsetof(A2FO_N11_API9,submit_world_all_parts)==76,"API9 append offset");
_Static_assert(sizeof(A2FO_N11_API8)==76,"API8 prefix retained");

int32_t native11_c_query9(A2FO_N11_GetInterface get,A2FO_N11_API9* out) {
    return get(9,sizeof(*out),out);
}
int32_t native11_c_submit_all(const A2FO_N11_API9* api,A2FO_N11_Session session,
                              const A2FO_N11_Frame* frame) {
    return api->submit_world_all_parts(session,frame);
}
