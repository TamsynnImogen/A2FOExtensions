// Controlled Win32 client of the core copied-class light producer; no game or hooks.
#include "../core/native11_light_classes.hpp"
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
using namespace a2fo::native11_light_classes;
namespace {
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
constexpr std::uint64_t id=0x12340;
void* object(){return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));}
std::map<std::string,std::string> enabled(){return {{"lightSource","1"},{"lightType","point"},
    {"lightColor","0.2 0.4 0.8"}};}
struct Reader {HANDLE start;std::atomic<unsigned> checked{0};std::atomic<bool> bad{false};};
DWORD WINAPI read_while_replaced(void* raw){auto& reader=*static_cast<Reader*>(raw);
    if(WaitForSingleObject(reader.start,15000)!=WAIT_OBJECT_0){reader.bad=true;return 1;}
    for(unsigned i=0;i<64;++i){
        auto held=pin(id);const auto current_generation=generation();
        (void)current_generation;
        if(held && (held->class_id!=id||held->source_odf!="lights.odf"||
            (held->config.enabled&&held->config.light.type!=a2fo::native11::LightType::point)))reader.bad=true;
        ++reader.checked;
    }
    return 0;
}
}
int main(){
    disable();configure(true,nullptr);const auto initial_generation=generation();
    need(requested()&&!healthy()&&!pin(id),"requested without dispatcher has no current owners");
    auto fields=enabled();
    for(unsigned i=0;i<160;++i)fields["unrelated"+std::to_string(i)]=std::string(300,'x');
    need(!observe_class(object(),"lights.odf",fields)&&generation()==initial_generation&&
         !pin(id),"unavailable dispatch refuses observation without mutation");
    dispatch_available(true);need(healthy(),"dispatcher enables complete empty store");
    need(observe_class(object(),"lights.odf",fields),"copied class observed");
    auto original=pin(id);need(original&&original->class_id==id&&original->source_odf=="lights.odf"&&
        original->config.enabled&&original->config.light.type==a2fo::native11::LightType::point&&
        original->config.light.colour[0]==.2f&&original->config.light.colour[1]==.4f&&
        original->config.light.colour[2]==.8f,"immutable first record");
    const auto first_version=original->version;
    fields["lightColor"]="0 0 0";fields["lightType"]="broken";
    need(original->config.light.colour[0]==.2f&&original->config.light.colour[2]==.8f,
        "poisoned input map cannot change pinned config");
    const auto first_generation=generation();
    need(!observe_class(object(),"lights.odf",fields)&&generation()==first_generation+1&&
         !healthy()&&!pin(id),"invalid reload faults current lookup");
    fields=enabled();fields["lightColor"]="0.8 0.4 0.2";
    need(observe_class(object(),"lights.odf",fields)&&healthy(),"same class valid reload repairs fault");
    auto replacement=pin(id);need(replacement&&replacement->version>first_version&&
        replacement->config.light.colour[0]==.8f&&original->config.light.colour[0]==.2f,
        "replacement has new version; old pin survives");
    const auto replacement_generation=generation();
    std::map<std::string,std::string> off{{"lightSource","0"}};
    need(observe_class(object(),"lights.odf",off)&&generation()==replacement_generation+1,
        "disabled class replacement increments generation");
    auto disabled=pin(id);need(disabled&&!disabled->config.enabled&&
        disabled->version>replacement->version,"disabled class has record");
    const auto before_disable=generation();disable();
    need(generation()==before_disable+1&&!healthy()&&!pin(id)&&
        original->config.enabled&&replacement->config.enabled&&!disabled->config.enabled,
        "disable clears current lookup but held snapshots survive");
    configure(false,nullptr);dispatch_available(true);
    need(!requested()&&!healthy()&&!pin(id)&&!observe_class(object(),"lights.odf",fields),
        "configured false refuses current owners and observations");
    const auto before_rearm=generation();configure(true,nullptr);
    need(generation()==before_rearm+1&&!healthy()&&!pin(id),"rearm requires dispatcher again");
    dispatch_available(true);need(observe_class(object(),"lights.odf",enabled())&&healthy()&&pin(id),
        "rearm recovers current class");
    Reader reader{};reader.start=CreateEventW(nullptr,TRUE,FALSE,nullptr);need(reader.start,"reader gate");
    HANDLE worker=CreateThread(nullptr,0,read_while_replaced,&reader,0,nullptr);need(worker,"reader thread");
    need(SetEvent(reader.start)!=0,"reader starts");
    for(unsigned i=0;i<64;++i){
        auto value=enabled();value["lightColor"]=(i&1)?"0.2 0.4 0.8":"0.8 0.4 0.2";
        need(observe_class(object(),"lights.odf",value),"concurrent replacement");
        if(i%16==15){disable();configure(true,nullptr);dispatch_available(true);
            need(observe_class(object(),"lights.odf",enabled()),"concurrent reconfigure recovery");}
    }
    need(WaitForSingleObject(worker,15000)==WAIT_OBJECT_0,"reader joined");
    DWORD exit_code=1;need(GetExitCodeThread(worker,&exit_code)!=0&&exit_code==0&&
        reader.checked==64&&!reader.bad,"concurrent pinned reads remain valid");
    CloseHandle(worker);CloseHandle(reader.start);
    disable();need(!pin(id)&&original->source_odf=="lights.odf","final clear preserves held record");
    std::puts("PASS native11 core light class producer");
}
