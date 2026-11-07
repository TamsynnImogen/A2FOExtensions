#ifndef A2FO_NATIVE11_PRESENTATION_API_H
#define A2FO_NATIVE11_PRESENTATION_API_H
#include "a2fo_native11_primitives_api.h"
#ifdef __cplusplus
extern "C" {
#endif
#define A2FO_N11_API_VERSION_PRESENTATION 8u
#define A2FO_N11_CAP_PRESENTATION 256u
/* Attach borrows an existing same-process, creating-thread HWND. No window is
 * created, shown, activated, resized or destroyed by the service. Null detaches.
 * Sizes must match the render target and the nonempty client area exactly.
 * A successful attach/resize requires a fresh complete submission before present.
 * Present returns S_OK for submission, S_FALSE for occlusion; neither proves
 * visible pixels. Failure or occlusion never authorizes host graphics takeover.
 * Hosts must detach/destroy on the creating thread before HWND or DLL teardown.
 * These operations use the native D3D11 device; no D3D8/9 surface is consumed. */
#pragma pack(push,8)
typedef struct A2FO_N11_PresentationTarget {
    uint32_t struct_size,flags;
    void* window;
    uint32_t width,height,reserved;
} A2FO_N11_PresentationTarget;
typedef struct A2FO_N11_Present {
    uint32_t struct_size,sync_interval,flags,reserved;
} A2FO_N11_Present;
typedef struct A2FO_N11_API8 {
    A2FO_N11_API7 base;
    int32_t (A2FO_N11_CALL *configure_presentation)(A2FO_N11_Session,const A2FO_N11_PresentationTarget*);
    int32_t (A2FO_N11_CALL *present)(A2FO_N11_Session,const A2FO_N11_Present*);
} A2FO_N11_API8;
#pragma pack(pop)
#ifdef __cplusplus
}
#endif
#endif
