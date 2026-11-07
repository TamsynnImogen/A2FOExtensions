#include "../renderer/native11/assets.h"
#include "../sdk/include/a2fo_native11_primitives_api.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==4,"i686 C fixture");
_Static_assert(sizeof(A2FO_N11_API7)==68,"API7 table ABI");
_Static_assert(offsetof(A2FO_N11_API7,submit_primitives)==64,"API7 append offset");
_Static_assert(sizeof(A2FO_N11_Vertex)==32,"vertex ABI");
_Static_assert(sizeof(A2FO_N11_PrimitiveMesh)==24,"mesh ABI");
_Static_assert(sizeof(A2FO_N11_PrimitiveTexture)==24,"texture ABI");
_Static_assert(sizeof(A2FO_N11_PrimitiveDraw)==160,"draw ABI");
_Static_assert(sizeof(A2FO_N11_PrimitiveFrame)==224,"frame ABI");
_Static_assert(sizeof(A2FO_N11_API6)==64,"API6 prefix retained");
int32_t native11_c_query7(A2FO_N11_GetInterface get,A2FO_N11_API7* out){
    return get(A2FO_N11_API_VERSION_PRIMITIVES,sizeof(*out),out);
}
