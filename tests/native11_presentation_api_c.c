#include "../sdk/include/a2fo_native11_presentation_api.h"
_Static_assert(sizeof(A2FO_N11_API8)==76,"API8 size");
_Static_assert(offsetof(A2FO_N11_API8,configure_presentation)==68,"append-only configure");
_Static_assert(offsetof(A2FO_N11_API8,present)==72,"append-only present");
_Static_assert(sizeof(A2FO_N11_PresentationTarget)==24,"target size");
_Static_assert(sizeof(A2FO_N11_Present)==16,"present size");
int native11_presentation_c_contract(void){return A2FO_N11_CAP_PRESENTATION==256u;}
