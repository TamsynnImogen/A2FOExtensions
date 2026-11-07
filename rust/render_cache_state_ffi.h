#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2FoRenderSpan {const std::uint8_t* data;std::size_t size;};
struct A2FoShaderInput {
    A2FoRenderSpan source,entry,target,directory;
    std::uint32_t flags;
    std::int32_t invalid_arg,fail,out_of_memory;
    std::uint32_t already_exists;
    std::size_t max_vector_size;
};
struct A2FoShaderReply {
    std::int32_t hr;
    std::uint32_t value,extra;
    std::int64_t size64,clock_start,clock_end,frequency;
    std::uint64_t file_size,file_time;
    A2FoRenderSpan first,second;
};
struct A2FoShaderPublish {
    A2FoRenderSpan bytes,diagnostics;
    std::uint64_t compile_us;
    std::uint32_t cache_hit,cache_written;
};
using A2FoShaderCall = int (*)(void*,std::uint32_t,A2FoRenderSpan,
                                A2FoShaderReply*) noexcept;
std::int32_t a2fo_rs_shader_compile(const A2FoShaderInput*,
                                    void*,A2FoShaderCall) noexcept;
}

enum : std::uint32_t {
 A2FO_SHADER_LOAD_47=1,A2FO_SHADER_LOAD_43=2,A2FO_SHADER_GET_PROC=3,
 A2FO_SHADER_IDENTITY=4,A2FO_SHADER_OPEN_READ=5,A2FO_SHADER_FILE_SIZE=6,
 A2FO_SHADER_READ=7,A2FO_SHADER_CLOSE_READ=8,A2FO_SHADER_COMPILE=9,
 A2FO_SHADER_CREATE_DIRECTORY=10,A2FO_SHADER_SUFFIX=11,
 A2FO_SHADER_OPEN_TEMP=12,A2FO_SHADER_WRITE=13,A2FO_SHADER_FLUSH=14,
 A2FO_SHADER_CLOSE_TEMP=15,A2FO_SHADER_MOVE=16,A2FO_SHADER_DELETE=17,
 A2FO_SHADER_PUBLISH=18,
};

extern "C" {
struct A2FoBcMip {std::uint32_t width,height,row_pitch,slice_pitch;std::size_t offset;};
struct A2FoBcEntryView {
 A2FoRenderSpan name;std::uint32_t width,height,mips,format,normal;
 std::uint64_t top_hash,file_hash;const std::uint64_t* mip_hashes;
};
struct A2FoBcEntry {
 A2FoRenderSpan name;std::uint32_t width,height,mips,format,normal;
 std::uint64_t top_hash,file_hash,mip_hashes[15];
};
struct A2FoBcPublication {A2FoRenderSpan directory,identity;};
struct A2FoBcReadRequest {A2FoRenderSpan path;std::size_t max_size;};
struct A2FoBcPublishImage {A2FoRenderSpan name;std::uint32_t normal;};
struct A2FoBcImageView {
 std::int32_t hr;std::uint32_t format,width,height,alpha;
 std::size_t mip_count;A2FoBcMip mips[15];
 A2FoRenderSpan bytes,parse_error;
};
struct A2FoBcChain {
 A2FoRenderSpan bytes,name;
 std::uint32_t format,width,height,alpha,normal;
 std::size_t mip_count;A2FoBcMip mips[15];
};
struct A2FoBcReply {std::uint32_t value;A2FoRenderSpan span;A2FoBcImageView image;};
using A2FoBcCall = int (*)(void*,std::uint32_t,A2FoRenderSpan,
                            A2FoBcReply*) noexcept;
int a2fo_rs_bc_load(A2FoRenderSpan modroot,int enabled,A2FoRenderSpan root_identity,
                     void*,A2FoBcCall) noexcept;
int a2fo_rs_bc_asset(A2FoRenderSpan filename,A2FoRenderSpan root,
                      const A2FoBcEntryView*,std::size_t,void*,A2FoBcCall) noexcept;
int a2fo_rs_bc_transport(A2FoRenderSpan filename,A2FoRenderSpan root,
                          const A2FoBcEntryView*,std::size_t,void*,A2FoBcCall) noexcept;
int a2fo_rs_bc_match(std::uint32_t width,std::uint32_t height,std::uint32_t mips,
                      A2FoRenderSpan base,A2FoRenderSpan root,
                      const A2FoBcEntryView*,std::size_t,void*,A2FoBcCall) noexcept;
}
enum : std::uint32_t {
 A2FO_BC_RESET=101,A2FO_BC_OPEN_CATALOG=102,A2FO_BC_NEXT_LINE=103,
 A2FO_BC_UNIQUE_NAME=104,A2FO_BC_STAGE_ENTRY=105,A2FO_BC_PUBLISH_LOAD=106,
 A2FO_BC_SET_ERROR=107,A2FO_BC_READ_CANONICAL=108,A2FO_BC_PARSE_DDS=109,
 A2FO_BC_PUBLISH_IMAGE=110,A2FO_BC_PUBLISH_PATH=111,A2FO_BC_PUBLISH_CHAIN=112,
};

extern "C" {
struct A2FoStateMask {
 std::uint8_t render[256],stage[8][33],transforms[11],textures[8],lights[8],
              streams[16],constants[96];
 std::uint8_t viewport,material,indices,vertex_shader,pixel_shader,clip;
};
struct A2FoStateReply {
 std::int32_t hr,src_pitch,dst_pitch;
 std::uint32_t value,other,width,height,dst_width,dst_height,format,levels;
 std::int32_t left,top,right,bottom,x,y;
 void *src_bits,*dst_bits;
 A2FoStateMask mask;
};
using A2FoStateCall = int (*)(void*,std::uint32_t,std::uint32_t,std::uint32_t,
                              A2FoStateReply*) noexcept;
std::int32_t a2fo_rs_state_run(std::uint32_t method,std::uint32_t arg0,
                               std::uint32_t arg1,void*,A2FoStateCall) noexcept;
}
enum : std::uint32_t {
 A2FO_STATE_BEGIN=1,A2FO_STATE_END,A2FO_STATE_APPLY,A2FO_STATE_CAPTURE,
 A2FO_STATE_DELETE,A2FO_STATE_CREATE,A2FO_STATE_SET_CLIP_STATUS,
 A2FO_STATE_GET_CLIP_STATUS,A2FO_STATE_SET_CLIP_PLANE,
 A2FO_STATE_GET_CLIP_PLANE,A2FO_STATE_UPDATE_TEXTURE,A2FO_STATE_COPY_RECTS,
 A2FO_STATE_GET_FRONT_BUFFER,
 A2FO_STATE_QUERY=101,A2FO_STATE_MAKE_RECORDING,A2FO_STATE_CLEAR_BINDING,
 A2FO_STATE_SET_NEXT,A2FO_STATE_EMPLACE_RECORDING,A2FO_STATE_MAKE_CREATED,
 A2FO_STATE_SET_ALL_MASK,A2FO_STATE_EMPLACE_CREATED,A2FO_STATE_COPY_FIELD,
 A2FO_STATE_ERASE,A2FO_STATE_SET_CLIP,A2FO_STATE_GET_CLIP,
 A2FO_STATE_TEXTURE_INFO,A2FO_STATE_TEXTURE_LOCKED,A2FO_STATE_TEXTURE_COPY,
 A2FO_STATE_TEXTURE_UPLOAD,A2FO_STATE_SURFACE_INFO,A2FO_STATE_COPY_TARGET,
 A2FO_STATE_LOCK_SOURCE,A2FO_STATE_LOCK_DEST,A2FO_STATE_UNLOCK_SOURCE,
 A2FO_STATE_UNLOCK_DEST,A2FO_STATE_RECT,A2FO_STATE_FAIL,
};
enum : std::uint32_t {
 A2FO_STATE_FIELD_RENDER=1,A2FO_STATE_FIELD_STAGE,A2FO_STATE_FIELD_TEXTURE,
 A2FO_STATE_FIELD_LIGHT,A2FO_STATE_FIELD_TRANSFORM,A2FO_STATE_FIELD_MATERIAL,
 A2FO_STATE_FIELD_VIEWPORT,A2FO_STATE_FIELD_STREAM,A2FO_STATE_FIELD_INDICES,
 A2FO_STATE_FIELD_VERTEX_SHADER,A2FO_STATE_FIELD_PIXEL_SHADER,
 A2FO_STATE_FIELD_CONSTANT,A2FO_STATE_FIELD_CLIP,
};
