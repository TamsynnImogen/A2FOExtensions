// Actual whole A1 source, with only file and engine primitives mocked.
#include <windows.h>
#include "../modules/A1Compat/a1_bzn_policy.hpp"
#include <string>
extern bool a2fo_fixture_bzn(const std::string&,a1compat::A1BznHeader&);
extern bool a2fo_fixture_exists(const std::string&);
extern "C" HANDLE WINAPI fixture_CreateFileA(LPCSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
extern "C" BOOL WINAPI fixture_GetFileSizeEx(HANDLE,LARGE_INTEGER*);
extern "C" BOOL WINAPI fixture_ReadFile(HANDLE,LPVOID,DWORD,LPDWORD,LPOVERLAPPED);
extern "C" BOOL WINAPI fixture_CloseHandle(HANDLE);
#include A1_SOURCE
static A2FO_ModuleApi fixture_api_storage{};
#include <stdexcept>
static const char* fixture_roots[]={"Data","A1","Override"};
static unsigned fixture_file_mode{};
static unsigned fixture_aip_mode{};
static unsigned fixture_bzn_mode{};
static unsigned fixture_mdf_mode{};
static std::uint8_t* fixture_armada{};
static a1compat::A1MdfData* fixture_mdf_mutable{};
static unsigned fixture_assign_count{};
static unsigned fixture_gateway_count{};
static void fixture_assign_stub(){}
static void fixture_aip_stub(){}
static constexpr const char* fixture_mdf_text="StartLocations=2\nStart1=0 117\nStart2=117 0\n";
static std::uint32_t A2FO_CALL fixture_root_count(){std::puts("root-count");return 3;}
static const char* A2FO_CALL fixture_root(std::uint32_t index){
    std::printf("root:%u\n",index);return index<3?fixture_roots[index]:nullptr;
}
static void A2FO_CALL fixture_log(const char*,const char* text){std::printf("log:%s\n",text);}
static A2FO_ModuleApi fixture_api(){A2FO_ModuleApi api{};api.extension_root_count=&fixture_root_count;
    api.extension_root=&fixture_root;api.log=&fixture_log;return api;}
bool a2fo_fixture_bzn(const std::string& path,a1compat::A1BznHeader& header){
    std::printf("bzn:%s\n",path.c_str());
    bool found=(fixture_bzn_mode==0&&path=="direct.bzn")||
        (fixture_bzn_mode==1&&path=="Override\\bzn\\map.bzn")||
        (fixture_bzn_mode==2&&path=="A1\\map.bzn");
    if(found){header.has_map_bounds=true;header.map_size[0]=100;header.map_size[1]=100;
        header.map_size[2]=100;}
    return found;
}
bool a2fo_fixture_exists(const std::string& path){
    std::printf("exists:%s\n",path.c_str());
    if(path=="A1\\a1compat.ini")return true;
    if(path=="Override\\AI\\AIPs\\fed_build_list.aip")return fixture_file_mode!=1;
    if(path=="Override\\AI\\AIPs\\fed_instant_action_build_list.aip")return fixture_file_mode==1;
    return false;
}
extern "C" HANDLE WINAPI fixture_CreateFileA(LPCSTR path,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE){
    std::printf("open:%s\n",path);return fixture_mdf_mode==1?INVALID_HANDLE_VALUE:reinterpret_cast<HANDLE>(0x1234);
}
extern "C" BOOL WINAPI fixture_GetFileSizeEx(HANDLE,LARGE_INTEGER* size){
    size->QuadPart=fixture_mdf_mode==2?0:std::strlen(fixture_mdf_text);return TRUE;
}
extern "C" BOOL WINAPI fixture_ReadFile(HANDLE,LPVOID output,DWORD count,LPDWORD read,LPOVERLAPPED){
    std::memcpy(output,fixture_mdf_text,count);*read=count;std::printf("read:%lu\n",static_cast<unsigned long>(count));return TRUE;
}
extern "C" BOOL WINAPI fixture_CloseHandle(HANDLE){std::puts("close-file");return TRUE;}
extern "C" std::uintptr_t a2fo_a1_call_thiscall_1(void* fn,void* self,std::uintptr_t arg){
    if(g_armada&&fn==*at<void*>(g_armada,kNativeStringAssignIatRva)){
        ++fixture_assign_count;
        const bool player=reinterpret_cast<void*>(arg)==at(g_armada,kStartLocationTypePlayerRva);
        std::printf("assign:%u:%u\n",fixture_assign_count,player);
        if(fixture_mdf_mutable&&fixture_mdf_mode==3&&fixture_assign_count==1)
            fixture_mdf_mutable->start_location_count=0;
        return 1;
    }
    if(fn==reinterpret_cast<void*>(&fixture_aip_stub)){
        ++fixture_gateway_count;const char* name=reinterpret_cast<const char*>(arg);
        std::printf("aip:%s\n",name);
        if(std::strcmp(name,"fed_build_list")==0)return fixture_aip_mode==1?0:0x1111;
        return 0x2222;
    }
    (void)self;std::abort();
}
static void map_cases(){
    g_api=&fixture_api_storage;
    for(unsigned mode=0;mode<4;++mode){
        fixture_bzn_mode=mode;std::printf("case:bzn:%u\n",mode);
        a1compat::A1BznHeader header{};std::string source;
        const char* name=mode==0?"direct.bzn":mode==3?"missing.bzn":".\\bzn\\map.bzn";
        bool found=find_a1_bzn_header(name,header,&source);
        std::printf("bzn-result:%u:%s\n",found,source.c_str());
    }
    g_api=nullptr;fixture_bzn_mode=0;
    a1compat::A1BznHeader header{};
    std::printf("bzn-noapi:%u\n",find_a1_bzn_header("absent.bzn",header));
    g_api=&fixture_api_storage;
    for(unsigned mode=0;mode<3;++mode){
        fixture_mdf_mode=mode;std::printf("case:mdf:%u\n",mode);
        a1compat::A1MdfData mdf{};std::string source;
        bool found=read_a1_companion_mdf(mode==0?"map.bzn":"map",mdf,&source);
        std::printf("mdf-result:%u:%zu:%s\n",found,mdf.start_location_count,source.c_str());
    }
}
static void apply_cases(){
    fixture_armada=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!fixture_armada)std::abort();
    g_armada=fixture_armada;
    *at<std::int32_t>(g_armada,kGameSetupSlotCountRva)=4;
    *at<void*>(g_armada,kNativeStringAssignIatRva)=reinterpret_cast<void*>(&fixture_assign_stub);
    std::uint8_t details[0x100]{};std::uint8_t locations[3][0x80]{};
    void* pointers[4]={nullptr,locations[0],locations[1],locations[2]};
    void* pointer_to_array=pointers;
    std::memcpy(details+kMapDetailsStartLocationArrayOffset,&pointer_to_array,sizeof(pointer_to_array));
    a1compat::A1BznHeader header{};header.has_map_bounds=true;
    header.minimum_extent[0]=0;header.minimum_extent[1]=0;header.minimum_extent[2]=0;
    header.map_size[0]=100;header.map_size[1]=100;header.map_size[2]=100;
    a1compat::A1MdfData mdf{};mdf.start_location_count=2;
    mdf.start_locations[0]={0,117};mdf.start_locations[1]={117,0};
    for(unsigned mode=0;mode<3;++mode){
        fixture_mdf_mode=mode==2?3:0;fixture_mdf_mutable=&mdf;fixture_assign_count=0;
        mdf.start_location_count=2;pointers[3]=mode==1?nullptr:locations[2];
        std::memset(details+kMapDetailsStartLocationCountOffset,0xff,4);
        std::printf("case:apply:%u\n",mode);
        bool result=apply_a1_mdf_start_locations(details,header,mdf);
        std::int32_t count{};std::memcpy(&count,details+kMapDetailsStartLocationCountOffset,4);
        std::printf("apply-result:%u:%u:%d\n",result,fixture_assign_count,count);
    }
    fixture_mdf_mutable=nullptr;g_armada=nullptr;VirtualFree(fixture_armada,0,MEM_RELEASE);
}
static void aip_cases(){
    g_api=&fixture_api_storage;g_a2fo_a1_aip_lookup_gateway=reinterpret_cast<void*>(&fixture_aip_stub);
    int manager{};
    for(unsigned mode=0;mode<4;++mode){
        fixture_file_mode=mode==1?1:0;fixture_aip_mode=mode==2?1:0;
        fixture_gateway_count=0;InterlockedExchange(&g_legacy_aip_name_fallback_count,mode==3?32:0);
        std::printf("case:aip:%u\n",mode);
        bool prefers=mod_chain_prefers_legacy_aip("fed_instant_action_build_list","fed_build_list");
        void* result=resolve_aip_lookup(&manager,"fed_instant_action_build_list");
        std::printf("aip-result:%u:%p:%u\n",prefers,result,fixture_gateway_count);
    }
    std::printf("aip-bad:%p\n",resolve_aip_lookup(&manager,"bad/name"));
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    fixture_api_storage=fixture_api();
    map_cases();apply_cases();aip_cases();std::puts("A1 maps runtime native fixture: PASS");
}
