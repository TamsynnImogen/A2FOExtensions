#include "../sdk/include/a2fo_native11_api.h"
#include <stddef.h>
_Static_assert(sizeof(void*)==4,"i686 fixture");
_Static_assert(sizeof(A2FO_N11_Host)==28,"host ABI");
_Static_assert(sizeof(A2FO_N11_Object)==76,"object ABI");
_Static_assert(sizeof(A2FO_N11_Light)==60,"light ABI");
_Static_assert(sizeof(A2FO_N11_Frame)==136,"frame ABI");
_Static_assert(offsetof(A2FO_N11_Frame,generation)==8,"generation ABI");
_Static_assert(offsetof(A2FO_N11_Frame,objects)==124,"object array ABI");
_Static_assert(sizeof(A2FO_N11_Readback)==40,"readback ABI");
_Static_assert(offsetof(A2FO_N11_Readback,capacity)==32,"capacity ABI");
_Static_assert(sizeof(A2FO_N11_Usage)==40,"residency ABI");
_Static_assert(sizeof(A2FO_N11_API)==40,"table ABI");
_Static_assert(sizeof(A2FO_N11_API2)==48,"version 2 table ABI");
_Static_assert(offsetof(A2FO_N11_API2,configure_shadows)==40,"version 2 table extension ABI");
_Static_assert(A2FO_N11_API2_SHADOWS_SIZE==44,"version 2 shadow-only prefix ABI");
_Static_assert(offsetof(A2FO_N11_API2,configure_materials)==44,"version 2 materials extension ABI");
_Static_assert(sizeof(A2FO_N11_Shadows)==16,"shadow settings ABI");
_Static_assert(sizeof(A2FO_N11_Materials)==16,"material policy ABI");
_Static_assert(sizeof(A2FO_N11_TextureBindings)==16,"texture bindings ABI");
_Static_assert(sizeof(A2FO_N11_API3)==52,"version 3 table ABI");
_Static_assert(offsetof(A2FO_N11_API3,configure_texture_bindings)==48,"version 3 extension ABI");
_Static_assert(sizeof(A2FO_N11_PartState)==76,"part state ABI");
_Static_assert(sizeof(A2FO_N11_PartStates)==16,"part state config ABI");
_Static_assert(sizeof(A2FO_N11_API4)==56,"version 4 table ABI");
_Static_assert(offsetof(A2FO_N11_API4,configure_part_states)==52,"version 4 extension ABI");
int32_t native11_c_query(A2FO_N11_GetInterface get,A2FO_N11_API* output){
    return get(A2FO_N11_API_VERSION,sizeof(*output),output);
}
int32_t native11_c_query2(A2FO_N11_GetInterface get,A2FO_N11_API2* output){
    return get(A2FO_N11_API_VERSION_SHADOWS,sizeof(*output),output);
}
int32_t native11_c_query3(A2FO_N11_GetInterface get,A2FO_N11_API3* output){
    return get(A2FO_N11_API_VERSION_TEXTURE_BINDINGS,sizeof(*output),output);
}
int32_t native11_c_query4(A2FO_N11_GetInterface get,A2FO_N11_API4* output){
    return get(A2FO_N11_API_VERSION_PART_STATES,sizeof(*output),output);
}
