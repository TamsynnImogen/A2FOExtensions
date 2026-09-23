#include "../modules/A2FOAnimations/module.cpp"
#include <iostream>
#include <stdexcept>
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);}while(0)
template<class T> void put(void* p,unsigned offset,T v){std::memcpy(static_cast<unsigned char*>(p)+offset,&v,sizeof(v));}
using Buffer=std::array<unsigned char,0x400>;
void __fastcall noop1(void*,void*,void*){}
void __fastcall noop0(void*,void*){}
void* clone_result=nullptr;
void* __fastcall clone_fake(void*,void*,void*){return clone_result;}
void* __fastcall first_fake(void* db,void*){return read<void*>(db,0x68);}
void* __fastcall next_fake(void* channel,void*){return read<void*>(channel,0x0c);}
void jump(unsigned rva,void* target){auto p=g_armada+rva;p[0]=0xb8;put(p,1,target);p[5]=0xff;p[6]=0xe0;}
unsigned installs=0, registrations=0;
bool A2FO_CALL install_fake(void*,void*,std::uint32_t,const std::uint8_t*,A2FO_InlineHook*){++installs;return true;}
void A2FO_CALL logger(const char*,const char*){}
void* A2FO_CALL armada(){return g_armada;}
bool A2FO_CALL register_class(const char*,const char*const*,std::uint32_t n,A2FO_GameObjectClassLoadedHandler,void*){CHECK(n<=64);++registrations;return true;}
bool A2FO_CALL register_producer(const char*,A2FO_ProducerEventHandler,void*){return true;}
bool A2FO_CALL register_weapon_class(const char*,const char*const*,std::uint32_t,A2FO_WeaponClassLoadedHandler,void*){return true;}
bool A2FO_CALL register_weapon_trigger(const char*,A2FO_WeaponTriggerHandler,void*){return true;}
void* weapon_owner=nullptr;
const char* weapon_filename="FPhoton.odf";
void* __fastcall owner_fake(void*,void*){return weapon_owner;}
const char* __fastcall odf_fake(void*,void*){return weapon_filename;}
bool A2FO_CALL register_craft(const char*,A2FO_CraftEventHandler,void*){return true;}
void fixture_headers(){
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(g_armada);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(g_armada+0x80);nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;nt->FileHeader.TimeDateStamp=a2fo::supported_armada::kCanonicalTimestamp;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;nt->OptionalHeader.SizeOfImage=a2fo::supported_armada::kCanonicalImageSize;
}
void tick(void* craft,float dt){A2FO_CraftEvent e{sizeof(e),A2FO_CRAFT_EVENT_SIMULATE_PRE,craft,dt};craft_event(&e,nullptr);}
void post(void* craft){A2FO_CraftEvent e{sizeof(e),A2FO_CRAFT_EVENT_SIMULATE_POST,craft,0};craft_event(&e,nullptr);}
void production(void* craft,unsigned kind){A2FO_ProducerEvent e{sizeof(e),kind,craft,nullptr};CHECK(producer_event(&e,nullptr));}
int main(){
    g_armada=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x405000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));CHECK(g_armada);
    fixture_headers();A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.log=logger;api.armada_module=armada;api.install_inline_hook=install_fake;
    api.register_game_object_class_loaded_handler=register_class;api.register_producer_event_handler=register_producer;api.register_craft_event_handler=register_craft;
    g_api=&api;
    api.register_weapon_class_loaded_handler=register_weapon_class;
    api.register_weapon_trigger_handler=register_weapon_trigger;
    for(auto spec:helper_specs())std::memcpy(at(spec.rva),spec.bytes.data(),spec.bytes.size());
    for(auto s:hook_specs())std::memcpy(at(s.rva),s.bytes.data(),s.bytes.size());
    *static_cast<unsigned char*>(at(0x2161c0))=0xcc;
    CHECK(!install() && installs==0);
    *static_cast<unsigned char*>(at(0x2161c0))=0x55;
    CHECK(A2FO_ModuleInit(&api) && g_ready && registrations==4 && installs==8);
    jump(0x22f1c0,reinterpret_cast<void*>(first_fake));jump(0x221080,reinterpret_cast<void*>(next_fake));
    Buffer klass{},yard{},ship{},instance{},other_instance{},db{},channel{},target{},queue{},engine{},cloned{},second_class{};
    put(yard.data(),0x40,static_cast<void*>(klass.data()));put(yard.data(),4,static_cast<void*>(instance.data()));put(yard.data(),0x28,std::uint32_t(10));
    put(ship.data(),0x40,static_cast<void*>(klass.data()));put(ship.data(),4,static_cast<void*>(other_instance.data()));put(ship.data(),0x28,std::uint32_t(11));
    put(instance.data(),0x80,static_cast<void*>(db.data()));put(other_instance.data(),0x80,static_cast<void*>(db.data()));
    put(db.data(),0x68,static_cast<void*>(channel.data()));put(channel.data(),0,at(0x2bc42c));put(channel.data(),0x3c,static_cast<void*>(target.data()));put(channel.data(),0x40,std::uint32_t(0x24));
    std::array<std::array<float,12>,11> data{};for(unsigned i=0;i<data.size();++i)data[i][9]=float(i);
    put(channel.data(),0x34,std::uint32_t(data.size()));put(channel.data(),0x2c,1.1f);put(channel.data(),0x4c,static_cast<void*>(data.data()));
    h_read.gateway=reinterpret_cast<void*>(noop1);read_hook(channel.data(),nullptr,nullptr);CHECK(g_samples.count(channel.data()));
    // Source snapshot survives native key removal and is shared by clones.
    clone_result=cloned.data();h_clone.gateway=reinterpret_cast<void*>(clone_fake);
    CHECK(clone_hook(channel.data(),nullptr,db.data())==cloned.data());
    CHECK(g_samples.at(cloned.data())==g_samples.at(channel.data()));
    put(channel.data(),0x34,std::uint32_t(2));
    // Actual class callback receives the core's effective inherited snapshot.
    const std::array<std::pair<const char*,const char*>,3> entries{{
        {"animation0event","spawn"},{"animation0start","0"},{"animation0end","1"}}};
    std::vector<A2FO_OdfFieldView> fields;
    for(auto field:entries) fields.push_back({{field.first,std::uint32_t(std::strlen(field.first))},{field.second,std::uint32_t(std::strlen(field.second))}});
    A2FO_GameObjectClassLoadedEvent loaded{sizeof(loaded),second_class.data(),{"fixture",7},fields.data(),std::uint32_t(fields.size())};
    class_loaded(&loaded,nullptr);CHECK(g_classes.at(second_class.data()).size()==1);
    Clip forward{"production_begin",0,10,true,false,false,1},reverse{"repair_begin",0,10,false,false,false,1};
    Clip close{"repair_end",0,10,false,false,false,1};
    g_classes[klass.data()]={forward,reverse,close,Clip{"production_complete",4,4},Clip{"production_cancel",2,2},Clip{"launch_begin",1,1},Clip{"launch_clear",8,8},Clip{"launch_abort",3,3},Clip{"spawn",0,0},Clip{"load",6,6}};
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);tick(yard.data(),0);tick(yard.data(),0.5f);
    CHECK(evaluate(instance.data(),channel.data(),nullptr));CHECK(read<float>(target.data(),0x24+9*4)==5);
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);CHECK(g_states.at(yard.data()).playback.sample()==5); // no per-tick restart
    emit(ship.data(),"repair_begin");tick(ship.data(),0);tick(ship.data(),0.2f);
    CHECK(evaluate(other_instance.data(),channel.data(),nullptr));CHECK(read<float>(target.data(),0x24+9*4)==8);
    CHECK(evaluate(instance.data(),channel.data(),nullptr));CHECK(read<float>(target.data(),0x24+9*4)==5); // shared model restored
    put(at(0x3ad508),0,static_cast<void*>(engine.data()));put(engine.data(),0x100,static_cast<void*>(other_instance.data()));
    evaluate_hook(channel.data(),nullptr,nullptr);CHECK(read<float>(target.data(),0x24+9*4)==8);
    production(yard.data(),A2FO_PRODUCER_EVENT_FINISHED);CHECK(g_states.at(yard.data()).playback.sample()==4);
    production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);CHECK(g_states.at(yard.data()).playback.sample()==2);
    h_dock.gateway=reinterpret_cast<void*>(noop1);dock_hook(yard.data(),nullptr,ship.data());tick(yard.data(),0.2f);
    auto cursor=g_states.at(yard.data()).playback.frame;dock_hook(yard.data(),nullptr,ship.data());CHECK(g_states.at(yard.data()).playback.frame==cursor);
    dock_hook(yard.data(),nullptr,nullptr);CHECK(g_states.at(yard.data()).playback.clip.event=="repair_end");CHECK(g_states.at(ship.data()).playback.clip.event=="repair_end");
    h_add.gateway=reinterpret_cast<void*>(noop1);h_output.gateway=reinterpret_cast<void*>(noop1);
    put(queue.data(),0,at(0x2b3714));put(queue.data(),0x1c,static_cast<void*>(yard.data()));put(yard.data(),0x2b4,static_cast<void*>(queue.data()));
    put(ship.data(),0x194,std::uint32_t(1));put(ship.data(),0x198,std::uint32_t(10));
    add_hook(queue.data(),nullptr,ship.data());CHECK(g_states.at(yard.data()).playback.sample()==1);
    output_hook(queue.data(),nullptr,ship.data());CHECK(g_states.at(yard.data()).playback.sample()==8 && g_launches.empty());
    add_hook(queue.data(),nullptr,ship.data());cleanup(ship.data());CHECK(g_states.at(yard.data()).playback.sample()==3 && g_launches.empty());
    A2FO_CraftEvent load{sizeof(load),A2FO_CRAFT_EVENT_POST_LOAD,yard.data(),0};craft_event(&load,nullptr);CHECK(g_states.at(yard.data()).playback.sample()==6);tick(yard.data(),0.1f);CHECK(g_states.at(yard.data()).playback.sample()==6);
    // Combined activity clips override their separate begin/end bindings.
    g_classes[klass.data()].push_back(Clip{"production",0,10,true,true,true,1});
    g_classes[klass.data()].push_back(Clip{"repair",0,10});
    cleanup(yard.data());production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);
    tick(yard.data(),0);tick(yard.data(),0.4f);CHECK(g_states.at(yard.data()).playback.sample()==4);
    CHECK(!g_states.at(yard.data()).playback.clip.repeat && !g_states.at(yard.data()).playback.clip.reset);
    production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);CHECK(g_states.at(yard.data()).playback.sample()==4);
    tick(yard.data(),0.2f);CHECK(g_states.at(yard.data()).playback.sample()==2);
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);CHECK(g_states.at(yard.data()).playback.sample()==2);
    tick(yard.data(),2);CHECK(g_states.at(yard.data()).playback.sample()==10);
    production(yard.data(),A2FO_PRODUCER_EVENT_FINISHED);tick(yard.data(),2);CHECK(g_states.at(yard.data()).playback.sample()==0);
    dock_hook(yard.data(),nullptr,ship.data());tick(yard.data(),0.4f);CHECK(g_states.at(yard.data()).playback.sample()==4);
    dock_hook(yard.data(),nullptr,nullptr);tick(yard.data(),0.4f);CHECK(g_states.at(yard.data()).playback.sample()==0);
    // Native construction rigs retain their production state until every bee
    // has landed, independently of the earlier job completion/cancel callback.
    std::array<unsigned char,0x500> rig_class{};
    put(rig_class.data(),0,at(0x2b1af4));put(rig_class.data(),0x490,std::int32_t(6));
    g_classes[rig_class.data()]=g_classes.at(klass.data());
    cleanup(yard.data());put(yard.data(),0,at(0x2b22ec));
    put(yard.data(),0x40,static_cast<void*>(rig_class.data()));put(yard.data(),0x2ac,std::int32_t(20));
    for(auto end:{A2FO_PRODUCER_EVENT_FINISHED,A2FO_PRODUCER_EVENT_CANCELLED}) {
        production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);tick(yard.data(),0);tick(yard.data(),2);
        // Even a stale 'all returned' value at the notification must not close
        // immediately: cancellation's recall/reset may not have completed yet.
        put(yard.data(),0x2b0,std::int32_t(6));production(yard.data(),end);
        CHECK(g_states.at(yard.data()).production_end && !g_states.at(yard.data()).returning);
        put(yard.data(),0x2b0,std::int32_t(0));post(yard.data());tick(yard.data(),2);
        CHECK(g_states.at(yard.data()).playback.sample()==10);
        for(int landed=1;landed<6;++landed) {
            put(yard.data(),0x2b0,landed);post(yard.data());
            CHECK(!g_states.at(yard.data()).returning && g_states.at(yard.data()).production_end);
        }
        put(yard.data(),0x2b0,std::int32_t(6));post(yard.data());
        CHECK(g_states.at(yard.data()).returning && !g_states.at(yard.data()).production_end);
        tick(yard.data(),0.2f);CHECK(g_states.at(yard.data()).playback.sample()==8);
        cleanup(yard.data());
    }
    // A new active job supersedes the old deferred edge, keeping the doors open.
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);tick(yard.data(),0);tick(yard.data(),2);
    put(yard.data(),0x2b0,std::int32_t(0));production(yard.data(),A2FO_PRODUCER_EVENT_FINISHED);
    put(yard.data(),0x2a0,std::uint32_t(42));production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);
    put(yard.data(),0x2b0,std::int32_t(6));post(yard.data());
    CHECK(!g_states.at(yard.data()).production_end && !g_states.at(yard.data()).returning);
    // Zero-bee and never-created groups close normally without a permanent wait.
    put(rig_class.data(),0x490,std::int32_t(0));production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);
    CHECK(g_states.at(yard.data()).returning);
    put(rig_class.data(),0x490,std::int32_t(6));put(yard.data(),0x2ac,std::int32_t(-1));
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);
    CHECK(!g_states.at(yard.data()).production_end && g_states.at(yard.data()).returning);
    // Separate completion/cancel clips share the same last-bee timing.
    g_classes[rig_class.data()]={forward,Clip{"production_complete",4,4},Clip{"production_cancel",2,2}};
    put(yard.data(),0x2ac,std::int32_t(21));
    for(auto end:{A2FO_PRODUCER_EVENT_FINISHED,A2FO_PRODUCER_EVENT_CANCELLED}) {
        cleanup(yard.data());production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);
        tick(yard.data(),0);tick(yard.data(),2);put(yard.data(),0x2b0,std::int32_t(5));
        production(yard.data(),end);post(yard.data());CHECK(g_states.at(yard.data()).playback.sample()==10);
        put(yard.data(),0x2b0,std::int32_t(6));post(yard.data());
        CHECK(g_states.at(yard.data()).playback.sample()==(end==A2FO_PRODUCER_EVENT_FINISHED?4u:2u));
    }
    // Load/cleanup discards pending return state.
    cleanup(yard.data());production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);
    put(yard.data(),0x2b0,std::int32_t(0));production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);
    CHECK(g_states.at(yard.data()).production_end);
    craft_event(&load,nullptr);CHECK(!g_states.at(yard.data()).production_end);
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);production(yard.data(),A2FO_PRODUCER_EVENT_CANCELLED);
    CHECK(g_states.at(yard.data()).production_end);cleanup(yard.data());CHECK(!g_states.count(yard.data()));
    // Borrowed hybrid instance vtables cannot opt a ResearchStation class into
    // ConstructionRig fields.
    cleanup(yard.data());put(yard.data(),0x40,static_cast<void*>(klass.data()));
    production(yard.data(),A2FO_PRODUCER_EVENT_STARTING_EFFECT);production(yard.data(),A2FO_PRODUCER_EVENT_FINISHED);
    CHECK(!g_states.at(yard.data()).production_end && g_states.at(yard.data()).returning);
    put(yard.data(),0,static_cast<void*>(nullptr));
    // Configured units suppress the vanilla clock even before any event fires.
    cleanup(yard.data());auto s=state(yard.data());CHECK(s && s->playback.active && s->playback.sample()==0);
    CHECK(A2FO_AnimationsControlled(instance.data()));
    put(instance.data(),0x0c,std::uint32_t(5)); // native reverse+loop cannot win
    CHECK(A2FO_AnimationsEvaluate(instance.data(),channel.data(),nullptr));CHECK(read<float>(target.data(),0x24+9*4)==0);
    // State priority: specific warp/impulse over generic move; attack above move.
    g_classes[klass.data()].push_back(Clip{"move",0,10});
    g_classes[klass.data()].push_back(Clip{"move_impulse",1,9});
    g_classes[klass.data()].push_back(Clip{"move_warp",2,8});
    g_classes[klass.data()].push_back(Clip{"attack",3,7});
    Buffer control{},vtable{},weapon{},weapon_class{},project{};
    put(yard.data(),0x1b0,static_cast<void*>(control.data()));put(control.data(),0,static_cast<void*>(vtable.data()));
    put(vtable.data(),0x18,at(0x0ad970));
    tick(yard.data(),0);
    observe_activity(yard.data(),*s);
    put(instance.data(),0x68,1.0f);observe_activity(yard.data(),*s);CHECK(s->playback.clip.event=="move_impulse");
    put(control.data(),0x20,std::uint32_t(1));put(instance.data(),0x68,2.0f);observe_activity(yard.data(),*s);CHECK(s->playback.clip.event=="move_warp");
    put(yard.data(),0x50,std::uint32_t(6));put(instance.data(),0x68,3.0f);observe_activity(yard.data(),*s);CHECK(s->playback.clip.event=="attack");
    tick(yard.data(),0.2f);CHECK(s->playback.sample()==5);
    put(yard.data(),0x50,std::uint32_t(0));observe_activity(yard.data(),*s);CHECK(s->returning && s->playback.sample()==5);
    tick(yard.data(),0.2f);CHECK(s->playback.sample()==3);
    // Weapon property and actual class filename select the owning unit's clip.
    jump(0x271050,reinterpret_cast<void*>(owner_fake));jump(0x2593a0,reinterpret_cast<void*>(odf_fake));weapon_owner=yard.data();
    put(weapon_class.data(),0x208,static_cast<void*>(project.data()));put(weapon.data(),4,static_cast<void*>(weapon_class.data()));
    A2FO_OdfFieldView enabled{{"animation",9},{"1",1}};
    A2FO_WeaponClassLoadedEvent we{sizeof(we),weapon_class.data(),nullptr,nullptr,&enabled,1};
    weapon_class_loaded(&we,nullptr);CHECK(g_weapon_events.at(weapon_class.data())=="fphoton");
    g_classes[klass.data()].push_back(Clip{"fphoton",6,10});
    A2FO_WeaponTriggerEvent trigger{sizeof(trigger),A2FO_WEAPON_TRIGGER_PRECHECK,weapon.data(),nullptr};
    CHECK(weapon_trigger(&trigger,nullptr));CHECK(s->playback.clip.event!="fphoton");
    trigger.kind=A2FO_WEAPON_TRIGGER_COMMITTED;CHECK(weapon_trigger(&trigger,nullptr));CHECK(s->playback.clip.event=="fphoton");
    tick(yard.data(),0.2f);CHECK(s->playback.sample()==8);
    A2FO_AnimationsWeaponFired(weapon.data());CHECK(s->playback.sample()==6);
    enabled.value={"0",1};weapon_class_loaded(&we,nullptr);CHECK(!g_weapon_events.count(weapon_class.data()));
    tick(yard.data(),0.2f);A2FO_AnimationsWeaponFired(weapon.data());CHECK(s->playback.sample()==8);
    tick(yard.data(),1.0f);observe_activity(yard.data(),*s); // attack timeout; pulse completes normally
    // Ending an activity before geometry arrives cannot open it belatedly.
    cleanup(ship.data());put(other_instance.data(),0x80,static_cast<void*>(nullptr));
    emit(ship.data(),"production_begin");CHECK(g_states.at(ship.data()).pending=="production");
    emit(ship.data(),"production_cancel");CHECK(g_states.at(ship.data()).pending=="$idle");
    put(other_instance.data(),0x80,static_cast<void*>(db.data()));tick(ship.data(),0);
    CHECK(g_states.at(ship.data()).playback.clip.event!="production");
    auto invalid=forward;invalid.end=100;CHECK(!bind(g_states.at(yard.data()),invalid));
    h_destroy.gateway=reinterpret_cast<void*>(noop0);destroy_hook(channel.data(),nullptr);CHECK(!g_samples.count(channel.data()));
    CHECK(g_samples.at(cloned.data())->matrices.size()==11);
    destroy_hook(cloned.data(),nullptr);CHECK(g_samples.empty());
    cleanup(yard.data());cleanup(ship.data());CHECK(g_states.empty() && g_instances.empty());
    // Check the deliverable DLLs load and publish their actual C bridge names.
    for(auto filename:{"build/modules/A2FOAnimations.dll","build/modules/A2FOAnimatedHardpoints.dll","build/modules/A2FOMuzzleFlashes.dll"}) {
        HMODULE module=LoadLibraryA(filename);CHECK(module);
        CHECK(GetProcAddress(module,"A2FO_ModuleInit"));CHECK(GetProcAddress(module,"A2FO_ModuleShutdown"));
        if(std::strstr(filename,"A2FOAnimations.dll")) {
            CHECK(GetProcAddress(module,"A2FO_AnimationsEvaluate"));
            CHECK(GetProcAddress(module,"A2FO_AnimationsControlled"));
            CHECK(GetProcAddress(module,"A2FO_AnimationsWeaponFired"));
        }
        FreeLibrary(module);
    }
    VirtualFree(g_armada,0,MEM_RELEASE);
    std::cout<<"Animation x86 integration: signature fail-closed, registration limits, source frames, shared poses, render bridge, paired reversal, movement/warp/attack, weapon opt-in, vanilla override, production, repair, launch/abort, load and cleanup passed\n";
}
