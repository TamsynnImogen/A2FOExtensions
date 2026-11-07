#ifdef A2FO_STATE_ONLY
#include "state_mock.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
namespace a2fo::renderer::game {std::vector<std::string> state_trace;}
namespace {
using namespace a2fo::renderer::game;
std::string hx(const std::vector<BYTE>& bytes){std::ostringstream o;o<<std::hex<<std::setfill('0');for(auto b:bytes)o<<std::setw(2)<<unsigned(b);return o.str();}
void row(const char* name,HRESULT hr,Device& d){std::cout<<name<<'|'<<std::hex<<static_cast<DWORD>(hr)<<std::dec<<'|'<<d.next_block<<':'<<d.blocks.size()<<':'<<bool(d.recording)<<'|'<<d.current.gpu.render[7]<<':'<<d.current.gpu.stage[2][4]<<':'<<d.current.textures[3].value<<':'<<d.current.clip.dwFlags;for(auto&x:state_trace)std::cout<<'|'<<x;std::cout<<'\n';state_trace.clear();}
void surfrow(const char* name,HRESULT hr,Surface8& a,Surface8& b){std::cout<<name<<'|'<<std::hex<<static_cast<DWORD>(hr)<<std::dec<<'|'<<hx(a.bytes)<<'|'<<hx(b.bytes);for(auto&x:state_trace)std::cout<<'|'<<x;std::cout<<'\n';state_trace.clear();}
}
int main(){using namespace a2fo::renderer::game;Device d;DWORD token=99;
 row("end-invalid",d.EndStateBlock(&token),d);row("create-null",d.CreateStateBlock(D3DSBT_ALL,nullptr),d);
 row("create-typed",d.CreateStateBlock(D3DSBT_PIXELSTATE,&token),d);
 d.current.gpu.render[7]=41;d.current.gpu.stage[2][4]=19;d.current.textures[3].value=8;
 row("begin",d.BeginStateBlock(),d);row("begin-again",d.BeginStateBlock(),d);
 D3DCLIPSTATUS8 clip{3,5};row("clip-set",d.SetClipStatus(&clip),d);
 float plane[4]{1,2,3,4};row("plane-set",d.SetClipPlane(2,plane),d);
 d.recording->data.gpu.render[7]=77;d.recording->mask.render.set(7);
 d.recording->data.gpu.stage[2][4]=88;d.recording->mask.stage[2].set(4);
 d.recording->data.textures[3].value=9;d.recording->mask.textures.set(3);
 row("end",d.EndStateBlock(&token),d);
 d.current.gpu.render[7]=0;d.current.gpu.stage[2][4]=0;d.current.textures[3].value=0;
 row("apply",d.ApplyStateBlock(token),d);
 d.current.gpu.render[7]=12;row("capture",d.CaptureStateBlock(token),d);
 d.current.gpu.render[7]=0;row("apply-captured",d.ApplyStateBlock(token),d);
 row("delete",d.DeleteStateBlock(token),d);row("apply-deleted",d.ApplyStateBlock(token),d);
 row("create-all",d.CreateStateBlock(D3DSBT_ALL,&token),d);
 d.current.gpu.render[7]=53;row("apply-all",d.ApplyStateBlock(token),d);
 d.next_block=0;row("create-wrap",d.CreateStateBlock(D3DSBT_ALL,&token),d);
 row("begin-wrap",d.BeginStateBlock(),d);row("end-wrap",d.EndStateBlock(&token),d);
 d.next_block=0;row("end-zero",d.BeginStateBlock(),d);row("end-zero-token",d.EndStateBlock(&token),d);
 row("clip-invalid",d.SetClipPlane(6,plane),d);D3DCLIPSTATUS8 got{};row("clip-get",d.GetClipStatus(&got),d);
 float gotplane[4]{};row("plane-get",d.GetClipPlane(2,gotplane),d);
 Texture8 a,b;a.mips={{{1,2}},{{3,4}}};b.mips={{{0}},{{0}}};a.locked={false,false};b.locked={false,false};
 Resource ra{Resource::texture,&a},rb{Resource::texture,&b};d.resources[&a]=&ra;d.resources[&b]=&rb;
 row("texture-copy",d.UpdateTexture(reinterpret_cast<IDirect3DBaseTexture8*>(&a),reinterpret_cast<IDirect3DBaseTexture8*>(&b)),d);
 std::cout<<"texture-bytes|"<<hx(b.mips[0].bytes)<<'|'<<hx(b.mips[1].bytes)<<'\n';
 b.locked[1]=true;row("texture-locked",d.UpdateTexture(reinterpret_cast<IDirect3DBaseTexture8*>(&a),reinterpret_cast<IDirect3DBaseTexture8*>(&b)),d);b.locked[1]=false;
 b.upload_result=std::int32_t(0x80004005);row("texture-upload-fail",d.UpdateTexture(reinterpret_cast<IDirect3DBaseTexture8*>(&a),reinterpret_cast<IDirect3DBaseTexture8*>(&b)),d);
 b.width=8;row("texture-mismatch",d.UpdateTexture(reinterpret_cast<IDirect3DBaseTexture8*>(&a),reinterpret_cast<IDirect3DBaseTexture8*>(&b)),d);
 Surface8 s,t;s.bytes.resize(64);t.bytes.resize(64);for(int i=0;i<64;++i)s.bytes[i]=BYTE(i);
 Resource rs{Resource::surface,&s},rt{Resource::surface,&t};d.resources[&s]=&rs;d.resources[&t]=&rt;
 auto*sp=reinterpret_cast<IDirect3DSurface8*>(&s);auto*tp=reinterpret_cast<IDirect3DSurface8*>(&t);
 surfrow("copy-full",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);
 std::fill(t.bytes.begin(),t.bytes.end(),0);RECT r{1,1,3,3};POINT p{0,0};
 surfrow("copy-rect",d.CopyRects(sp,&r,1,tp,&p),s,t);r={-1,0,2,2};
 surfrow("copy-invalid",d.CopyRects(sp,&r,1,tp,&p),s,t);
 surfrow("copy-args",d.CopyRects(sp,&r,0,tp,&p),s,t);
 s.lock_result=std::int32_t(0x80004005);surfrow("copy-source-lock",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);s.lock_result=0;
 t.lock_result=std::int32_t(0x80004005);surfrow("copy-dest-lock",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);t.lock_result=0;
 t.unlock_result=std::int32_t(0x80004005);surfrow("copy-unlock-fail",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);t.unlock_result=0;
 t.desc.Format=D3DFMT_DXT1;surfrow("copy-format",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);t.desc.Format=s.desc.Format;
 t.target=std::make_unique<int>(1);surfrow("copy-target-dest",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);s.target=std::make_unique<int>(2);surfrow("copy-target-both",d.CopyRects(sp,nullptr,0,tp,nullptr),s,t);
 s.target.reset();t.target.reset();s.desc.Format=t.desc.Format=D3DFMT_DXT1;s.desc.Width=t.desc.Width=8;s.desc.Height=t.desc.Height=8;s.pitch=t.pitch=16;s.bytes.resize(32);t.bytes.assign(32,0);for(int i=0;i<32;++i)s.bytes[i]=BYTE(i+1);
 RECT bc_rect{4,0,8,8};POINT bc_point{0,0};surfrow("copy-bc",d.CopyRects(sp,&bc_rect,1,tp,&bc_point),s,t);bc_point={1,0};surfrow("copy-bc-invalid",d.CopyRects(sp,&bc_rect,1,tp,&bc_point),s,t);
 row("front",d.GetFrontBuffer(nullptr),d);
}
#else
#include "bytecode_cache.hpp"
#include "bc_catalog.hpp"
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Open {std::string path;std::size_t at=0;bool write=false;};
std::map<std::string,std::vector<std::uint8_t>> files;
std::vector<std::string> trace;
std::string failing;
std::vector<std::uint8_t> shader_bytes{'D','X','B','C',1,2,3,4};
std::vector<std::uint8_t> diagnostic_bytes{'w','a','r','n'};
ID3DBlob shader_blob,error_blob;
int writes=0,clock_calls=0;
DWORD last_error=5;
void add(const std::string& line){trace.push_back(line);}
std::string hex(const std::uint8_t* p,std::size_t n){std::ostringstream o;o<<std::hex<<std::setfill('0');for(std::size_t i=0;i<n;++i)o<<std::setw(2)<<unsigned(p[i]);return o.str();}
std::string hex(const std::vector<std::uint8_t>& v){return hex(v.data(),v.size());}
std::string hex(const std::string& v){return hex(reinterpret_cast<const std::uint8_t*>(v.data()),v.size());}
void dump(const std::string& name,HRESULT hr,const a2fo::renderer::ShaderCompileResult& out){
 std::cout<<name<<'|'<<std::hex<<static_cast<std::uint32_t>(hr)<<std::dec<<'|'<<hex(out.bytes)
 <<'|'<<hex(out.diagnostics)<<'|'<<out.cache_hit<<','<<out.cache_written<<','<<out.compile_us;
 for(const auto& x:trace)std::cout<<"|"<<x;
 for(const auto& [path,data]:files)std::cout<<"|FILE:"<<path<<':'<<hex(data);
 std::cout<<'\n';trace.clear();
}
void reset(const std::string& fail={}){files.clear();trace.clear();failing=fail;writes=clock_calls=0;shader_bytes={'D','X','B','C',1,2,3,4};diagnostic_bytes={'w','a','r','n'};last_error=5;}
} // namespace
void* ID3DBlob::GetBufferPointer(){return this==&shader_blob?shader_bytes.data():diagnostic_bytes.data();}
SIZE_T ID3DBlob::GetBufferSize(){return this==&shader_blob?shader_bytes.size():diagnostic_bytes.size();}
UINT ID3DBlob::Release(){add(this==&shader_blob?"release-shader":"release-errors");return 0;}
BOOL FreeLibrary(HMODULE){add("free-module");return 1;}
BOOL CloseHandle(HANDLE h){auto* file=static_cast<Open*>(h);add("close:"+file->path);delete file;return 1;}
BOOL DeleteFileA(const char* path){add(std::string("delete:")+path);files.erase(path);return 1;}
HANDLE CreateFileA(const char* path,DWORD access,DWORD,void*,DWORD creation,DWORD,HANDLE){
 add(std::string("open:")+path+":"+std::to_string(access)+":"+std::to_string(creation));
 if((creation==OPEN_EXISTING&&(files.count(path)==0||failing=="open-read"))||
    (creation==CREATE_NEW&&(files.count(path)!=0||failing=="open-temp")))return INVALID_HANDLE_VALUE;
 if(creation==CREATE_NEW)files[path]={};return new Open{path,0,creation==CREATE_NEW};
}
BOOL GetFileSizeEx(HANDLE h,LARGE_INTEGER* size){add("file-size");if(failing=="file-size")return 0;size->QuadPart=files[static_cast<Open*>(h)->path].size();return 1;}
BOOL ReadFile(HANDLE h,void* data,DWORD size,DWORD* done,void*){auto* f=static_cast<Open*>(h);add("read:"+std::to_string(size));if(failing=="read")return 0;
 auto& bytes=files[f->path];*done=std::min<std::size_t>(size,bytes.size()-f->at);if(*done)std::memcpy(data,bytes.data()+f->at,*done);f->at+=*done;return 1;}
BOOL WriteFile(HANDLE h,const void* data,DWORD size,DWORD* done,void*){auto* f=static_cast<Open*>(h);++writes;add("write:"+std::to_string(size));
 if(failing=="write"&&writes==2)return 0;auto& bytes=files[f->path];bytes.insert(bytes.end(),static_cast<const std::uint8_t*>(data),static_cast<const std::uint8_t*>(data)+size);*done=size;return 1;}
BOOL FlushFileBuffers(HANDLE){add("flush");return failing!="flush";}
BOOL MoveFileExA(const char* from,const char* to,DWORD flags){add(std::string("move:")+from+":"+to+":"+std::to_string(flags));if(failing=="move")return 0;files[to]=files[from];files.erase(from);return 1;}
BOOL CreateDirectoryA(const char* path,void*){add(std::string("mkdir:")+path);if(failing=="mkdir"||failing=="mkdir-exists"){last_error=failing=="mkdir"?5:ERROR_ALREADY_EXISTS;return 0;}return 1;}
DWORD GetLastError(){add("last-error");return last_error;}
DWORD GetCurrentProcessId(){add("pid");return 42;}
DWORD GetCurrentThreadId(){add("tid");return 7;}
LONG InterlockedIncrement(volatile LONG* p){add("serial");return ++*p;}
HMODULE LoadLibraryA(const char* name){add(std::string("load:")+name);if(failing=="load-both"||(failing=="load-47"&&std::strstr(name,"47")))return nullptr;return reinterpret_cast<HMODULE>(std::strstr(name,"47")?47:43);}
HRESULT WINAPI fake_compile(LPCVOID source,SIZE_T size,LPCSTR,const D3D_SHADER_MACRO*,ID3DInclude*,LPCSTR entry,LPCSTR target,UINT flags,UINT,ID3DBlob** shader,ID3DBlob** errors){
 add("compile:"+std::to_string(size)+":"+entry+":"+target+":"+std::to_string(flags));
 (void)source;*errors=&error_blob;if(failing=="compile")return E_FAIL;
 if(failing!="no-shader")*shader=&shader_blob;return S_OK;
}
FARPROC GetProcAddress(HMODULE,const char* name){add(std::string("proc:")+name);return failing=="proc"?nullptr:reinterpret_cast<FARPROC>(&fake_compile);}
DWORD GetModuleFileNameA(HMODULE module,char* out,DWORD cap){add("module-path");if(failing=="identity")return 0;
 const std::string path=module==reinterpret_cast<HMODULE>(47)?"C:\\d3dcompiler_47.dll":"C:\\d3dcompiler_43.dll";
 if(path.size()+1>cap)return cap;std::memcpy(out,path.c_str(),path.size()+1);return path.size();}
BOOL GetFileAttributesExA(const char*,int,WIN32_FILE_ATTRIBUTE_DATA* out){add("module-attrs");if(failing=="identity-attrs")return 0;
 out->nFileSizeLow=123;out->ftLastWriteTime.dwLowDateTime=456;return 1;}
BOOL QueryPerformanceFrequency(LARGE_INTEGER* x){add("frequency");x->QuadPart=1000000;return 1;}
BOOL QueryPerformanceCounter(LARGE_INTEGER* x){add("counter");x->QuadPart=clock_calls++*50;return 1;}
namespace {void catalog_tests();}
int main(){using namespace a2fo::renderer;
 ShaderCompileRequest req{"float4 VS() : SV_Position { return 0; }","VS","vs_5_0",0};
 for(const auto& test:std::vector<std::string>{"","load-47","load-both","proc","identity","identity-attrs","compile","no-shader","mkdir","mkdir-exists","open-temp","write","flush","move"}){
  reset(test);ShaderCompileResult out;HRESULT hr=compile_shader(req,"C:\\Cache",out);dump("first-"+test,hr,out);
 }
 reset();ShaderCompileResult out;HRESULT hr=compile_shader(req,"C:\\Cache",out);dump("cache-first",hr,out);
 hr=compile_shader(req,"C:\\Cache",out);dump("cache-hit",hr,out);
 for(const auto& test:std::vector<std::string>{"header","checksum","truncated","read","file-size","open-read"}){
  failing=test;if(test=="header")files.begin()->second[0]='X';if(test=="checksum")files.begin()->second.back()^=1;
  if(test=="truncated")files.begin()->second.resize(10);
  hr=compile_shader(req,"C:\\Cache",out);dump("cache-"+test,hr,out);
  reset();compile_shader(req,"C:\\Cache",out);trace.clear();
 }
 reset();req.source.push_back('\0');hr=compile_shader(req,"C:\\Cache",out);dump("invalid-nul",hr,out);
 req.source.pop_back();req.entry.clear();hr=compile_shader(req,"C:\\Cache",out);dump("invalid-entry",hr,out);
 req.entry="VS";hr=compile_shader(req,"",out);dump("empty-cache",hr,out);
 catalog_tests();
}

namespace a2fo::renderer::textures {
HRESULT parse_compressed_dds(const void* data,std::size_t size,CompressedImage& out,
                             std::string& error) noexcept {
 add("parse-dds:"+std::to_string(size));
 if(failing=="parse-dds"||size!=144||std::memcmp(data,"DDS ",4)!=0){error="bad DDS";return E_INVALIDARG;}
 const auto* bytes=static_cast<const std::uint8_t*>(data);
 CompressedImage image;image.format=bytes[4]==83?DXGI_FORMAT_BC5_UNORM:DXGI_FORMAT_BC7_UNORM;
 image.width=4;image.height=4;image.mips.push_back({4,4,16,16,0});
 image.bytes.assign(bytes+128,bytes+144);out=std::move(image);error.clear();return S_OK;
}
}
namespace {
std::uint64_t fnv(const std::uint8_t* p,std::size_t n){std::uint64_t h=14695981039346656037ULL;
 for(std::size_t i=0;i<n;++i)h=(h^p[i])*1099511628211ULL;return h;}
std::string h64(std::uint64_t n){std::ostringstream o;o<<std::hex<<std::setfill('0')<<std::setw(16)<<n;return o.str();}
std::vector<std::uint8_t> dds(int i){std::vector<std::uint8_t> raw(144);raw[0]='D';raw[1]='D';raw[2]='S';raw[3]=' ';
 raw[4]=98;raw[128]=static_cast<std::uint8_t>(i);
 raw[129]=static_cast<std::uint8_t>(i>>8);
 for(int j=2;j<16;++j)raw[128+j]=static_cast<std::uint8_t>(i*17+j);
 return raw;}
std::string catalog_root(int i){return "build/rust-migration-stage5/render-cache-work/catalog-root"+std::to_string(i);}
void write_catalog(int id,const std::string& variant){const std::string root=catalog_root(id);
 std::ofstream output(root+"\\.a2fo-dx11\\catalog.txt",std::ios::binary);
 output<<(variant=="header"?"BAD HEADER":"A2FO_BC_CATALOG 2")<<'\n';
 output<<"COUNT "<<(variant=="count"?406:407)<<'\n';
 for(int i=0;i<(variant=="truncated"?2:407);++i){const auto raw=dds(i);
  const auto top=fnv(raw.data()+128,16),file=fnv(raw.data(),raw.size());
  const std::string name=i==0?"ship.dds":"entry"+std::to_string(i)+".dds";
  output<<(variant=="entry"&&i==1?"bad/name.dds":name)<<"|4|4|1|98|raw|"
      <<h64(top)<<'|'<<h64(file)<<'|'<<(variant=="mip"&&i==1?"wronghash0000000":h64(top))<<'\n';
 }
 if(variant=="extra")output<<"unexpected\n";
}
void catalog_row(const std::string& name,bool ok,const std::string& error,
                 const a2fo::renderer::textures::CompressedImage* image=nullptr,
                 const std::string& asset={}){
 std::cout<<name<<'|'<<ok<<'|'<<hex(error)<<'|'<<asset;
 if(image)std::cout<<'|'<<image->width<<'x'<<image->height<<':'<<image->mips.size()<<':'<<hex(image->bytes);
 for(const auto& x:trace)std::cout<<'|'<<x;
 std::cout<<'\n';trace.clear();
}
void catalog_tests(){using namespace a2fo::renderer::game;
 std::string error;int id=0;
 for(const auto& variant:std::vector<std::string>{"header","count","entry","mip","truncated","extra","valid"}){
  reset();write_catalog(id,variant);const bool loaded=load_bc_catalog(catalog_root(id),
    [](const std::string& s){add("LOG:"+s);},error);
  catalog_row("catalog-"+variant,loaded,error);++id;
 }
 const std::string root=catalog_root(id-1)+"\\";
 const auto raw=dds(0);files[root+"Textures\\ship.dds"]=raw;
 a2fo::renderer::textures::CompressedImage image;bool normal=true;std::string asset="old";
 bool ok=load_bc_asset("Textures\\ship.dds",image,normal,asset,error);
 catalog_row("asset-valid",ok,error,&image,asset);
 ok=load_bc_asset("missing.dds",image,normal,asset,error);
 catalog_row("asset-miss",ok,error,&image,asset);
 const auto path=bc_transport_path("C:/foo/ship.dds");
 catalog_row("transport-valid",!path.empty(),path);
 const auto no_path=bc_transport_path("C:/foo/unlisted.dds");
 catalog_row("transport-miss",!no_path.empty(),no_path);
 ok=match_bc_texture(4,4,1,raw.data()+128,16,image,normal,asset,error);
 catalog_row("match-valid",ok,error,&image,asset);
 std::vector<std::uint8_t> no_match(16,0xff);
 ok=match_bc_texture(4,4,1,no_match.data(),no_match.size(),image,normal,asset,error);
 catalog_row("match-miss",ok,error,&image,asset);
 files[root+"Textures\\ship.dds"][0]='X';
 ok=load_bc_asset("ship.dds",image,normal,asset,error);
 catalog_row("asset-corrupt",ok,error,&image,asset);
 ok=load_bc_catalog(catalog_root(id-1),nullptr,error);
 catalog_row("catalog-same-root",ok,error);
 ok=load_bc_catalog("different-root",nullptr,error);
 catalog_row("catalog-different-root",ok,error);
}
}

#endif
