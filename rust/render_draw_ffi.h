#ifndef A2FO_RUST_RENDER_DRAW_FFI_H
#define A2FO_RUST_RENDER_DRAW_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { int32_t ok,e_pointer,e_nointerface,invalid_call,notfound,moredata,outmemory; } A2foRsRenderHr;
typedef struct { int32_t hr; uint8_t kind; } A2foRsInterfacePlan;
typedef struct { uint32_t dxgi,legacy; } A2foRsFormatPair;
typedef struct { int32_t hr; uint32_t needed; uint8_t copy; } A2foRsPrivateGet;
typedef struct { int32_t left,top,right,bottom; } A2foRsRect;
typedef struct { uint32_t width,height,row_pitch; size_t offset; } A2foRsMip;
typedef struct { int32_t hr,pitch; size_t offset; } A2foRsLockPlan;
typedef struct { int32_t hr; uint8_t error_kind; } A2foRsCreatePlan;
typedef struct { void *context;
    int32_t (*increment_public)(void *); int32_t (*decrement_public)(void *);
    int32_t (*increment_implicit)(void *); int32_t (*decrement_implicit)(void *);
    int32_t (*load_public)(void *); void (*parent_addref)(void *);
    void (*parent_release)(void *); void (*delete_self)(void *);
    void (*texture_addref)(void *); void (*texture_release)(void *);
} A2foRsTextureRefs;
A2foRsInterfacePlan a2fo_rs_texture_query(const uint8_t *iid,const uint8_t *known_guids,uint8_t surface,uint8_t has_output,A2foRsRenderHr hr);
int32_t a2fo_rs_texture_private_set(uint8_t has_data,uint32_t size,uint32_t flags,uint32_t unknown_flag,uint32_t pointer_size,A2foRsRenderHr hr);
A2foRsPrivateGet a2fo_rs_texture_private_get(uint8_t has_size,uint8_t found,uint8_t object,uint32_t byte_count,uint32_t supplied,uint8_t has_data,uint32_t pointer_size,A2foRsRenderHr hr);
int32_t a2fo_rs_texture_private_erase(uint8_t found,A2foRsRenderHr hr);
uint32_t a2fo_rs_texture_legacy_format(uint32_t dxgi,const A2foRsFormatPair *formats,size_t count,uint32_t unknown);
uint32_t a2fo_rs_texture_exchange_u32(uint32_t old,uint32_t value,uint32_t *next);
int32_t a2fo_rs_texture_output_valid(uint8_t has_output,int32_t invalid,A2foRsRenderHr hr);
int32_t a2fo_rs_texture_binding_valid(uint8_t has_output,uint8_t has_binding,A2foRsRenderHr hr);
uint32_t a2fo_rs_texture_set_lod(uint32_t old,uint32_t value,uint32_t levels,uint32_t *next);
int32_t a2fo_rs_texture_level_valid(uint8_t has_output,uint32_t level,size_t count,A2foRsRenderHr hr);
int32_t a2fo_rs_texture_lock_pre(uint8_t has_output,uint32_t level,size_t count,uint8_t locked,uint32_t flags,uint32_t read_flag,uint32_t allowed_flags,A2foRsRenderHr hr);
A2foRsLockPlan a2fo_rs_texture_lock(uint8_t has_output,uint32_t level,size_t count,uint8_t locked,uint32_t flags,uint32_t read_flag,uint32_t allowed_flags,uint8_t has_region,A2foRsRect region,A2foRsMip mip,uint32_t format,uint32_t bc1,uint32_t bc1_srgb,uint32_t bc4,uint32_t bc4_snorm,A2foRsRenderHr hr);
int32_t a2fo_rs_texture_unlock(uint32_t level,size_t count,uint8_t locked,A2foRsRenderHr hr);
A2foRsCreatePlan a2fo_rs_texture_create(uint8_t has_output,uint8_t has_owner,uint8_t has_device,uint32_t interpretation,uint32_t raw,uint32_t normal_xy,uint32_t format,uint32_t bc5_unorm,uint32_t bc5_snorm,A2foRsRenderHr hr);
uint8_t a2fo_rs_texture_binding_after(int32_t result);
uint32_t a2fo_rs_texture_addref(A2foRsTextureRefs ops);
uint32_t a2fo_rs_texture_release(A2foRsTextureRefs ops);
uint32_t a2fo_rs_texture_retain_binding(A2foRsTextureRefs ops);
uint32_t a2fo_rs_texture_release_binding(A2foRsTextureRefs ops);
uint32_t a2fo_rs_surface_addref(A2foRsTextureRefs ops);
uint32_t a2fo_rs_surface_release(A2foRsTextureRefs ops);
typedef struct { const uint8_t *data; size_t length; uint32_t stride; uint8_t present,locked; } A2foRsDrawSource;
typedef struct { uint32_t stream,offset,reg,type; } A2foRsDrawElement;
typedef struct { uint32_t stride,position_components,texture_count,normal_offset,diffuse_offset,specular_offset;
    uint32_t texture_offsets[8],texture_components[8];
    uint8_t transformed,normal,diffuse,specular; } A2foRsDrawLayout;
typedef struct { uint8_t present,compressed,normal_xy,needs_upload; uint32_t lod; } A2foRsDrawStage;
typedef struct { uintptr_t base; size_t size; uint32_t state,protect; uint8_t valid; } A2foRsMemoryRegion;
typedef struct { uint32_t type,count,first,user_stride,index_format,min_index,vertex_count,base;
    const void *user_vertices,*user_indices; uint8_t in_scene,recording,pixel_shader;
    uint32_t format16,format32,vertex_bytes,mem_commit,page_noaccess,page_guard;
    int32_t invalid_call,outmemory,ok; } A2foRsDrawInput;
typedef struct { void *context;
    int32_t (*primitive_count)(void *,uint32_t,uint32_t,uint32_t *);
    A2foRsMemoryRegion (*memory_region)(void *,uintptr_t);
    uint8_t (*resize_indices)(void *,uint32_t);
    uint8_t (*resize_vertices)(void *,uint32_t);
    void (*set_index)(void *,uint32_t,uint32_t);
    float *(*vertex)(void *,uint32_t);
    uint8_t (*has_shader)(void *);
    uint8_t (*shader_hlsl_empty)(void *);
    uint32_t (*shader_element_count)(void *);
    A2foRsDrawElement (*shader_element)(void *,uint32_t);
    int32_t (*decode_layout)(void *,A2foRsDrawLayout *);
    uint8_t (*set_state)(void *,uint8_t,uint8_t,uint8_t,uint8_t,uint8_t,const uint32_t *);
    A2foRsDrawSource (*stream)(void *,uint32_t);
    A2foRsDrawStage (*stage)(void *,uint32_t);
    int32_t (*compressed_view)(void *,uint32_t);
    int32_t (*upload)(void *,uint32_t);
    int32_t (*texture_view)(void *,uint32_t);
    void (*stage_finish)(void *,uint32_t,uint8_t,uint32_t);
    uint8_t (*stage_view_present)(void *,uint32_t);
    int32_t (*backend_draw)(void *);
    void (*success)(void *,uint8_t,uint8_t,uint8_t);
    int32_t (*fail)(void *,uint32_t,int32_t);
} A2foRsDrawOps;
int32_t a2fo_rs_draw_run(A2foRsDrawInput input,A2foRsDrawOps ops);
int32_t a2fo_rs_draw_samples(A2foRsDrawOps ops,int32_t ok,int32_t outmemory);
typedef struct { void *context;
    A2foRsMemoryRegion (*memory_region)(void *,uintptr_t);
    uint8_t (*append_word)(void *,uint32_t);
    uint32_t (*parse_words)(void *,uint8_t);
} A2foRsShaderWordsOps;
uint8_t a2fo_rs_shader_words(const uint32_t *input,uint8_t declaration,
    uint32_t mem_commit,uint32_t page_noaccess,uint32_t page_guard,A2foRsShaderWordsOps ops);
typedef struct { void *context; uint8_t (*append_element)(void *,A2foRsDrawElement); } A2foRsDeclarationOps;
uint8_t a2fo_rs_shader_declaration(const uint32_t *tokens,size_t count,
    A2foRsDeclarationOps ops,uint32_t *register_mask);
uint8_t a2fo_rs_shader_mask_valid(uint32_t input_mask,uint32_t declared);
typedef struct { int32_t hr; uint32_t needed; } A2foRsShaderBlobPlan;
A2foRsShaderBlobPlan a2fo_rs_shader_blob(const uint8_t *bytes,size_t count,
    uint8_t *output,uint8_t has_size,uint32_t capacity,int32_t invalid,int32_t ok,int32_t moredata);
int32_t a2fo_rs_shader_indexed_valid(uint8_t has_indices,uint8_t locked,int32_t count_hr,
    uint32_t start,uint32_t count,uint32_t index_size,size_t buffer_size,int32_t invalid,int32_t ok);
uint32_t a2fo_rs_shader_index_size(uint32_t format,uint32_t format16);
int32_t a2fo_rs_shader_up_valid(uint8_t has_indices,uint8_t has_data,int32_t invalid,int32_t ok);
int32_t a2fo_rs_shader_constant(uint32_t first,const uint8_t *data,uint32_t count,
    uint8_t *target,void *mask_context,void (*mark)(void *,uint32_t),int32_t invalid,int32_t ok);
int32_t a2fo_rs_shader_constant_valid(uint32_t first,uint8_t has_data,uint32_t count,int32_t invalid,int32_t ok);
int32_t a2fo_rs_shader_set_pixel(uint32_t handle,int32_t invalid,int32_t ok);
int32_t a2fo_rs_shader_handle_valid(uint8_t known,int32_t fvf_hr,int32_t invalid,int32_t ok);
int32_t a2fo_rs_shader_erase_result(uint8_t erased,int32_t invalid,int32_t ok);
uint32_t a2fo_rs_shader_next_handle(uint32_t current,uint32_t *next);
uint8_t a2fo_rs_shader_diagnostic(uint8_t has_log,uint32_t current,uint32_t *next);
#ifdef __cplusplus
}
#endif
#endif
