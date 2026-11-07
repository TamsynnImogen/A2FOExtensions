// Actual frozen/current core source, with inert ParameterDB and engine bridges.
#include CORE_SOURCE
#include <cstdlib>
#include <new>
#include <stdexcept>

static int budget=-1;
__attribute__((noinline)) void* operator new(std::size_t n){
    if(budget==0){budget=-1;throw std::bad_alloc();}
    if(budget>0)--budget;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* p)noexcept{std::free(p);}
__attribute__((noinline)) void operator delete(void* p,std::size_t)noexcept{std::free(p);}
__attribute__((noinline)) void* operator new[](std::size_t n){return ::operator new(n);}
__attribute__((noinline)) void operator delete[](void* p)noexcept{::operator delete(p);}
__attribute__((noinline)) void operator delete[](void* p,std::size_t)noexcept{::operator delete(p);}

static std::string reads;
static std::string trace;
static bool throw_in_getter=false;
static bool flip_next=false;
static bool throw_handler=false;
static bool reject=false;
static bool mutate_kind=false;
struct MockDb{std::map<std::string,std::string> values;};
extern "C" bool a2fo_parameter_db_get_string(void*,void* db,const char* key,
    char* output,std::uint32_t size,const char*){
    reads+=key;reads+=',';
    if(throw_in_getter){throw_in_getter=false;throw std::runtime_error("get");}
    const auto it=static_cast<MockDb*>(db)->values.find(key);
    if(it==static_cast<MockDb*>(db)->values.end())return false;
    const auto count=std::min<std::size_t>(size-1,it->second.size());
    std::memcpy(output,it->second.data(),count);output[count]=0;return true;
}
extern "C" bool a2fo_parameter_db_get_scalar(void*,void*,const char*,void*,std::uint32_t){return false;}

static void A2FO_CALL class_first(const A2FO_WeaponClassLoadedEvent* event,void*){
    trace+='A';std::printf("class:%u:%u:%p:%p:%p:",event->struct_size,event->odf_field_count,
        event->weapon_class,event->parent_class,event->parameter_db);
    for(std::uint32_t i=0;i<event->odf_field_count;++i){
        const auto& f=event->odf_fields[i];
        std::printf("%.*s=%.*s,",static_cast<int>(f.name.size),f.name.data,
            static_cast<int>(f.value.size),f.value.data);
    }
    std::puts("");
    if(flip_next)g_weapon_class_loaded_handlers.set_handler(2,nullptr);
    if(throw_handler)throw std::runtime_error("class handler");
}
static void A2FO_CALL class_last(const A2FO_WeaponClassLoadedEvent*,void*){trace+='C';}
static bool A2FO_CALL trigger_first(const A2FO_WeaponTriggerEvent* event,void*){
    trace+='T';std::printf("trigger:%u:%u:%p:%p\n",event->struct_size,event->kind,event->weapon,event->target);
    if(mutate_kind)const_cast<A2FO_WeaponTriggerEvent*>(event)->kind=A2FO_WEAPON_TRIGGER_COMMITTED;
    if(flip_next)g_weapon_trigger_handlers.set_handler(2,nullptr);
    if(throw_handler)throw std::runtime_error("trigger handler");
    return !reject;
}
static bool A2FO_CALL trigger_last(const A2FO_WeaponTriggerEvent* event,void*){
    trace+='U';std::printf("trigger-later:%u\n",event->kind);return true;
}
static void A2FO_CALL craft_first(const A2FO_CraftEvent* event,void*){
    trace+='F';std::uint32_t bits=0;std::memcpy(&bits,&event->elapsed_seconds,sizeof(bits));
    std::printf("craft:%u:%u:%p:%08x\n",event->struct_size,event->kind,event->craft,bits);
    const_cast<A2FO_CraftEvent*>(event)->kind=99;
    if(flip_next)g_craft_event_handlers.set_handler(2,nullptr);
    if(throw_handler)throw std::runtime_error("craft handler");
}
static void A2FO_CALL craft_last(const A2FO_CraftEvent* event,void*){
    trace+='G';std::printf("craft-later:%u\n",event->kind);
}

int main(){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    g_log=INVALID_HANDLE_VALUE;
    g_parameter_db_get_string_hook.gateway=reinterpret_cast<void*>(0x1111);
    MockDb db{{{"alpha","one"},{"beta","two"}}};
    g_weapon_class_odf_fields={"alpha","beta","missing"};
    g_weapon_class_loaded_handlers={{"first",{},class_first,nullptr},
        {"null",{},nullptr,nullptr},{"last",{},class_last,nullptr}};
    dispatch_weapon_class_loaded(nullptr,reinterpret_cast<void*>(2),reinterpret_cast<void*>(1));
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,nullptr);
    g_weapon_class_loaded_handlers.clear();
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,reinterpret_cast<void*>(1));
    std::printf("class-off:%s:%s\n",trace.c_str(),reads.c_str());
    g_weapon_class_loaded_handlers={{"first",{},class_first,nullptr},
        {"null",{},nullptr,nullptr},{"last",{},class_last,nullptr}};
    trace.clear();reads.clear();dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),&db);
    std::printf("class-normal:%s:%s\n",trace.c_str(),reads.c_str());
    trace.clear();reads.clear();flip_next=true;
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,&db);
    flip_next=false;std::printf("class-flip:%s:%s\n",trace.c_str(),reads.c_str());
    g_weapon_class_loaded_handlers.set_handler(2,class_last);
    trace.clear();reads.clear();throw_handler=true;
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,&db);
    throw_handler=false;std::printf("class-throw:%s:%s\n",trace.c_str(),reads.c_str());
    trace.clear();reads.clear();throw_in_getter=true;
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,&db);
    std::printf("class-get-throw:%s:%s\n",trace.c_str(),reads.c_str());
    trace.clear();reads.clear();budget=0;
    dispatch_weapon_class_loaded(reinterpret_cast<void*>(1),nullptr,&db);
    budget=-1;std::printf("class-alloc:%s:%s\n",trace.c_str(),reads.c_str());

    g_weapon_trigger_handlers={{"first",trigger_first,nullptr},
        {"null",nullptr,nullptr},{"last",trigger_last,nullptr}};
    for(unsigned k:{A2FO_WEAPON_TRIGGER_PRECHECK,A2FO_WEAPON_TRIGGER_COMMITTED}){
        for(int mode=0;mode<5;++mode){
            trace.clear();reject=mode==1;throw_handler=mode==2;
            mutate_kind=mode==3;flip_next=mode==4;
            const bool result=dispatch_weapon_trigger(k,reinterpret_cast<void*>(3),reinterpret_cast<void*>(4));
            std::printf("trigger-result:%u:%d:%u:%s\n",k,mode,result,trace.c_str());
            g_weapon_trigger_handlers.set_handler(2,trigger_last);
        }
    }
    reject=throw_handler=mutate_kind=flip_next=false;
    g_craft_event_handlers={{"first",craft_first,nullptr},{"null",nullptr,nullptr},{"last",craft_last,nullptr}};
    for(int mode=0;mode<3;++mode){
        trace.clear();flip_next=mode==1;throw_handler=mode==2;
        dispatch_craft_event(A2FO_CRAFT_EVENT_SIMULATE_PRE,reinterpret_cast<void*>(5),-0.0f);
        std::printf("craft-result:%d:%s\n",mode,trace.c_str());
        g_craft_event_handlers.set_handler(2,craft_last);
    }
    std::puts("core dispatch workflow: PASS");
}
