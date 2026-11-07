#include "../build/rust-migration-stage5/d3dx-work/mock_d3dx.hpp"
#include <cstdio>
#include <cctype>
#include <memory>
#include <stdexcept>
using namespace a2fo::renderer;
namespace {
using Info=HRESULT(WINAPI*)(LPCSTR,void*);
using Create=HRESULT(WINAPI*)(IDirect3DDevice8*,LPCSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture8**);
HRESULT WINAPI info_original(LPCSTR,void*);
HRESULT WINAPI create_original(IDirect3DDevice8*,LPCSTR,UINT,UINT,UINT,DWORD,D3DFORMAT,D3DPOOL,DWORD,DWORD,D3DCOLOR,void*,PALETTEENTRY*,IDirect3DTexture8**);
struct Image {std::vector<BYTE> bytes;Image():bytes(0x3000){} BYTE* base(){return bytes.data();}};
std::vector<std::unique_ptr<Image>> images;
std::vector<std::string> calls;
IDirect3DDevice8 device;IDirect3DTexture8 texture;
int protect_call=0,protect_fail_at=0,release_count=0;
bool bc_device=true,bc_valid=true,bc_texture=true,return_texture=true;
bool transport_throw=false,is_bc_throw=false,load_throw=false,bc_texture_throw=false;
bool info_throw=false,create_throw=false,logger_throw=false;
HRESULT info_result=S_OK,create_result=S_OK;
std::string name_of(const void* p) {
 auto at=reinterpret_cast<std::uintptr_t>(p);
 for(std::size_t i=0;i<images.size();++i) {
  auto base=reinterpret_cast<std::uintptr_t>(images[i]->bytes.data());
  if(at>=base&&at-base<images[i]->bytes.size())
   return "image"+std::to_string(i)+"+"+std::to_string(at-base);
 }
 if(!p)return "null";
 return "outside";
}
void dump(const char *scenario) {
 std::printf("scenario %s\n",scenario);
 for(const auto &s:calls)std::printf("call %s\n",s.c_str());
 calls.clear();
}
void write_bytes(Image &image,std::size_t at,const void *source,std::size_t n){
 std::memcpy(image.base()+at,source,n);
}
template<class T>void write_value(Image &image,std::size_t at,const T &value){
 write_bytes(image,at,&value,sizeof(value));
}
Image &new_image() {
 auto image=std::make_unique<Image>();auto &out=*image;images.push_back(std::move(image));
 auto *dos=reinterpret_cast<IMAGE_DOS_HEADER*>(out.base());
 dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x100;
 auto *nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(out.base()+0x100);
 nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
 nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;
 nt->OptionalHeader.SizeOfImage=static_cast<DWORD>(out.bytes.size());
 nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress=0x300;
 nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size=2*sizeof(IMAGE_IMPORT_DESCRIPTOR);
 auto *desc=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(out.base()+0x300);
 desc->Name=0x500;desc->OriginalFirstThunk=0x600;desc->FirstThunk=0x700;
 constexpr char dll[]="D3DX81ab.dll";
 write_bytes(out,0x500,dll,sizeof dll);
 const DWORD thunks[]={0x800,0x850,0};write_bytes(out,0x600,thunks,sizeof thunks);
 constexpr char info[]="D3DXGetImageInfoFromFileA";
 constexpr char create[]="D3DXCreateTextureFromFileExA";
 write_bytes(out,0x802,info,sizeof info);write_bytes(out,0x852,create,sizeof create);
 Info original_info=nullptr;
 Create original_create=nullptr;
 original_info=info_original;original_create=create_original;
 write_value(out,0x700,original_info);write_value(out,0x704,original_create);
 return out;
}
void clear_behavior(){bc_device=bc_valid=bc_texture=return_texture=true;
 transport_throw=is_bc_throw=load_throw=bc_texture_throw=false;
 info_throw=create_throw=logger_throw=false;
 info_result=create_result=S_OK;protect_call=protect_fail_at=release_count=0;}
bool install(const char *name,Image *image,game::Log logger) {
 bool ok=game::install_d3dx_file_bridge(image?reinterpret_cast<HMODULE>(image->base()):nullptr,logger);
 std::printf("install %s %d",name,ok);
 if(image) {
  Info info{};Create create{};
  std::memcpy(&info,image->base()+0x700,sizeof info);
  std::memcpy(&create,image->base()+0x704,sizeof create);
  std::printf(" info_changed=%d create_changed=%d",info!=info_original,create!=create_original);
 }
 std::putchar('\n');dump(name);return ok;
}
void invoke_info(Image &image,const char *name,const char *path) {
 Info fn{};std::memcpy(&fn,image.base()+0x700,sizeof fn);
 int marker=7;auto hr=fn(path,&marker);
 std::printf("info %s %08x marker=%d\n",name,static_cast<unsigned>(hr),marker);
 dump(name);
}
void invoke_create(Image &image,const char *name,const char *path,UINT width,UINT height,
 UINT mips,DWORD usage,D3DFORMAT format,bool has_output=true) {
 Create fn{};std::memcpy(&fn,image.base()+0x704,sizeof fn);
 IDirect3DTexture8 *output=reinterpret_cast<IDirect3DTexture8*>(0x1);
 auto hr=fn(&device,path,width,height,mips,usage,format,D3DPOOL_MANAGED,1,2,3,
  nullptr,nullptr,has_output?&output:nullptr);
 std::printf("create %s %08x output=%d released=%d\n",name,static_cast<unsigned>(hr),
  output==&texture,release_count);
 dump(name);
}
void logger(const std::string &line){calls.push_back("log "+line);
 if(logger_throw)throw std::runtime_error("mock log");}
}
SIZE_T mock_VirtualQuery(LPCVOID pointer,MEMORY_BASIC_INFORMATION* out,SIZE_T) {
 calls.push_back("VirtualQuery "+name_of(pointer));
 auto at=reinterpret_cast<std::uintptr_t>(pointer);
 for(const auto &image:images) {
  auto base=reinterpret_cast<std::uintptr_t>(image->bytes.data());
  if(at>=base&&at-base<image->bytes.size()) {
   out->BaseAddress=image->bytes.data();out->RegionSize=image->bytes.size();
   out->State=MEM_COMMIT;out->Protect=PAGE_READWRITE;return sizeof(*out);
  }
 }
 return 0;
}
BOOL mock_VirtualProtect(LPVOID pointer,SIZE_T n,DWORD protection,PDWORD old) {
 ++protect_call;
 bool ok=protect_call!=protect_fail_at;
 calls.push_back("VirtualProtect "+name_of(pointer)+" size="+std::to_string(n)+
  " protection="+std::to_string(protection)+" ok="+std::to_string(ok));
 if(ok)*old=PAGE_READONLY;
 return ok;
}
PVOID mock_InterlockedExchangePointer(PVOID volatile* slot,PVOID value) {
 calls.push_back("InterlockedExchangePointer "+name_of(const_cast<PVOID*>(slot)));
 PVOID old=*slot;*slot=value;return old;
}
LONG mock_InterlockedIncrement(LONG volatile* value) {
 ++*value;calls.push_back("InterlockedIncrement value="+std::to_string(*value));return *value;
}
int mock_strnicmp(const char *a,const char *b,std::size_t n) {
 calls.push_back("strnicmp n="+std::to_string(n));
 for(std::size_t i=0;i<n;++i) {
  int x=std::tolower(static_cast<unsigned char>(a[i]));
  int y=std::tolower(static_cast<unsigned char>(b[i]));
  if(x!=y||!a[i]||!b[i])return x-y;
 }
 return 0;
}
ULONG IDirect3DTexture8::Release(){++release_count;calls.push_back("Texture.Release");return 0;}
namespace a2fo::renderer::game {
std::string bc_transport_path(const char *path) {
 calls.push_back("bc_transport_path "+std::string(path?path:"<null>"));
 if(transport_throw)throw std::runtime_error("mock transport");
 return path&&std::string(path).find(".bc")!=std::string::npos
  ?std::string(path)+".transport":std::string{};
}
bool is_bc_device(IDirect3DDevice8* ptr){
 calls.push_back("is_bc_device "+std::to_string(ptr==&device));
 if(is_bc_throw)throw std::runtime_error("mock BC device");
 return bc_device;
}
bool load_bc_asset(const char *path,textures::CompressedImage &image,bool &normal,
 std::string &asset,std::string &error) {
 calls.push_back("load_bc_asset "+std::string(path?path:"<null>"));
 if(load_throw)throw std::runtime_error("mock BC load");
 if(!bc_valid){error="mock invalid DDS";return false;}
 image.width=8;image.height=8;image.mips.resize(3);
 normal=false;asset="mock";return true;
}
bool is_bc_texture(IDirect3DDevice8* dev,IDirect3DTexture8* ptr){
 calls.push_back("is_bc_texture "+std::to_string(dev==&device)+" texture="+
  std::to_string(ptr==&texture));
 if(bc_texture_throw)throw std::runtime_error("mock BC texture");
 return bc_texture;
}
}
namespace {
HRESULT WINAPI info_original(LPCSTR path,void *info) {
 calls.push_back("original_info path="+std::string(path?path:"<null>")+
  " info="+std::to_string(info!=nullptr));
 if(info_throw)throw std::runtime_error("mock original info");
 return info_result;
}
HRESULT WINAPI create_original(IDirect3DDevice8* dev,LPCSTR path,UINT width,UINT height,
 UINT mips,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mip_filter,
 D3DCOLOR key,void* info,PALETTEENTRY* palette,IDirect3DTexture8** output) {
 calls.push_back("original_create path="+std::string(path?path:"<null>")+
  " device="+std::to_string(dev==&device)+" size="+std::to_string(width)+"x"+
  std::to_string(height)+" mips="+std::to_string(mips)+" usage="+
  std::to_string(usage)+" format="+std::to_string(format)+" pool="+
  std::to_string(pool)+" filters="+std::to_string(filter)+","+
  std::to_string(mip_filter)+" key="+std::to_string(key)+
  " info="+std::to_string(info!=nullptr)+" palette="+std::to_string(palette!=nullptr)+
  " output="+std::to_string(output!=nullptr));
 if(output)*output=return_texture?&texture:nullptr;
 if(create_throw)throw std::runtime_error("mock original create");
 return create_result;
}
void invalid_cases() {
 clear_behavior();calls.clear();install("null_module",nullptr,logger);
 struct Case {const char *name;void (*change)(Image&);};
 const Case cases[]{
  {"bad_dos",[](Image &i){reinterpret_cast<IMAGE_DOS_HEADER*>(i.base())->e_magic=0;}},
  {"negative_lfanew",[](Image &i){reinterpret_cast<IMAGE_DOS_HEADER*>(i.base())->e_lfanew=-1;}},
  {"large_lfanew",[](Image &i){reinterpret_cast<IMAGE_DOS_HEADER*>(i.base())->e_lfanew=0x100001;}},
  {"bad_nt",[](Image &i){reinterpret_cast<IMAGE_NT_HEADERS32*>(i.base()+0x100)->Signature=0;}},
  {"bad_machine",[](Image &i){reinterpret_cast<IMAGE_NT_HEADERS32*>(i.base()+0x100)->FileHeader.Machine=0;}},
  {"bad_magic",[](Image &i){reinterpret_cast<IMAGE_NT_HEADERS32*>(i.base()+0x100)->OptionalHeader.Magic=0;}},
  {"no_import",[](Image &i){reinterpret_cast<IMAGE_NT_HEADERS32*>(i.base()+0x100)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress=0;}},
  {"wrong_dll",[](Image &i){i.base()[0x500]='X';}},
  {"null_info",[](Image &i){std::memset(i.base()+0x700,0,4);}},
  {"bad_name_rva",[](Image &i){DWORD v=0x3fff;write_value(i,0x600,v);}},
  {"missing_create",[](Image &i){DWORD v=0;write_value(i,0x604,v);}},
 };
 for(const auto &test:cases){auto &image=new_image();test.change(image);install(test.name,&image,logger);}
}
void bridge_cases() {
 clear_behavior();calls.clear();auto &image=new_image();
 if(!install("valid",&image,logger))return;
 install("idempotent",&image,logger);
 invoke_info(image,"info_plain","C:\\Assets\\plain.dds");
 invoke_info(image,"info_bc","C:\\Assets\\unit.bc");
 info_result=E_FAIL;invoke_info(image,"info_fail","C:\\Assets\\failed.bc");info_result=S_OK;
 invoke_create(image,"create_plain","C:\\Assets\\plain.dds",8,8,1,0,D3DFMT_UNKNOWN);
 bc_device=false;invoke_create(image,"create_non_bc_device","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);bc_device=true;
 invoke_create(image,"create_default_mips","C:\\Assets\\unit.bc",8,8,0,0,D3DFMT_UNKNOWN);
 invoke_create(image,"create_all_mips","C:\\Assets\\unit.bc",8,8,0xffffffffu,0,D3DFMT_UNKNOWN);
 invoke_create(image,"create_reduce_mips","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_DXT4);
 invoke_create(image,"bad_width","C:\\Assets\\unit.bc",7,8,1,0,D3DFMT_UNKNOWN);
 invoke_create(image,"bad_height","C:\\Assets\\unit.bc",8,9,1,0,D3DFMT_UNKNOWN);
 invoke_create(image,"too_many_mips","C:\\Assets\\unit.bc",8,8,4,0,D3DFMT_UNKNOWN);
 invoke_create(image,"bad_format","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_DXT1);
 invoke_create(image,"bad_usage","C:\\Assets\\unit.bc",8,8,1,1,D3DFMT_UNKNOWN);
 bc_valid=false;invoke_create(image,"bad_bc_asset","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);bc_valid=true;
 bc_texture=false;invoke_create(image,"decoded_fallback","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);bc_texture=true;
 return_texture=false;invoke_create(image,"null_texture","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);return_texture=true;
 create_result=E_FAIL;invoke_create(image,"original_fail","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);create_result=S_OK;
 invoke_create(image,"no_output","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN,false);
 for(int i=0;i<17;++i)invoke_info(image,"report_limit","C:\\Assets\\unit.bc");
}
void reservation_cases() {
 clear_behavior();calls.clear();auto &first=new_image();protect_fail_at=3;
 install("second_patch_fail",&first,logger);
 protect_fail_at=0;protect_call=0;install("reserved_retry",&first,logger);
 clear_behavior();auto &second=new_image();protect_fail_at=2;
 install("restore_fail",&second,logger);
 protect_fail_at=0;protect_call=0;install("restore_retry",&second,logger);
}
void exception_cases() {
 clear_behavior();calls.clear();auto &image=new_image();
 if(!install("exception_ready",&image,logger))return;
 transport_throw=true;invoke_info(image,"info_transport_throw","C:\\Assets\\unit.bc");
 invoke_create(image,"create_transport_throw","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);
 transport_throw=false;
 is_bc_throw=true;invoke_create(image,"is_bc_throw","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);
 is_bc_throw=false;
 load_throw=true;invoke_create(image,"load_throw","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);
 load_throw=false;
 info_throw=true;invoke_info(image,"info_original_throw","C:\\Assets\\unit.bc");info_throw=false;
 create_throw=true;invoke_create(image,"create_original_throw","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);
 create_throw=false;
 bc_texture_throw=true;invoke_create(image,"bc_texture_throw","C:\\Assets\\unit.bc",8,8,1,0,D3DFMT_UNKNOWN);
 bc_texture_throw=false;
 logger_throw=true;invoke_info(image,"info_logger_throw","C:\\Assets\\unit.bc");
 logger_throw=false;
 auto &next=new_image();logger_throw=true;
 install("install_logger_throw",&next,logger);
 logger_throw=false;
 install("install_logger_retry",&next,logger);
}
}
int main(){invalid_cases();bridge_cases();reservation_cases();exception_cases();}
