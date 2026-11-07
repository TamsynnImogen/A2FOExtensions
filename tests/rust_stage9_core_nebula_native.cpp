// Actual whole core renderer; COM, files and checked reads are inert mocks.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
static HANDLE WINAPI fake_CreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
static BOOL WINAPI fake_GetFileSizeEx(HANDLE,PLARGE_INTEGER);
static BOOL WINAPI fake_ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
static BOOL WINAPI fake_CloseHandle(HANDLE);
static UINT WINAPI fake_GetPrivateProfileIntA(LPCSTR,LPCSTR,INT,LPCSTR);
static SIZE_T WINAPI fake_VirtualQuery(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
static LONG WINAPI fake_InterlockedCompareExchange(LONG volatile*,LONG,LONG);
#undef CreateFileA
#define CreateFileA fake_CreateFileA
#undef GetFileSizeEx
#define GetFileSizeEx fake_GetFileSizeEx
#undef ReadFile
#define ReadFile fake_ReadFile
#undef CloseHandle
#define CloseHandle fake_CloseHandle
#undef GetPrivateProfileIntA
#define GetPrivateProfileIntA fake_GetPrivateProfileIntA
#undef VirtualQuery
#define VirtualQuery fake_VirtualQuery
#undef InterlockedCompareExchange
#define InterlockedCompareExchange fake_InterlockedCompareExchange
#include CORE_SOURCE
#undef CreateFileA
#undef GetFileSizeEx
#undef ReadFile
#undef CloseHandle
#undef GetPrivateProfileIntA
#undef VirtualQuery
#undef InterlockedCompareExchange
#include <cstdlib>
static unsigned scenario,tick,mutation,release_count,read_position[2],pixel;
static EmissiveComposite* active_composite;
static unsigned char craft_bytes[0x200]{},controller_bytes[0x80]{};
static bool event(const char* name){++tick;std::printf("call:%u:%s\n",tick,name);return tick!=scenario;}
static HANDLE WINAPI fake_CreateFileA(LPCSTR path,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){std::printf("open:%s\n",path);if(!event("open"))return INVALID_HANDLE_VALUE;return reinterpret_cast<HANDLE>(std::strcmp(path,"left")==0?1:2);}
static unsigned index(HANDLE h){return static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(h))-1;}
static BOOL WINAPI fake_GetFileSizeEx(HANDLE h,PLARGE_INTEGER out){bool ok=event("size");out->QuadPart=scenario==30?0:scenario==31?20000:5;if(scenario==32&&index(h)==1)++out->QuadPart;return ok;}
static BOOL WINAPI fake_ReadFile(HANDLE h,LPVOID output,DWORD count,LPDWORD done,LPOVERLAPPED){bool ok=event("read");unsigned length=scenario==30?0:scenario==31?20000:5;unsigned n=std::min<unsigned>(count,length-read_position[index(h)]);auto* out=static_cast<unsigned char*>(output);for(unsigned i=0;i<n;++i)out[i]=static_cast<unsigned char>(read_position[index(h)]+i);if(scenario==33&&index(h)==1&&n)out[0]^=1;read_position[index(h)]+=n;*done=n;std::printf("read:%u:%u\n",index(h),n);return ok;}
static BOOL WINAPI fake_CloseHandle(HANDLE h){std::printf("close:%u\n",index(h));return event("close");}
static UINT WINAPI fake_GetPrivateProfileIntA(LPCSTR section,LPCSTR key,INT fallback,LPCSTR path){std::printf("ini:%s:%s:%d:%s\n",section,key,fallback,path);event("ini");if(mutation)g_dxvk_backend_active=!g_dxvk_backend_active;return scenario;}
static SIZE_T WINAPI fake_VirtualQuery(LPCVOID address,PMEMORY_BASIC_INFORMATION info,SIZE_T size){bool ok=event("query");if(tick==mutation){g_neutral_bump_when_cloaked=!g_neutral_bump_when_cloaked;std::puts("query-mutation");}std::memset(info,0,sizeof(*info));info->BaseAddress=reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(address)&~std::uintptr_t{4095});info->RegionSize=4096;info->State=MEM_COMMIT;info->Protect=PAGE_READWRITE;return ok?size:0;}
static LONG WINAPI fake_InterlockedCompareExchange(LONG volatile* p,LONG desired,LONG expected){const char* name=p==&g_fast_nonbump_enabled?"flat":p==&g_runtime_enabled?"runtime":p==&g_logged_emissive_cache_eviction?"eviction":p==&g_logged_emissive_cache_floor?"floor":"other";std::printf("atomic:%s:%ld:%ld\n",name,desired,expected);event("atomic");LONG old=*p;if(old==expected)*p=desired;return old;}
struct FakeCom {void** table;unsigned id;};
static void* texture_table[19]{};static void* device_table[97]{};
static FakeCom textures[4]{{texture_table,1},{texture_table,2},{texture_table,3},{texture_table,4}};
static FakeCom device{device_table,9};
static ULONG WINAPI release(void* self){++release_count;std::printf("release:%u\n",static_cast<FakeCom*>(self)->id);event("release");if(mutation&&active_composite){active_composite->estimated_bytes=17;g_emissive_composite_cache_bytes=111;std::puts("release-mutation");}return 1;}
static HRESULT WINAPI create(void*,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,IDirect3DTexture8** out){std::printf("create:%u:%u:%u:%lu:%u:%u\n",w,h,levels,static_cast<unsigned long>(usage),static_cast<unsigned>(format),static_cast<unsigned>(pool));event("create");*out=scenario==2?nullptr:reinterpret_cast<IDirect3DTexture8*>(&textures[0]);return scenario==1?E_FAIL:S_OK;}
static HRESULT WINAPI lock(void*,UINT level,D3DLOCKED_RECT* out,const RECT*,DWORD flags){std::printf("lock:%u:%lu\n",level,static_cast<unsigned long>(flags));event("lock");out->Pitch=4;out->pBits=scenario==4?nullptr:&pixel;return scenario==3?E_FAIL:S_OK;}
static HRESULT WINAPI unlock(void*,UINT level){std::printf("unlock:%u\n",level);event("unlock");return E_FAIL;}
static void logger(const std::string& line){event("log");std::printf("log:%s\n",line.c_str());}
static void reset(){
 tick=0;release_count=0;read_position[0]=read_position[1]=0;pixel=0;active_composite=nullptr;
 g_black_emissive_texture=nullptr;g_emissive_composite_cache_bytes=0;g_emissive_composite_use_clock=0;g_logged_emissive_cache_eviction=0;g_logged_emissive_cache_floor=0;
 g_emissive_policies.clear();g_specular_policies.clear();g_diffuse_texture_keys.clear();g_log=logger;g_root_directory="root";
 g_m3d_active_render_mesh=nullptr;g_fast_unmapped_draw_depth=0;g_fast_nonbump_enabled=0;g_neutral_bump_when_cloaked=true;g_dxvk_backend_active=true;g_craft_render_overflow=0;g_runtime_enabled=1;g_craft_render_depth=1;g_craft_render_stack[0]=craft_bytes;
 std::memset(craft_bytes,0,sizeof(craft_bytes));std::memset(controller_bytes,0,sizeof(controller_bytes));void* controller=controller_bytes;std::memcpy(craft_bytes+kGameObjectCloakControllerOffset,&controller,sizeof(controller));
}
static std::unique_ptr<EmissiveMaterialPolicy> material(unsigned base){auto m=std::make_unique<EmissiveMaterialPolicy>();for(unsigned i=0;i<3;++i){const unsigned bytes=(i+1)*31;m->composites.emplace(static_cast<std::uint16_t>(i),EmissiveComposite{reinterpret_cast<IDirect3DTexture8*>(&textures[i]),bytes,base+(i==2?0:i)});g_emissive_composite_cache_bytes+=bytes;}return m;}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 texture_table[2]=reinterpret_cast<void*>(&release);texture_table[16]=reinterpret_cast<void*>(&lock);texture_table[17]=reinterpret_cast<void*>(&unlock);device_table[20]=reinterpret_cast<void*>(&create);
 for(scenario=0;scenario<=33;++scenario){reset();mutation=0;std::printf("file-case:%u\n",scenario);std::printf("file-result:%u\n",same_file_contents("left","right"));}
 scenario=99;
 for(unsigned flags=0;flags<128;++flags)for(unsigned state=0;state<6;++state)for(mutation=0;mutation<7;++mutation){
  reset();g_m3d_active_render_mesh=flags&1?craft_bytes:nullptr;g_fast_unmapped_draw_depth=flags&2;g_fast_nonbump_enabled=flags&4;g_neutral_bump_when_cloaked=(flags&8)!=0;g_dxvk_backend_active=(flags&16)!=0;g_craft_render_overflow=flags&32;g_runtime_enabled=flags&64;std::memcpy(controller_bytes+kCloakControllerStateOffset,&state,4);
  std::printf("flat-case:%u:%u:%u\n",flags,state,mutation);std::printf("flat-result:%u\n",fast_nonbump_requested_for_draw());
 }
 for(scenario=0;scenario<6;++scenario){reset();mutation=0;std::printf("black-case:%u\n",scenario);auto* result=ensure_black_emissive_texture(scenario==5?nullptr:reinterpret_cast<IDirect3DDevice8*>(&device));std::printf("black-result:%u:%u:%08x\n",result!=nullptr,release_count,pixel);std::printf("black-again:%u\n",ensure_black_emissive_texture(nullptr)!=nullptr);}
 scenario=99;
 for(mutation=0;mutation<2;++mutation){
  reset();EmissiveComposite one{reinterpret_cast<IDirect3DTexture8*>(&textures[0]),128,7};g_emissive_composite_cache_bytes=mutation?64:200;active_composite=&one;release_emissive_composite(one);std::printf("one-result:%u:%u:%u\n",static_cast<unsigned>(g_emissive_composite_cache_bytes),one.texture!=nullptr,release_count);active_composite=nullptr;
 }
 reset();mutation=0;auto first=std::make_unique<EmissiveClassPolicy>();first->materials.push_back(material(10));first->materials.push_back(nullptr);g_emissive_policies.emplace(reinterpret_cast<void*>(1),std::move(first));g_emissive_policies.emplace(reinterpret_cast<void*>(2),nullptr);
 auto second=std::make_unique<EmissiveClassPolicy>();second->materials.push_back(material(1));g_emissive_policies.emplace(reinterpret_cast<void*>(3),std::move(second));g_emissive_composite_cache_bytes+=kEmissiveCompositeCacheBudgetBytes;trim_emissive_composite_cache();std::printf("trim-result:%u:%u:%ld\n",static_cast<unsigned>(g_emissive_composite_cache_bytes),release_count,g_logged_emissive_cache_floor);
 g_black_emissive_texture=reinterpret_cast<IDirect3DTexture8*>(&textures[3]);release_all_emissive_gpu_caches();std::printf("all-result:%u:%u\n",static_cast<unsigned>(g_emissive_composite_cache_bytes),release_count);
 auto spec=std::make_unique<SpecularClassPolicy>();spec->materials.push_back(nullptr);auto row=std::make_unique<SpecularMaterialPolicy>();row->texture=reinterpret_cast<IDirect3DTexture8*>(&textures[2]);row->device=reinterpret_cast<IDirect3DDevice8*>(&device);row->load_attempted=true;spec->materials.push_back(std::move(row));g_specular_policies.emplace(reinterpret_cast<void*>(1),std::move(spec));release_all_specular_gpu_caches();std::printf("spec-result:%u:%u:%u\n",release_count,g_specular_policies.begin()->second->materials[1]->texture!=nullptr,g_specular_policies.begin()->second->materials[1]->load_attempted);
 for(unsigned width:{0u,1u,2u,7u,16384u,0xffffffffu})for(unsigned height:{0u,1u,3u,16u,0xffffffffu})std::printf("bytes:%u:%u:%u\n",width,height,static_cast<unsigned>(emissive_texture_bytes(width,height)));
 g_faction_texture_suffixes={"_b","fed","longfed","","longer-than-the-input"};for(const char* key:{"","base","base_b","baselongfed","fed"})std::printf("suffix:%s:%s\n",key,faction_base_texture_key(key).c_str());
 std::puts("core nebula fixture: PASS");
}
