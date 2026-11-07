#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
struct A2FoRefractParams {
 float clip[16],lights[19][4];
 float visibility,time,hull_floor,bend,ripple,rim,front,shimmer;
 std::uint64_t snapshot_key;
 float bounds_center[3],bounds_extent[3];
};
struct A2FoRefractDraw {
 std::uint32_t primitive_type;
 std::int32_t base_vertex;
 std::uint32_t min_vertex,num_vertices,start_index,primitive_count;
};
struct A2FoRefractState {
 void *source,*device,*decl,*vs,*ps,*copy;
 std::uint32_t width,height,format;
 std::uint64_t snapshot_key;
 void *snapshot_target;
 std::uint32_t snapshot_x,snapshot_y;
};
struct A2FoRefractRequest {
 void *first,*second;
 const float *floats;
 std::uint32_t index,count,value,extra;
 std::int32_t left,top,right,bottom;
};
struct A2FoRefractReply {
 std::int32_t hr;
 void *handle,*other;
 std::uint32_t a,b,c,d,format,version,extra;
 const char *detail;
};
using A2FoRefractCall = int (*)(void*,std::uint32_t,const A2FoRefractRequest*,A2FoRefractReply*) noexcept;
std::uint32_t a2fo_rs_refraction_draw(void*,const A2FoRefractParams*,const A2FoRefractDraw*,
                                      A2FoRefractState*,void*,A2FoRefractCall) noexcept;
void a2fo_rs_refraction_release(A2FoRefractState*,void*,A2FoRefractCall) noexcept;
}
enum : std::uint32_t {
 A2FO_REF_QI=1,A2FO_REF_CAPS,A2FO_REF_CREATE_DECL,A2FO_REF_CREATE_VS,
 A2FO_REF_CREATE_PS,A2FO_REF_CREATE_TEX,A2FO_REF_GET_STREAM,A2FO_REF_GET_INDICES,
 A2FO_REF_GET_HULL,A2FO_REF_GET_TARGET,A2FO_REF_GET_VIEWPORT,A2FO_REF_GET_DESC,
 A2FO_REF_GET_SURFACE,A2FO_REF_STRETCH,A2FO_REF_CREATE_STATE,A2FO_REF_CAPTURE,
 A2FO_REF_APPLY,A2FO_REF_DRAW,A2FO_REF_SET_DECL,A2FO_REF_SET_VS,
 A2FO_REF_SET_PS,A2FO_REF_SET_VSCONST,A2FO_REF_SET_PSCONST,A2FO_REF_SET_TEXTURE,
 A2FO_REF_SET_SAMPLER,A2FO_REF_SET_RENDER,A2FO_REF_RELEASE,A2FO_REF_LOAD47,
 A2FO_REF_LOAD43,A2FO_REF_PROC,A2FO_REF_FREE,A2FO_REF_COMPILE,
 A2FO_REF_BLOB_POINTER,A2FO_REF_MESSAGE_ERROR,A2FO_REF_MESSAGE_TEXT,
};
