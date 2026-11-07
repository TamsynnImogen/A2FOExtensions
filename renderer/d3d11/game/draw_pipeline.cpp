#include "contracts.hpp"
#include "../../../rust/game_draw_ffi.h"
#include "../../shaders/bytecode_cache.hpp"
#include "../../shaders/bc_normals.hpp"
#include "../../shaders/vs11.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <exception>
#include <map>
#include <new>
#include <set>
#include <string>

namespace a2fo::renderer::game {
namespace {
template<class T> void drop(T*& p) { if(p){p->Release();p=nullptr;} }
template<class T> std::string key(const T& v) {return std::string(reinterpret_cast<const char*>(&v),sizeof(v));}
template<class T> void clear(std::map<std::string,T*>& m) {for(auto& v:m)drop(v.second);m.clear();}
constexpr unsigned max_states=256;
float bits(DWORD v){float f;std::memcpy(&f,&v,4);return f;}

struct alignas(16) FfConstants {
    float world[16],view[16],projection[16],normal[16],texture[8][16];
    float material_diffuse[4],material_ambient[4],material_specular[4],material_emissive[4];
    float global_ambient[4],camera[4],viewport[4],fog[4];
    float light_diffuse[8][4],light_ambient[8][4],light_specular[8][4];
    float light_position[8][4],light_direction[8][4],light_attenuation[8][4];
    unsigned flags[4], material_source[4],light_type[8][4],texcoord[8][4];
};
struct alignas(16) PsConstants {
    unsigned stage[8][16]; // one 64-byte register group per texture stage
    unsigned control[4]; // alpha test/ref/function, specular
    float fog[4]; // start,end,density,mode
    float fog_color[4],factor[4];
};

// These sources are original compatibility shaders; stage parameters retain D3D8 numeric values.
constexpr const char* ff_vs=R"hlsl(
cbuffer Ff : register(b2) {
 row_major float4x4 world,view,projection,normal_matrix;
 row_major float4x4 tex_matrix[8];
 float4 md,ma,ms,me,global_ambient,camera,viewport,fog;
 float4 ld[8],la[8],ls[8],lp[8],ldir[8],latten[8];
 uint4 flags,material_source;uint4 light_type[8],texcoord[8];
};
struct VSInput {float4 v0:TEXCOORD0;float4 v1:TEXCOORD1;float4 v2:TEXCOORD2;float4 v3:TEXCOORD3;
float4 v4:TEXCOORD4;float4 v5:TEXCOORD5;float4 v6:TEXCOORD6;float4 v7:TEXCOORD7;
float4 v8:TEXCOORD8;float4 v9:TEXCOORD9;float4 v10:TEXCOORD10;float4 v11:TEXCOORD11;};
struct VSOutput {float4 position:SV_Position;float4 diffuse:COLOR0;float4 specular:COLOR1;
float4 tex0:TEXCOORD0;float4 tex1:TEXCOORD1;float4 tex2:TEXCOORD2;float4 tex3:TEXCOORD3;
float4 tex4:TEXCOORD4;float4 tex5:TEXCOORD5;float4 tex6:TEXCOORD6;float4 tex7:TEXCOORD7;
float fog:FOG;};
float4 select_uv(VSInput i,uint n){if(n==0)return i.v4;if(n==1)return i.v5;if(n==2)return i.v6;
if(n==3)return i.v7;if(n==4)return i.v8;if(n==5)return i.v9;if(n==6)return i.v10;return i.v11;}
float4 material_color(uint source,float4 material,float4 diffuse,float4 specular){
 return source==1 ? diffuse : source==2 ? specular : material;
}
VSOutput VSMain(VSInput i){
 VSOutput o=(VSOutput)0;
 float4 wp=float4(0,0,0,1),vp=wp;
 if(flags.x!=0){
   float rhw=max(i.v0.w,1e-20);
   float2 ndc=float2(2*(i.v0.x+.5-viewport.x)/viewport.z-1,1-2*(i.v0.y+.5-viewport.y)/viewport.w);
   o.position=float4(ndc/rhw,i.v0.z/rhw,1/rhw);
 } else {
   wp=mul(float4(i.v0.xyz,1),world);vp=mul(wp,view);o.position=mul(vp,projection);
 }
 float4 vertex_diffuse=flags.z!=0 ? i.v2 : float4(1,1,1,1);
 float4 vertex_specular=flags.w!=0 ? i.v3 : 0;
 float4 diffuse=material_color(material_source.x,md,vertex_diffuse,vertex_specular);
 float4 ambient=material_color(material_source.y,ma,vertex_diffuse,vertex_specular);
 float4 specular=material_color(material_source.z,ms,vertex_diffuse,vertex_specular);
 float4 emissive=material_color(material_source.w,me,vertex_diffuse,vertex_specular);
 o.diffuse=vertex_diffuse;o.specular=vertex_specular;
 if(flags.y!=0){
   float3 n=mul(float4(i.v1.xyz,0),normal_matrix).xyz;
   if(camera.w!=0)n=normalize(n);
   float3 eye=normalize(camera.xyz-wp.xyz);
   float4 sum=emissive+ambient*global_ambient;
   float3 shine=0;
   [unroll]for(uint l=0;l<8;++l){
      if(light_type[l].x==0)continue;
      float3 delta=lp[l].xyz-wp.xyz;
      float dist=length(delta);
      float3 ray=light_type[l].x==3 ? normalize(-ldir[l].xyz) : delta/max(dist,1e-20);
      float attenuation=light_type[l].x==3 ? 1 :
       (dist>latten[l].w ? 0 : 1/max(latten[l].x+latten[l].y*dist+latten[l].z*dist*dist,1e-20));
      float ndl=saturate(dot(n,ray));
      sum+=attenuation*(ambient*la[l]+diffuse*ld[l]*ndl);
      if(ndl>0&&ms.a>=0){float3 h=normalize(ray+eye);shine+=attenuation*ls[l].rgb*pow(saturate(dot(n,h)),max(ms.a,0.0001));}
   }
   o.diffuse=saturate(float4(sum.rgb,diffuse.a));o.specular=float4(saturate(shine*specular.rgb),vertex_specular.a);
 }
 float4 uv[8]={i.v4,i.v5,i.v6,i.v7,i.v8,i.v9,i.v10,i.v11};
 [unroll]for(uint t=0;t<8;++t){
   uint source=texcoord[t].x;float4 coord=select_uv(i,source&7);
   if((source&0xffff0000u)==0){uint components=texcoord[t].z;
     if(components==0)coord=float4(0,0,0,1);
     else if(components==1)coord=float4(coord.x,0,0,1);
     else if(components==2)coord=float4(coord.xy,0,1);
     else if(components==3)coord=float4(coord.xyz,1);
   }
   float3 camera_normal=normalize(mul(float4(mul(float4(i.v1.xyz,0),normal_matrix).xyz,0),view).xyz);
   if((source&0xffff0000u)==0x00010000u)coord=float4(camera_normal,1);
   else if((source&0xffff0000u)==0x00020000u)coord=float4(vp.xyz,1);
   else if((source&0xffff0000u)==0x00030000u)coord=float4(reflect(normalize(vp.xyz),camera_normal),1);
   if(texcoord[t].y!=0){coord=mul(coord,tex_matrix[t]);
     if((texcoord[t].y&255)==1)coord.y=0;}
   uv[t]=coord;
 }
 o.tex0=uv[0];o.tex1=uv[1];o.tex2=uv[2];o.tex3=uv[3];
 o.tex4=uv[4];o.tex5=uv[5];o.tex6=uv[6];o.tex7=uv[7];
 float distance=abs(vp.z);
 o.fog=fog.w==3 ? saturate((fog.y-distance)/max(fog.y-fog.x,1e-20)) :
       fog.w==1 ? saturate(exp(-fog.z*distance)) :
       fog.w==2 ? saturate(exp(-pow(fog.z*distance,2))) : 1;
 if(flags.x!=0)o.fog=flags.w!=0 ? i.v3.a : 1;
 return o;
}
)hlsl";

const std::string ps_head=R"hlsl(
struct Stage {uint4 c,a,m,x;};
cbuffer PixelState : register(b3){Stage stages[8];
 uint4 control;float4 fog;float4 fog_color;float4 factor;};
Texture2D tex0:register(t0);Texture2D tex1:register(t1);Texture2D tex2:register(t2);Texture2D tex3:register(t3);
Texture2D tex4:register(t4);Texture2D tex5:register(t5);Texture2D tex6:register(t6);Texture2D tex7:register(t7);
SamplerState samp0:register(s0);SamplerState samp1:register(s1);SamplerState samp2:register(s2);SamplerState samp3:register(s3);
SamplerState samp4:register(s4);SamplerState samp5:register(s5);SamplerState samp6:register(s6);SamplerState samp7:register(s7);
float4 sample_tex(uint n,float2 uv){switch(n){case 0:return tex0.Sample(samp0,uv);case 1:return tex1.Sample(samp1,uv);
case 2:return tex2.Sample(samp2,uv);case 3:return tex3.Sample(samp3,uv);case 4:return tex4.Sample(samp4,uv);
case 5:return tex5.Sample(samp5,uv);case 6:return tex6.Sample(samp6,uv);default:return tex7.Sample(samp7,uv);}}
float4 argument(uint code,float4 current,float4 diffuse,float4 texel,float4 specular,float4 temp){
 uint base=code&15;float4 a=base==0?diffuse:base==1?current:base==2?texel:base==3?factor:base==4?specular:temp;
 if((code&16)!=0)a=1-a;if((code&32)!=0)a=a.aaaa;return a;
}
float4 combine(uint op,float4 a,float4 b,float4 c,float4 current,float4 diffuse,float4 texel){
 if(op==2)return a;if(op==3)return b;if(op==4)return a*b;if(op==5)return 2*a*b;
 if(op==6)return 4*a*b;if(op==7)return a+b;if(op==8)return a+b-.5;if(op==9)return 2*(a+b-.5);
 if(op==10)return a-b;if(op==11)return a+b*(1-a);
 if(op==12)return lerp(b,a,diffuse.a);if(op==13)return lerp(b,a,texel.a);
 if(op==14)return lerp(b,a,factor.a);if(op==15)return a+b*(1-texel.a);
 if(op==16)return lerp(b,a,current.a);
 if(op==18)return float4(a.rgb+b.rgb*a.a,a.a);if(op==19)return float4(a.rgb*b.rgb+a.aaa,b.a);
 if(op==20)return float4(a.rgb+b.rgb*(1-a.a),a.a);
 if(op==21)return float4((1-a.rgb)*b.rgb+a.aaa,a.a);
 if(op==24){float value=4*dot(a.rgb-.5,b.rgb-.5);return float4(value,value,value,value);}
 if(op==25)return c+a*b;if(op==26)return c*a+(1-c)*b;return 0;
}
bool passes(uint value,uint reference,uint f){return f==8||(f==2&&value<reference)||(f==3&&value==reference)||
 (f==4&&value<=reference)||(f==5&&value>reference)||(f==6&&value!=reference)||(f==7&&value>=reference);}
)hlsl";

const std::string ps_body=R"hlsl(
float4 PSMain(VSOutput i):SV_Target {
 float4 current=i.diffuse,temp=0;
 float4 uv[8]={i.tex0,i.tex1,i.tex2,i.tex3,i.tex4,i.tex5,i.tex6,i.tex7};
 [unroll]for(uint n=0;n<8;++n){
   uint4 c=stages[n].c,a=stages[n].a,m=stages[n].m;if(c.x==1)break;
   float4 texel=1;
   if(m.x!=0){float2 sample_uv=uv[n].xy;
     if((stages[n].x.x&256)!=0){uint count=stages[n].x.x&255;
       float divisor=count==2?uv[n].y:count==3?uv[n].z:uv[n].w;
       divisor=(divisor<0?-1:1)*max(abs(divisor),1e-20);
       sample_uv=float2(uv[n].x/divisor,count==2?0:uv[n].y/divisor);
     }
     texel=sample_tex(n,sample_uv);if(m.y!=0){float2 xy=m.y==2?texel.xy:texel.xy*2-1;
      texel=float4(a2fo_reconstruct_bc5(xy)*.5+.5,1);}}
   float4 ca=argument(c.y,current,i.diffuse,texel,i.specular,temp);
   float4 cb=argument(c.z,current,i.diffuse,texel,i.specular,temp);
   float4 cc=argument(a.w,current,i.diffuse,texel,i.specular,temp);
   float4 result=combine(c.x,ca,cb,cc,current,i.diffuse,texel);
   if(a.x!=1){float4 aa=argument(a.y,current,i.diffuse,texel,i.specular,temp);
     float4 ab=argument(a.z,current,i.diffuse,texel,i.specular,temp);
     float4 ac=argument(m.z,current,i.diffuse,texel,i.specular,temp);
     result.a=combine(a.x,aa,ab,ac,current,i.diffuse,texel).a;
   }else if(c.x!=24)result.a=current.a;
   if(m.w!=0)temp=saturate(result);else current=saturate(result);
 }
 if(control.w!=0)current.rgb=saturate(current.rgb+i.specular.rgb);
 if(control.x!=0){uint value=(uint)floor(saturate(current.a)*255+.5);clip(passes(value,control.y,control.z)?1:-1);}
 float fog_factor=1;
#if FF_FOG
 fog_factor=i.fog;
#endif
 if(fog.w!=0){float distance=i.position.z;
   if(fog.w==3)fog_factor=saturate((fog.y-distance)/max(fog.y-fog.x,1e-20));
   else if(fog.w==1)fog_factor=saturate(exp(-fog.z*distance));
   else if(fog.w==2)fog_factor=saturate(exp(-pow(fog.z*distance,2)));
 }
 if(fog_color.a!=0)current.rgb=lerp(fog_color.rgb,current.rgb,fog_factor);
 return saturate(current);
}
)hlsl";
} // namespace

struct DrawPipeline::Impl {
 ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;DWORD owner=0;
 Log log=nullptr;std::string cache;std::set<std::string> warnings,alpha_notes;
 ID3D11Buffer *vb=nullptr,*ib=nullptr,*ff_buffer=nullptr,*ps_buffer=nullptr,*legacy_buffer=nullptr,*raster_buffer=nullptr;
 UINT vb_capacity=0,ib_capacity=0;
 struct Program{ID3D11VertexShader* shader=nullptr;ID3D11InputLayout* input=nullptr;};
 std::map<std::string,Program> programs;ID3D11PixelShader* pixel[2]{};
 std::map<std::string,ID3D11BlendState*> blends;
 std::map<std::string,ID3D11DepthStencilState*> depths;
 std::map<std::string,ID3D11RasterizerState*> rasters;
 std::map<std::string,ID3D11SamplerState*> samplers;
 std::vector<std::uint32_t> fan;
 ~Impl(){for(auto& p:programs){drop(p.second.shader);drop(p.second.input);}for(auto*& p:pixel)drop(p);
 clear(blends);clear(depths);clear(rasters);clear(samplers);
 drop(vb);drop(ib);drop(ff_buffer);drop(ps_buffer);drop(legacy_buffer);drop(raster_buffer);drop(context);drop(device);}
 HRESULT invalid(const std::string& why){
  if(a2fo_draw_log_admit(log!=nullptr,warnings.size())&&warnings.insert(why).second){
   log("D3D11 game draw rejected: "+why);
   if(a2fo_draw_log_limit(warnings.size()))log("D3D11 game: rejection diagnostic limit reached; further categories suppressed");
  }return invalid_call;
 }
 HRESULT buffer(ID3D11Buffer*& b,UINT& capacity,UINT bytes,UINT binding,const void* data){
  const UINT next=a2fo_draw_buffer_capacity(capacity,bytes);
  if(!next)return invalid_call;
  D3D11_MAPPED_SUBRESOURCE mapped{};
  const auto perform=[&](UINT step)->HRESULT{
   switch(step){
   case 0:{D3D11_BUFFER_DESC d{};d.ByteWidth=next;d.Usage=D3D11_USAGE_DYNAMIC;
    d.BindFlags=binding;d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* new_buffer=nullptr;HRESULT hr=device->CreateBuffer(&d,nullptr,&new_buffer);
    if(FAILED(hr))return hr;
    drop(b);b=new_buffer;capacity=next;return S_OK;}
   case 1:return context->Map(b,0,D3D11_MAP_WRITE_DISCARD,0,&mapped);
   case 2:std::memcpy(mapped.pData,data,bytes);context->Unmap(b,0);return S_OK;
   default:return invalid_call;
   }
  };
  struct BufferContext {const decltype(perform)* operation;std::exception_ptr error;};
  BufferContext callback_context{&perform,{}};
  const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
   auto& c=*static_cast<BufferContext*>(opaque);
   try{return (*c.operation)(step);}catch(...){c.error=std::current_exception();return E_FAIL;}
  };
  const HRESULT hr=a2fo_draw_buffer_workflow(&callback_context,capacity<bytes,callback);
  if(callback_context.error)std::rethrow_exception(callback_context.error);
  return hr;
 }
 template<class T> HRESULT constants(ID3D11Buffer*& b,const T& value){UINT cap=b?sizeof(T):0;return buffer(b,cap,sizeof(T),D3D11_BIND_CONSTANT_BUFFER,&value);}
 HRESULT program(const std::string& source,Program*& result){
  ShaderCompileResult compiled;Program p;
  const auto perform=[&](UINT step)->HRESULT{
   switch(step){
   case 0:{auto it=programs.find(source);if(it==programs.end())return S_OK;
    result=&it->second;return S_FALSE;}
   case 1:{HRESULT hr=compile_shader({source,"VSMain","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS},cache,compiled);
    if(FAILED(hr)&&log)log(compiled.diagnostics);
    return hr;}
   case 2:return device->CreateVertexShader(compiled.bytes.data(),compiled.bytes.size(),nullptr,&p.shader);
   case 3:{D3D11_INPUT_ELEMENT_DESC elements[16]{};
    for(UINT i=0;i<16;++i)elements[i]={"TEXCOORD",i,DXGI_FORMAT_R32G32B32A32_FLOAT,0,i*16,D3D11_INPUT_PER_VERTEX_DATA,0};
    HRESULT hr=device->CreateInputLayout(elements,16,compiled.bytes.data(),compiled.bytes.size(),&p.input);
    if(FAILED(hr))drop(p.shader);
    return hr;}
   case 4:if(a2fo_draw_cache_evict(programs.size(),32)){
     for(auto& v:programs){drop(v.second.shader);drop(v.second.input);}programs.clear();}
    result=&programs.emplace(source,p).first->second;return S_OK;
   default:return invalid_call;
   }
  };
  struct ProgramContext {const decltype(perform)* operation;std::exception_ptr error;};
  ProgramContext context{&perform,{}};
  const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
   auto& c=*static_cast<ProgramContext*>(opaque);
   try{return (*c.operation)(step);}catch(...){c.error=std::current_exception();return E_FAIL;}
  };
  const HRESULT hr=a2fo_draw_program_workflow(&context,callback);
  if(context.error)std::rethrow_exception(context.error);
  return hr;
 }
 HRESULT ensure_pixel(bool ff){const UINT index=ff?1:0;
  std::string source;ShaderCompileResult compiled;
  const auto perform=[&](UINT step)->HRESULT{
   switch(step){
   case 0:return pixel[index]?S_FALSE:S_OK;
   case 1:{source=std::string("#define FF_FOG ")+(ff?"1\n":"0\n")+
     (ff ? std::string(ff_vs).substr(std::string(ff_vs).find("struct VSOutput"),std::string(ff_vs).find("float4 select_uv")-std::string(ff_vs).find("struct VSOutput")) : shaders::vs11_output_interface())+
     ps_head+shaders::bc5_normal_functions+ps_body;
    HRESULT hr=compile_shader({source,"PSMain","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS},cache,compiled);
    if(FAILED(hr)&&log)log(compiled.diagnostics);
    return hr;}
   case 2:return device->CreatePixelShader(compiled.bytes.data(),compiled.bytes.size(),nullptr,&pixel[index]);
   default:return invalid_call;
   }
  };
  struct PixelContext {const decltype(perform)* operation;std::exception_ptr error;};
  PixelContext context{&perform,{}};
  const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
   auto& c=*static_cast<PixelContext*>(opaque);
   try{return (*c.operation)(step);}catch(...){c.error=std::current_exception();return E_FAIL;}
  };
  const HRESULT hr=a2fo_draw_pixel_workflow(&context,callback);
  if(context.error)std::rethrow_exception(context.error);
  return hr;
 }
 template<class Desc,class T,class Make>
 HRESULT cached_state(std::map<std::string,T*>& cache_map,const Desc& desc,T*& out,Make make){
  const auto cache_key=key(desc);
  const auto perform=[&](UINT step)->HRESULT{
   switch(step){
   case 0:{auto found=cache_map.find(cache_key);if(found==cache_map.end())return S_OK;
    out=found->second;return S_FALSE;}
   case 1:if(a2fo_draw_cache_evict(cache_map.size(),max_states))clear(cache_map);return S_OK;
   case 2:return make(&desc,&out);
   case 3:cache_map.emplace(cache_key,out);return S_OK;
   default:return invalid_call;
   }
  };
  struct StateContext {const decltype(perform)* operation;std::exception_ptr error;};
  StateContext context{&perform,{}};
  const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
   auto& c=*static_cast<StateContext*>(opaque);
   try{return (*c.operation)(step);}catch(...){c.error=std::current_exception();return E_FAIL;}
  };
  const HRESULT hr=a2fo_draw_state_workflow(&context,callback);
  if(context.error)std::rethrow_exception(context.error);
  return hr;
 }
};

DrawPipeline::DrawPipeline()=default;DrawPipeline::~DrawPipeline()=default;
HRESULT DrawPipeline::initialize(ID3D11Device* device,ID3D11DeviceContext* context,const std::string& cache,Log log){
 static_assert(D3D_FEATURE_LEVEL_11_0==0xb000 && D3D11_DEVICE_CONTEXT_IMMEDIATE==0);
 const auto perform=[&](UINT step)->std::int32_t{
  switch(step){
  case 0:return device->GetFeatureLevel();
  case 1:return context->GetType();
  case 2:{ID3D11Device* owner=nullptr;context->GetDevice(&owner);
   const bool same=owner==device;drop(owner);return same?1:0;}
  case 3:try{auto p=std::make_unique<Impl>();p->device=device;p->context=context;
    device->AddRef();context->AddRef();p->owner=GetCurrentThreadId();p->log=log;
    p->cache=cache;impl_=std::move(p);return S_OK;}
   catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return E_FAIL;}
  default:return invalid_call;
  }
 };
 struct InitializeContext {const decltype(perform)* operation;std::exception_ptr error;};
 InitializeContext callback_context{&perform,{}};
 const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
  auto& c=*static_cast<InitializeContext*>(opaque);
  try{return (*c.operation)(step);}catch(...){c.error=std::current_exception();return step==1?1:0;}
 };
 const HRESULT hr=a2fo_draw_initialize_workflow(&callback_context,device!=nullptr,context!=nullptr,callback);
 if(callback_context.error)std::rethrow_exception(callback_context.error);
 return hr;
}

HRESULT DrawPipeline::draw(const State& state,const Draw& draw,const std::array<Sampling,8>& texture){
 if(!impl_||impl_->owner!=GetCurrentThreadId())return invalid_call;
 auto& p=*impl_;const bool ff=state.vertex_hlsl.empty();
 const auto fail=[&](const std::string& message){return p.invalid(message);};
 const auto admit=a2fo_draw_admit(state.width,state.height,state.min_z,state.max_z,
    draw.primitive,draw.vertices.size(),draw.indices.size(),state.transformed,
    draw.vertices.empty()?nullptr:&draw.vertices.front().v[0][0]);
 switch(admit.code){
 case 0:return S_OK;
 case 1:return fail("viewport");case 2:return fail("primitive topology");
 case 3:return fail("vertex count");case 4:return fail("index count");
 case 5:return fail("nonfinite position");
 case 6:return fail("invalid RHW value="+std::to_string(admit.rhw)+" vertex_shader="+(ff?"FVF":"program"));
 default:break;
 }
 std::vector<std::uint32_t> indices=draw.indices;
 if(indices.empty()){indices.resize(draw.vertices.size());a2fo_draw_sequence(indices.data(),indices.size());}
 std::size_t final_count=0;
 const auto index_code=a2fo_draw_indices(indices.data(),indices.size(),draw.vertices.size(),
    draw.primitive,nullptr,0,&final_count);
 if(index_code==1)return fail("index range");
 if(index_code==2)return fail("triangle fan count");
 if(index_code==3)return fail("index count");
 if(index_code==4)return fail("primitive count");
 if(draw.primitive==6){p.fan.clear();p.fan.resize(final_count);
  a2fo_draw_indices(indices.data(),indices.size(),draw.vertices.size(),draw.primitive,
    p.fan.data(),p.fan.size(),&final_count);indices=p.fan;}
 const auto& r=state.render;
 std::array<std::uint32_t,256> render_pod{};
 std::array<std::array<std::uint32_t,33>,8> stage_pod{};
 const auto read_render=[&]{for(std::size_t i=0;i<render_pod.size();++i)render_pod[i]=r[i];};
 const auto read_stage=[&](std::size_t n){
  for(std::size_t i=0;i<stage_pod[n].size();++i)stage_pod[n][i]=state.stage[n][i];
 };
 read_render();
 const auto render_code=a2fo_draw_render(render_pod.data(),ff,state.transformed,draw.primitive);
 switch(render_code){
 case 1:return fail("render state");case 2:return fail("stencil or fog state");
 case 3:return fail("unsupported raster or material state");
 case 4:return fail("fixed-function vertex blending unsupported");
 case 5:return fail("user clip planes unsupported mask="+std::to_string(r[152]));
 case 6:return fail("point sprite/scaling expansion unsupported");
 case 7:return fail("fixed-function infinite-viewer specular unsupported");
 case 8:return fail("texture coordinate wrap flags");default:break;
 }
 A2foDrawBlend blend_plan{};
 if(!a2fo_draw_blend(r[19],r[20],&blend_plan))return fail("blend factor");
 const auto src=static_cast<D3D11_BLEND>(blend_plan.source),dst=static_cast<D3D11_BLEND>(blend_plan.dest);
 const auto srca=static_cast<D3D11_BLEND>(blend_plan.source_alpha),dsta=static_cast<D3D11_BLEND>(blend_plan.dest_alpha);
 if(render_code==9)return fail("programmable vertex fog output unavailable");
 PsConstants ps{};bool use_texture[8]{};int active=0;
 for(int n=0;n<8;++n){const auto& s=state.stage[n];if(n&&s[1]==1)break;
  read_stage(n);
  const auto pre_code=a2fo_draw_stage_pre(stage_pod[n].data());
  if(pre_code==1)return fail("texture stage operation or coordinates");
  if(pre_code==2)return fail("color-only operation in alpha stage");
  // Armada uses this nominally undefined combination. Paired tests against
  // its installed D3D8 provider retain incoming alpha, except DOT3 which
  // replicates the signed dot product to alpha as well as RGB.
  if(a2fo_draw_alpha_note(s[1],s[4],p.log!=nullptr,p.alpha_notes.size())){
   const auto note="stage="+std::to_string(n)+" color_op="+std::to_string(s[1])+
    " color_args="+std::to_string(s[2])+","+std::to_string(s[3])+
    " blend="+std::to_string(r[27])+","+std::to_string(r[19])+","+std::to_string(r[20])+
    " vertex="+(ff?std::string("FVF"):std::string("program"));
   if(p.alpha_notes.insert(note).second)p.log("[D3D8Compat] Legacy alpha-disable draw admitted "+note);
  }
  read_stage(n);
  const auto stage_plan=a2fo_draw_stage_post(stage_pod[n].data(),ff,state.transformed,
    state.has_normal,state.tex_components.data());
  if(stage_plan.code==3)return fail("programmable texture coordinate generation or transform");
  if(stage_plan.code==4)return fail("texture coordinate width");
  if(stage_plan.code==5)return fail("texture coordinate generation source");
  use_texture[n]=stage_plan.uses_texture;
  std::memcpy(ps.stage[n],stage_plan.row,sizeof(stage_plan.row));
  // The D3D8 fixed pixel cascade reads white when a stage references a null texture.
  ps.stage[n][8]=use_texture[n]&&texture[n].view;
  if(use_texture[n]&&texture[n].view){
   D3D11_SHADER_RESOURCE_VIEW_DESC desc{};texture[n].view->GetDesc(&desc);
   static_assert(D3D11_SRV_DIMENSION_TEXTURE2D==4 && DXGI_FORMAT_BC5_UNORM==0x53 && DXGI_FORMAT_BC5_SNORM==0x54);
   if(!a2fo_draw_view_dimension(desc.ViewDimension))return fail("non-2D sampled texture");
   ID3D11Device* owner=nullptr;texture[n].view->GetDevice(&owner);const bool same=owner==p.device;drop(owner);
   if(!a2fo_draw_view_owner(same))return fail("texture from another device");
   const auto mode=a2fo_draw_normal_mode(texture[n].normal_xy,desc.Format);
   if(mode==3)return fail("normal_xy requires BC5");
   ps.stage[n][9]=mode;
  }
  read_stage(n);
  a2fo_draw_stage_finish(stage_pod[n].data(),ps.stage[n]);
  ++active;
 }
 if(!active)return fail("no active texture stage");
 a2fo_draw_finish_stages(&ps.stage[0][0],active);
 ID3D11RenderTargetView* bound_color=nullptr;ID3D11DepthStencilView* bound_depth=nullptr;
 p.context->OMGetRenderTargets(1,&bound_color,&bound_depth);
 const bool has_color=bound_color!=nullptr;
 const bool has_depth=bound_depth!=nullptr;
 ID3D11Resource* color_resource=nullptr;ID3D11Resource* depth_resource=nullptr;
 if(bound_color)bound_color->GetResource(&color_resource);
 if(bound_depth)bound_depth->GetResource(&depth_resource);
 bool aliases=false;
 for(int n=0;n<active;++n)if(use_texture[n]&&texture[n].view){
  ID3D11Resource* source=nullptr;texture[n].view->GetResource(&source);
  aliases=a2fo_draw_alias_step(aliases,source!=nullptr,
    source==color_resource,source==depth_resource);drop(source);
 }
 drop(color_resource);drop(depth_resource);drop(bound_color);drop(bound_depth);
 const auto target_code=a2fo_draw_target_admit(has_color,has_depth,r[7]||r[52],aliases);
 if(target_code==1)return fail("no active render target");
 if(target_code==2)return fail("no active depth/stencil target");
 if(target_code==3)return fail("texture aliases active render target");
 read_render();
 a2fo_draw_pixel_constants(render_pod.data(),ps.control,ps.fog,ps.fog_color,ps.factor);
 for(int n=0;n<active;++n){if(!use_texture[n]||!texture[n].view)continue;
  const auto& s=state.stage[n];read_stage(n);
  if(!a2fo_draw_sampler_validate(stage_pod[n].data()))return fail("sampler stage="+std::to_string(n)+
    " address="+std::to_string(s[13])+","+std::to_string(s[14])+","+std::to_string(s[25])+
    " min="+std::to_string(s[17])+" mag="+std::to_string(s[16])+" mip="+std::to_string(s[18])+
    " max_anisotropy="+std::to_string(s[21]));
 }
 // Input buffers grow geometrically and are updated with WRITE_DISCARD.
 Impl::Program* program=nullptr;
 const auto prepare=[&](UINT step)->HRESULT{
  switch(step){
  case 0:return p.buffer(p.vb,p.vb_capacity,UINT(draw.vertices.size()*sizeof(Vertex)),D3D11_BIND_VERTEX_BUFFER,draw.vertices.data());
  case 1:return p.buffer(p.ib,p.ib_capacity,UINT(indices.size()*4),D3D11_BIND_INDEX_BUFFER,indices.data());
  case 2:return p.program(ff?std::string(ff_vs):state.vertex_hlsl,program);
  case 3:return p.ensure_pixel(ff);
  case 4:return p.constants(p.ps_buffer,ps);
  case 5:{if(!ff)return p.constants(p.legacy_buffer,state.constants);
  static_assert(sizeof(Matrix)==16*sizeof(float));
  static_assert(sizeof(FfConstants)==sizeof(A2foDrawFixed) && alignof(FfConstants)==alignof(A2foDrawFixed));
  read_render();for(std::size_t n=0;n<stage_pod.size();++n)read_stage(n);
  A2foDrawFixed pod{};
  const A2foDrawFixedInput input{state.world.m,state.view.m,state.projection.m,
   &state.texture[0].m[0],state.material.diffuse,state.material.ambient,
   state.material.specular,state.material.emissive,state.material.power,
   render_pod.data(),stage_pod[0].data(),state.tex_components.data(),
   state.x,state.y,state.width,state.height,state.transformed,state.has_normal,
   state.has_diffuse,state.has_specular,static_cast<UINT>(active)};
  const bool need_normal=a2fo_draw_fixed_base(&input,&pod);
  if(!state.transformed&&need_normal){
   float inverse_world[9]{},inverse_view[9]{};
   if(!a2fo_draw_inverse3(state.world.m,inverse_world))return fail("singular world normal transform");
   for(int row=0;row<3;++row)for(int col=0;col<3;++col)pod.normal[row*4+col]=inverse_world[col*3+row];
   if(r[137]){
    if(!a2fo_draw_inverse3(state.view.m,inverse_view))return fail("singular view eye transform");
    a2fo_draw_camera(state.view.m,inverse_view,pod.camera);
   }
  }
  A2foDrawLight lights[8]{};
  for(int n=0;n<8;++n){const auto& l=state.lights[n];auto& dst=lights[n];
   dst.enabled=l.enabled;dst.type=l.type;
   std::memcpy(dst.diffuse,l.diffuse,16);std::memcpy(dst.ambient,l.ambient,16);
   std::memcpy(dst.specular,l.specular,16);std::memcpy(dst.position,l.position,12);
   std::memcpy(dst.direction,l.direction,12);
   dst.attenuation0=l.attenuation0;dst.attenuation1=l.attenuation1;
   dst.attenuation2=l.attenuation2;dst.range=l.range;
  }
  if(!a2fo_draw_fixed_lights(lights,&pod))return fail("spot or invalid light");
  FfConstants c{};std::memcpy(&c,&pod,sizeof(c));
  return p.constants(p.ff_buffer,c);}
  case 6:{float inv[4]{};a2fo_draw_raster_inverse(state.width,state.height,inv);
   return p.constants(p.raster_buffer,inv);}
  default:return invalid_call;
  }
 };
 struct PrepareContext {const decltype(prepare)* operation;std::exception_ptr error;};
 PrepareContext prepare_context{&prepare,{}};
 const auto callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
  auto& context=*static_cast<PrepareContext*>(opaque);
  try{return (*context.operation)(step);}catch(...){context.error=std::current_exception();return E_FAIL;}
 };
 HRESULT hr=a2fo_draw_prepare_workflow(&prepare_context,ff,callback);
 if(prepare_context.error)std::rethrow_exception(prepare_context.error);
 if(FAILED(hr))return hr;
 read_render();
 A2foDrawStates state_plan{};a2fo_draw_states(render_pod.data(),draw.primitive,&state_plan);
 D3D11_BLEND_DESC bd{};bd.RenderTarget[0].BlendEnable=state_plan.blend_enabled;bd.RenderTarget[0].SrcBlend=src;
 bd.RenderTarget[0].DestBlend=dst;bd.RenderTarget[0].SrcBlendAlpha=srca;bd.RenderTarget[0].DestBlendAlpha=dsta;
 bd.RenderTarget[0].BlendOp=static_cast<D3D11_BLEND_OP>(state_plan.blend_op);bd.RenderTarget[0].BlendOpAlpha=bd.RenderTarget[0].BlendOp;
 bd.RenderTarget[0].RenderTargetWriteMask=static_cast<UINT8>(state_plan.write_mask);
 ID3D11BlendState* bs=nullptr;
 hr=p.cached_state(p.blends,bd,bs,[&](const D3D11_BLEND_DESC* desc,ID3D11BlendState** out){
  return p.device->CreateBlendState(desc,out);});if(FAILED(hr))return hr;
 read_render();a2fo_draw_states(render_pod.data(),draw.primitive,&state_plan);
 D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthEnable=state_plan.depth_enabled;
 dd.DepthWriteMask=static_cast<D3D11_DEPTH_WRITE_MASK>(state_plan.depth_write);
 dd.DepthFunc=static_cast<D3D11_COMPARISON_FUNC>(state_plan.depth_func);dd.StencilEnable=state_plan.stencil_enabled;
 dd.StencilReadMask=static_cast<UINT8>(state_plan.stencil_read);dd.StencilWriteMask=static_cast<UINT8>(state_plan.stencil_write);
 dd.FrontFace.StencilFailOp=static_cast<D3D11_STENCIL_OP>(state_plan.stencil_fail);
 dd.FrontFace.StencilDepthFailOp=static_cast<D3D11_STENCIL_OP>(state_plan.stencil_depth_fail);
 dd.FrontFace.StencilPassOp=static_cast<D3D11_STENCIL_OP>(state_plan.stencil_pass);
 dd.FrontFace.StencilFunc=static_cast<D3D11_COMPARISON_FUNC>(state_plan.stencil_func);
 dd.BackFace=dd.FrontFace;
 ID3D11DepthStencilState* ds=nullptr;
 hr=p.cached_state(p.depths,dd,ds,[&](const D3D11_DEPTH_STENCIL_DESC* desc,ID3D11DepthStencilState** out){
  return p.device->CreateDepthStencilState(desc,out);});if(FAILED(hr))return hr;
 read_render();a2fo_draw_states(render_pod.data(),draw.primitive,&state_plan);
 D3D11_RASTERIZER_DESC rd{};rd.FillMode=static_cast<D3D11_FILL_MODE>(state_plan.fill);rd.CullMode=static_cast<D3D11_CULL_MODE>(state_plan.cull);
 rd.FrontCounterClockwise=FALSE;rd.DepthClipEnable=state_plan.depth_clip;ID3D11RasterizerState* rs=nullptr;
 hr=p.cached_state(p.rasters,rd,rs,[&](const D3D11_RASTERIZER_DESC* desc,ID3D11RasterizerState** out){
  return p.device->CreateRasterizerState(desc,out);});if(FAILED(hr))return hr;
 ID3D11SamplerState* sampler[8]{};ID3D11ShaderResourceView* views[8]{};
 for(int n=0;n<active;++n){if(!use_texture[n]||!texture[n].view)continue;
  read_stage(n);A2foDrawSampler plan{};
  a2fo_draw_sampler_plan(stage_pod[n].data(),texture[n].minimum_lod,&plan);
  D3D11_SAMPLER_DESC sd{};
  sd.Filter=static_cast<D3D11_FILTER>(plan.filter);
  sd.AddressU=static_cast<D3D11_TEXTURE_ADDRESS_MODE>(plan.address_u);sd.AddressV=static_cast<D3D11_TEXTURE_ADDRESS_MODE>(plan.address_v);
  sd.AddressW=static_cast<D3D11_TEXTURE_ADDRESS_MODE>(plan.address_w);a2fo_draw_rgba(plan.border,sd.BorderColor);
  sd.MipLODBias=bits(plan.bias);sd.MaxAnisotropy=plan.max_anisotropy;sd.ComparisonFunc=D3D11_COMPARISON_NEVER;
  sd.MinLOD=plan.min_lod;sd.MaxLOD=plan.max_lod;
  hr=p.cached_state(p.samplers,sd,sampler[n],[&](const D3D11_SAMPLER_DESC* desc,ID3D11SamplerState** out){
   return p.device->CreateSamplerState(desc,out);});if(FAILED(hr))return hr;
  views[n]=texture[n].view;
 }
 float area[4]{};a2fo_draw_viewport(state.x,state.y,state.width,state.height,area);
 D3D11_VIEWPORT viewport{};viewport.TopLeftX=area[0];viewport.TopLeftY=area[1];
 viewport.Width=area[2];viewport.Height=area[3];viewport.MinDepth=state.min_z;viewport.MaxDepth=state.max_z;
 const UINT stride=sizeof(Vertex),offset=0;
 read_render();a2fo_draw_states(render_pod.data(),draw.primitive,&state_plan);
 const D3D11_PRIMITIVE_TOPOLOGY topology=static_cast<D3D11_PRIMITIVE_TOPOLOGY>(state_plan.topology);
 const float blend_factor[4]{};
 const auto dispatch=[&](UINT step)->HRESULT{
  switch(step){
  case 0:p.context->RSSetViewports(1,&viewport);break;
  case 1:p.context->IASetInputLayout(program->input);break;
  case 2:p.context->IASetVertexBuffers(0,1,&p.vb,&stride,&offset);break;
  case 3:p.context->IASetIndexBuffer(p.ib,DXGI_FORMAT_R32_UINT,0);break;
  case 4:p.context->IASetPrimitiveTopology(topology);break;
  case 5:p.context->VSSetShader(program->shader,nullptr,0);break;
  case 6:p.context->HSSetShader(nullptr,nullptr,0);break;
  case 7:p.context->DSSetShader(nullptr,nullptr,0);break;
  case 8:p.context->GSSetShader(nullptr,nullptr,0);break;
  case 9:if(ff)p.context->VSSetConstantBuffers(2,1,&p.ff_buffer);
   else {p.context->VSSetConstantBuffers(0,1,&p.legacy_buffer);
    p.context->VSSetConstantBuffers(1,1,&p.raster_buffer);}break;
  case 10:p.context->PSSetShader(p.pixel[ff?1:0],nullptr,0);break;
  case 11:p.context->PSSetConstantBuffers(3,1,&p.ps_buffer);break;
  case 12:p.context->PSSetShaderResources(0,8,views);break;
  case 13:p.context->PSSetSamplers(0,8,sampler);break;
  case 14:p.context->RSSetState(rs);break;
  case 15:p.context->OMSetBlendState(bs,blend_factor,0xffffffffu);break;
  case 16:p.context->OMSetDepthStencilState(ds,r[57]);break;
  case 17:p.context->DrawIndexed(static_cast<UINT>(indices.size()),0,0);break;
  default:return invalid_call;
  }return S_OK;
 };
 struct DispatchContext {const decltype(dispatch)* operation;std::exception_ptr error;};
 DispatchContext dispatch_context{&dispatch,{}};
 const auto dispatch_callback=[](void* opaque,std::uint32_t step) noexcept -> std::int32_t {
  auto& context=*static_cast<DispatchContext*>(opaque);
  try{return (*context.operation)(step);}catch(...){context.error=std::current_exception();return E_FAIL;}
 };
 hr=a2fo_draw_dispatch_workflow(&dispatch_context,dispatch_callback);
 if(dispatch_context.error)std::rethrow_exception(dispatch_context.error);
 return hr;
}
} // namespace a2fo::renderer::game
