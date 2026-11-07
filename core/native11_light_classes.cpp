#include "native11_light_classes.hpp"
#include "../renderer/native11/class_update_barrier.hpp"
#include <windows.h>
#include <cstring>
namespace a2fo::native11_light_classes {
namespace {
SRWLOCK lock=SRWLOCK_INIT;
native11::LightClassStore store;
native11::ClassUpdateBarrier updates;
bool wanted=false,configured=false,dispatcher=false;
std::uint64_t epoch=0;
// Native invocations outlive source resets. Tickets may be discarded by a
// lifecycle change, but the work itself must drain before pins are admitted.
std::uint64_t in_flight=0;
bool in_flight_exhausted=false;
Log logger=nullptr;
struct Held{bool active=true;Held(){AcquireSRWLockExclusive(&lock);}void release(){if(active){active=false;ReleaseSRWLockExclusive(&lock);}}~Held(){release();}};
void report(const char* text) noexcept {try{if(logger)logger(text);}catch(...){}}
void advance_epoch() noexcept {
    if(epoch!=UINT64_MAX)++epoch;
    if(epoch==UINT64_MAX)configured=false;
}
bool update_locked(void* object_class,const std::string& odf,
    const std::map<std::string,std::string>& values,std::string& error) noexcept {
    try {
        std::vector<A2FO_OdfFieldView> fields;fields.reserve(10);
        for(const auto& value:values){
            bool known=false;
            for(const char* name:native11::requested_light_fields())if(!_stricmp(value.first.c_str(),name)){known=true;break;}
            if(known)fields.push_back({{value.first.data(),static_cast<std::uint32_t>(value.first.size())},
                {value.second.data(),static_cast<std::uint32_t>(value.second.size())}});
        }
        A2FO_GameObjectClassLoadedEvent event{sizeof(event),object_class,
            {odf.data(),static_cast<std::uint32_t>(odf.size())},fields.data(),static_cast<std::uint32_t>(fields.size())};
        const bool good=store.update(event,error);
        advance_epoch();return good&&configured;
    }catch(...){configured=false;advance_epoch();return false;}
}
}
ClassLoadScope::ClassLoadScope() noexcept:owner_(GetCurrentThreadId()) {
    Held held;
    if(in_flight==UINT64_MAX)in_flight_exhausted=true;
    else {++in_flight;counted_=true;}
    advance_epoch();
    if(!wanted||!configured||!dispatcher)return;
    ticket_=updates.begin();
}
ClassLoadScope::~ClassLoadScope() {
    Held held;
    if(ticket_)updates.finish(ticket_,completed_&&!failed_&&owner_==GetCurrentThreadId());
    if(counted_){--in_flight;advance_epoch();}
}
bool ClassLoadScope::observe_class(void* object_class,const std::string& odf,
    const std::map<std::string,std::string>& values) noexcept {
    std::string error;
    Held held;
    if(owner_!=GetCurrentThreadId()||!ticket_||!updates.active(ticket_)||
       updates.failed()||!wanted||!configured||!dispatcher)return false;
    if(observed_||completed_){failed_=true;return false;}
    observed_=true;
    const bool good=update_locked(object_class,odf,values,error);
    if(!good)failed_=true;
    held.release();
    if(!good){
        report("Native11 class completion refused: copied class fields unavailable");
        if(!error.empty())report(error.c_str());
    }
    return good;
}
bool ClassLoadScope::complete() noexcept {
    Held held;
    if(owner_!=GetCurrentThreadId()||!ticket_||failed_||completed_||updates.failed()||!updates.active(ticket_))return false;
    completed_=true;return true;
}
void ClassLoadScope::fail() noexcept {
    Held held;if(owner_==GetCurrentThreadId()&&ticket_&&updates.active(ticket_))failed_=true;
}
void configure(bool request,Log log) noexcept {
    Held held;logger=log;store.clear();updates.clear();wanted=request;configured=request&&epoch!=UINT64_MAX;
    dispatcher=false;advance_epoch();
}
void configure_from_roots(const std::vector<std::string>& roots,Log log) noexcept {
    try {
        bool request=false,valid=true;
        for(const auto& root:roots){char value[16]{};
            const auto file=root+"\\A2FORenderer.ini";
            GetPrivateProfileStringA("Native11","CaptureWorldLights",request?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){valid=false;request=true;break;}
            request=std::strcmp(value,"1")==0;
        }
        configure(request,log);
        if(!valid){{Held held;configured=false;}report("Native11 class lights refused: CaptureWorldLights must be 0 or 1");}
    }catch(...){configure(true,log);Held held;configured=false;}
}
void dispatch_available(bool ready) noexcept {
    bool missing=false;
    {Held held;if(dispatcher!=ready){
        if(!ready){store.clear();updates.clear();}
        dispatcher=ready;advance_epoch();
    }missing=!ready&&wanted;}
    if(missing)report("Native11 class lights unavailable: copied ODF dispatcher not armed");
}
bool requested() noexcept {Held held;return wanted;}
bool healthy() noexcept {Held held;return wanted&&configured&&dispatcher&&!in_flight&&!in_flight_exhausted&&store.healthy()&&updates.quiescent();}
std::uint64_t generation() noexcept {Held held;return epoch;}
bool observe_class(void* object_class,const std::string& odf,const std::map<std::string,std::string>& values) noexcept {
    std::string error;
    Held held;if(!wanted||!configured||!dispatcher||in_flight||in_flight_exhausted||!updates.quiescent())return false;
    const bool good=update_locked(object_class,odf,values,error);held.release();
    if(!good){
        report("Native11 class light configuration refused; complete world lighting unavailable until recovery");
        if(!error.empty())report(error.c_str());
    }
    return good;
}
std::shared_ptr<const native11::LightClassRecord> pin(std::uint64_t id) noexcept {
    Held held;return wanted&&configured&&dispatcher&&!in_flight&&!in_flight_exhausted&&store.healthy()&&updates.quiescent()?store.find(id):nullptr;
}
void disable() noexcept {Held held;configured=false;dispatcher=false;store.clear();updates.clear();advance_epoch();}
}
