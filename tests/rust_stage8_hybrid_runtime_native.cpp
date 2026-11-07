// Whole frozen/current runtime; only provider/Win32 primitives are synthetic.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <stdexcept>
static unsigned modules=0,exports=0;
static bool ready_value=false;
static const char* failure="";
static HMODULE fixture_module(const char* name){
    std::printf("module:%s\n",name);
    if(!std::strcmp(failure,"module"))throw std::runtime_error("module");
    unsigned bit=!std::strcmp(name,"A2FOExtensions.dll")?1:2;
    return (modules&bit)?reinterpret_cast<HMODULE>(bit):nullptr;
}
static bool __cdecl fixture_ready() noexcept{
    std::printf("ready:%u\n",ready_value);return ready_value;
}
static void __cdecl fixture_function(){}
template<class T>static FARPROC fixture_proc_bits(T value){
    FARPROC output=nullptr;static_assert(sizeof(output)==sizeof(value));
    std::memcpy(&output,&value,sizeof(output));return output;
}
static FARPROC fixture_proc(HMODULE provider,const char* name){
    std::printf("export:%u:%s\n",static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(provider)),name);
    if(!std::strcmp(failure,"proc"))throw std::runtime_error("proc");
    const char* names[]={"A2FOAlwaysShowShields_UpdateCraft","A2FO_NebulaBeginCraftRender",
        "A2FO_NebulaEndCraftRender","A2FO_ProducerLogicalQueueHasRoom",
        "A2FO_ProducerQueueExtendedBuild","A2FO_ProducerCanQueueExtendedBuild","A2FO_ProducerEventsReady"};
    for(unsigned i=0;i<7;++i)if(!std::strcmp(name,names[i])&&(exports&(1u<<((i==0&&provider==reinterpret_cast<HMODULE>(2))?7:i))))
        return i==6?fixture_proc_bits(&fixture_ready):fixture_proc_bits(&fixture_function);
    return nullptr;
}
static HMODULE fixture_startup_provider(const char* name,std::initializer_list<const char*> names){
    std::printf("provider:%s",name);for(const auto* item:names)std::printf(":%s",item);std::puts("");
    if(!std::strcmp(failure,"provider"))throw std::runtime_error("provider");
    return (modules&4)?reinterpret_cast<HMODULE>(4):nullptr;
}
#define GetModuleHandleA fixture_module
#define GetProcAddress fixture_proc
#include HYBRID_SOURCE
#undef GetModuleHandleA
#undef GetProcAddress
static bool room=false;
static unsigned target_calls=0,mutation=0;
static void** live_buttons=nullptr;
static bool __cdecl queue_room(void* producer){
    std::printf("room:%u:%u\n",producer!=nullptr,room);return room;
}
static bool __cdecl queue_target(void* producer,void* target){
    const auto id=static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(target));
    std::printf("target:%u:%u\n",producer!=nullptr,id);
    if(target_calls++==0){
        if(mutation==1)live_buttons[1]=nullptr;
        if(mutation==2)a2fo::g_feature_pack_can_queue_extended_build=nullptr;
        if(mutation==3)a2fo::g_fleet_ops=nullptr;
    }
    return id%2==0;
}
static bool A2FO_CALL producer_event(const A2FO_ProducerEvent* event){
    std::printf("event:%u:%u:%u:%u\n",event->struct_size,event->kind,
        event->producer!=nullptr,event->target_class!=nullptr);return event->kind==7;
}
static void queue_cases(A2FO_ModuleApi& api){
    alignas(void*) std::array<std::uint8_t,0x300> producer{};
    auto* count=reinterpret_cast<std::uint32_t*>(producer.data()+a2fo::kQueueCountOffset);
    for(unsigned n:{0u,9u,10u,11u,UINT32_MAX})for(unsigned present=0;present<2;++present){
        *count=n;a2fo::g_feature_pack_logical_queue_has_room=present?queue_room:nullptr;
        for(unsigned r=0;r<2;++r){room=r!=0;
            std::printf("room-case:%u:%u:%u:%u:%u\n",n,present,r,
                a2fo::feature_pack_extended_queue_has_room(nullptr),
                a2fo::feature_pack_extended_queue_has_room(producer.data()));
        }
    }
    a2fo::g_feature_pack_logical_queue_has_room=queue_room;
    for(unsigned present=0;present<2;++present)for(unsigned valid=0;valid<4;++valid){
        a2fo::g_feature_pack_can_queue_extended_build=present?queue_target:nullptr;
        target_calls=1;room=true;
        std::printf("can-case:%u:%u:%u\n",present,valid,a2fo::feature_pack_can_queue_extended_build(
            valid&1?producer.data():nullptr,valid&2?reinterpret_cast<void*>(3):nullptr));
    }
    std::vector<std::uint32_t> fleet(0x250000/4);
    std::array<std::array<std::uint32_t,0x100/4>,6> buttons{};
    std::array<std::array<std::uint32_t,0x20/4>,6> modes{};
    live_buttons=reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(fleet.data())+a2fo::kFoPopupButtonPointerArrayRva);
    for(mutation=0;mutation<4;++mutation)for(unsigned n:{9u,10u,UINT32_MAX}){
        std::fill(fleet.begin(),fleet.end(),0u);
        for(unsigned i=0;i<6;++i){
            buttons[i].fill(0);modes[i].fill(0);live_buttons[i]=buttons[i].data();
            *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(buttons[i].data())+a2fo::kControlButtonModeInfoOffset)=i==4?nullptr:modes[i].data();
            buttons[i][a2fo::kControlButtonStateOffset/4]=99;
            modes[i][a2fo::kModeInfoTypeOffset/4]=i==3?2:1;
            *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(modes[i].data())+a2fo::kModeInfoTargetClassOffset)=i==5?nullptr:reinterpret_cast<void*>(i+3);
        }
        *count=n;target_calls=0;a2fo::g_feature_pack_can_queue_extended_build=queue_target;
        a2fo::g_fleet_ops=reinterpret_cast<HMODULE>(fleet.data());a2fo::g_logged_extended_queue_tail_mask=false;
        std::printf("mask-case:%u:%u\n",mutation,n);a2fo::apply_extended_queue_tail_mask(producer.data());
        for(const auto& button:buttons)std::printf("state:%u\n",button[a2fo::kControlButtonStateOffset/4]);
        // Second call proves one-time logging and null module/provider gates.
        a2fo::apply_extended_queue_tail_mask(producer.data());
    }
    a2fo::apply_extended_queue_tail_mask(nullptr);
    a2fo::g_feature_pack_can_queue_extended_build=nullptr;
    a2fo::g_fleet_ops=reinterpret_cast<HMODULE>(fleet.data());a2fo::apply_extended_queue_tail_mask(producer.data());
    a2fo::g_fleet_ops=nullptr;
    for(unsigned gate=0;gate<5;++gate)for(unsigned kind:{7u,8u}){
        api.struct_size=gate==1?0:sizeof(api);api.capabilities=gate==2?0u:static_cast<std::uint32_t>(A2FO_CAP_PRODUCER_EVENTS);
        api.dispatch_producer_event=gate==3?nullptr:producer_event;a2fo::g_api=gate==0?nullptr:&api;
        std::printf("dispatch-case:%u:%u:%u\n",gate,kind,a2fo::dispatch_producer_event(kind,producer.data(),reinterpret_cast<void*>(3)));
    }
    a2fo::g_api=&api;
}
static void A2FO_CALL fixture_log(const char* module,const char* text){
    std::printf("log:%s:%s\n",module,text);
    if(!std::strcmp(failure,"log"))throw std::runtime_error("log");
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    std::set_terminate([]{std::printf("terminate:%u\n",std::current_exception()!=nullptr);std::fflush(stdout);std::_Exit(77);});
    A2FO_ModuleApi api{};api.log=fixture_log;a2fo::g_api=&api;
    if(argc>1){failure=argv[1];modules=7;exports=127;ready_value=true;
        if(!std::strcmp(failure,"provider"))a2fo::resolve_feature_pack_extended_queue();
        else a2fo::resolve_shield_craft_observer();
        return 2;}
    for(modules=0;modules<8;++modules)for(exports=0;exports<256;++exports){
        // Reset globals to nonnull stale values: every resolver must replace them.
        a2fo::g_shield_craft_observer=reinterpret_cast<a2fo::ShieldCraftObserver>(1);
        a2fo::g_nebula_begin_craft_render=reinterpret_cast<a2fo::NebulaCraftRenderObserver>(1);
        a2fo::g_nebula_end_craft_render=reinterpret_cast<a2fo::NebulaCraftRenderObserver>(1);
        std::printf("case:%u:%u\n",modules,exports);
        a2fo::resolve_shield_craft_observer();a2fo::resolve_nebula_craft_render_observers();
        std::printf("observers:%u:%u:%u\n",a2fo::g_shield_craft_observer!=nullptr,
            a2fo::g_nebula_begin_craft_render!=nullptr,a2fo::g_nebula_end_craft_render!=nullptr);
        for(int value=0;value<2;++value){ready_value=value!=0;a2fo::resolve_feature_pack_extended_queue();
            std::printf("queue:%u:%u:%u\n",a2fo::g_feature_pack_logical_queue_has_room!=nullptr,
                a2fo::g_feature_pack_queue_extended_build!=nullptr,a2fo::g_feature_pack_can_queue_extended_build!=nullptr);}
    }
    queue_cases(api);
    std::puts("hybrid provider fixture: PASS");return 0;
}
