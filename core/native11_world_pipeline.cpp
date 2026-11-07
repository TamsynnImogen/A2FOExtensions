#include "native11_world_pipeline.hpp"
#include "native11_light_classes.hpp"
#include "native11_preview_export.hpp"
#include "../renderer/native11/readiness_report.hpp"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstring>
#include <map>
#include <limits>
#include <utility>
#if defined(A2FO_NATIVE11_PIPELINE_TESTING)
extern "C" void a2fo_native11_world_pipeline_after_submit() noexcept;
extern "C" void a2fo_native11_world_pipeline_after_diagnostic_copy() noexcept;
#endif
namespace a2fo::native11_world_pipeline {
namespace {
volatile LONG busy=0;
volatile LONG sources_epoch=1;
std::atomic<bool> wanted{false};
bool configured=false;
bool share_world_geometry=false;
bool live_materials=false,live_preview=false,world_wanted=false;
MaterialValidator material_validator=nullptr;
std::shared_ptr<const native11::BoundMaterialCapture> material_packet;
std::uint32_t material_ticket=0;
bool material_preview_eligible=false;
std::vector<std::string> roots;
float configured_ambient[3]{.04f,.04f,.04f};
Log logger=nullptr;HMODULE helper=nullptr;
DWORD owner=0;
std::uint32_t target_width=0,target_height=0;
bool image_eligible=false;
std::uint32_t published_ticket=0;
std::uint64_t published_class_generation=0;
GeometryProvider provider=nullptr;void* provider_context=nullptr;
CandidateProvider candidate_provider=nullptr;void* candidate_context=nullptr;
native11::ObservedWorldAssembler candidate_assembler;
std::uint64_t candidate_epoch=0,candidate_class_generation=0;
std::uint32_t candidate_ticket=0;
native11::WorldRenderSession session;
SRWLOCK status_lock=SRWLOCK_INIT;
Status current_status;
MaterialStatus current_material_status;
void record_material(MaterialState state,std::uint64_t frame,std::uint64_t epoch,
    std::uint32_t ticket,const std::string& reason) noexcept {
    AcquireSRWLockExclusive(&status_lock);
    current_material_status.state=state;current_material_status.frame=frame;
    current_material_status.source_epoch=epoch;current_material_status.source_ticket=ticket;
    if(state==MaterialState::prepared||state==MaterialState::previewed)++current_material_status.submitted_frames;
    try{current_material_status.reason=reason;}catch(...){current_material_status.reason.clear();}
    ReleaseSRWLockExclusive(&status_lock);
}
std::string current_diagnostic;
std::uint32_t diagnostic_ticket=0;
std::uint64_t diagnostic_class_generation=0;
struct Admission {bool entered;Admission():entered(InterlockedCompareExchange(&busy,1,0)==0){}~Admission(){if(entered)InterlockedExchange(&busy,0);}};
void record(State state,std::uint64_t frame,const std::string& reason) noexcept {
    try {AcquireSRWLockExclusive(&status_lock);
        current_status.state=state;current_status.frame=frame;current_status.reason=reason;
        if(state==State::rendered){++current_status.rendered_frames;current_status.published_frame=frame;}
        ReleaseSRWLockExclusive(&status_lock);
    }catch(...){ReleaseSRWLockExclusive(&status_lock);}
}
void clear_diagnostic() noexcept {
    AcquireSRWLockExclusive(&status_lock);
    current_diagnostic.clear();diagnostic_ticket=0;diagnostic_class_generation=0;
    ReleaseSRWLockExclusive(&status_lock);
}
void publish_diagnostic(std::uint32_t ticket,std::uint64_t class_generation,
    std::string&& json) noexcept {
    if(!native11_light_classes::healthy()||
       class_generation!=native11_light_classes::generation())return;
    AcquireSRWLockExclusive(&status_lock);
    if(ticket&&ticket==source_epoch()){
        current_diagnostic=std::move(json);diagnostic_ticket=ticket;
        diagnostic_class_generation=class_generation;
    }
    ReleaseSRWLockExclusive(&status_lock);
    if(!native11_light_classes::healthy()||
       class_generation!=native11_light_classes::generation())clear_diagnostic();
}
bool matching_visible(const native11::BoundMaterialCapture& visible,const native11::BoundMaterialCapture& all) {
    const auto& input=visible.frame();const auto& frame=all.frame();
    if(input.camera!=frame.camera||input.generation!=frame.generation||!frame.requires_part_states||
       std::memcmp(input.view.world_to_clip,frame.view.world_to_clip,sizeof(input.view.world_to_clip))||
       std::memcmp(input.view.viewport,frame.view.viewport,sizeof(input.view.viewport)))return false;
    std::map<std::uint32_t,bool> remaining;
    for(std::size_t i=0;i<input.object_count;++i){const auto& object=input.objects[i];
        if(object.shadow_only||!remaining.emplace(object.instance,true).second)return false;}
    for(std::size_t i=0;i<frame.object_count;++i){const auto& object=frame.objects[i];
        if(object.shadow_only)continue;
        const auto found=remaining.find(object.instance);
        if(found==remaining.end()||!found->second||!visible.same_instance_contents(object.instance,all))return false;
        found->second=false;
    }
    return std::none_of(remaining.begin(),remaining.end(),[](const auto& value){return value.second;});
}
bool load(std::string& error) {
    if(helper)return true;
    for(auto it=roots.rbegin();it!=roots.rend();++it){const auto file=*it+"\\A2FONative11Renderer.dll";
        const auto attributes=GetFileAttributesA(file.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))continue;
        helper=LoadLibraryExA(file.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        if(!helper){error="native11 renderer helper load failed";return false;}
        return true;
    }
    error="native11 API5 renderer helper not found in extension roots";return false;
}
bool dimensions(const native11::Frame& frame,std::uint32_t& width,std::uint32_t& height) noexcept {
    const auto& v=frame.view.viewport;const double x=double(v[0])+v[2],y=double(v[1])+v[3];
    if(!std::isfinite(x)||!std::isfinite(y)||x<1||y<1||x>4096||y>4096||
       x!=std::floor(x)||y!=std::floor(y))return false;
    width=static_cast<std::uint32_t>(x);height=static_cast<std::uint32_t>(y);return true;
}
bool ensure_material_session(const native11::Frame& frame,std::string& error){
    std::uint32_t width=0,height=0;
    if(!dimensions(frame,width,height)){error="live material viewport exceeds bounds";return false;}
    if(!helper){
        if(!load(error))return false;
        auto symbol=GetProcAddress(helper,"A2FO_Native11_GetInterface");A2FO_N11_GetInterface get=nullptr;
        static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
        A2FO_N11_API11 api{};
        if(!get||get(A2FO_N11_API_VERSION_LIVE_MATERIALS,sizeof(api),&api)!=0||
           !session.initialize_live_materials(api,width,height,error,share_world_geometry)){
            std::string ignored;if(session.close(ignored)){FreeLibrary(helper);helper=nullptr;}
            if(error.empty())error="API11 live material service unavailable";
            return false;
        }
        owner=GetCurrentThreadId();target_width=width;target_height=height;
    }else if(width!=target_width||height!=target_height){
        image_eligible=false;material_preview_eligible=false;
        if(!session.resize(width,height,error))return false;
        target_width=width;target_height=height;
    }
    return true;
}
struct MaterialCheckpoint {std::uint32_t ticket;std::shared_ptr<const native11::BoundMaterialCapture> packet;};
bool material_checkpoint(void* context) noexcept {
    const auto& held=*static_cast<const MaterialCheckpoint*>(context);
    return held.ticket&&held.ticket==source_epoch()&&material_validator&&material_validator(held.packet);
}
void submit_materials(const std::shared_ptr<const native11::BoundMaterialCapture>& visible,
    std::uint32_t ticket){
    material_preview_eligible=false;material_packet.reset();material_ticket=0;
    const auto generation=visible?visible->frame().generation:0;
    if(!visible||!visible->frame().object_count||!material_validator||!material_validator(visible)){
        record_material(MaterialState::waiting_sources,generation,0,ticket,"waiting for a current sealed observed material frame");return;
    }
    std::string error;MaterialCheckpoint checkpoint{ticket,visible};
    if(!material_checkpoint(&checkpoint)||!ensure_material_session(visible->frame(),error)||
       !material_checkpoint(&checkpoint)){
        record_material(MaterialState::unavailable,generation,visible->source_epoch(),ticket,
            error.empty()?"live material source revoked before submission":error);return;
    }
    const bool preview=live_preview&&visible->frame().requires_part_states;
    // Both entry points share the target. A preview attempt must revoke the
    // previous complete-world image before any renderer call can replace it.
    if(preview)image_eligible=false;
    if(!session.submit_live_materials(visible,preview,configured_ambient,material_checkpoint,&checkpoint,error)||
       !material_checkpoint(&checkpoint)){
        record_material(MaterialState::unavailable,generation,visible->source_epoch(),ticket,error);return;
    }
    material_packet=visible;material_ticket=ticket;material_preview_eligible=preview;
    record_material(preview?MaterialState::previewed:MaterialState::prepared,generation,
        visible->source_epoch(),ticket,"observed authored-material/selected-diffuse packet submitted to D3D11; no world/presentation authority");
    if(logger&&(generation<=8||generation%300==0))try{
        logger(preview?"Native11 live materials submitted and rendered to private preview; no world completeness or presentation takeover":
            "Native11 live materials submitted to D3D11; world geometry/casters remain separately gated");
    }catch(...){}
}
}
bool configure_from_roots(const std::vector<std::string>& search,Log log) noexcept {
    Admission guard;if(!guard.entered||helper)return false;
    try {
        invalidate_sources();
        if(!native11_preview_export::stop())return false;
        wanted=false;world_wanted=false;live_materials=false;live_preview=false;
        configured=false;share_world_geometry=false;roots=search;logger=log;owner=0;
        material_packet.reset();material_ticket=0;material_preview_eligible=false;
        image_eligible=false;
        std::string colour=".04 .04 .04";
        bool export_preview=false;std::uint32_t export_interval=1000;
        for(const auto& root:roots){char value[128]{};const auto file=root+"\\A2FORenderer.ini";
            const auto length=GetPrivateProfileStringA("Native11","OffscreenWorld",wanted?"1":"0",value,sizeof(value),file.c_str());
            if(length>=sizeof(value)-1||(std::strcmp(value,"0")&&std::strcmp(value,"1"))){wanted=true;record(State::unavailable,0,"OffscreenWorld must be 0 or 1");return false;}
            wanted=std::strcmp(value,"1")==0;
            const auto live=GetPrivateProfileStringA("Native11","LiveMaterials",live_materials?"1":"0",value,sizeof(value),file.c_str());
            if(live>=sizeof(value)-1||(std::strcmp(value,"0")&&std::strcmp(value,"1"))){wanted=true;record(State::unavailable,0,"LiveMaterials must be 0 or 1");return false;}
            live_materials=std::strcmp(value,"1")==0;
            const auto preview=GetPrivateProfileStringA("Native11","LiveMaterialPreview",live_preview?"1":"0",value,sizeof(value),file.c_str());
            if(preview>=sizeof(value)-1||(std::strcmp(value,"0")&&std::strcmp(value,"1"))){wanted=true;record(State::unavailable,0,"LiveMaterialPreview must be 0 or 1");return false;}
            live_preview=std::strcmp(value,"1")==0;
            const auto export_count=GetPrivateProfileStringA("Native11","LivePreviewExport",export_preview?"1":"0",value,sizeof(value),file.c_str());
            if(export_count>=sizeof(value)-1||(std::strcmp(value,"0")&&std::strcmp(value,"1"))){wanted=true;record(State::unavailable,0,"LivePreviewExport must be 0 or 1");return false;}
            export_preview=std::strcmp(value,"1")==0;
            const auto interval_default=std::to_string(export_interval);
            const auto interval_count=GetPrivateProfileStringA("Native11","LivePreviewIntervalMs",interval_default.c_str(),value,sizeof(value),file.c_str());
            std::uint32_t parsed_interval=0;
            if(!interval_count||interval_count>5){wanted=true;record(State::unavailable,0,"invalid LivePreviewIntervalMs");return false;}
            for(std::uint32_t i=0;i<interval_count;++i){
                if(value[i]<'0'||value[i]>'9'){wanted=true;record(State::unavailable,0,"invalid LivePreviewIntervalMs");return false;}
                parsed_interval=parsed_interval*10+static_cast<unsigned>(value[i]-'0');
            }
            if(parsed_interval<250||parsed_interval>10000){wanted=true;record(State::unavailable,0,"LivePreviewIntervalMs must be 250..10000");return false;}
            export_interval=parsed_interval;
            const auto sharing=GetPrivateProfileStringA("Native11","ShareWorldGeometry",share_world_geometry?"1":"0",value,sizeof(value),file.c_str());
            if(sharing>=sizeof(value)-1||(std::strcmp(value,"0")&&std::strcmp(value,"1"))){
                record(State::unavailable,0,"ShareWorldGeometry must be 0 or 1");return false;}
            share_world_geometry=std::strcmp(value,"1")==0;
            const auto count=GetPrivateProfileStringA("Native11","WorldAmbient",colour.c_str(),value,sizeof(value),file.c_str());
            if(count>=sizeof(value)-1){record(State::unavailable,0,"WorldAmbient exceeds input bound");return false;}colour=value;
        }
        native11::LightConfig parsed;std::string error;
        if(!native11::parse_light_config({{"lightSource","1"},{"lightType","point"},{"lightColor",colour}},parsed,error)){
            record(State::unavailable,0,"invalid configured WorldAmbient");return false;
        }
        std::copy(parsed.light.colour,parsed.light.colour+3,configured_ambient);
        if(export_preview&&(!live_materials||!live_preview)){
            wanted=true;record(State::unavailable,0,"LivePreviewExport requires LiveMaterials and LiveMaterialPreview");return false;
        }
        if(!native11_preview_export::configure(export_preview,export_interval,logger)){
            wanted=true;record(State::unavailable,0,"preview export setup unavailable; inspect Core log");return false;
        }
        configured=true;
        world_wanted=wanted;wanted=world_wanted||live_materials;
        record_material(live_materials?MaterialState::waiting_sources:MaterialState::disabled,0,0,source_epoch(),
            live_materials?"waiting for sealed observed materials":"live material submission disabled");
        record(wanted?State::waiting_sources:State::disabled,0,wanted?"waiting for checked scene/light/geometry producers":"offscreen world disabled");
        return true;
    }catch(...){wanted=true;record(State::unavailable,0,"world pipeline configuration failure");return false;}
}
bool requested() noexcept {return wanted;}
bool set_material_validator(MaterialValidator validator) noexcept {
    Admission guard;if(!guard.entered)return false;
    if(material_validator!=validator){material_validator=validator;invalidate_sources();}
    return true;
}
void ambient(float out[3]) noexcept {if(out)std::copy(configured_ambient,configured_ambient+3,out);}
bool set_geometry_provider(GeometryProvider value,void* context) noexcept {
    Admission guard;if(!guard.entered||(!value&&context))return false;
    invalidate_sources();candidate_assembler.clear();candidate_provider=nullptr;candidate_context=nullptr;
    provider=value;provider_context=context;return true;
}
bool set_candidate_provider(CandidateProvider value,void* context) noexcept {
    Admission guard;if(!guard.entered||(!value&&context))return false;
    invalidate_sources();candidate_assembler.clear();provider=nullptr;provider_context=nullptr;
    candidate_provider=value;candidate_context=context;return true;
}
std::uint32_t source_epoch() noexcept {return static_cast<std::uint32_t>(InterlockedCompareExchange(&sources_epoch,0,0));}
void invalidate_sources() noexcept {
    for(;;){const auto previous=InterlockedCompareExchange(&sources_epoch,0,0);if(!previous)break;
        const LONG next=previous==LONG_MAX?0:previous+1;
        if(InterlockedCompareExchange(&sources_epoch,next,previous)==previous)break;
    }
    clear_diagnostic();
    if(wanted)record(State::waiting_sources,0,"native world source lifecycle invalidated");
}
bool on_frame(std::shared_ptr<const native11::BoundMaterialCapture> visible,
    std::shared_ptr<const native11_world_lights::Snapshot> lights,std::uint32_t source_ticket) noexcept {
    Admission guard;if(!guard.entered)return false;
    clear_diagnostic();
    // Revocation can occur concurrently; owned CPU purge belongs to the next
    // serialized admission (or setter/shutdown), never the invalidating hook.
    if(candidate_ticket!=source_epoch()||!native11_light_classes::healthy()||
       candidate_class_generation!=native11_light_classes::generation()){
        candidate_assembler.clear();candidate_ticket=0;candidate_class_generation=0;
    }
    if(!wanted)return false;
    const auto generation=visible?visible->frame().generation:0;
    try {
        if(!configured){record(State::unavailable,generation,"world pipeline configuration unavailable");return false;}
        const auto ticket=source_epoch();
        if(!ticket||(source_ticket&&source_ticket!=ticket)){record(State::waiting_sources,generation,"world source ticket expired");return false;}
        if(owner&&owner!=GetCurrentThreadId()){record(State::unavailable,generation,"world pipeline foreign render thread");return false;}
        if(live_materials){
            submit_materials(visible,ticket);
            const auto submitted=material_status();
            if(submitted.state!=MaterialState::prepared&&submitted.state!=MaterialState::previewed){
                record(State::waiting_sources,generation,"live material submission unavailable: "+submitted.reason);return false;
            }
        }
        if(!world_wanted){record(State::disabled,generation,"world rendering disabled; material submission is independent");return false;}
        if(!visible||!lights||!native11_light_classes::healthy()||
           lights->class_generation!=native11_light_classes::generation()||lights->camera!=visible->frame().camera||
           lights->frame_generation!=generation||!visible->frame().requires_part_states){
            record(State::waiting_sources,generation,"scene, observed poses or current complete world lights unavailable");return false;
        }
        if(!provider&&!candidate_provider){
            std::string reason="no checked complete world-caster provider; visible geometry is insufficient";
            native11::CoverageReport coverage;std::string diagnostic_error;
            if(native11::audit_coverage(*visible,lights->captured,{},coverage,diagnostic_error)){
                reason="GameObject geometry "+std::to_string(coverage.observed_objects)+"/"+
                    std::to_string(coverage.world_objects)+"; "+reason;
                native11::ReadinessReport readiness;std::string json;
                if(native11::make_readiness_report(visible->frame().camera,generation,coverage,{},
                       readiness,diagnostic_error)&&
                   native11::format_readiness_report(readiness,json,diagnostic_error))
                    publish_diagnostic(ticket,lights->class_generation,std::move(json));
                else reason+="; readiness diagnostic unavailable: "+diagnostic_error.substr(0,160);
            }else reason+="; coverage audit unavailable: "+diagnostic_error.substr(0,160);
            if(ticket!=source_epoch()||!native11_light_classes::healthy()||
               lights->class_generation!=native11_light_classes::generation()){
                clear_diagnostic();record(State::waiting_sources,generation,"world sources changed during diagnostic assembly");return false;
            }
            record(State::waiting_geometry,generation,reason);return false;
        }
        Geometry geometry;std::string error;bool assembled=false;
        if(candidate_provider){
            CandidateGeometry input;
            if(candidate_provider(candidate_context,*visible,*lights,input,error)){
                if(candidate_ticket!=ticket||candidate_class_generation!=lights->class_generation){
                    candidate_assembler.clear();candidate_ticket=ticket;candidate_class_generation=lights->class_generation;
                }
                const auto previous=session.current();
                if(previous&&previous->materials)candidate_epoch=std::max(candidate_epoch,previous->materials->source_epoch());
                if(candidate_epoch==std::numeric_limits<std::uint64_t>::max())error="candidate resource epoch exhausted";
                else assembled=candidate_assembler.update(visible,lights->captured,input.shadow_candidates,
                    input.absent_domains,++candidate_epoch,error);
                native11::CoverageReport coverage;native11::ReadinessReport readiness;
                std::string json,diagnostic_error;
                if(native11::audit_coverage(*visible,lights->captured,input.shadow_candidates,coverage,diagnostic_error)&&
                   native11::make_readiness_report(visible->frame().camera,generation,coverage,input.absent_domains,readiness,diagnostic_error)&&
                   native11::format_readiness_report(readiness,json,diagnostic_error))
                    publish_diagnostic(ticket,lights->class_generation,std::move(json));
                if(assembled){geometry.all=candidate_assembler.current();
                    geometry.complete_casters=true;geometry.complete_visibility=true;}
            }
        }else assembled=provider(provider_context,*visible,*lights,geometry,error);
        if(!assembled||!geometry.all||!geometry.complete_casters||
           !geometry.complete_visibility||!matching_visible(*visible,*geometry.all)){
            record(State::waiting_geometry,generation,error.empty()?"incomplete/mismatched world geometry or visible-object coverage":error);return false;
        }
        if(!native11_light_classes::healthy()||lights->class_generation!=native11_light_classes::generation()){
            record(State::waiting_sources,generation,"light classes changed during world geometry assembly");return false;
        }
        if(ticket!=source_epoch()){record(State::waiting_sources,generation,"world sources changed during geometry assembly");return false;}
        const auto& viewport=visible->frame().view.viewport;
        const double wide=double(viewport[0])+viewport[2],high=double(viewport[1])+viewport[3];
        if(!std::isfinite(wide)||!std::isfinite(high)||wide<1||high<1||wide>4096||high>4096||
           wide!=std::floor(wide)||high!=std::floor(high)){
            record(State::unavailable,generation,"offscreen world viewport exceeds configured bounds");return false;
        }
        const auto width=static_cast<std::uint32_t>(wide),height=static_cast<std::uint32_t>(high);
        if(helper&&generation<=session.accepted_generation()){
            record(State::unavailable,generation,"duplicate or stale offscreen frame generation");return false;
        }
        if(!helper){
            if(!load(error)){record(State::unavailable,generation,error);return false;}
            auto symbol=GetProcAddress(helper,"A2FO_Native11_GetInterface");A2FO_N11_GetInterface get=nullptr;
            static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
            bool initialized=false;
            if(get){
                if(share_world_geometry){A2FO_N11_API6 api{};
                    initialized=get(6,sizeof(api),&api)>=0&&session.initialize_shared(api,width,height,error);
                }else{A2FO_N11_API5 api{};
                    initialized=get(5,sizeof(api),&api)>=0&&session.initialize(api,width,height,error);}
            }
            if(!initialized){
                std::string close_error;
                if(session.close(close_error)){FreeLibrary(helper);helper=nullptr;}
                record(State::unavailable,generation,error.empty()?"helper requested API admission refused":error);return false;
            }
            owner=GetCurrentThreadId();
            target_width=width;target_height=height;
        }else if((width!=target_width||height!=target_height)){
            image_eligible=false;
            if(!session.resize(width,height,error)){record(State::unavailable,generation,error);return false;}
            target_width=width;target_height=height;
        }
        auto packet=std::make_shared<native11::OwnedWorldFrame>();packet->materials=geometry.all;packet->lighting=lights->lighting;
        packet->light_camera=lights->camera;packet->light_generation=lights->frame_generation;packet->complete_casters=true;
        image_eligible=false;
        material_preview_eligible=false;
        if(!session.render(packet,error)){record(State::unavailable,generation,error);return false;}
#if defined(A2FO_NATIVE11_PIPELINE_TESTING)
        a2fo_native11_world_pipeline_after_submit();
#endif
        if(ticket!=source_epoch()||!native11_light_classes::healthy()||lights->class_generation!=native11_light_classes::generation()){
            record(State::waiting_sources,generation,"world sources changed before offscreen publication");return false;
        }
        image_eligible=true;published_ticket=ticket;published_class_generation=lights->class_generation;
        record(State::rendered,generation,"owned native11 frame rendered offscreen; legacy presentation unchanged");
        try{if(logger&&(generation<=8||generation%300==0))logger("Native11 world pipeline rendered an owned offscreen frame; no presentation takeover");}catch(...){}
        return true;
    }catch(...){record(State::unavailable,generation,"world pipeline allocation/provider failure");return false;}
}
Status status() noexcept {Status copy;try{AcquireSRWLockShared(&status_lock);copy=current_status;ReleaseSRWLockShared(&status_lock);}catch(...){ReleaseSRWLockShared(&status_lock);}return copy;}
MaterialStatus material_status() noexcept {
    MaterialStatus copy;
    try{AcquireSRWLockShared(&status_lock);copy=current_material_status;ReleaseSRWLockShared(&status_lock);}
    catch(...){ReleaseSRWLockShared(&status_lock);return {};}
    if((copy.state==MaterialState::prepared||copy.state==MaterialState::previewed)&&copy.source_ticket!=source_epoch()){
        copy.state=MaterialState::waiting_sources;try{copy.reason="live material source lifecycle invalidated";}catch(...){}
    }
    return copy;
}
bool readback_material_preview(std::vector<std::uint8_t>& bytes,std::uint32_t& width,
    std::uint32_t& height,std::string& error) noexcept {
    Admission guard;if(!guard.entered)return false;
    try{
        const auto current=[&](){return material_preview_eligible&&material_ticket==source_epoch()&&
            material_validator&&material_packet&&material_validator(material_packet);};
        if(!current()){error="no current live material preview";return false;}
        std::vector<std::uint8_t> candidate;std::uint32_t w=0,h=0;
        if(!session.readback(candidate,w,h,error)||!current())return false;
        bytes=std::move(candidate);width=w;height=h;error.clear();return true;
    }catch(...){try{error="live material preview readback failed";}catch(...){}return false;}
}
std::string diagnostic_json() noexcept {
    std::string copy;
    if(!native11_light_classes::healthy())return copy;
    const auto class_generation=native11_light_classes::generation();
    const auto ticket=source_epoch();
    try{AcquireSRWLockShared(&status_lock);
        if(diagnostic_ticket&&diagnostic_ticket==source_epoch()&&
           diagnostic_class_generation==class_generation)copy=current_diagnostic;
        ReleaseSRWLockShared(&status_lock);
    }catch(...){ReleaseSRWLockShared(&status_lock);}
#if defined(A2FO_NATIVE11_PIPELINE_TESTING)
    a2fo_native11_world_pipeline_after_diagnostic_copy();
#endif
    if(ticket!=source_epoch()||!native11_light_classes::healthy()||
       class_generation!=native11_light_classes::generation())copy.clear();
    return copy;
}
bool readback(std::vector<std::uint8_t>& bytes,std::uint32_t& width,std::uint32_t& height,std::string& error) noexcept {
    Admission guard;if(!guard.entered)return false;
    try {
        const auto current=[&](){return image_eligible&&published_ticket==source_epoch()&&native11_light_classes::healthy()&&
            published_class_generation==native11_light_classes::generation();};
        if(!current()){error="no current published offscreen image";return false;}
        std::vector<std::uint8_t> candidate;std::uint32_t wide=0,high=0;
        if(!session.readback(candidate,wide,high,error))return false;
        if(!current()){error="offscreen sources changed during readback";return false;}
        bytes=std::move(candidate);width=wide;height=high;return true;
    }catch(...){try{error="offscreen readback allocation failure";}catch(...){}return false;}
}
bool shutdown() noexcept {
    Admission guard;if(!guard.entered)return false;
    invalidate_sources();
    std::string error;if(helper&&!session.close(error)){record(State::unavailable,0,error);return false;}
    if(helper){FreeLibrary(helper);helper=nullptr;}
    if(!native11_preview_export::stop()){record(State::unavailable,0,"preview exporter is busy");return false;}
    live_materials=live_preview=world_wanted=false;material_packet.reset();material_ticket=0;material_preview_eligible=false;
    record_material(MaterialState::disabled,0,0,0,"live material pipeline shut down");
    wanted=false;configured=false;owner=0;target_width=target_height=0;image_eligible=false;provider=nullptr;provider_context=nullptr;candidate_provider=nullptr;candidate_context=nullptr;candidate_assembler.clear();record(State::disabled,0,"world pipeline shut down");return true;
}
}
