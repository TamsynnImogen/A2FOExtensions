// Complete actual A1 source; runner replaces only unsafe hook primitives.
#include A1_SOURCE
#include <new>
#include <cstdlib>
#include <stdexcept>
static int allocation_budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n){
    if(allocation_budget==0){allocation_budget=-1;throw std::bad_alloc();}
    if(allocation_budget>0)--allocation_budget;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* p)noexcept{std::free(p);}
__attribute__((noinline)) void operator delete(void* p,std::size_t)noexcept{std::free(p);}
__attribute__((noinline)) void* operator new[](std::size_t n){return ::operator new(n);}
__attribute__((noinline)) void operator delete[](void* p)noexcept{::operator delete(p);}
__attribute__((noinline)) void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

static unsigned failure=UINT32_MAX,throw_at=UINT32_MAX;
static bool safe_mode{},settings_found=true,cursor_found{},tactical_found{};
static bool mutate_resource_after_ship{},fail_officer_reserve{};
static std::uint8_t* armada_image{},*fleet_image{};
static unsigned log_count{};
extern "C" bool a2fo_a1_fixture_safe(){return safe_mode;}
extern "C" bool a2fo_a1_fixture_tactical(){return tactical_found;}
extern "C" bool a2fo_a1_fixture_mutate_resource(){return mutate_resource_after_ship;}
extern "C" bool a2fo_a1_fixture_step(unsigned id){
    std::printf("step:%u\n",id);
    if(id==throw_at)throw std::runtime_error("fixture native callback throw");
    if(id==115&&fail_officer_reserve)allocation_budget=0;
    if(id==65)return settings_found;
    if(id==77)return cursor_found;
    return id!=failure;
}
static void A2FO_CALL fixture_log(const char*,const char* message){
    ++log_count;std::printf("log:%s\n",message);
}
static void* A2FO_CALL fixture_armada(){std::puts("sdk:armada");return armada_image;}
static void* A2FO_CALL fixture_fleet(){std::puts("sdk:fleet");return fleet_image;}
static std::uint32_t A2FO_CALL fixture_root_count(){return 1;}
static const char* A2FO_CALL fixture_root(std::uint32_t){return "unused-fixture-root";}
static bool A2FO_CALL unexpected_inline(void*,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook*){
    std::fputs("unexpected native inline hook\n",stderr);std::abort();
}
static bool A2FO_CALL unexpected_patch(void*,void*,const std::uint8_t*,std::size_t){
    std::fputs("unexpected native patch\n",stderr);std::abort();
}
static bool A2FO_CALL class_alias(const char*,const char*,const char*){
    return a2fo_a1_fixture_step(37);
}
static bool A2FO_CALL class_defaults(const char*,const char* name,
    const A2FO_ClasslabelOdfDefault*,std::uint32_t){
    unsigned id=0;
    if(std::strcmp(name,"wingman")==0)id=39;
    else if(std::strcmp(name,"constructionrig")==0)id=41;
    else if(std::strcmp(name,"freighter")==0)id=43;
    else if(std::strcmp(name,"mining")==0)id=45;
    else if(std::strcmp(name,"shipyard")==0)id=47;
    else if(std::strcmp(name,"research")==0)id=49;
    if(!id)std::abort();
    return a2fo_a1_fixture_step(id);
}
static bool A2FO_CALL game_class(const char*,const char* const*,std::uint32_t,
    A2FO_GameObjectClassLoadedHandler,void*){return a2fo_a1_fixture_step(51);}
static bool A2FO_CALL overlay(const char*,const char*,std::uint32_t){
    return a2fo_a1_fixture_step(53);
}
static bool A2FO_CALL race_defaults(const char*,const A2FO_RaceOdfDefault*,std::uint32_t){
    std::fputs("unexpected direct race default callback\n",stderr);std::abort();
}
static bool A2FO_CALL race_loaded(const char*,const char* const*,std::uint32_t,
    A2FO_RaceLoadedHandler,void*){return a2fo_a1_fixture_step(61);}
static bool A2FO_CALL producer(const char*,A2FO_ProducerEventHandler,void*){
    return a2fo_a1_fixture_step(119);
}
static A2FO_ModuleApi make_api(){
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.api_revision=17;
    api.capabilities=A2FO_CAP_CLASSLABEL_ODF_DEFAULTS|A2FO_CAP_ODF_OVERLAY_DIRECTORIES|
        A2FO_CAP_PRODUCER_EVENTS|A2FO_CAP_GAME_OBJECT_CLASS_LOADED|
        A2FO_CAP_RACE_LOADED|A2FO_CAP_RACE_ODF_DEFAULTS;
    api.log=&fixture_log;api.armada_module=&fixture_armada;api.fleetops_module=&fixture_fleet;
    api.install_inline_hook=&unexpected_inline;api.patch_call=&unexpected_patch;api.patch_jump=&unexpected_patch;
    api.extension_root_count=&fixture_root_count;api.extension_root=&fixture_root;
    api.register_classlabel_alias=&class_alias;
    api.register_classlabel_odf_defaults=&class_defaults;
    api.register_odf_overlay_directory=&overlay;
    api.register_producer_event_handler=&producer;
    api.register_game_object_class_loaded_handler=&game_class;
    api.register_race_loaded_handler=&race_loaded;
    api.register_race_odf_defaults=&race_defaults;
    return api;
}
static void scenario(const char* label,A2FO_ModuleApi api){
    std::printf("scenario:%s\n",label);log_count=0;
    g_api=nullptr;g_armada=nullptr;g_fleet_ops=nullptr;
    g_officer_state_lock_ready=false;
    bool result=false,threw=false;
    try{result=A2FO_ModuleInit(&api);}catch(const std::runtime_error&){threw=true;}
    allocation_budget=-1;
    std::printf("outcome:%s:%u:%u:%u:%u:%u:%u:%u:%u\n",label,result,threw,
        g_api!=nullptr,g_armada!=nullptr,g_fleet_ops!=nullptr,
        g_race_menu_callback_ready,g_legacy_resource_panel_runtime_ready,
        log_count);
    if(g_officer_state_lock_ready){DeleteCriticalSection(&g_officer_state_lock);
        g_officer_state_lock_ready=false;}
}
int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    armada_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    fleet_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!armada_image||!fleet_image)return 3;
    std::memcpy(armada_image+kInterfaceSpriteDatabaseGetRva,kExpectedInterfaceSpriteDatabaseGet,
        sizeof(kExpectedInterfaceSpriteDatabaseGet));
    std::memcpy(fleet_image+kFoSpriteSetColourRva,kExpectedFoSpriteSetColour,sizeof(kExpectedFoSpriteSetColour));
    std::memcpy(fleet_image+kFoSpriteDrawScaled2DRva,kExpectedFoSpriteDrawScaled2D,sizeof(kExpectedFoSpriteDrawScaled2D));
    auto api=make_api();
    scenario("success",api);
    api=make_api();api.struct_size=0;scenario("short-api",api);
    api=make_api();api.api_revision=16;scenario("old-revision",api);
    for(unsigned id:{34u,37u,39u,41u,43u,45u,47u,49u,51u,53u,55u,59u,61u,70u}){
        api=make_api();failure=id;
        char label[32]{};std::snprintf(label,sizeof(label),"failure-%u",id);scenario(label,api);
        failure=UINT32_MAX;
    }
    api=make_api();throw_at=37;scenario("throw-registration",api);throw_at=UINT32_MAX;
    api=make_api();safe_mode=true;scenario("safe-mode",api);safe_mode=false;
    api=make_api();settings_found=false;scenario("settings-absent",api);settings_found=true;
    api=make_api();cursor_found=true;scenario("cursor-detected",api);cursor_found=false;
    api=make_api();failure=80;scenario("optional-cursor-fail",api);failure=UINT32_MAX;
    api=make_api();failure=90;scenario("resource-prerequisite-fail",api);failure=UINT32_MAX;
    api=make_api();armada_image[kInterfaceSpriteDatabaseGetRva]^=1;
    scenario("resource-signature-fail",api);
    armada_image[kInterfaceSpriteDatabaseGetRva]^=1;
    api=make_api();mutate_resource_after_ship=true;scenario("late-resource-mutation",api);
    mutate_resource_after_ship=false;
    api=make_api();failure=145;scenario("ship-fail",api);failure=UINT32_MAX;
    api=make_api();fail_officer_reserve=true;scenario("reserve-alloc-fail",api);
    fail_officer_reserve=false;
    VirtualFree(armada_image,0,MEM_RELEASE);VirtualFree(fleet_image,0,MEM_RELEASE);
    std::puts("A1 lifecycle native fixture: PASS");
}
