#include "../sdk/include/a2fo_native11_geometry_api.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==4,"i686 C fixture");
_Static_assert(sizeof(A2FO_N11_MeshSharing)==16,"mesh sharing config ABI");
_Static_assert(sizeof(A2FO_N11_API6)==64,"API6 table ABI");
_Static_assert(offsetof(A2FO_N11_API6,configure_mesh_sharing)==60,"API6 append offset");
_Static_assert(sizeof(A2FO_N11_Object)==76,"object ABI retained");
_Static_assert(sizeof(A2FO_N11_Frame)==136,"frame ABI retained");
int32_t native11_c_query6(A2FO_N11_GetInterface get,A2FO_N11_API6* out){
    return get(A2FO_N11_API_VERSION_GEOMETRY,sizeof(*out),out);
}
