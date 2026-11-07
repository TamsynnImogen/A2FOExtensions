// Full frozen/current DrawPipeline translation unit with synthetic COM only.
#include A2FO_DRAW_SOURCE
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

namespace a2fo::renderer {
static HRESULT shader_result=E_FAIL;
static game::State* mutation_state=nullptr;
static bool mutate_compile=false,mutate_alpha_note=false;
HRESULT compile_shader(const ShaderCompileRequest& request,const std::string&,ShaderCompileResult& result){
    if(mutate_compile&&mutation_state&&request.entry=="VSMain"){
        mutate_compile=false;
        mutation_state->render[27]=1;
        mutation_state->render[136]=1;
        mutation_state->render[139]=0x10203040;
        mutation_state->render[19]=3; // Blend factors were cached before compile.
        mutation_state->stage[0][11]=1;
        std::puts("compile-mutated-state");
    }
    result.bytes={1,2,3,4};
    result.diagnostics="fixture compile failed";
    std::printf("compile|%s|%s\n",request.entry.c_str(),request.target.c_str());
    return shader_result;
}
namespace shaders {std::string vs11_output_interface(){return {};}}
}
namespace a2fo::renderer::game {
struct MockCom {void** methods;ULONG refs=1;explicit MockCom(void** table):methods(table){}};
struct MockBuffer:MockCom {std::vector<BYTE> bytes;MockBuffer(void** table,UINT size):MockCom(table),bytes(size){}};
static unsigned calls=0,logs=0;
static bool blend_failure=true;
static std::vector<std::unique_ptr<MockBuffer>> buffers;
static std::vector<std::uint64_t> written_hashes;
static MockCom *shader_object=nullptr,*pixel_object=nullptr,*layout_object=nullptr;
static MockCom *blend_object=nullptr,*depth_object=nullptr,*raster_object=nullptr,*sampler_object=nullptr;
static void** buffer_methods=nullptr;
static bool color_bound=false;
static MockCom* color_object=nullptr;
static MockCom* resource_object=nullptr;
static ULONG STDMETHODCALLTYPE add_ref(void* self){return ++static_cast<MockCom*>(self)->refs;}
static ULONG STDMETHODCALLTYPE release_ref(void* self){return --static_cast<MockCom*>(self)->refs;}
static void STDMETHODCALLTYPE get_resource(void*,ID3D11Resource** out){
    *out=reinterpret_cast<ID3D11Resource*>(resource_object);add_ref(resource_object);
}
static void STDMETHODCALLTYPE get_targets(void*,UINT,ID3D11RenderTargetView** color,
                                         ID3D11DepthStencilView** depth){
    ++calls;*color=color_bound?reinterpret_cast<ID3D11RenderTargetView*>(color_object):nullptr;
    if(*color)add_ref(color_object);
    *depth=nullptr;
}
static void log_message(const std::string& message){
    ++logs;
    if(a2fo::renderer::mutate_alpha_note&&a2fo::renderer::mutation_state&&
       message.find("Legacy alpha-disable draw admitted")!=std::string::npos){
        a2fo::renderer::mutate_alpha_note=false;
        auto& stage=a2fo::renderer::mutation_state->stage[1];
        stage[1]=2;stage[4]=1;stage[28]=1;
        std::puts("alpha-note-mutated-next-stage");
    }
}
static HRESULT STDMETHODCALLTYPE create_buffer(void*,const D3D11_BUFFER_DESC* desc,
    const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer** out){
    buffers.push_back(std::make_unique<MockBuffer>(buffer_methods,desc->ByteWidth));
    *out=reinterpret_cast<ID3D11Buffer*>(buffers.back().get());
    std::printf("create-buffer|%u|%u\n",desc->ByteWidth,desc->BindFlags);
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE map_buffer(void*,ID3D11Resource* resource,UINT,D3D11_MAP,
    UINT,D3D11_MAPPED_SUBRESOURCE* mapped){
    auto* buffer=reinterpret_cast<MockBuffer*>(resource);
    mapped->pData=buffer->bytes.data();mapped->RowPitch=0;mapped->DepthPitch=0;
    std::printf("map|%u\n",unsigned(buffer->bytes.size()));return S_OK;
}
static void STDMETHODCALLTYPE unmap_buffer(void*,ID3D11Resource* resource,UINT){
    const auto* buffer=reinterpret_cast<MockBuffer*>(resource);
    std::uint64_t hash=14695981039346656037ull;
    for(BYTE b:buffer->bytes)hash=(hash^b)*1099511628211ull;
    written_hashes.push_back(hash);std::printf("unmap|%016llx\n",static_cast<unsigned long long>(hash));
}
static HRESULT STDMETHODCALLTYPE create_vs(void*,const void*,SIZE_T,ID3D11ClassLinkage*,
    ID3D11VertexShader** out){*out=reinterpret_cast<ID3D11VertexShader*>(shader_object);add_ref(shader_object);return S_OK;}
static HRESULT STDMETHODCALLTYPE create_ps(void*,const void*,SIZE_T,ID3D11ClassLinkage*,
    ID3D11PixelShader** out){*out=reinterpret_cast<ID3D11PixelShader*>(pixel_object);add_ref(pixel_object);return S_OK;}
static HRESULT STDMETHODCALLTYPE create_layout(void*,const D3D11_INPUT_ELEMENT_DESC*,UINT,
    const void*,SIZE_T,ID3D11InputLayout** out){*out=reinterpret_cast<ID3D11InputLayout*>(layout_object);add_ref(layout_object);return S_OK;}
static HRESULT STDMETHODCALLTYPE create_blend(void*,const D3D11_BLEND_DESC* desc,ID3D11BlendState** out){
    std::printf("blend-desc|%u|%u|%u|%u\n",unsigned(desc->RenderTarget[0].BlendEnable),
        unsigned(desc->RenderTarget[0].SrcBlend),unsigned(desc->RenderTarget[0].DestBlend),
        unsigned(desc->RenderTarget[0].RenderTargetWriteMask));
    if(blend_failure)return E_FAIL;
    *out=reinterpret_cast<ID3D11BlendState*>(blend_object);add_ref(blend_object);return S_OK;
}
static HRESULT STDMETHODCALLTYPE create_depth(void*,const D3D11_DEPTH_STENCIL_DESC* desc,
    ID3D11DepthStencilState** out){
    std::printf("depth-desc|%u|%u|%u\n",unsigned(desc->DepthEnable),unsigned(desc->DepthFunc),unsigned(desc->StencilEnable));
    *out=reinterpret_cast<ID3D11DepthStencilState*>(depth_object);add_ref(depth_object);return S_OK;
}
static HRESULT STDMETHODCALLTYPE create_raster(void*,const D3D11_RASTERIZER_DESC* desc,
    ID3D11RasterizerState** out){
    std::printf("raster-desc|%u|%u|%u\n",unsigned(desc->FillMode),unsigned(desc->CullMode),unsigned(desc->DepthClipEnable));
    *out=reinterpret_cast<ID3D11RasterizerState*>(raster_object);add_ref(raster_object);return S_OK;
}
static HRESULT STDMETHODCALLTYPE create_sampler(void*,const D3D11_SAMPLER_DESC* desc,
    ID3D11SamplerState** out){
    std::printf("sampler-desc|%u|%u|%u\n",unsigned(desc->Filter),unsigned(desc->AddressU),unsigned(desc->AddressV));
    *out=reinterpret_cast<ID3D11SamplerState*>(sampler_object);add_ref(sampler_object);return S_OK;
}
static void STDMETHODCALLTYPE set_viewport(void*,UINT,const D3D11_VIEWPORT*){std::puts("set-viewport");}
static void STDMETHODCALLTYPE set_input(void*,ID3D11InputLayout*){std::puts("set-input");}
static void STDMETHODCALLTYPE set_vb(void*,UINT,UINT,ID3D11Buffer*const*,const UINT*,const UINT*){std::puts("set-vb");}
static void STDMETHODCALLTYPE set_ib(void*,ID3D11Buffer*,DXGI_FORMAT,UINT){std::puts("set-ib");}
static void STDMETHODCALLTYPE set_topology(void*,D3D11_PRIMITIVE_TOPOLOGY topology){std::printf("topology|%u\n",unsigned(topology));}
static void STDMETHODCALLTYPE set_shader(void*,void*,void*const*,UINT){std::puts("set-shader");}
static void STDMETHODCALLTYPE set_constants(void*,UINT,UINT,ID3D11Buffer*const*){std::puts("set-constants");}
static void STDMETHODCALLTYPE set_views(void*,UINT,UINT,ID3D11ShaderResourceView*const*){std::puts("set-views");}
static void STDMETHODCALLTYPE set_samplers(void*,UINT,UINT,ID3D11SamplerState*const*){std::puts("set-samplers");}
static void STDMETHODCALLTYPE set_raster(void*,ID3D11RasterizerState*){std::puts("set-raster");}
static void STDMETHODCALLTYPE set_blend(void*,ID3D11BlendState*,const FLOAT*,UINT){std::puts("set-blend");}
static void STDMETHODCALLTYPE set_depth(void*,ID3D11DepthStencilState*,UINT){std::puts("set-depth");}
static void STDMETHODCALLTYPE draw_indexed(void*,UINT count,UINT,INT){std::printf("draw-indexed|%u\n",count);}
struct DrawFixture {
    static void ready(DrawPipeline& p,MockCom& context){
        p.impl_=std::make_unique<DrawPipeline::Impl>();
        p.impl_->context=reinterpret_cast<ID3D11DeviceContext*>(&context);
        p.impl_->owner=GetCurrentThreadId();p.impl_->log=&log_message;
    }
    static void device(DrawPipeline& p,MockCom& device){
        p.impl_->device=reinterpret_cast<ID3D11Device*>(&device);
    }
    static void clear_notes(DrawPipeline& p){p.impl_->alpha_notes.clear();}
};
}
using namespace a2fo::renderer::game;
int main(){
    void* context_table[120]{};void* device_table[64]{};void* buffer_table[16]{};
    void* color_table[16]{};void* resource_table[16]{};void* state_table[16]{};
    for(auto* table:{context_table,device_table,buffer_table,color_table,resource_table,state_table}){
        table[1]=reinterpret_cast<void*>(&add_ref);table[2]=reinterpret_cast<void*>(&release_ref);
    }
    buffer_methods=buffer_table;
    context_table[89]=reinterpret_cast<void*>(&get_targets);
    context_table[14]=reinterpret_cast<void*>(&map_buffer);
    context_table[15]=reinterpret_cast<void*>(&unmap_buffer);
    context_table[44]=reinterpret_cast<void*>(&set_viewport);
    context_table[17]=reinterpret_cast<void*>(&set_input);
    context_table[18]=reinterpret_cast<void*>(&set_vb);
    context_table[19]=reinterpret_cast<void*>(&set_ib);
    context_table[24]=reinterpret_cast<void*>(&set_topology);
    for(int index:{9,11,23,60,64})context_table[index]=reinterpret_cast<void*>(&set_shader);
    for(int index:{7,16})context_table[index]=reinterpret_cast<void*>(&set_constants);
    context_table[8]=reinterpret_cast<void*>(&set_views);
    context_table[10]=reinterpret_cast<void*>(&set_samplers);
    context_table[43]=reinterpret_cast<void*>(&set_raster);
    context_table[35]=reinterpret_cast<void*>(&set_blend);
    context_table[36]=reinterpret_cast<void*>(&set_depth);
    context_table[12]=reinterpret_cast<void*>(&draw_indexed);
    device_table[3]=reinterpret_cast<void*>(&create_buffer);
    device_table[11]=reinterpret_cast<void*>(&create_layout);
    device_table[12]=reinterpret_cast<void*>(&create_vs);
    device_table[15]=reinterpret_cast<void*>(&create_ps);
    device_table[20]=reinterpret_cast<void*>(&create_blend);
    device_table[21]=reinterpret_cast<void*>(&create_depth);
    device_table[22]=reinterpret_cast<void*>(&create_raster);
    device_table[23]=reinterpret_cast<void*>(&create_sampler);
    color_table[7]=reinterpret_cast<void*>(&get_resource);
    MockCom context(context_table),device(device_table),color(color_table),resource(resource_table);
    MockCom vertex_shader(state_table),pixel_shader(state_table),layout(state_table),
        blend(state_table),depth(state_table),raster(state_table),sampler(state_table);
    shader_object=&vertex_shader;pixel_object=&pixel_shader;layout_object=&layout;
    blend_object=&blend;depth_object=&depth;raster_object=&raster;sampler_object=&sampler;
    color_object=&color;resource_object=&resource;
    DrawPipeline pipeline;DrawFixture::ready(pipeline,context);
    State state;state.width=8;state.height=8;
    auto& r=state.render;r[22]=1;r[23]=8;r[25]=8;r[171]=1;r[168]=15;
    r[8]=3;r[9]=2;r[19]=2;r[20]=1;
    auto& s=state.stage[0];s[1]=2;s[4]=1;s[28]=1;
    state.stage[1][1]=1;
    Draw draw;draw.vertices.resize(3);draw.indices={0,1,2};
    auto show=[&](const char* name){
        const HRESULT hr=pipeline.draw(state,draw,{});
        std::printf("%s|%08x|calls=%u|logs=%u\n",name,
            unsigned(hr),calls,logs);
    };
    state.width=0;show("viewport");state.width=8;
    draw.primitive=0;show("topology");draw.primitive=4;
    draw.vertices.clear();draw.indices.clear();show("empty");
    draw.indices={0,1,2};show("no-vertices");draw.vertices.resize(3);
    draw.vertices[0].v[0][0]=std::numeric_limits<float>::infinity();show("nonfinite");
    draw.vertices[0].v[0][0]=0;
    state.transformed=true;draw.vertices[0].v[0][3]=0;show("rhw");
    state.transformed=false;draw.indices={0,1,99};show("index-range");
    draw.indices={0,1};draw.primitive=6;show("fan-count");draw.primitive=4;
    show("primitive-count");draw.indices={0,1,2};
    r[22]=0;show("render");r[22]=1;
    r[52]=1;r[53]=0;show("stencil");r[52]=0;
    r[8]=1;show("raster");r[8]=3;
    r[152]=1;show("clip");r[152]=0;
    r[19]=0;show("blend");r[19]=2;
    s[1]=0;show("stage");s[1]=2;
    s[4]=18;show("alpha-stage");s[4]=1;
    s[11]=0x10000;state.transformed=true;show("texcoord-source");
    state.transformed=false;s[11]=0;
    a2fo::renderer::mutation_state=&state;
    a2fo::renderer::mutate_alpha_note=true;DrawFixture::clear_notes(pipeline);
    show("alpha-note-mutation");state.stage[1][1]=1;
    show("no-target");
    color_bound=true;r[7]=1;show("no-depth");r[7]=0;
    DrawFixture::device(pipeline,device);
    show("shader-fail");
    a2fo::renderer::shader_result=S_OK;
    a2fo::renderer::mutate_compile=true;
    r[137]=1;state.has_normal=true;
    show("singular-world");
    for(int axis=0;axis<4;++axis)state.world.m[axis*4+axis]=1;
    show("singular-view");
    for(int axis=0;axis<4;++axis)state.view.m[axis*4+axis]=1;
    show("blend-fail");
    blend_failure=false;show("draw-success");
    draw.primitive=6;draw.indices={0,1,2,0};show("fan-success");
    std::printf("refs|%lu,%lu,%lu|buffers=%u|writes=%u\n",context.refs,color.refs,resource.refs,
        unsigned(buffers.size()),unsigned(written_hashes.size()));
}
