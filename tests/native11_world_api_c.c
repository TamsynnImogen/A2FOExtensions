#include "../sdk/include/a2fo_native11_world_api.h"
_Static_assert(sizeof(A2FO_N11_API5)==60,"API5 i686 table size");
_Static_assert(offsetof(A2FO_N11_API5,submit_world)==56,"API4 prefix retained");
_Static_assert(sizeof(A2FO_N11_Frame)==136,"frame size retained");
_Static_assert(sizeof(A2FO_N11_Object)==76,"object size retained");
int32_t native11_c_query5(A2FO_N11_GetInterface get,A2FO_N11_API5* out){return get(5,sizeof(*out),out);}
