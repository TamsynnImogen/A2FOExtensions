#include "../core/class_load_context.hpp"
#include "../core/native11_light_classes.hpp"
#include "../rust/stage8_core_classload_ffi.h"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace classes=a2fo::native11_light_classes;
namespace {
void need(bool condition,const char* message) {
    if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
constexpr std::uintptr_t first_id=0x12340,second_id=0x12380;
void* object(std::uintptr_t id) {return reinterpret_cast<void*>(id);}
const std::map<std::string,std::string> fields{};
void arm() {classes::configure(true,nullptr);classes::dispatch_available(true);}
struct Context {
    classes::ClassLoadScope* scope;
    Context* previous=nullptr;
    void* klass=nullptr;
    bool throw_factory=false;
    bool nested_factory=false;
    bool inner_class=false;
    bool handler_seen=false;
    bool observe_ok=false;
    std::exception_ptr error{};
};
thread_local Context* current=nullptr;
std::uint64_t step(void* raw,std::uint32_t op,std::uintptr_t) noexcept {
    auto& c=*static_cast<Context*>(raw);
    try {switch(op) {
    case 0:c.previous=current;current=&c;return 0;
    case 1:
        need(current==&c,"factory sees its own TLS context");
        need(!classes::healthy()&&!classes::pin(first_id),"factory cannot pin during class load");
        if(c.throw_factory)throw std::runtime_error("synthetic factory failure");
        if(c.nested_factory){
            classes::ClassLoadScope nested;
            Context inner{&nested};inner.inner_class=true;
            {a2fo::RestoreClassLoadContext<Context> restore(current);
                a2fo_rs_stage8_core_classload(2,step,&inner);
            }
            need(!inner.error&&inner.observe_ok&&inner.handler_seen&&current==&c,
                 "reentrant factory restored outer TLS context");
            need(nested.complete(),"reentrant factory completes");
            need(!classes::pin(first_id),"outer scope still blocks after reentrant factory");
        }
        c.klass=object(c.inner_class?first_id:second_id);return 0;
    case 2:current=c.previous;return 0;
    case 3:return c.klass!=nullptr;
    case 4:return 1; // Synthetic factory observed its copied fields.
    case 6:return 1; // Synthetic ODF name is present.
    case 8:return 1; // Synthetic name is terminated.
    case 10:return 1; // Synthetic basename is valid.
    case 13:return 0; // No diagnostic field-name list.
    case 19:
        need(current==c.previous,"class publication sees restored TLS context");
        c.observe_ok=c.scope->observe_class(c.klass,"synthetic.odf",fields);
        need(c.observe_ok,"scope accepts copied class fields");
        need(!classes::healthy()&&!classes::pin(c.inner_class?first_id:second_id),
             "observed class remains unpublished in scope");
        return 0;
    case 21:return 1; // One synthetic class-loaded handler.
    case 23:return 0; // Empty copied-field view.
    case 27:return c.handler_seen?0:1;
    case 28:return 1;
    case 29:
        need(current==c.previous,"handler sees restored TLS context");
        need(!classes::healthy()&&!classes::pin(c.inner_class?first_id:second_id),
             "handler cannot pin observed class");
        c.handler_seen=true;return 0;
    default:return 0;
    }}catch(...){c.error=std::current_exception();return UINT64_MAX;}
}
void coordinator() {
    arm();
    const auto before=classes::generation();
    {
        classes::ClassLoadScope scope;
        need(classes::generation()>before,"scope entry advances owner generation");
        const auto entered=classes::generation();
        Context c{&scope};c.nested_factory=true;
        {a2fo::RestoreClassLoadContext<Context> restore(current);
            a2fo_rs_stage8_core_classload(2,step,&c);
            need(!c.error&&c.observe_ok&&c.handler_seen,"Rust coordinator ran observation and handler");
            need(current==nullptr,"coordinator restored TLS after factory");
        }
        need(classes::generation()>entered,"class update advances owner generation");
        need(scope.complete(),"completed factory and handlers accepted");
        need(!classes::pin(second_id),"completed live scope still blocks pins");
    }
    const auto record=classes::pin(second_id);
    need(record&&classes::healthy()&&record->source_odf=="synthetic.odf"&&
         classes::generation()>before,"class published after complete scope destruction");
    need(classes::pin(first_id)!=nullptr,"reentrant class published after outer scope destruction");
    const auto version=record->version;
    {
        classes::ClassLoadScope scope;
        need(scope.observe_class(object(second_id),"updated.odf",fields),"class replacement accepted in scope");
        need(!classes::pin(second_id),"old version blocked while replacement active");
        need(scope.complete(),"replacement scope completes");
    }
    const auto replaced=classes::pin(second_id);
    need(replaced&&replaced->version>version&&replaced->source_odf=="updated.odf"&&
         record->source_odf=="synthetic.odf","replacement publishes new owner version");
}
void failure_and_abort() {
    arm();
    {
        classes::ClassLoadScope scope;
        Context c{&scope};c.throw_factory=true;
        {a2fo::RestoreClassLoadContext<Context> restore(current);
            a2fo_rs_stage8_core_classload(2,step,&c);
            need(static_cast<bool>(c.error),"native callback recorded synthetic exception");
        }
        need(current==nullptr,"RAII restores TLS when Rust skips op 2");
        scope.fail();
        need(!scope.complete(),"failed callback cannot complete");
    }
    need(!classes::healthy()&&!classes::pin(second_id),"failed callback faults source");
    arm();
    {classes::ClassLoadScope scope;
        need(scope.observe_class(object(first_id),"abort.odf",fields),"aborted scope observed class");
        need(!classes::pin(first_id),"aborted scope has no early pin");
    }
    need(!classes::healthy()&&!classes::pin(first_id),"aborted scope faults source");
    arm();need(classes::healthy(),"explicit reconfiguration clears fault");
}
void invalid_publication() {
    arm();
    need(classes::observe_class(object(first_id),"standalone.odf",fields)&&
         classes::pin(first_id)!=nullptr,"standalone publication stays compatible");
    {classes::ClassLoadScope scope;
        need(!classes::observe_class(object(first_id),"standalone.odf",fields),
             "standalone publication refuses active scope");
        need(scope.observe_class(object(first_id),"first.odf",fields),"first observation accepted");
        need(!scope.observe_class(object(first_id),"repeat.odf",fields),"repeat observation refused");
        need(!scope.complete(),"repeat observation faults scope");
    }
    need(!classes::healthy(),"repeat observation faults source");
    arm();
    {classes::ClassLoadScope scope;
        need(scope.complete(),"scope with no observation completes");
        need(!scope.complete(),"second completion refused");
        need(!scope.observe_class(object(first_id),"late.odf",fields),"post-completion observation refused");
    }
    need(!classes::healthy(),"post-completion observation faults source");
}
struct Foreign {classes::ClassLoadScope* scope;bool observed=false,completed=false;};
DWORD WINAPI foreign_thread(void* raw) {
    auto& f=*static_cast<Foreign*>(raw);
    f.observed=f.scope->observe_class(object(first_id),"foreign.odf",fields);
    f.completed=f.scope->complete();return 0;
}
void foreign_refusal() {
    arm();
    {classes::ClassLoadScope scope;
        Foreign f{&scope};HANDLE thread=CreateThread(nullptr,0,foreign_thread,&f,0,nullptr);
        need(thread!=nullptr,"foreign test thread created");
        need(WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,"foreign test thread joined");
        need(CloseHandle(thread)!=0,"foreign test thread closed");
        need(!f.observed&&!f.completed,"foreign thread cannot observe or complete scope");
        need(scope.observe_class(object(first_id),"owner.odf",fields),"owner can observe after foreign refusal");
        need(scope.complete(),"owner can complete after foreign refusal");
    }
    need(classes::pin(first_id)!=nullptr&&classes::healthy(),"foreign refusal left owner scope intact");
}
void nested_and_lifecycle() {
    arm();
    const auto old=classes::pin(first_id);
    {
        classes::ClassLoadScope outer;
        need(outer.observe_class(object(first_id),"outer.odf",fields),"outer observes class");
        {classes::ClassLoadScope inner;
            need(inner.observe_class(object(second_id),"inner.odf",fields),"nested factory observes class");
            need(inner.complete(),"nested factory completes");
        }
        need(!classes::pin(first_id)&&!classes::pin(second_id)&&!classes::healthy(),
             "outer factory keeps all generations unavailable");
        need(outer.complete(),"outer factory completes");
    }
    need(classes::pin(first_id)&&classes::pin(second_id)&&classes::healthy(),
         "nested classes publish only after outer exit");
    (void)old;
    auto stale=std::make_unique<classes::ClassLoadScope>();
    need(stale->observe_class(object(first_id),"stale.odf",fields),"stale scope observed class");
    arm();
    need(!classes::healthy()&&!classes::pin(first_id)&&
         !classes::observe_class(object(first_id),"blocked.odf",fields),
         "reconfigure cannot expose pins or standalone publication during old factory");
    auto fresh=std::make_unique<classes::ClassLoadScope>();
    need(fresh->observe_class(object(second_id),"fresh.odf",fields),"fresh scope observed class");
    stale.reset();
    need(!classes::pin(second_id),"stale destructor cannot release fresh scope");
    need(fresh->complete(),"fresh scope remains completable");fresh.reset();
    need(classes::pin(second_id)&&classes::healthy(),"fresh scope published after stale destructor");
    auto old_reset=std::make_unique<classes::ClassLoadScope>();
    need(old_reset->observe_class(object(first_id),"old-reset.odf",fields),
         "old factory observes before repeated resets");
    arm();classes::disable();arm();
    classes::dispatch_available(false);classes::dispatch_available(true);
    need(!classes::healthy()&&!classes::pin(first_id)&&
         !classes::observe_class(object(first_id),"blocked-again.odf",fields),
         "multiple resets preserve old in-flight barrier");
    {
        classes::ClassLoadScope new_after_reset;
        need(new_after_reset.observe_class(object(second_id),"new-after-reset.odf",fields),
             "new factory can observe while old invocation drains");
        need(new_after_reset.complete(),"new factory completes before old invocation");
    }
    need(!classes::healthy()&&!classes::pin(second_id)&&
         !classes::observe_class(object(first_id),"still-blocked.odf",fields),
         "new factory exit cannot publish while old native factory remains");
    need(!old_reset->complete(),"reset invalidated old ticket");
    old_reset.reset();
    need(classes::healthy()&&classes::pin(second_id)!=nullptr,
         "new class publishes when final old native factory exits");
    auto disabled=std::make_unique<classes::ClassLoadScope>();
    need(disabled->observe_class(object(first_id),"disabled.odf",fields),"pre-disable scope observed class");
    classes::dispatch_available(false);classes::dispatch_available(true);
    need(!disabled->complete()&&!classes::pin(first_id)&&!classes::pin(second_id),
         "dispatcher loss discards records and active token");
    need(!classes::healthy()&&!classes::observe_class(object(first_id),"dispatch-blocked.odf",fields),
         "dispatcher rearm waits for old native factory");
    disabled.reset();
    need(classes::healthy(),"stale destructor after dispatcher loss cannot fault rearmed source");
    auto reset=std::make_unique<classes::ClassLoadScope>();
    classes::disable();arm();
    need(!classes::healthy()&&!classes::observe_class(object(first_id),"disable-blocked.odf",fields),
         "disable and reconfigure wait for old native factory");
    need(!reset->complete(),"disable invalidates live scope");reset.reset();
    need(classes::healthy(),"disable and reconfigure leave clean source");
    classes::configure(false,nullptr);
    {classes::ClassLoadScope inert;
        arm();need(!inert.observe_class(object(first_id),"inert.odf",fields)&&!inert.complete(),
                   "scope created without source cannot publish after reconfigure");
        need(!classes::healthy()&&!classes::pin(first_id)&&
             !classes::observe_class(object(first_id),"inert-blocked.odf",fields),
             "inert native factory still blocks rearmed source");
    }
    need(classes::healthy(),"inert native factory exit releases rearmed source");
}
}
int main() {
    coordinator();failure_and_abort();invalid_publication();foreign_refusal();nested_and_lifecycle();
    classes::disable();std::puts("PASS native11 class load scope");
}
