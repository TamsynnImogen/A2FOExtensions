// Actual whole frozen/current A1 source; only engine getter/name primitives mocked.
#include <windows.h>
#include <cstddef>
bool a2fo_fixture_parameter_odf_name(void*,char*,std::size_t) noexcept;
#include A1_SOURCE

static unsigned getter_mode{};
static const char* next_odf_name="";
static std::string getter_trace;
static std::uint8_t* fake_armada{};

static std::uintptr_t __cdecl fixture_get_float(void* db,std::uintptr_t context,
    const char* key,float* output,float default_value){
    if(!db||context!=0||default_value!=0.0f||!output)std::abort();
    getter_trace+=key;getter_trace+=',';
    *output=17.0f;
    if(std::strcmp(key,"combatSpeed")==0)return getter_mode==1;
    if(std::strcmp(key,"impulseSpeed")==0)return getter_mode==2;
    if(std::strcmp(key,"warpSpeed")==0)return getter_mode==3;
    std::abort();
}

bool a2fo_fixture_parameter_odf_name(void*,char* output,std::size_t size) noexcept {
    std::printf("name-call:%u:%zu\n",output?static_cast<unsigned char>(output[0]):255u,size);
    if(!output||size==0)return false;
    std::snprintf(output,size,"%s",next_odf_name);
    return output[0]!=0;
}

static void install_getter_thunk(){
    fake_armada=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,
        MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!fake_armada)std::abort();
    std::uint8_t thunk[7]={0xb8,0,0,0,0,0xff,0xe0};
    const auto target=reinterpret_cast<std::uintptr_t>(&fixture_get_float);
    static_assert(sizeof(target)==4,"fixture targets the 32-bit game ABI");
    std::memcpy(thunk+1,&target,4);
    std::memcpy(fake_armada+kParameterDbGetFloatRva,thunk,sizeof(thunk));
    FlushInstructionCache(GetCurrentProcess(),fake_armada+kParameterDbGetFloatRva,sizeof(thunk));
    g_armada=fake_armada;
}

int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    g_api=nullptr;
    int db=0;
    getter_trace.clear();
    const bool null_result=legacy_physics_database(nullptr);
    std::printf("physics:null:%u:%s\n",null_result,getter_trace.c_str());
    getter_trace.clear();
    const bool no_armada_result=legacy_physics_database(&db);
    std::printf("physics:no-armada:%u:%s\n",no_armada_result,getter_trace.c_str());
    install_getter_thunk();
    for(unsigned mode=0;mode<4;++mode){
        getter_mode=mode;getter_trace.clear();
        const bool result=legacy_physics_database(&db);
        const char* label=mode==0?"none":mode==1?"combat":mode==2?"impulse":"warp";
        std::printf("physics:%s:%u:%s\n",label,result,getter_trace.c_str());
    }
    for(const char* path:{"", "cnstphys.odf", "folder/constphysZ.odf",
            "folder\\BATTPHYS.odf", "folder/other.odf", "folder\\cnstphys/battphys"}){
        next_odf_name=path;char name[80];std::memset(name,'Q',sizeof(name));
        const auto profile=legacy_smooth_profile(&db,name,sizeof(name));
        const char* label=profile==LegacySmoothProfile::construction?"construction":
            profile==LegacySmoothProfile::battle?"battle":"destroyer";
        std::printf("profile:%s:%s:%s\n",label,path,name);
    }
    next_odf_name="ignored";
    std::printf("profile-null:%u\n",static_cast<unsigned>(legacy_smooth_profile(&db,nullptr,0)));
    char zero_size[]="battphys";
    const auto zero_profile=legacy_smooth_profile(&db,zero_size,0);
    std::printf("profile-zero:%u:%s\n",static_cast<unsigned>(zero_profile),zero_size);
    for(auto profile:{LegacySmoothProfile::destroyer,LegacySmoothProfile::construction,
            LegacySmoothProfile::battle,static_cast<LegacySmoothProfile>(99)}){
        std::printf("name:%u:%s\n",static_cast<unsigned>(profile),smooth_profile_name(profile));
    }
    g_armada=nullptr;VirtualFree(fake_armada,0,MEM_RELEASE);
    std::puts("A1 physics native fixture: PASS");
}
