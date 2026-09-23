// Event-driven SOD matrix clips. Native gameplay callbacks always continue.
#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "playback.hpp"
#include "api.hpp"
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <memory>
#include <unordered_map>

namespace {
using namespace a2fo::animations;
const A2FO_ModuleApi* g_api=nullptr;
std::uint8_t* g_armada=nullptr;
bool g_ready=false;
constexpr char name[]="A2FOAnimations";
A2FO_InlineHook h_read{},h_clone{},h_destroy{},h_evaluate{},h_animate{},h_dock{},h_add{},h_output{};
void* g_explicit_instance=nullptr;
using Unary=void (__fastcall*)(void*,void*,void*);
using Nullary=void (__fastcall*)(void*,void*);
using Clone=void* (__fastcall*)(void*,void*,void*);

void log(const char* text) {if(g_api && g_api->log) g_api->log(name,text);}
void* at(std::uintptr_t rva){return g_armada+rva;}
bool accessible(const void* p,std::size_t size,bool write=false) {
    auto cursor=reinterpret_cast<std::uintptr_t>(p);
    if(!cursor || size>UINTPTR_MAX-cursor) return false;
    auto end=cursor+size;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION m{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&m,sizeof(m)) || m.State!=MEM_COMMIT ||
            (m.Protect&(PAGE_NOACCESS|PAGE_GUARD))) return false;
        auto flags=m.Protect&0xff;
        if(write && flags!=PAGE_READWRITE && flags!=PAGE_WRITECOPY &&
            flags!=PAGE_EXECUTE_READWRITE && flags!=PAGE_EXECUTE_WRITECOPY) return false;
        auto next=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
        if(next<=cursor) return false;
        cursor=next;
    }
    return true;
}
template<class T> T read(const void* base,std::size_t offset,T fallback={}) {
    if(!base) return fallback;
    auto p=static_cast<const std::uint8_t*>(base)+offset;
    if(!accessible(p,sizeof(T))) return fallback;
    T value;std::memcpy(&value,p,sizeof(value));return value;
}
struct Samples {
    std::vector<std::array<float,12>> matrices;
    double spacing=0;
};
std::unordered_map<void*,std::shared_ptr<const Samples>> g_samples;
std::unordered_map<void*,std::vector<Clip>> g_classes;
std::unordered_map<void*,std::string> g_class_names;
std::unordered_map<void*,std::string> g_weapon_events;
struct State {
    std::uint32_t handle=0;
    void* klass=nullptr;
    void* instance=nullptr;
    void* database=nullptr;
    Playback playback;
    std::string pending;
    bool warned=false;
    bool seen_simulation=false;
    bool event_selected=false;
    bool returning=false;
    bool pending_paired=false, pending_return=false;
    const char* production_end=nullptr;
    unsigned conditions=0;
    double recent_attack=0;
    std::array<float,3> previous_position{};
    bool position_valid=false;
};
std::unordered_map<void*,State> g_states;
std::unordered_map<void*,void*> g_instances;
struct Launch {void* yard=nullptr;std::uint32_t yard_handle=0;};
std::unordered_map<void*,Launch> g_launches;
std::unordered_map<void*,void*> g_docks;
std::unordered_map<void*,std::uint32_t> g_jobs;

void bootstrap(State& s);
void erase_state(void* craft) {
    auto it=g_states.find(craft);
    if(it!=g_states.end()) {g_instances.erase(it->second.instance);g_states.erase(it);}
}
State* state(void* craft) {
    if(!craft) return nullptr;
    auto klass=read<void*>(craft,0x40);
    if(g_classes.find(klass)==g_classes.end()) return nullptr;
    auto handle=read<std::uint32_t>(craft,0x28);
    auto instance=read<void*>(craft,4);
    auto& s=g_states[craft];
    if(s.handle!=handle || s.klass!=klass || s.instance!=instance) {
        g_instances.erase(s.instance);s=State{};
        s.handle=handle;s.klass=klass;s.instance=instance;
    }
    if(instance) g_instances[instance]=craft;
    if(!s.playback.active && s.pending.empty()) bootstrap(s);
    return &s;
}
State* instance_state(void* instance) {
    auto i=g_instances.find(instance);
    if(i==g_instances.end()) return nullptr;
    auto s=g_states.find(i->second);
    if(s==g_states.end() || read<void*>(i->second,4)!=instance ||
        read<std::uint32_t>(i->second,0x28)!=s->second.handle) return nullptr;
    return &s->second;
}
void* first_channel(void* db) {
    return reinterpret_cast<void* (__fastcall*)(void*,void*)>(at(0x22f1c0))(db,nullptr);
}
void* next_channel(void* channel) {
    return reinterpret_cast<void* (__fastcall*)(void*,void*)>(at(0x221080))(channel,nullptr);
}
bool matrix_channel(void* c){return read<void*>(c,0)==at(0x2bc42c);}
bool bind(State& s,const Clip& clip) {
    void* db=read<void*>(s.instance,0x80);
    if(!db) return false;
    double spacing=0;std::size_t channels=0, matrices=0;
    void* c=first_channel(db);
    for(;c && channels<4096;c=next_channel(c),++channels) {
        if(!matrix_channel(c)) continue;
        ++matrices;
        auto data=g_samples.find(c);
        if(data==g_samples.end() || data->second->matrices.empty()) return false;
        const auto& samples=*data->second;
        if(samples.matrices.size()==1) continue; // A constant track follows all clips.
        if(clip.end>=samples.matrices.size()) return false;
        if(spacing && std::abs(samples.spacing-spacing)>1e-5*std::max(spacing,samples.spacing)) return false;
        spacing=samples.spacing;
    }
    if(c || !matrices) return false;
    if(!spacing) {if(clip.start || clip.end) return false;spacing=1;}
    s.database=db;s.playback.start(clip,spacing);return true;
}
const Clip* definition(const State& s,const std::string& event) {
    auto found=g_classes.find(s.klass);if(found==g_classes.end()) return nullptr;
    for(const auto& clip:found->second) if(clip.event==event) return &clip;
    return nullptr;
}
void bootstrap(State& s) {
    const auto& clips=g_classes.at(s.klass);if(clips.empty()) return;
    Clip neutral=clips.front();neutral.event="$idle";neutral.end=neutral.start;
    neutral.forward=true;neutral.repeat=false;neutral.reset=false;
    if(!bind(s,neutral)) s.pending="$idle";
}
void select_clip(State& s,const Clip& source,bool paired=false,bool returning=false) {
    Clip clip=source;
    if(paired) {clip.forward=returning?!source.forward:source.forward;clip.repeat=false;clip.reset=false;}
    s.event_selected=true;s.returning=returning;s.warned=false;
    s.pending=source.event;s.pending_paired=paired;s.pending_return=returning;
    if(paired && s.playback.active && s.playback.clip.event==source.event &&
        s.database==read<void*>(s.instance,0x80)) {
        s.playback.redirect(clip.forward);s.pending.clear();
    } else if(bind(s,clip)) s.pending.clear();
}
void reconcile(State& s,bool force) {
    const Clip* selected=nullptr;
    for(unsigned i=0;i<paired_events.size();++i) if(s.conditions&(1u<<i)) {
        selected=definition(s,paired_events[i]);if(selected) break;
    }
    const bool playing_pair=paired_index(s.playback.clip.event)>=0;
    if(selected) {
        if(s.playback.active && s.playback.clip.event==selected->event && !s.returning) return;
        if(!force && !playing_pair && !s.playback.finished && s.playback.clip.event!="$idle") return;
        select_clip(s,*selected,true,false);
    } else if(playing_pair && !s.returning) {
        if(auto clip=definition(s,s.playback.clip.event)) select_clip(s,*clip,true,true);
    } else if(!playing_pair && s.pending_paired && !s.pending.empty()) {
        // The activity ended before its model could bind; never open it later.
        s.pending="$idle";s.pending_paired=false;s.returning=false;
    }
}
void set_condition(State& s,const char* event,bool active) {
    const int index=paired_index(event);if(index<0) return;
    auto changed=active?(s.conditions | (1u<<index)):(s.conditions & ~(1u<<index));
    if(changed==s.conditions) return;
    s.conditions=changed;reconcile(s,true);
}
void emit(void* craft,const char* event) {
    State* s=state(craft);if(!s) return;
    const char* pair=nullptr;bool active=false;
    if(std::strcmp(event,"production_begin")==0) {pair="production";active=true;}
    if(std::strcmp(event,"production_complete")==0 || std::strcmp(event,"production_cancel")==0) pair="production";
    if(std::strcmp(event,"repair_begin")==0) {pair="repair";active=true;}
    if(std::strcmp(event,"repair_end")==0) pair="repair";
    if(pair) {
        set_condition(*s,pair,active);
        // A combined binding owns both edges, overriding the separate clips.
        if(definition(*s,pair)) return;
    }
    if(auto clip=definition(*s,event)) select_clip(*s,*clip);
}
bool construction_rig(void* craft,const State& s) {
    // Hybrid research producers temporarily borrow the instance vtable/tail;
    // their class remains ResearchStationClass and must not use these fields.
    return read<void*>(craft,0)==at(0x2b22ec) &&
        read<void*>(s.klass,0)==at(0x2b1af4);
}
void production_ended(void* craft,const char* event) {
    const bool started=g_jobs.erase(craft)!=0;
    State* s=state(craft);
    if(s && construction_rig(craft,*s) && (started || s->production_end) &&
        read<std::int32_t>(craft,0x2ac,-1)>=0 && read<std::int32_t>(s->klass,0x490)>0) {
        // CANCELLED can be dispatched before the native cancellation returns.
        // Wait until POST simulation to observe the completed recall setup.
        s->production_end=event;
        return;
    }
    emit(craft,event);
}
void finish_returned_bees(void* craft,State& s) {
    if(!s.production_end || !construction_rig(craft,s)) return;
    const auto total=read<std::int32_t>(s.klass,0x490);
    const auto returned=read<std::int32_t>(craft,0x2b0);
    if(total>0 && returned<total) return;
    const char* event=s.production_end;s.production_end=nullptr;
    emit(craft,event);
}
void retry(State& s) {
    if(s.pending.empty()) return;
    if(s.pending=="$idle") {s.pending.clear();bootstrap(s);return;}
    if(auto source=definition(s,s.pending)) {
        Clip clip=*source;
        if(s.pending_paired) {clip.forward=s.pending_return?!clip.forward:clip.forward;clip.repeat=false;clip.reset=false;}
        if(bind(s,clip)) s.pending.clear();
        else if(!s.warned) {
            const auto message=g_class_names[s.klass]+": "+s.pending+
                ": clip pending (model not ready, missing source samples, incompatible timing or frame range); controlled/native previous pose retained";
            log(message.c_str());s.warned=true;
        }
    }
}
// Actual translation, not a move order or a change in facing. Sampling is POST
// simulation so input orders alone do not open movement-specific geometry.
void observe_activity(void* craft,State& s) {
    std::array<float,3> position{};bool finite=true;
    for(unsigned i=0;i<3;++i) {position[i]=read<float>(s.instance,0x68+i*4);finite &= std::isfinite(position[i]);}
    double distance=0;
    if(finite && s.position_valid) for(unsigned i=0;i<3;++i) {
        const double delta=double(position[i])-s.previous_position[i];distance+=delta*delta;
    }
    const bool moving=finite && s.position_valid && distance>1e-6;
    s.previous_position=position;s.position_valid=finite;
    void* control=read<void*>(craft,0x1b0);
    void* warp_method=read<void*>(read<void*>(control,0),0x18);
    // Verified cHoverCraftControlBase (also Borg/smooth inherited control).
    const bool warp=warp_method==at(0x0ad970) && read<std::uint32_t>(control,0x20)!=0;
    const bool attacking=read<std::uint32_t>(craft,0x50)==6 ||
        read<std::uint32_t>(read<void*>(craft,0x44),0x3c)==6 || s.recent_attack>0;
    unsigned next=s.conditions & 3u; // Preserve repair and production edges.
    if(attacking) next|=1u<<2;
    if(moving && warp) next|=1u<<3;
    if(moving && !warp) next|=1u<<4;
    if(moving) next|=1u<<5;
    const bool changed=next!=s.conditions;s.conditions=next;
    reconcile(s,changed);
}
bool evaluate(void* instance,void* channel,void* fallback) {
    if(!g_ready || !matrix_channel(channel)) return false;
    State* s=instance_state(instance);
    if(!s || !s->playback.active || read<void*>(instance,0x80)!=s->database) return false;
    auto found=g_samples.find(channel);if(found==g_samples.end()) return false;
    const auto& frames=found->second->matrices;
    auto index=frames.size()==1?0:s->playback.sample();
    if(index>=frames.size()) return false;
    void* target=read<void*>(channel,0x3c);if(!target) target=fallback;
    auto offset=read<std::uint32_t>(channel,0x40);
    if(!target || offset>0x1000) return false;
    auto output=static_cast<std::uint8_t*>(target)+offset;
    if(!accessible(output,48,true)) return false;
    std::memcpy(output,frames[index].data(),48);
    return true;
}
void __fastcall read_hook(void* channel,void*,void* stream) {
    reinterpret_cast<Unary>(h_read.gateway)(channel,nullptr,stream);
    if(!g_ready) return;
    try {
        g_samples.erase(channel);
        const auto count=read<std::uint32_t>(channel,0x34);
        const auto duration=read<float>(channel,0x2c);
        auto data=read<void*>(channel,0x4c);
        if(!count || count>65535 || !std::isfinite(duration) || duration<=0 || !accessible(data,count*48u)) return;
        auto samples=std::make_shared<Samples>();
        samples->matrices.resize(count);samples->spacing=double(duration)/count;
        std::memcpy(samples->matrices.data(),data,count*48u);
        g_samples[channel]=std::move(samples);
    } catch(...) {log("Cannot retain SOD source frames; affected clips disabled");}
}
void* __fastcall clone_hook(void* channel,void*,void* db) {
    // Keep the source alive even if the native clone path manipulates channels.
    std::shared_ptr<const Samples> source;
    if(g_ready) {auto it=g_samples.find(channel);if(it!=g_samples.end()) source=it->second;}
    void* result=reinterpret_cast<Clone>(h_clone.gateway)(channel,nullptr,db);
    if(g_ready && result) try {g_samples.erase(result);if(source)g_samples[result]=std::move(source);} catch(...) {log("Cannot retain cloned SOD frames");}
    return result;
}
void __fastcall destroy_hook(void* channel,void*) {
    g_samples.erase(channel);
    reinterpret_cast<Nullary>(h_destroy.gateway)(channel,nullptr);
}
void __fastcall evaluate_hook(void* channel,void*,void* fallback) {
    void* engine=read<void*>(at(0x3ad508),0);
    void* instance=g_explicit_instance?g_explicit_instance:read<void*>(engine,0x100);
    if(!evaluate(instance,channel,fallback)) reinterpret_cast<Unary>(h_evaluate.gateway)(channel,nullptr,fallback);
}
void __fastcall animate_hook(void* instance,void*) {
    void* saved=g_explicit_instance;g_explicit_instance=instance;
    reinterpret_cast<Nullary>(h_animate.gateway)(instance,nullptr);
    g_explicit_instance=saved;
}
void end_launch(void* craft,const char* event) {
    auto it=g_launches.find(craft);if(it==g_launches.end()) return;
    auto launch=it->second;g_launches.erase(it);
    emit(craft,event);
    if(read<std::uint32_t>(launch.yard,0x28)!=launch.yard_handle) return;
    // Keep yard doors active until its last tracked output leaves.
    for(const auto& entry:g_launches) if(entry.second.yard==launch.yard) return;
    emit(launch.yard,event);
}
void __fastcall add_hook(void* queue,void*,void* craft) {
    reinterpret_cast<Unary>(h_add.gateway)(queue,nullptr,craft);
    if(!g_ready) return;
    try {
        auto yard=read<void*>(queue,0x1c);
        if(read<void*>(queue,0)!=at(0x2b3714) || read<void*>(yard,0x2b4)!=queue ||
            !read<std::uint32_t>(craft,0x194) ||
            read<std::uint32_t>(craft,0x198)!=read<std::uint32_t>(yard,0x28)) return;
        if(g_launches.find(craft)!=g_launches.end()) return;
        bool first=true;for(const auto& e:g_launches) if(e.second.yard==yard) first=false;
        g_launches[craft]={yard,read<std::uint32_t>(yard,0x28)};
        emit(craft,"launch_begin");if(first) emit(yard,"launch_begin");
    } catch(...) {log("Launch animation event failed");}
}
void __fastcall output_hook(void* queue,void*,void* craft) {
    reinterpret_cast<Unary>(h_output.gateway)(queue,nullptr,craft);
    if(g_ready) try {end_launch(craft,"launch_clear");} catch(...) {log("Launch-clear animation event failed");}
}
void __fastcall dock_hook(void* yard,void*,void* craft) {
    reinterpret_cast<Unary>(h_dock.gateway)(yard,nullptr,craft);
    if(!g_ready) return;
    try {
        auto it=g_docks.find(yard);void* previous=it==g_docks.end()?nullptr:it->second;
        if(previous==craft) return;
        if(previous) {g_docks.erase(yard);emit(previous,"repair_end");emit(yard,"repair_end");}
        if(craft) {g_docks[yard]=craft;emit(craft,"repair_begin");emit(yard,"repair_begin");}
    } catch(...) {log("Repair animation event failed");}
}
std::string string_view(A2FO_StringView s){return s.data?std::string(s.data,s.size):std::string{};}
void A2FO_CALL class_loaded(const A2FO_GameObjectClassLoadedEvent* e,void*) {
    if(!g_ready || !e || e->struct_size<sizeof(*e)) return;
    try {
        for(auto i=g_states.begin();i!=g_states.end();) {
            if(i->second.klass==e->object_class) {g_instances.erase(i->second.instance);i=g_states.erase(i);}
            else ++i;
        }
        g_classes.erase(e->object_class);g_class_names[e->object_class]=string_view(e->source_odf);Fields fields;
        for(std::uint32_t i=0;i<e->odf_field_count;++i) {
            auto key=string_view(e->odf_fields[i].name);
            std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return char(std::tolower(c));});
            fields[key]=string_view(e->odf_fields[i].value);
        }
        std::vector<Clip> clips;std::string error;
        if(!parse(fields,clips,error)) {auto message=string_view(e->source_odf)+": "+error;log(message.c_str());return;}
        if(!clips.empty()) {g_classes[e->object_class]=std::move(clips);auto message=string_view(e->source_odf)+": animation clips registered";log(message.c_str());}
    } catch(...) {log("Animation ODF registration failed");}
}
void cleanup(void* craft) {
    end_launch(craft,"launch_abort");
    for(auto i=g_launches.begin();i!=g_launches.end();) {
        if(i->second.yard==craft) {auto ship=i->first;i=g_launches.erase(i);emit(ship,"launch_abort");}
        else ++i;
    }
    for(auto i=g_docks.begin();i!=g_docks.end();) {
        if(i->first==craft || i->second==craft) {
            auto yard=i->first,ship=i->second;i=g_docks.erase(i);
            if(yard!=craft) emit(yard,"repair_end");
            if(ship!=craft) emit(ship,"repair_end");
        } else ++i;
    }
    g_jobs.erase(craft);erase_state(craft);
}
void A2FO_CALL craft_event(const A2FO_CraftEvent* e,void*) {
    if(!g_ready || !e || e->struct_size<sizeof(*e)) return;
    try {
        if(e->kind==A2FO_CRAFT_EVENT_CLEANUP) {cleanup(e->craft);return;}
        if(e->kind==A2FO_CRAFT_EVENT_POST_LOAD) {
            cleanup(e->craft);emit(e->craft,"load");
            if(auto s=state(e->craft)) s->seen_simulation=true;
            return;
        }
        if(e->kind!=A2FO_CRAFT_EVENT_SIMULATE_PRE && e->kind!=A2FO_CRAFT_EVENT_SIMULATE_POST) return;
        if(e->kind==A2FO_CRAFT_EVENT_SIMULATE_POST && g_launches.count(e->craft) &&
            !read<std::uint32_t>(e->craft,0x194)) end_launch(e->craft,"launch_abort");
        State* s=state(e->craft);if(!s) return;
        const bool fresh=!s->seen_simulation;
        s->seen_simulation=true;
        if(fresh && !s->event_selected) emit(e->craft,"spawn");
        if(s->playback.active && s->database!=read<void*>(s->instance,0x80)) {
            s->pending=s->playback.clip.event;s->playback.active=false;
        }
        retry(*s);
        if(e->kind==A2FO_CRAFT_EVENT_SIMULATE_PRE && !fresh) {
            s->playback.advance(e->elapsed_seconds);
            if(std::isfinite(e->elapsed_seconds) && e->elapsed_seconds>0)
                s->recent_attack=std::max(0.0,s->recent_attack-e->elapsed_seconds);
        }
        if(e->kind==A2FO_CRAFT_EVENT_SIMULATE_POST) {
            finish_returned_bees(e->craft,*s);
            observe_activity(e->craft,*s);
        }
    } catch(...) {log("Animation simulation event failed");}
}
bool A2FO_CALL producer_event(const A2FO_ProducerEvent* e,void*) {
    if(!g_ready || !e || e->struct_size<sizeof(*e)) return true;
    try {
        if(e->kind==A2FO_PRODUCER_EVENT_STARTING_EFFECT) {
            if(auto s=state(e->producer)) s->production_end=nullptr;
            auto id=read<std::uint32_t>(e->producer,0x2a0);
            auto it=g_jobs.find(e->producer);
            if(it==g_jobs.end() || it->second!=id) {g_jobs[e->producer]=id;emit(e->producer,"production_begin");}
        } else if(e->kind==A2FO_PRODUCER_EVENT_FINISHED) {
            production_ended(e->producer,"production_complete");
        } else if(e->kind==A2FO_PRODUCER_EVENT_CANCELLED) {
            production_ended(e->producer,"production_cancel");
        } else if(e->kind==A2FO_PRODUCER_EVENT_DESTROYING) cleanup(e->producer);
    } catch(...) {log("Production animation event failed");}
    return true;
}
std::string bounded_string(const char* value) {
    std::string result;
    for(unsigned i=0;value && i<255;++i) {
        if(!accessible(value+i,1)) return {};
        if(!value[i]) return result;
        result.push_back(value[i]);
    }
    return {};
}
void A2FO_CALL weapon_class_loaded(const A2FO_WeaponClassLoadedEvent* e,void*) {
    if(!g_ready || !e || e->struct_size<sizeof(*e)) return;
    try {
        g_weapon_events.erase(e->weapon_class);
        bool enabled=false;
        for(unsigned i=0;i<e->odf_field_count;++i) if(event_key(string_view(e->odf_fields[i].name))=="animation") {
            double number_value=0;
            if(number(clean(string_view(e->odf_fields[i].value)),number_value)) enabled=number_value!=0;
            else log("Weapon animation must be numeric (use animation = 1 to enable)");
        }
        if(!enabled) return;
        // WeaponClass ctor stores a copy of ParameterDB's cPrjID at +0x208.
        auto project=read<void*>(e->weapon_class,0x208);if(!project) return;
        auto odf=reinterpret_cast<const char* (__fastcall*)(void*,void*)>(at(0x2593a0))(project,nullptr);
        auto key=bounded_string(odf);
        const auto slash=key.find_last_of("/\\");if(slash!=std::string::npos) key=key.substr(slash+1);
        key=event_key(key);
        if(!valid_event(key) || builtin(key)) {log("Weapon animation name invalid or reserved by a built-in event");return;}
        g_weapon_events[e->weapon_class]=key;
    } catch(...) {log("Weapon animation registration failed");}
}
void weapon_used(void* weapon) {
    if(!weapon) return;
    void* owner=reinterpret_cast<void* (__fastcall*)(void*,void*)>(at(0x271050))(weapon,nullptr);
    State* s=state(owner);if(!s) return;
    s->recent_attack=1.0;
    set_condition(*s,"attack",true);
    auto policy=g_weapon_events.find(read<void*>(weapon,4));
    if(policy!=g_weapon_events.end()) emit(owner,policy->second.c_str());
}
bool A2FO_CALL weapon_trigger(const A2FO_WeaponTriggerEvent* e,void*) {
    if(g_ready && e && e->struct_size>=sizeof(*e) && e->kind==A2FO_WEAPON_TRIGGER_COMMITTED)
        try {weapon_used(e->weapon);} catch(...) {log("Weapon-use animation event failed");}
    return true; // Never claim or prevent a native weapon use.
}
struct HelperSpec {std::uintptr_t rva;std::vector<std::uint8_t> bytes;};
bool supported_helper(const HelperSpec& spec) {
    auto entry=static_cast<const std::uint8_t*>(at(spec.rva));
    if(accessible(entry,spec.bytes.size()) &&
        !std::memcmp(entry,spec.bytes.data(),spec.bytes.size())) return true;
    // FleetOpsHook replaces cPrjID::GetOdfName during its filesystem startup.
    // Keep calling that entry: its project IDs belong to FOFS, not stock Armada.
    // Accept only this helper's checked ECX-based replacement inside that DLL.
    if(spec.rva!=0x2593a0 || !accessible(entry,5) || entry[0]!=0xe9) return false;
    const auto displacement=read<std::int32_t>(entry,1);
    auto destination=reinterpret_cast<const std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(entry)+5+displacement);
    HMODULE fleet_ops=GetModuleHandleA("FleetOpsHook.dll");
    MEMORY_BASIC_INFORMATION region{};
    if(!fleet_ops || !VirtualQuery(destination,&region,sizeof(region)) ||
        region.State!=MEM_COMMIT || region.AllocationBase!=fleet_ops ||
        (region.Protect&(PAGE_GUARD|PAGE_NOACCESS))) return false;
    const auto protection=region.Protect&0xff;
    if(protection!=PAGE_EXECUTE && protection!=PAGE_EXECUTE_READ &&
        protection!=PAGE_EXECUTE_READWRITE && protection!=PAGE_EXECUTE_WRITECOPY) return false;
    constexpr std::uint8_t expected[]{0x55,0x8b,0xec,0x51,0x89,0x4d,0xfc,
        0x8b,0x45,0xfc,0x8b,0x00,0x85,0xc0,0x74,0x15};
    if(!accessible(destination,sizeof(expected)) ||
        std::memcmp(destination,expected,sizeof(expected))) return false;
    log("Using checked FleetOpsHook cPrjID::GetOdfName entry");
    return true;
}
std::vector<HelperSpec> helper_specs() {
    return {
        {0x2593a0,{0x8b,0x01,0x85,0xc0}}, // cPrjID::GetOdfName
        {0x271050,{0x8b,0x49,0x18,0x51,0xe8}}, // Weapon::GetOwner
        {0x0ad970,{0x8b,0x51,0x20,0x33,0xc0}}, // control IsWarping field
        // WorkerBeeHasReturned increments rig+2b0, compares class+490.
        {0x0b0140,{0x56,0x8b,0xf1,0x8b,0x86,0xb0,0x02,0x00,0x00,0x8b,0x4e,0x40,0x40,0x89,0x86,0xb0,0x02,0x00,0x00,0x8b,0x91,0x90,0x04,0x00,0x00,0x3b,0xc2}}
    };
}
struct HookSpec {std::uintptr_t rva;void* function;A2FO_InlineHook* hook;std::vector<std::uint8_t> bytes;};
std::vector<HookSpec> hook_specs() {
    return {
        {0x216270,reinterpret_cast<void*>(read_hook),&h_read,{0x55,0x8b,0xec,0x83,0xec,0x38}},
        {0x2165a0,reinterpret_cast<void*>(clone_hook),&h_clone,{0x55,0x8b,0xec,0x6a,0xff}},
        {0x213800,reinterpret_cast<void*>(destroy_hook),&h_destroy,{0x55,0x8b,0xec,0x51,0x56}},
        {0x2161c0,reinterpret_cast<void*>(evaluate_hook),&h_evaluate,{0x55,0x8b,0xec,0x53,0x8b,0xd9}},
        {0x22eab0,reinterpret_cast<void*>(animate_hook),&h_animate,{0x56,0x57,0x8b,0xf9,0x8a,0x47,0x76}},
        {0x0bb7f0,reinterpret_cast<void*>(dock_hook),&h_dock,{0x55,0x8b,0xec,0x56,0x8b,0xf1}},
        {0x139210,reinterpret_cast<void*>(add_hook),&h_add,{0x55,0x8b,0xec,0x83,0xec,0x18}},
        {0x13a500,reinterpret_cast<void*>(output_hook),&h_output,{0x55,0x8b,0xec,0x8b,0x41,0x1c}}
    };
}
bool install() {
    if(a2fo::supported_armada::identify(reinterpret_cast<HMODULE>(g_armada))==a2fo::supported_armada::Identity::unsupported) return false;
    for(const auto& s:helper_specs()) if(!supported_helper(s)) {
        char text[160];std::snprintf(text,sizeof(text),
            "Unsupported animation helper signature at RVA %08lx; runtime disabled",
            static_cast<unsigned long>(s.rva));log(text);return false;
    }
    auto specs=hook_specs();
    for(const auto& s:specs) if(!accessible(at(s.rva),s.bytes.size()) || std::memcmp(at(s.rva),s.bytes.data(),s.bytes.size())) {
        char text[100];std::snprintf(text,sizeof(text),"Unsupported animation hook signature at RVA %08lx",static_cast<unsigned long>(s.rva));log(text);return false;
    }
    for(const auto& s:specs) if(!g_api->install_inline_hook(at(s.rva),s.function,s.bytes.size(),s.bytes.data(),s.hook)) return false;
    return true;
}
} // namespace

extern "C" __declspec(dllexport) bool __cdecl A2FO_AnimationsEvaluate(void* instance,void* channel,void* fallback) {
    return evaluate(instance,channel,fallback);
}
extern "C" __declspec(dllexport) bool __cdecl A2FO_AnimationsControlled(void* instance) {
    auto s=g_ready?instance_state(instance):nullptr;
    return s && s->playback.active && read<void*>(instance,0x80)==s->database;
}
extern "C" __declspec(dllexport) void __cdecl A2FO_AnimationsWeaponFired(void* weapon) {
    if(g_ready && weapon) try {weapon_used(weapon);} catch(...) {log("Weapon-fire animation event failed");}
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if(!api || api->api_version!=A2FO_MODULE_API_VERSION ||
        !A2FO_MODULE_API_HAS(api,register_craft_event_handler) || !api->register_craft_event_handler ||
        !api->register_game_object_class_loaded_handler || !api->register_producer_event_handler ||
        !api->register_weapon_class_loaded_handler || !api->register_weapon_trigger_handler ||
        !api->log || !api->armada_module || !api->install_inline_hook) return false;
    g_api=api;g_armada=static_cast<std::uint8_t*>(api->armada_module());
    if(!g_armada) return false;
    try {
        if(!install()) {log("Animation runtime disabled; native playback retained");return true;}
        std::vector<std::string> storage;std::vector<const char*> fields;
        for(std::size_t i=0;i<=max_clips;++i) for(auto suffix:{"event","start","end","direction","repeat","resetonend","speed"})
            storage.push_back("animation"+std::to_string(i)+suffix);
        for(const auto& s:storage) fields.push_back(s.c_str());
        // The core collects the union before dispatching the full inherited
        // snapshot, but allows at most 64 requested fields per registration.
        bool registered=true;
        for(std::size_t offset=0;offset<fields.size();offset+=64) {
            auto count=std::min<std::size_t>(64,fields.size()-offset);
            A2FO_GameObjectClassLoadedHandler callback = offset+count==fields.size()
                ? class_loaded : +[](const A2FO_GameObjectClassLoadedEvent*,void*){};
            registered=api->register_game_object_class_loaded_handler(
                name,fields.data()+offset,count,callback,nullptr) && registered;
        }
        const char* weapon_fields[]={"animation"};
        g_ready=registered && api->register_producer_event_handler(name,producer_event,nullptr) &&
            api->register_craft_event_handler(name,craft_event,nullptr) &&
            api->register_weapon_class_loaded_handler(name,weapon_fields,1,weapon_class_loaded,nullptr) &&
            api->register_weapon_trigger_handler(name,weapon_trigger,nullptr);
        log(g_ready?"SOD animation clips initialized (32 clips, 16 built-in events plus weapon names)":"Animation callbacks unavailable; native playback retained");
    } catch(...) {g_ready=false;log("Animation initialization failed; native playback retained");}
    // Never unload after installing process-lifetime trampolines.
    return true;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {}
