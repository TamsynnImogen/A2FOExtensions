bool fixture_builtin_enabled=true;
#include MODULE_SOURCE
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef TEST_BUILTIN
namespace a2fo { bool builtin_module_enabled(BuiltinFeature) noexcept { return fixture_builtin_enabled; } }
#endif

namespace {
int logs=0,registrations=0,hooks=0,particles=0,owners=0,finds=0;
bool registration_ok=true,hook_ok=true,found=true;
void* owner_pointer=nullptr;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void A2FO_CALL mock_log(const char*,const char* message){++logs;std::printf("log:%s\n",message);}
void* A2FO_CALL module_handle(){return g_armada;}
std::uintptr_t __attribute__((thiscall)) fake_build(void*,std::uintptr_t,std::uintptr_t,std::uintptr_t){return 1;}
void* __attribute__((thiscall)) fake_owner(void*){++owners;return owner_pointer;}
void* __attribute__((thiscall)) fake_find(void*,const char*,std::uintptr_t){++finds;return found?reinterpret_cast<void*>(0x1234):nullptr;}
void __cdecl fake_particle(const char* sprite,const float* colour,void* owner,void* node,float size,float duration){
    check(std::strcmp(sprite,"spark")==0&&colour[0]==0.2f&&owner==owner_pointer&&node&&size==4.0f&&duration==0.5f,"particle args");
    ++particles;
}
bool A2FO_CALL mock_register(const char*,const char* const* fields,std::uint32_t count,
    A2FO_WeaponClassLoadedHandler handler,void*){
    check(count==4&&fields&&handler==weapon_class_loaded_handler,"registration shape");
    ++registrations;return registration_ok;
}
bool A2FO_CALL mock_hook(void*,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook* hook){
    ++hooks;hook->gateway=reinterpret_cast<void*>(&fake_build);return hook_ok;
}
void jump(void* target,void* source){
    auto* destination=static_cast<std::uint8_t*>(target);destination[0]=0xe9;
    const auto delta=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(source)-
        reinterpret_cast<std::uintptr_t>(target)-5);
    std::memcpy(destination+1,&delta,4);
}
void seed(){
    std::memcpy(at(kOrdnanceClassBuildRva),kExpectedBuild,sizeof(kExpectedBuild));
    std::memcpy(at(kWeaponGetOwnerRva),kExpectedGetOwner,sizeof(kExpectedGetOwner));
    std::memcpy(at(kNodeParticleAddRva),kExpectedParticleAdd,sizeof(kExpectedParticleAdd));
    std::memcpy(at(kDatabaseFindElementRva),kExpectedFindElement,sizeof(kExpectedFindElement));
}
A2FO_OdfFieldView field(const char* name,const char* value){
    return {{name,static_cast<std::uint32_t>(std::strlen(name))},
            {value,static_cast<std::uint32_t>(std::strlen(value))}};
}
void run(){
    g_armada=static_cast<HMODULE>(VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    check(g_armada!=nullptr,"image alloc");seed();
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_WEAPON_CLASS_LOADED;api.log=mock_log;api.armada_module=module_handle;
    api.install_inline_hook=mock_hook;api.register_weapon_class_loaded_handler=mock_register;
    check(!A2FO_ModuleInit(nullptr),"null api");
    auto bad=api;bad.api_version=0;check(!A2FO_ModuleInit(&bad),"bad version");
    auto* signature=static_cast<std::uint8_t*>(at(kNodeParticleAddRva));*signature^=1;
    check(!A2FO_ModuleInit(&api)&&registrations==0,"signature failure");*signature^=1;
    registration_ok=false;check(!A2FO_ModuleInit(&api)&&registrations==1&&hooks==0,"registration failure");
    registration_ok=true;hook_ok=false;check(A2FO_ModuleInit(&api)&&!g_ready&&hooks==1,"hook failure resident");
    hook_ok=true;check(A2FO_ModuleInit(&api)&&g_ready&&hooks==2,"hook success");
    std::array<std::uint8_t,0x80> weapon{},owner{},node{},storm{},database{};
    owner_pointer=owner.data();std::memcpy(weapon.data()+kWeaponClassOffset,&owner_pointer,sizeof(owner_pointer));
    void* klass=reinterpret_cast<void*>(0x43210000);
    std::memcpy(weapon.data()+kWeaponClassOffset,&klass,sizeof(klass));
    void* db=database.data();std::memcpy(storm.data()+kSpriteDatabaseOffset,&db,sizeof(db));
    void* st=storm.data();std::memcpy(at(kStorm3dGlobalRva),&st,sizeof(st));
    jump(at(kWeaponGetOwnerRva),reinterpret_cast<void*>(&fake_owner));
    jump(at(kDatabaseFindElementRva),reinterpret_cast<void*>(&fake_find));
    jump(at(kNodeParticleAddRva),reinterpret_cast<void*>(&fake_particle));
    A2FO_OdfFieldView fields[]{field("muzzleFlashSprite","spark"),field("muzzleFlashSize","4"),
        field("muzzleFlashDuration","0.5"),field("muzzleFlashColor","0.2 0.3 0.4")};
    A2FO_WeaponClassLoadedEvent event{};event.struct_size=sizeof(event);event.weapon_class=klass;
    event.odf_fields=fields;event.odf_field_count=4;
    weapon_class_loaded_handler(&event,nullptr);check(g_policies.size()==1,"valid policy");
    emit_flash(weapon.data(),node.data());check(particles==1&&owners==1&&finds==1,"flash");
#ifdef TEST_BUILTIN
    fixture_builtin_enabled=false;emit_flash(weapon.data(),node.data());
    check(particles==1&&owners==1&&finds==1,"live builtin gate");fixture_builtin_enabled=true;
#endif
    found=false;emit_flash(weapon.data(),node.data());emit_flash(weapon.data(),node.data());
    check(particles==1&&finds==3,"missing sprite skip");
    fields[1]=field("muzzleFlashSize","-2");weapon_class_loaded_handler(&event,nullptr);
    check(g_policies.empty(),"invalid erases policy");
    fields[1]=field("muzzleFlashSize","4f");found=true;
    weapon_class_loaded_handler(&event,nullptr);check(g_policies.size()==1,"float suffix");
    fields[3]=field("muzzleFlashColor","0.2 0.3 nan");
    weapon_class_loaded_handler(&event,nullptr);check(g_policies.empty(),"nonfinite colour");
    fields[3]=field("muzzleFlashColor","0.2 0.3 0.4");
    fields[1]=field("muzzleFlashSize","4");fields[0]=field("muzzleFlashSprite","");
    weapon_class_loaded_handler(&event,nullptr);check(g_policies.empty(),"explicit empty sprite");
    fields[0]={{"muzzleFlashSprite",17},{nullptr,4}};
    weapon_class_loaded_handler(&event,nullptr);check(g_policies.empty(),"null value");
    event.struct_size=1;weapon_class_loaded_handler(&event,nullptr);check(g_policies.empty(),"short event");
    event.struct_size=sizeof(event);fields[0]=field("muzzleFlashSprite","spark");
    fields[2]=field("muzzleFlashDuration","0.5");
    fields[3]=field("muzzleFlashColor","0.2 0.3 0.4");
    const char* sizes[]{"0","-0","1e-45","4f","4ff","10000","10000.00001","10000.1","nan","inf"};
    for(const char* value:sizes){
        fields[1]=field("muzzleFlashSize",value);weapon_class_loaded_handler(&event,nullptr);
        std::printf("size:%s:%d\n",value,!g_policies.empty());
    }
    fields[1]=field("muzzleFlashSize","4");
    const char* durations[]{"0","1e-45","10","10.0000001","10.1","nan"};
    for(const char* value:durations){
        fields[2]=field("muzzleFlashDuration",value);weapon_class_loaded_handler(&event,nullptr);
        std::printf("duration:%s:%d\n",value,!g_policies.empty());
    }
    fields[2]=field("muzzleFlashDuration","0.5");
    const char* colours[]{"0.2 0.3 0.4","1 1 1","1.0000001 0 0","1.1 0 0","nan 0 0","0 0"};
    for(const char* value:colours){
        fields[3]=field("muzzleFlashColor",value);weapon_class_loaded_handler(&event,nullptr);
        std::printf("colour:%s:%d\n",value,!g_policies.empty());
    }
    A2FO_ModuleShutdown();check(!g_ready&&g_policies.empty(),"shutdown");
    std::printf("counts:%d:%d:%d:%d:%d\n",logs,registrations,hooks,particles,finds);
    VirtualFree(g_armada,0,MEM_RELEASE);
}
}
int main(){try{run();return 0;}catch(const std::exception& error){std::fprintf(stderr,"FAIL:%s\n",error.what());return 1;}}
