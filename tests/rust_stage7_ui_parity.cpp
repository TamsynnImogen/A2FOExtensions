// Full production UI renderer against constructed foreign-COM ABI mocks.
// No HWND, native graphics provider, device creation, GUI or input automation.
#include <windows.h>
#include <d3d11.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include UNIT_SOURCE
using namespace a2fo::renderer;
extern "C" std::uint32_t ui_device_slot(unsigned);
extern "C" std::uint32_t ui_context_slot(unsigned);
extern "C" std::uint32_t ui_child_slot(unsigned);
namespace {
void require(bool c,const char* text){if(!c){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
unsigned hash(const void* p,std::size_t n){unsigned h=2166136261u;const auto* b=static_cast<const unsigned char*>(p);for(std::size_t i=0;i<n;++i)h=(h^b[i])*16777619u;return h;}
struct Com {
 void** methods;
 unsigned refs=1,id;
 const char* kind;
 Com* owner=nullptr;
 Com* resource=nullptr;
 std::vector<unsigned char> bytes;
 D3D11_TEXTURE2D_DESC desc{};
 unsigned view_dimension=D3D11_RTV_DIMENSION_TEXTURE2D,mip=0;
 Com(void** table,unsigned number,const char* name):methods(table),id(number),kind(name){}
};
void* device_vtable[64]{};void* context_vtable[128]{};void* child_vtable[16]{};void* texture_vtable[16]{};
std::vector<std::unique_ptr<Com>> objects;
Com* device=nullptr;
unsigned calls=0,fail_at=0;
bool throw_compile=false,fail_compile=false;
UiDrawState* mutate_state=nullptr;
void trace(const char* name){std::printf("call %s\n",name);}
bool fail(const char* name){++calls;std::printf("native %u %s\n",calls,name);return calls==fail_at;}
Com* make(const char* name,void** table=child_vtable){objects.push_back(std::make_unique<Com>(table,static_cast<unsigned>(objects.size()+1),name));auto* p=objects.back().get();p->owner=device;p->desc.Width=p->desc.Height=16;p->desc.ArraySize=p->desc.SampleDesc.Count=1;p->desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;return p;}
ULONG STDMETHODCALLTYPE add(void* p){auto& c=*static_cast<Com*>(p);require(c.refs>0,"retain live COM");++c.refs;std::printf("retain %u %u\n",c.id,c.refs);return c.refs;}
ULONG STDMETHODCALLTYPE drop(void* p){auto& c=*static_cast<Com*>(p);require(c.refs>0,"release live COM");--c.refs;std::printf("release %u %u\n",c.id,c.refs);if(!c.refs&&c.resource&&std::strcmp(c.kind,"srv")==0){drop(c.resource);c.resource=nullptr;}return c.refs;}
HRESULT STDMETHODCALLTYPE query(void* p,REFIID,void** out){trace("QI");if(fail("QI")){*out=nullptr;return E_NOINTERFACE;}*out=p;add(p);return S_OK;}
void STDMETHODCALLTYPE get_device(void* p,ID3D11Device** out){auto* d=static_cast<Com*>(p)->owner;std::printf("owner %u\n",static_cast<Com*>(p)->id);*out=reinterpret_cast<ID3D11Device*>(d);if(d)add(d);}
void STDMETHODCALLTYPE get_resource(void* p,ID3D11Resource** out){auto* r=static_cast<Com*>(p)->resource;std::printf("resource %u\n",static_cast<Com*>(p)->id);*out=reinterpret_cast<ID3D11Resource*>(r);if(r)add(r);}
void STDMETHODCALLTYPE view_desc(void* p,void* out){auto* c=static_cast<Com*>(p);trace("view-desc");if(std::strcmp(c->kind,"srv")==0){auto& d=*static_cast<D3D11_SHADER_RESOURCE_VIEW_DESC*>(out);d={};d.ViewDimension=static_cast<D3D11_SRV_DIMENSION>(c->view_dimension);d.Texture2D.MostDetailedMip=c->mip;}else if(std::strcmp(c->kind,"depth")==0){auto& d=*static_cast<D3D11_DEPTH_STENCIL_VIEW_DESC*>(out);d={};d.ViewDimension=static_cast<D3D11_DSV_DIMENSION>(c->view_dimension);d.Texture2D.MipSlice=c->mip;}else{auto& d=*static_cast<D3D11_RENDER_TARGET_VIEW_DESC*>(out);d={};d.ViewDimension=static_cast<D3D11_RTV_DIMENSION>(c->view_dimension);d.Texture2D.MipSlice=c->mip;}}
void STDMETHODCALLTYPE image_desc(void* p,D3D11_TEXTURE2D_DESC* out){trace("image-desc");*out=static_cast<Com*>(p)->desc;}
D3D_FEATURE_LEVEL STDMETHODCALLTYPE feature(void*){trace("feature");return D3D_FEATURE_LEVEL_11_0;}
D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE context_type(void*){trace("context-type");return D3D11_DEVICE_CONTEXT_IMMEDIATE;}
HRESULT STDMETHODCALLTYPE create_buffer(void*,const D3D11_BUFFER_DESC* d,const D3D11_SUBRESOURCE_DATA*,ID3D11Buffer** out){std::printf("buffer %u %u\n",d->ByteWidth,d->BindFlags);if(fail("buffer"))return E_FAIL;auto* c=make("buffer");c->bytes.resize(d->ByteWidth);*out=reinterpret_cast<ID3D11Buffer*>(c);return S_OK;}
HRESULT STDMETHODCALLTYPE create_texture(void*,const D3D11_TEXTURE2D_DESC* d,const D3D11_SUBRESOURCE_DATA* data,ID3D11Texture2D** out){std::printf("texture %u %u %u %u\n",d->Width,d->Height,d->Format,data?data->SysMemPitch:0);if(fail("texture"))return E_FAIL;auto* c=make("image",texture_vtable);c->desc=*d;*out=reinterpret_cast<ID3D11Texture2D*>(c);return S_OK;}
HRESULT STDMETHODCALLTYPE create_srv(void*,ID3D11Resource* image,const D3D11_SHADER_RESOURCE_VIEW_DESC*,ID3D11ShaderResourceView** out){if(fail("srv"))return E_FAIL;auto* c=make("srv");c->resource=reinterpret_cast<Com*>(image);add(c->resource);c->view_dimension=D3D11_SRV_DIMENSION_TEXTURE2D;*out=reinterpret_cast<ID3D11ShaderResourceView*>(c);return S_OK;}
HRESULT STDMETHODCALLTYPE create_vs(void*,const void* p,SIZE_T n,ID3D11ClassLinkage*,ID3D11VertexShader** out){std::printf("vs-bytes %u\n",hash(p,n));if(fail("vs"))return E_FAIL;*out=reinterpret_cast<ID3D11VertexShader*>(make("vs"));return S_OK;}
HRESULT STDMETHODCALLTYPE create_ps(void*,const void* p,SIZE_T n,ID3D11ClassLinkage*,ID3D11PixelShader** out){std::printf("ps-bytes %u\n",hash(p,n));if(fail("ps"))return E_FAIL;*out=reinterpret_cast<ID3D11PixelShader*>(make("ps"));return S_OK;}
HRESULT STDMETHODCALLTYPE create_input(void*,const D3D11_INPUT_ELEMENT_DESC* d,UINT n,const void*,SIZE_T,ID3D11InputLayout** out){for(UINT i=0;i<n;++i)std::printf("element %s %u %u %u\n",d[i].SemanticName,d[i].SemanticIndex,d[i].Format,d[i].AlignedByteOffset);if(fail("input"))return E_FAIL;*out=reinterpret_cast<ID3D11InputLayout*>(make("input"));return S_OK;}
#define CREATE_STATE(fn,type,interface,name) HRESULT STDMETHODCALLTYPE fn(void*,const type* d,interface** out){std::printf("desc " name " %u\n",hash(d,sizeof(*d)));if(fail(name))return E_FAIL;*out=reinterpret_cast<interface*>(make(name));if(mutate_state&&std::strcmp(name,"blend")==0){mutate_state->alpha_ref=37;mutate_state->address_u=2;}return S_OK;}
CREATE_STATE(create_blend,D3D11_BLEND_DESC,ID3D11BlendState,"blend")
CREATE_STATE(create_depth,D3D11_DEPTH_STENCIL_DESC,ID3D11DepthStencilState,"depth-state")
CREATE_STATE(create_raster,D3D11_RASTERIZER_DESC,ID3D11RasterizerState,"raster")
CREATE_STATE(create_sampler,D3D11_SAMPLER_DESC,ID3D11SamplerState,"sampler")
#undef CREATE_STATE
HRESULT STDMETHODCALLTYPE map(void*,ID3D11Resource* p,UINT,D3D11_MAP,UINT,D3D11_MAPPED_SUBRESOURCE* out){if(fail("map"))return E_FAIL;auto* c=reinterpret_cast<Com*>(p);out->pData=c->bytes.data();out->RowPitch=static_cast<UINT>(c->bytes.size());return S_OK;}
void STDMETHODCALLTYPE unmap(void*,ID3D11Resource* p,UINT){auto* c=reinterpret_cast<Com*>(p);std::printf("unmap %u %u\n",c->id,hash(c->bytes.data(),c->bytes.size()));}
void STDMETHODCALLTYPE om(void*,UINT,ID3D11RenderTargetView*const*,ID3D11DepthStencilView*){trace("targets");}
void STDMETHODCALLTYPE viewport(void*,UINT,const D3D11_VIEWPORT* p){std::printf("viewport %g %g %g %g\n",p->TopLeftX,p->TopLeftY,p->Width,p->Height);}
void STDMETHODCALLTYPE input(void*,ID3D11InputLayout*){trace("input");}
void STDMETHODCALLTYPE vb(void*,UINT,UINT,ID3D11Buffer*const*,const UINT* stride,const UINT*){std::printf("vertex-buffer %u\n",*stride);}
void STDMETHODCALLTYPE ib(void*,ID3D11Buffer*,DXGI_FORMAT f,UINT){std::printf("index-buffer %u\n",f);}
void STDMETHODCALLTYPE topology(void*,D3D11_PRIMITIVE_TOPOLOGY p){std::printf("topology %u\n",p);}
#define SHADER(fn,type,name) void STDMETHODCALLTYPE fn(void*,type*,ID3D11ClassInstance*const*,UINT){trace(name);}
SHADER(vs,ID3D11VertexShader,"VS") SHADER(hs,ID3D11HullShader,"HS") SHADER(ds,ID3D11DomainShader,"DS") SHADER(gs,ID3D11GeometryShader,"GS") SHADER(ps,ID3D11PixelShader,"PS")
#undef SHADER
void STDMETHODCALLTYPE vcb(void*,UINT,UINT,ID3D11Buffer*const*){trace("VS-constants");}
void STDMETHODCALLTYPE pcb(void*,UINT,UINT,ID3D11Buffer*const*){trace("PS-constants");}
void STDMETHODCALLTYPE srv(void*,UINT,UINT,ID3D11ShaderResourceView*const*){trace("SRV");}
void STDMETHODCALLTYPE sampler(void*,UINT,UINT,ID3D11SamplerState*const*){trace("sampler");}
void STDMETHODCALLTYPE raster(void*,ID3D11RasterizerState*){trace("raster");}
void STDMETHODCALLTYPE blend(void*,ID3D11BlendState*,const FLOAT*,UINT){trace("blend");}
void STDMETHODCALLTYPE depth(void*,ID3D11DepthStencilState*,UINT){trace("depth");}
void STDMETHODCALLTYPE draw_indexed(void*,UINT n,UINT,INT){std::printf("draw-indexed %u\n",n);}
void STDMETHODCALLTYPE draw(void*,UINT n,UINT){std::printf("draw %u\n",n);}
void log(const std::string& s){std::printf("log %s\n",s.c_str());}
void setup_tables(){
 for(auto* t:{device_vtable,context_vtable,child_vtable,texture_vtable}){t[0]=reinterpret_cast<void*>(&query);t[1]=reinterpret_cast<void*>(&add);t[2]=reinterpret_cast<void*>(&drop);}
 void* d[]={reinterpret_cast<void*>(&query),reinterpret_cast<void*>(&add),reinterpret_cast<void*>(&drop),reinterpret_cast<void*>(&create_buffer),reinterpret_cast<void*>(&create_texture),reinterpret_cast<void*>(&create_srv),reinterpret_cast<void*>(&create_input),reinterpret_cast<void*>(&create_vs),reinterpret_cast<void*>(&create_ps),reinterpret_cast<void*>(&create_blend),reinterpret_cast<void*>(&create_depth),reinterpret_cast<void*>(&create_raster),reinterpret_cast<void*>(&create_sampler),reinterpret_cast<void*>(&feature)};
 for(unsigned i=0;i<sizeof(d)/sizeof(*d);++i)device_vtable[ui_device_slot(i)]=d[i];
 void* c[]={reinterpret_cast<void*>(&get_device),reinterpret_cast<void*>(&context_type),reinterpret_cast<void*>(&map),reinterpret_cast<void*>(&unmap),reinterpret_cast<void*>(&om),reinterpret_cast<void*>(&viewport),reinterpret_cast<void*>(&input),reinterpret_cast<void*>(&vb),reinterpret_cast<void*>(&ib),reinterpret_cast<void*>(&topology),reinterpret_cast<void*>(&vs),reinterpret_cast<void*>(&hs),reinterpret_cast<void*>(&ds),reinterpret_cast<void*>(&gs),reinterpret_cast<void*>(&vcb),reinterpret_cast<void*>(&ps),reinterpret_cast<void*>(&pcb),reinterpret_cast<void*>(&srv),reinterpret_cast<void*>(&sampler),reinterpret_cast<void*>(&raster),reinterpret_cast<void*>(&blend),reinterpret_cast<void*>(&depth),reinterpret_cast<void*>(&draw_indexed),reinterpret_cast<void*>(&draw)};
 for(unsigned i=0;i<sizeof(c)/sizeof(*c);++i)context_vtable[ui_context_slot(i)]=c[i];
 child_vtable[ui_child_slot(0)]=reinterpret_cast<void*>(&get_device);child_vtable[ui_child_slot(1)]=reinterpret_cast<void*>(&get_resource);child_vtable[ui_child_slot(2)]=reinterpret_cast<void*>(&view_desc);
 texture_vtable[ui_child_slot(0)]=reinterpret_cast<void*>(&get_device);texture_vtable[ui_child_slot(3)]=reinterpret_cast<void*>(&image_desc);
}
void result(const char* name,HRESULT hr){std::printf("result %s %08lx\n",name,static_cast<unsigned long>(hr));}
void run(unsigned failure,bool mutate){
 std::printf("case %u %u\n",failure,mutate);objects.clear();device=nullptr;calls=0;fail_at=failure;device=make("device",device_vtable);device->owner=device;auto* context=make("context",context_vtable);
 {
 UiRenderer ui;result("init",ui.initialize(reinterpret_cast<ID3D11Device*>(device),reinterpret_cast<ID3D11DeviceContext*>(context),"fixture-cache",&log));
 auto* image=make("image",texture_vtable);auto* target=make("target");target->resource=image;auto* depth_image=make("image",texture_vtable);auto* depth_view=make("depth");depth_view->resource=depth_image;depth_view->view_dimension=D3D11_DSV_DIMENSION_TEXTURE2D;
 result("target",ui.set_target(reinterpret_cast<ID3D11RenderTargetView*>(target),reinterpret_cast<ID3D11DepthStencilView*>(depth_view),16,16));
 result("viewport",ui.set_viewport({1,2,12,10,0,1}));result("bad-viewport",ui.set_viewport({15,2,12,10,0,1}));
 struct V{float x,y,z,w;DWORD color;};V v[]={{0,0,.5f,1,0xff102030},{8,0,.5f,1,0xff112233},{0,8,.5f,1,0xff445566},{8,8,.5f,1,0xff778899}};
 UiDrawState state;state.color_op=state.alpha_op=1;mutate_state=mutate?&state:nullptr;
 result("draw",ui.draw_up(4,1,0x44,v,sizeof(v),sizeof(V),state));
 result("cached",ui.draw_up(4,1,0x44,v,sizeof(v),sizeof(V),state));
 const std::uint16_t ix[]={0,1,2,3};result("fan",ui.draw_indexed_up(6,2,0x44,v,sizeof(v),4,sizeof(V),ix,sizeof(ix),101,state));
 const std::uint16_t bad[]={0,4,2};result("bad-indices",ui.draw_indexed_up(4,1,0x44,v,sizeof(v),4,sizeof(V),bad,sizeof(bad),101,state));
 result("zero",ui.draw_up(4,0,0x44,nullptr,0,sizeof(V),state));
 v[1].w=0;result("bad-position",ui.draw_up(4,1,0x44,v,sizeof(v),sizeof(V),state));v[1].w=1;
 UiRenderer::Rect rects[]={{-5,-5,8,8},{5,6,25,26}};result("clear",ui.clear_color(0xffabcdef,2,rects));UiRenderer::Rect reversed{5,5,2,2};result("bad-rect",ui.clear_color(0xff123456,1,&reversed));
 compat::TextureUpload upload;upload.width=upload.height=2;upload.format=DXGI_FORMAT_R8G8B8A8_UNORM;upload.row_pitch=8;upload.bytes.resize(16,0x55);ID3D11ShaderResourceView* view=nullptr;result("upload",ui.upload_texture(upload,&view));if(view){result("texture",ui.set_texture(view));view->Release();}upload.row_pitch=0;result("bad-upload",ui.upload_texture(upload,&view));result("null-output",ui.upload_texture(upload,nullptr));
 if(!failure&&!mutate&&!throw_compile&&!fail_compile){
 std::vector<V> large(60,V{2,3,.5f,1,0xff102030});result("grow-buffer",ui.draw_up(4,20,0x44,large.data(),large.size()*sizeof(V),sizeof(V),state));
 for(unsigned n=0;n<310;++n){UiDrawState varied=state;varied.src_blend=1+n%11;varied.dst_blend=1+(n/11)%11;varied.blend_op=1+(n/121)%5;varied.min_filter=1+n%2;varied.mag_filter=1+(n/2)%2;varied.mip_filter=(n/4)%3;varied.address_u=1+(n/12)%5;varied.address_v=1+(n/60)%5;result("cache-eviction",ui.draw_up(4,1,0x44,v,sizeof(v),sizeof(V),varied));}
 compat::TextureUpload tex;tex.width=tex.height=2;tex.format=DXGI_FORMAT_R8G8B8A8_UNORM;tex.row_pitch=8;tex.bytes.resize(16,0x61);ID3D11ShaderResourceView* sampled=nullptr;result("sample-upload",ui.upload_texture(tex,&sampled));if(sampled){result("sample-set",ui.set_texture(sampled));sampled->Release();struct TV{float x,y,z,w;DWORD color;float u,v;};TV tv[]={{0,0,.5f,1,0xffffffff,0,0},{8,0,.5f,1,0xffeeeeee,1,0},{0,8,.5f,1,0xffdddddd,0,1}};UiDrawState textured;result("textured",ui.draw_up(4,1,0x144,tv,sizeof(tv),sizeof(TV),textured));}
 for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_BC1_UNORM,DXGI_FORMAT_BC2_UNORM,DXGI_FORMAT_BC3_UNORM,DXGI_FORMAT_UNKNOWN})for(unsigned width:{0u,1u,4u,5u,16384u,16385u})for(unsigned height:{0u,1u,3u,4u,5u,16384u,16385u})for(unsigned pitch:{0u,1u,8u,16u,0xffffffffu}){compat::TextureUpload check;check.width=width;check.height=height;check.format=format;check.row_pitch=pitch;check.bytes.resize(64);std::printf("payload %u %u %u %u %u\n",format,width,height,pitch,valid_upload(check));}
 }
 mutate_state=nullptr;result("target-clear",ui.set_target(nullptr,nullptr,0,0));ui.shutdown();
 }
 for(const auto& c:objects)std::printf("refs %u %s %u\n",c->id,c->kind,c->refs);
}
}
namespace a2fo::renderer {
HRESULT compile_shader(const ShaderCompileRequest& request,const std::string&,ShaderCompileResult& out){std::printf("compile %s %s %u\n",request.entry.c_str(),request.target.c_str(),hash(request.source.data(),request.source.size()));if(throw_compile)throw std::bad_alloc();out.diagnostics="fixture compile diagnostics";if(fail_compile)return E_FAIL;out.bytes={1,3,5,7};return S_OK;}
HRESULT create_compressed_texture(ID3D11Device*,const textures::CompressedImage&,ID3D11ShaderResourceView**,CompressedTextureInfo&,std::string&) noexcept {trace("compressed");return E_FAIL;}
}
int main(){setup_tables();for(unsigned failure=0;failure<=20;++failure)run(failure,false);run(0,true);throw_compile=true;run(0,false);throw_compile=false;fail_compile=true;run(0,false);std::puts("PASS full native UI mock");}
