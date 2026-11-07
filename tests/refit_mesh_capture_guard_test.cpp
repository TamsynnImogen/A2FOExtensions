#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace fixture {
bool exact=true;unsigned patches=0,fail_patch=0,notifications=0;
void* mesh=nullptr;void* material=nullptr;std::uint32_t pass=0,route=0;
struct ScopeEvent {bool begin=false,qualified=false;void* mesh=nullptr,*material=nullptr;
                   std::uint32_t pass=0,route=0;};
ScopeEvent scope_events[16]{};
unsigned scope_count=0,scope_count_at_observe[2]{};
}
namespace a2fo::native11_capture {
bool exact_engine_image(HMODULE) noexcept {return fixture::exact;}
}
extern "C" void a2fo_refit_mesh_fast_tap() {}
extern "C" void a2fo_refit_mesh_legacy_tap() {}
extern "C" bool a2fo_rs_windows_patch_call(void* address,void* target,
    const std::uint8_t* expected,std::size_t size) {
    ++fixture::patches;
    if(fixture::patches==fixture::fail_patch||size!=5||std::memcmp(address,expected,5))return false;
    auto* bytes=static_cast<std::uint8_t*>(address);
    bytes[0]=0xe8;
    const auto relative=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(target)-
        reinterpret_cast<std::uintptr_t>(address)-5);
    std::memcpy(bytes+1,&relative,4);return true;
}
#include "../core/refit_mesh_capture.cpp"

namespace {
void need(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void capture_event(void* mesh,std::uint32_t pass,void* material,std::uint32_t route) {
    ++fixture::notifications;fixture::mesh=mesh;fixture::pass=pass;fixture::material=material;fixture::route=route;
    fixture::scope_count_at_observe[route]=fixture::scope_count;
}
void other(void*,std::uint32_t,void*,std::uint32_t) {std::abort();}
void scope_event(bool begin,void* mesh,std::uint32_t pass,void* material,
                 std::uint32_t route,bool qualified) {
    if(fixture::scope_count>=16)std::abort();
    fixture::scope_events[fixture::scope_count++]={begin,qualified,mesh,material,pass,route};
}
void other_scope(bool,void*,std::uint32_t,void*,std::uint32_t,bool) {std::abort();}
void silent(const char*) {}
void reset(unsigned char* base) {
    image=nullptr;owner=0;installed[0]=installed[1]=active=false;::observe=nullptr;notify_scope=nullptr;
    a2fo_refit_mesh_fast_original=a2fo_refit_mesh_legacy_original=0;
    fixture::patches=fixture::fail_patch=fixture::notifications=fixture::scope_count=0;
    fixture::scope_count_at_observe[0]=fixture::scope_count_at_observe[1]=0;
    fixture::exact=true;
    std::memcpy(base+0x2327cf,a2fo::refit_scene::profile::mesh_render_dispatch_bytes.data(),
        a2fo::refit_scene::profile::mesh_render_dispatch_bytes.size());
    const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base))-0x400000u;
    for(const auto& r:a2fo::refit_scene::profile::mesh_body_regions){
        std::memcpy(base+r.rva,r.bytes,r.size);
        for(std::size_t i=0;i<r.relocation_count;++i){std::uint32_t value=0;
            std::memcpy(&value,base+r.rva+r.relocations[i],4);value+=delta;
            std::memcpy(base+r.rva+r.relocations[i],&value,4);
        }
    }
    for(const auto& r:a2fo::refit_scene::profile::mesh_draw_regions){
        std::memcpy(base+r.rva,r.bytes,r.size);
        for(std::size_t i=0;i<r.relocation_count;++i){std::uint32_t value=0;
            std::memcpy(&value,base+r.rva+r.relocations[i],4);value+=delta;
            std::memcpy(base+r.rva+r.relocations[i],&value,4);
        }
    }
}
DWORD WINAPI nonowner(void*) {
    a2fo_refit_observe_mesh(reinterpret_cast<void*>(0x1010),2,reinterpret_cast<void*>(0x2020),0);
    a2fo_refit_mesh_finished(0);return 0;
}
}
int main() {
    static_assert(sizeof(void*)==4);
    // The checked workspace vtable region ends beyond RVA 0x2bcd8c.
    auto* base=static_cast<unsigned char*>(VirtualAlloc(nullptr,0x2c0000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    need(base!=nullptr,"synthetic read-only-code fixture allocated");const auto module=reinterpret_cast<HMODULE>(base);
    reset(base);
    need(!a2fo::refit_mesh_capture::initialize(nullptr,&capture_event,silent),"null image refused");
    need(!a2fo::refit_mesh_capture::initialize(module,nullptr,silent),"null observer refused");
    fixture::exact=false;
    need(!a2fo::refit_mesh_capture::initialize(module,&capture_event,silent)&&fixture::patches==0,"file admission refused before writes");
    reset(base);base[0x2327cf]^=1;
    need(!a2fo::refit_mesh_capture::initialize(module,&capture_event,silent)&&fixture::patches==0,"dispatch mutation refused before writes");
    reset(base);base[sites[1]]^=1;
    need(!a2fo::refit_mesh_capture::initialize(module,&capture_event,silent)&&fixture::patches==0,"second call mutation refused before writes");
    reset(base);base[0x231f10]^=1;
    need(!a2fo::refit_mesh_capture::initialize(module,&capture_event,silent)&&fixture::patches==0,"modified native helper body refused before wrapping");
    for(unsigned failed=1;failed<=2;++failed){
        reset(base);fixture::fail_patch=failed;
        need(!a2fo::refit_mesh_capture::initialize(module,&capture_event,silent,&scope_event),"partial install refused");
        need(!a2fo::refit_mesh_capture::owns_calls(),"partial install inactive");
        need(a2fo_refit_mesh_fast_original==reinterpret_cast<std::uintptr_t>(base)+0x231f10&&
             a2fo_refit_mesh_legacy_original==reinterpret_cast<std::uintptr_t>(base)+0x231fd0,"both original targets published first");
        a2fo_refit_observe_mesh(reinterpret_cast<void*>(0x1010),2,reinterpret_cast<void*>(0x2020),0);
        a2fo_refit_mesh_finished(0);
        need(!fixture::notifications&&!fixture::scope_count,"partial install never notifies");
        need(!a2fo::refit_mesh_capture::initialize(module,&other,silent),"partial install cannot rearm/rebind");
    }
    reset(base);need(a2fo::refit_mesh_capture::initialize(module,&capture_event,silent,&scope_event),"qualified calls admitted");
    need(fixture::patches==2&&a2fo::refit_mesh_capture::owns_calls(),"both owned calls active");
    need(a2fo::refit_mesh_capture::draw_source_current(),"complete native draw source admitted");
    base[0x247b80]^=1;
    need(a2fo::refit_mesh_capture::owns_calls()&&!a2fo::refit_mesh_capture::draw_source_current(),"workspace mutation blocks draw source");
    base[0x247b80]^=1;base[0x231f10]^=1;
    need(!a2fo::refit_mesh_capture::draw_source_current(),"native body mutation blocks draw source");
    base[0x231f10]^=1;
    const auto& draw_regions=a2fo::refit_scene::profile::mesh_draw_regions;
    static_assert(sizeof(draw_regions)/sizeof(draw_regions[0])>=17);
    for(const auto& region:draw_regions){
        char reason[96]{};
        std::snprintf(reason,sizeof(reason),"draw source region RVA %08x mutation/refusal",
                      static_cast<unsigned>(region.rva));
        need(region.size>0 && region.rva<0x2c0000 &&
             region.size<=0x2c0000-region.rva,reason);
        base[region.rva]^=1;
        need(a2fo::refit_mesh_capture::owns_calls()&&
             !a2fo::refit_mesh_capture::draw_source_current(),reason);
        base[region.rva]^=1;
        std::snprintf(reason,sizeof(reason),"draw source region RVA %08x restoration",
                      static_cast<unsigned>(region.rva));
        need(a2fo::refit_mesh_capture::draw_source_current(),reason);
    }
    for(unsigned route=0;route<2;++route){
        a2fo_refit_observe_mesh(reinterpret_cast<void*>(0x1010),0x12345678,reinterpret_cast<void*>(0x2020),route);
        a2fo_refit_mesh_finished(route);
        need(fixture::notifications==route+1&&fixture::mesh==reinterpret_cast<void*>(0x1010)&&
             fixture::pass==0x12345678&&fixture::material==reinterpret_cast<void*>(0x2020)&&fixture::route==route,
             "exact observer arguments and route");
        const auto& begin=fixture::scope_events[route*2];
        const auto& end=fixture::scope_events[route*2+1];
        need(fixture::scope_count==2*(route+1)&&begin.begin&&begin.qualified&&
             begin.mesh==reinterpret_cast<void*>(0x1010)&&begin.pass==0x12345678&&
             begin.material==reinterpret_cast<void*>(0x2020)&&begin.route==route&&
             !end.begin&&!end.qualified&&!end.mesh&&!end.material&&!end.pass&&end.route==route&&
             fixture::scope_count_at_observe[route]==route*2+1,
             "qualified scope begins before observation and ends once on each route");
    }
    a2fo_refit_observe_mesh(nullptr,0,nullptr,2);a2fo_refit_mesh_finished(2);
    need(fixture::notifications==2&&fixture::scope_count==4,"unknown route refused");
    HANDLE thread=CreateThread(nullptr,0,nonowner,nullptr,0,nullptr);
    need(thread&&WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"non-owner joined");CloseHandle(thread);
    need(fixture::notifications==2&&fixture::scope_count==4,"non-owner never notifies");
    need(!a2fo::refit_mesh_capture::initialize(module,&other,silent,&other_scope),
         "stable observer and scope callback cannot be replaced");
    a2fo_refit_observe_mesh(reinterpret_cast<void*>(0x3030),7,reinterpret_cast<void*>(0x4040),0);
    a2fo_refit_mesh_finished(0);
    need(fixture::notifications==3&&fixture::scope_count==6&&
         fixture::scope_events[4].begin&&fixture::scope_events[4].qualified&&
         !fixture::scope_events[5].begin,
         "original observer and scope remain installed after rebind refusal");
    base[0x2327cf]^=1;need(!a2fo::refit_mesh_capture::owns_calls(),"context ownership loss detected");
    a2fo_refit_observe_mesh(reinterpret_cast<void*>(0x5050),9,reinterpret_cast<void*>(0x6060),0);
    a2fo_refit_mesh_finished(0);
    need(fixture::notifications==3&&fixture::scope_count==8&&
         fixture::scope_events[6].begin&&!fixture::scope_events[6].qualified&&
         fixture::scope_events[6].mesh==reinterpret_cast<void*>(0x5050)&&
         fixture::scope_events[6].pass==9&&
         fixture::scope_events[6].material==reinterpret_cast<void*>(0x6060)&&
         !fixture::scope_events[7].begin&&!fixture::scope_events[7].qualified,
         "lost dispatch ownership opens unsupported scope and always closes it");
    base[0x2327cf]^=1;std::memcpy(base+sites[0],native_call(0).data(),5);
    need(!a2fo::refit_mesh_capture::owns_calls(),"call ownership loss detected");
    a2fo_refit_observe_mesh(nullptr,0,nullptr,1);a2fo_refit_mesh_finished(1);
    need(fixture::notifications==3&&fixture::scope_count==10&&
         fixture::scope_events[8].begin&&!fixture::scope_events[8].qualified&&
         !fixture::scope_events[9].begin&&fixture::scope_events[9].route==1,
         "changed call opens unsupported scope without ordinary observation");
    reset(base);VirtualFree(base,0,MEM_RELEASE);
    std::puts("PASS pre-batch mesh profile, partial-install, observer ownership and argument guards; no native function executed");
}
