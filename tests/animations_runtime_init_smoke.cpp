// Map the installed binaries without running game or DLL entry points, then
// reproduce the cPrjID detour installed by FleetOpsHook's filesystem startup.
#include "../modules/A2FOAnimations/module.cpp"
#include <stdexcept>
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);}while(0)
unsigned installs=0,registrations=0;
void A2FO_CALL logger(const char*,const char* message){std::puts(message);}
void* A2FO_CALL armada(){return g_armada;}
bool A2FO_CALL install_fake(void* entry,void*,std::uint32_t size,const std::uint8_t* expected,A2FO_InlineHook*) {
    CHECK(!std::memcmp(entry,expected,size));++installs;return true;
}
bool A2FO_CALL register_class(const char*,const char*const*,std::uint32_t n,A2FO_GameObjectClassLoadedHandler,void*) {
    CHECK(n<=64);++registrations;return true;
}
bool A2FO_CALL register_craft(const char*,A2FO_CraftEventHandler,void*){return true;}
bool A2FO_CALL register_producer(const char*,A2FO_ProducerEventHandler,void*){return true;}
bool A2FO_CALL register_weapon_class(const char*,const char*const*,std::uint32_t,A2FO_WeaponClassLoadedHandler,void*){return true;}
bool A2FO_CALL register_weapon_trigger(const char*,A2FO_WeaponTriggerHandler,void*){return true;}
void detour(void* target) {
    auto entry=static_cast<std::uint8_t*>(at(0x2593a0));DWORD old=0;
    CHECK(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&old));
    const auto displacement=std::uint32_t(reinterpret_cast<std::uintptr_t>(target)-reinterpret_cast<std::uintptr_t>(entry)-5);
    entry[0]=0xe9;std::memcpy(entry+1,&displacement,4);
    DWORD ignored=0;CHECK(VirtualProtect(entry,5,old,&ignored));
}
int main(int argc,char** argv) {
    if(argc!=3) {std::puts("Usage: animations_runtime_init_smoke.exe ArmadaL.exe FleetOpsHook.dll");return 2;}
    try {
        auto image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);CHECK(image);
        auto fleet=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);CHECK(fleet);
        g_armada=reinterpret_cast<std::uint8_t*>(image);
        A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
        api.log=logger;api.armada_module=armada;api.install_inline_hook=install_fake;
        api.register_game_object_class_loaded_handler=register_class;
        api.register_craft_event_handler=register_craft;api.register_producer_event_handler=register_producer;
        api.register_weapon_class_loaded_handler=register_weapon_class;api.register_weapon_trigger_handler=register_weapon_trigger;
        g_api=&api;
        CHECK(a2fo::supported_armada::identify(image)!=a2fo::supported_armada::Identity::unsupported);
        const auto helper=helper_specs().front();CHECK(supported_helper(helper));
        // Verified installed FleetOpsHook map: FOFS_cPrjID_GetOdfName RVA 106290.
        auto replacement=reinterpret_cast<std::uint8_t*>(fleet)+0x106290;
        detour(at(0x271050));CHECK(!install() && installs==0);
        detour(replacement+1);CHECK(!install() && installs==0);
        // Identical code in an arbitrary executable allocation is not trusted.
        auto foreign=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);CHECK(foreign);
        std::memcpy(foreign,replacement,32);detour(foreign);CHECK(!install() && installs==0);
        detour(replacement);
        CHECK(A2FO_ModuleInit(&api) && g_ready && installs==8 && registrations==4);
        CHECK(read<std::uint8_t>(at(0x2593a0),0)==0xe9); // Replacement remains intact.
        VirtualFree(foreign,0,MEM_RELEASE);FreeLibrary(fleet);FreeLibrary(image);
        std::puts("Installed-image startup: checked FOFS detour accepted; all 8 native hooks and 4 ODF registrations passed; unknown targets rejected.");
        return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
