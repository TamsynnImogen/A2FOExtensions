#include "refraction.hpp"
#include <windows.h>
#include <d3d9.h>
#include <d3dcompiler.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
namespace {
std::vector<std::string> trace;
std::string failing;
int fail_index=-1,set_index=0;
IDirect3DDevice9 device;
IDirect3DVertexDeclaration9 declaration;
IDirect3DVertexShader9 vertex;
IDirect3DPixelShader9 pixel;
IDirect3DTexture9 copy;
IDirect3DVertexBuffer9 stream;
IDirect3DIndexBuffer9 indices;
IDirect3DBaseTexture9 hull("hull");
IDirect3DSurface9 target("target"),surface("copy-surface");
IDirect3DStateBlock9 saved;
ID3DBlob vsblob("vertex-blob"),psblob("pixel-blob"),errors("errors");
int compile_count=0;
HRESULT status(const std::string& name){trace.push_back(name);return failing==name?E_FAIL:0;}
HRESULT setstatus(const std::string& name){trace.push_back(name);++set_index;
 return failing==name||(failing=="set-at"&&set_index==fail_index)?E_FAIL:0;}
std::string floats(const float*data,UINT count){std::string out;for(UINT i=0;i<count*4;++i){std::uint32_t bits;std::memcpy(&bits,data+i,4);if(i)out+=",";out+=std::to_string(bits);}return out;}
std::string hex(const std::string& s){static const char* chars="0123456789abcdef";std::string out;for(unsigned char c:s){out+=chars[c>>4];out+=chars[c&15];}return out;}
void row(const std::string& name,a2fo::cloak::Outcome result,const a2fo::cloak::Refraction& r){
 std::cout<<name<<'|'<<unsigned(result)<<'|'<<hex(r.diagnostic());
 for(auto& item:trace)std::cout<<'|'<<item;
 std::cout<<'\n';trace.clear();set_index=0;
}
a2fo::cloak::Parameters params(){a2fo::cloak::Parameters p;p.visibility=.6f;p.snapshot_key=44;
 for(int i=0;i<16;++i)p.clip[i]=float(i)/16;
 for(int i=0;i<19;++i)for(int j=0;j<4;++j)p.lights[i][j]=float(i*4+j)/8;
 p.bounds_center[0]=1.25f;p.bounds_extent[0]=2;p.bounds_extent[1]=3;p.bounds_extent[2]=4;return p;}
void scenario(const std::string& name,const std::string& fault){
 failing=fault;trace.clear();set_index=0;a2fo::cloak::Refraction r;auto p=params();a2fo::cloak::Draw d;d.num_vertices=3;d.primitive_count=1;
 row(name,r.draw(static_cast<IUnknown*>(&device),p,d),r);r.release();
 std::cout<<name<<"-release";for(auto&item:trace)std::cout<<'|'<<item;std::cout<<'\n';trace.clear();
}
}
const int IID_IDirect3DDevice9=9;
ULONG IUnknown::Release(){trace.push_back("release:"+tag);return 0;}
HRESULT IUnknown::QueryInterface(const int&,void**out){auto hr=status("QI");if(!FAILED(hr))*out=&device;return hr;}
HRESULT IDirect3DDevice9::GetDeviceCaps(D3DCAPS9*out){auto hr=status("caps");*out={};if(failing=="sm3")out->VertexShaderVersion=0;return hr;}
HRESULT IDirect3DDevice9::CreateVertexDeclaration(const D3DVERTEXELEMENT9*,IDirect3DVertexDeclaration9**out){auto hr=status("create-decl");if(!FAILED(hr))*out=&declaration;return hr;}
HRESULT IDirect3DDevice9::CreateVertexShader(const DWORD*,IDirect3DVertexShader9**out){auto hr=status("create-vs");if(!FAILED(hr))*out=&vertex;return hr;}
HRESULT IDirect3DDevice9::CreatePixelShader(const DWORD*,IDirect3DPixelShader9**out){auto hr=status("create-ps");if(!FAILED(hr))*out=&pixel;return hr;}
HRESULT IDirect3DDevice9::GetStreamSource(UINT,IDirect3DVertexBuffer9**out,UINT*offset,UINT*stride){auto hr=status("get-stream");if(!FAILED(hr)){*out=&stream;*offset=3;*stride=failing=="bad-stride"?12:68;}return hr;}
HRESULT IDirect3DDevice9::GetIndices(IDirect3DIndexBuffer9**out){auto hr=status("get-indices");if(!FAILED(hr))*out=&indices;return hr;}
HRESULT IDirect3DDevice9::GetTexture(UINT,IDirect3DBaseTexture9**out){auto hr=status("get-hull");if(!FAILED(hr))*out=&hull;return hr;}
HRESULT IDirect3DDevice9::GetRenderTarget(UINT,IDirect3DSurface9**out){auto hr=status("get-target");if(!FAILED(hr))*out=&target;return hr;}
HRESULT IDirect3DDevice9::GetViewport(D3DVIEWPORT9*out){auto hr=status("get-viewport");*out={};if(failing=="bad-viewport")out->Width=0;return hr;}
HRESULT IDirect3DSurface9::GetDesc(D3DSURFACE_DESC*out){auto hr=status("get-desc");*out={};return hr;}
HRESULT IDirect3DDevice9::CreateTexture(UINT w,UINT h,UINT,DWORD,D3DFORMAT f,UINT,IDirect3DTexture9**out,void*){auto hr=status("create-copy:"+std::to_string(w)+"x"+std::to_string(h)+":"+std::to_string(f));if(failing=="create-copy")hr=E_FAIL;if(!FAILED(hr))*out=&copy;return hr;}
HRESULT IDirect3DTexture9::GetSurfaceLevel(UINT,IDirect3DSurface9**out){auto hr=status("get-surface");if(!FAILED(hr))*out=&surface;return hr;}
HRESULT IDirect3DDevice9::StretchRect(IDirect3DSurface9*,const RECT*src,IDirect3DSurface9*,const RECT*dst,UINT){auto hr=status("stretch:"+std::to_string(src->left)+","+std::to_string(src->top)+":"+std::to_string(dst->right)+"x"+std::to_string(dst->bottom));if(failing=="stretch")hr=E_FAIL;return hr;}
HRESULT IDirect3DDevice9::CreateStateBlock(UINT,IDirect3DStateBlock9**out){auto hr=status("create-state");if(!FAILED(hr))*out=&saved;return hr;}
HRESULT IDirect3DStateBlock9::Capture(){return status("capture");}
HRESULT IDirect3DStateBlock9::Apply(){return status("restore");}
HRESULT IDirect3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9*){return setstatus("set-decl");}
HRESULT IDirect3DDevice9::SetVertexShader(IDirect3DVertexShader9*){return setstatus("set-vs");}
HRESULT IDirect3DDevice9::SetPixelShader(IDirect3DPixelShader9*){return setstatus("set-ps");}
HRESULT IDirect3DDevice9::SetVertexShaderConstantF(UINT reg,const float*data,UINT n){return setstatus("vconst:"+std::to_string(reg)+":"+std::to_string(n)+":"+floats(data,n));}
HRESULT IDirect3DDevice9::SetPixelShaderConstantF(UINT reg,const float*data,UINT n){return setstatus("pconst:"+std::to_string(reg)+":"+std::to_string(n)+":"+floats(data,n));}
HRESULT IDirect3DDevice9::SetTexture(UINT stage,IDirect3DBaseTexture9*){return setstatus("set-texture:"+std::to_string(stage));}
HRESULT IDirect3DDevice9::SetSamplerState(UINT stage,UINT kind,DWORD value){return setstatus("sampler:"+std::to_string(stage)+":"+std::to_string(kind)+":"+std::to_string(value));}
HRESULT IDirect3DDevice9::SetRenderState(UINT kind,DWORD value){return setstatus("render:"+std::to_string(kind)+":"+std::to_string(value));}
HRESULT IDirect3DDevice9::DrawIndexedPrimitive(UINT kind,int base,UINT min,UINT n,UINT start,UINT count){return status("dip:"+std::to_string(kind)+":"+std::to_string(base)+":"+std::to_string(min)+":"+std::to_string(n)+":"+std::to_string(start)+":"+std::to_string(count));}
void* ID3DBlob::GetBufferPointer(){trace.push_back("blob:"+tag);static DWORD code[4]{1,2,3,4};static char detail[]="mock compile failure";return this==&errors?static_cast<void*>(detail):static_cast<void*>(code);}
SIZE_T ID3DBlob::GetBufferSize(){return 16;}
HMODULE LoadLibraryA(const char*name){auto hr=status(std::string("load:")+name);return failing=="load-both"||FAILED(hr)?nullptr:reinterpret_cast<HMODULE>(47);}
BOOL FreeLibrary(HMODULE){trace.push_back("free-library");return 1;}
HRESULT fake_compile(LPCVOID,SIZE_T,LPCSTR,const void*,void*,LPCSTR,LPCSTR profile,UINT,UINT,ID3DBlob**out,ID3DBlob**err){
 trace.push_back(std::string("compile:")+profile);++compile_count;
 *err=&errors;if(failing==std::string("compile-")+profile)return E_FAIL;
 *out=profile[0]=='v'?&vsblob:&psblob;return 0;
}
FARPROC GetProcAddress(HMODULE,const char*name){trace.push_back(std::string("proc:")+name);
 if(failing=="proc")return nullptr;
 return reinterpret_cast<FARPROC>(&fake_compile);}
int main(int argc,char**argv){using namespace a2fo::cloak;
 if(argc>1){scenario(argv[1],argv[1]);return 0;}
 scenario("valid","");
 {Refraction r;auto p=params();Draw d;d.num_vertices=3;d.primitive_count=1;failing.clear();row("cache-first",r.draw(static_cast<IUnknown*>(&device),p,d),r);
 row("cache-hit",r.draw(static_cast<IUnknown*>(&device),p,d),r);p.snapshot_key=0;
 row("zero-key",r.draw(static_cast<IUnknown*>(&device),p,d),r);r.release();
 trace.clear();p.snapshot_key=45;
 row("after-release",r.draw(static_cast<IUnknown*>(&device),p,d),r);
 IUnknown alternate("device8-alternate");
 row("new-source",r.draw(&alternate,p,d),r);r.release();trace.clear();}
 for(auto fault:{"QI","caps","sm3","compile-vs_3_0","compile-ps_3_0","create-decl",
 "create-vs","create-ps","get-stream","get-indices","get-hull","get-target","get-viewport",
 "get-desc","bad-stride","bad-viewport","create-copy","get-surface","stretch",
 "create-state","capture","set-decl","set-vs","set-ps","restore","set-at"}){
 fail_index=7;scenario(fault,fault);
 }
 {Refraction r;auto p=params();Draw d;d.num_vertices=3;d.primitive_count=1;d.primitive_count=1;failing="dip:4:0:0:3:0:1";
 row("draw-fail",r.draw(static_cast<IUnknown*>(&device),p,d),r);r.release();trace.clear();}
 {Refraction r;auto p=params();Draw d;d.num_vertices=3;d.primitive_count=1;p.bounds_extent[2]=0;trace.clear();
 row("invalid",r.draw(static_cast<IUnknown*>(&device),p,d),r);r.release();}
}
