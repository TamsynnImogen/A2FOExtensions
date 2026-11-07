#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foDrawAdmission {std::uint32_t code;float rhw;};
A2foDrawAdmission a2fo_draw_admit(std::uint32_t width,std::uint32_t height,
    float min_z,float max_z,std::uint32_t primitive,std::size_t vertex_count,
    std::size_t index_count,bool transformed,const float* vertices);
void a2fo_draw_sequence(std::uint32_t* output,std::size_t count);
std::uint32_t a2fo_draw_indices(const std::uint32_t* indices,std::size_t count,
    std::size_t vertex_count,std::uint32_t primitive,std::uint32_t* fan,
    std::size_t fan_capacity,std::size_t* fan_count);
std::uint32_t a2fo_draw_render(const std::uint32_t* render,bool ff,
    bool transformed,std::uint32_t primitive);
struct A2foDrawBlend {std::uint32_t source,dest,source_alpha,dest_alpha;};
bool a2fo_draw_blend(std::uint32_t source,std::uint32_t dest,A2foDrawBlend* out);
struct A2foDrawStage {std::uint32_t code;bool uses_texture;std::uint32_t row[16];};
std::uint32_t a2fo_draw_stage_pre(const std::uint32_t* stage);
A2foDrawStage a2fo_draw_stage_post(const std::uint32_t* stage,bool ff,
    bool transformed,bool has_normal,const std::uint32_t* tex_components);
void a2fo_draw_stage_finish(const std::uint32_t* stage,std::uint32_t* row);
struct A2foDrawSampler {std::uint32_t filter,address_u,address_v,address_w;
    std::uint32_t border,bias,max_anisotropy;float min_lod,max_lod;};
bool a2fo_draw_sampler_validate(const std::uint32_t* stage);
bool a2fo_draw_sampler_plan(const std::uint32_t* stage,std::uint32_t minimum_lod,
    A2foDrawSampler* out);
std::uint32_t a2fo_draw_buffer_capacity(std::uint32_t current,std::uint32_t bytes);
struct A2foDrawStates {
    std::uint32_t blend_enabled,blend_op,write_mask;
    std::uint32_t depth_enabled,depth_write,depth_func,stencil_enabled;
    std::uint32_t stencil_read,stencil_write,stencil_fail,stencil_depth_fail,
        stencil_pass,stencil_func;
    std::uint32_t fill,cull,depth_clip,topology;
};
void a2fo_draw_states(const std::uint32_t* render,std::uint32_t primitive,
    A2foDrawStates* out);
void a2fo_draw_rgba(std::uint32_t value,float* out);
bool a2fo_draw_view_dimension(std::uint32_t dimension);
bool a2fo_draw_view_owner(bool same);
std::uint32_t a2fo_draw_normal_mode(bool normal_xy,std::uint32_t format);
void a2fo_draw_finish_stages(std::uint32_t* stages,std::uint32_t active);
void a2fo_draw_pixel_constants(const std::uint32_t* render,
    std::uint32_t* control,float* fog,float* fog_color,float* factor);
struct A2foDrawFixedInput {
    const float *world,*view,*projection,*textures;
    const float *material_diffuse,*material_ambient,*material_specular,*material_emissive;
    float material_power;
    const std::uint32_t *render,*stages,*tex_components;
    std::uint32_t x,y,width,height;
    bool transformed,has_normal,has_diffuse,has_specular;
    std::uint32_t active;
};
struct alignas(16) A2foDrawFixed {
    float world[16],view[16],projection[16],normal[16],texture[8][16];
    float material_diffuse[4],material_ambient[4],material_specular[4],material_emissive[4];
    float global_ambient[4],camera[4],viewport[4],fog[4];
    float light_diffuse[8][4],light_ambient[8][4],light_specular[8][4];
    float light_position[8][4],light_direction[8][4],light_attenuation[8][4];
    std::uint32_t flags[4],material_source[4],light_type[8][4],texcoord[8][4];
};
bool a2fo_draw_fixed_base(const A2foDrawFixedInput* input,A2foDrawFixed* out);
struct A2foDrawLight {
    bool enabled;std::uint32_t type;
    float diffuse[4],ambient[4],specular[4],position[3],direction[3];
    float attenuation0,attenuation1,attenuation2,range;
};
bool a2fo_draw_fixed_lights(const A2foDrawLight* lights,A2foDrawFixed* out);
bool a2fo_draw_alpha_note(std::uint32_t color_op,std::uint32_t alpha_op,
    bool has_log,std::size_t note_count);
bool a2fo_draw_alias_step(bool prior,bool source_present,bool same_color,bool same_depth);
std::uint32_t a2fo_draw_target_admit(bool has_color,bool has_depth,
    bool need_depth,bool aliases);
bool a2fo_draw_log_admit(bool has_log,std::size_t warning_count);
bool a2fo_draw_log_limit(std::size_t warning_count);
bool a2fo_draw_cache_evict(std::size_t count,std::size_t limit);
bool a2fo_draw_inverse3(const float* matrix,float* out);
void a2fo_draw_camera(const float* view,const float* inverse,float* out);
typedef std::int32_t (*A2foDrawOperation)(void*,std::uint32_t);
std::int32_t a2fo_draw_prepare_workflow(void* context,bool fixed_function,
    A2foDrawOperation operation);
std::int32_t a2fo_draw_buffer_workflow(void* context,bool needs_create,
    A2foDrawOperation operation);
std::int32_t a2fo_draw_program_workflow(void* context,A2foDrawOperation operation);
std::int32_t a2fo_draw_pixel_workflow(void* context,A2foDrawOperation operation);
std::int32_t a2fo_draw_state_workflow(void* context,A2foDrawOperation operation);
void a2fo_draw_raster_inverse(std::uint32_t width,std::uint32_t height,float* out);
std::int32_t a2fo_draw_initialize_workflow(void* context,bool has_device,
    bool has_context,A2foDrawOperation operation);
std::int32_t a2fo_draw_dispatch_workflow(void* context,A2foDrawOperation operation);
void a2fo_draw_viewport(std::uint32_t x,std::uint32_t y,
    std::uint32_t width,std::uint32_t height,float* out);
}
