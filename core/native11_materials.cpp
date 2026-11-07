#include "native11_materials.hpp"
#include "native11_material_profile.hpp"
#include "native11_capture.hpp"
#include "native11_assets.hpp"
#include "native11_textures.hpp"
#include "native11_nodes.hpp"
#include "native11_animation_clock.hpp"
#include "../renderer/native11/material_stream.hpp"
#include "../renderer/native11/node_state.hpp"
#include "../rust/native_hooks_ffi.h"
#include "../rust/native11_model_ffi.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <list>
#include <set>

extern "C" {std::uintptr_t a2fo_native11_material_draw_original=0;}
namespace {
using namespace a2fo::native11;
constexpr std::uint32_t call_rva=0xd5a11;
constexpr std::array<std::uint8_t,5> original{{0xe8,0x6a,0x8d,0x15,0x00}};
SRWLOCK lock=SRWLOCK_INIT,cache_lock=SRWLOCK_INIT;
struct Held{SRWLOCK* value;explicit Held(SRWLOCK& v):value(&v){AcquireSRWLockExclusive(value);}~Held(){ReleaseSRWLockExclusive(value);}};
bool installed=false,enabled=false,active=false,failed=false;
std::uint64_t lifecycle=0,frame_lifecycle=0,sealed_epoch=0;
DWORD owner=0;std::uint32_t camera=0;
HMODULE image_owner=nullptr;
MaterialReadProfile read_profile{};
std::atomic<a2fo::native11_materials::Log> logger{nullptr};
std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
std::set<const ModelAsset*> observed_models;
std::map<std::uint64_t,const TextureAsset*> observed_textures;
std::size_t observed_parts=0,observed_bytes=0;
std::string failure_reason;
MaterialFrameStream material_stream; // accessed only under lock
a2fo::native11_materials::Listener listener{};
void clear_records() noexcept {
    observations.clear();observed_models.clear();observed_textures.clear();observed_parts=observed_bytes=0;
}
bool commit_observation(const std::shared_ptr<const MaterialObservation>& observed){
    if(!observed||observations.count(observed->instance)||observed->parts.size()>8192-observed_parts)return false;
    std::size_t extra=observed_models.count(observed->model.get())?0:observed->model->bytes.size();
    std::map<std::uint64_t,const TextureAsset*> additions;
    for(const auto& part:observed->parts)if(part.texture){
        const auto found=observed_textures.find(part.texture->serial);
        if(found!=observed_textures.end()){
            if(found->second!=part.texture.get())return false;
        }else{
            const auto inserted=additions.emplace(part.texture->serial,part.texture.get());
            if(!inserted.second&&inserted.first->second!=part.texture.get())return false;
            if(inserted.second){
                if(part.texture->bytes->size()>128u*1024*1024-extra)return false;
                extra+=part.texture->bytes->size();
            }
        }
    }
    if(additions.size()>256-observed_textures.size()||extra>128u*1024*1024-observed_bytes)return false;
    observed_models.insert(observed->model.get());observations.emplace(observed->instance,observed);
    observed_textures.merge(additions);observed_parts+=observed->parts.size();observed_bytes+=extra;
    return true;
}
struct SourceEntry{std::weak_ptr<const ModelAsset> model;std::uint64_t serial;
    std::vector<MaterialPartSource> parts;std::size_t bytes;};
std::list<SourceEntry> sources;
std::size_t source_bytes=0;
void report(const char* message) noexcept {try{if(auto f=logger.load())f(message);}catch(...){}}
void refusal(const char* reason) noexcept {
    static std::atomic<unsigned> count{0};const auto n=++count;
    if(n>8&&n%512)return;
    char message[384]{};std::snprintf(message,sizeof(message),"Native11 material observation refusal %u: %s",n,reason);
    report(message);
}
bool read_local(void*,std::uint32_t address,void* output,std::size_t size){
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),output,size,&copied)&&copied==size;
}
bool owns_call(HMODULE image) noexcept {
    std::array<std::uint8_t,5> actual{};
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    if(!read_local(nullptr,static_cast<std::uint32_t>(base+call_rva),actual.data(),actual.size()))return false;
    if(!installed)return actual==original;
    std::int32_t delta=0;std::memcpy(&delta,actual.data()+1,4);
    return actual[0]==0xe8&&base+call_rva+5+delta==reinterpret_cast<std::uintptr_t>(&a2fo_native11_material_draw_tap);
}
bool loaded_profile(HMODULE image) noexcept {
    namespace p=a2fo::native11_materials::profile;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image)),delta=base-p::preferred_base;
    if(!owns_call(image))return false;
    for(const auto& region:p::regions){std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||!read_local(nullptr,base+region.rva,bytes.data(),region.size))return false;
        if(installed&&region.rva<=call_rva&&call_rva+5<=region.rva+region.size)
            std::memcpy(bytes.data()+call_rva-region.rva,original.data(),5);
        for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:p::slots){std::uint32_t value=0;
        if(!read_local(nullptr,base+slot[0],&value,4)||value!=base+slot[1])return false;
    }
    return true;
}
bool source_for(const std::shared_ptr<const ModelAsset>& model,ModelMaterialSource& output,std::string& error){
    Held held(cache_lock);
    for(auto it=sources.begin();it!=sources.end();){if(it->model.expired()){
            source_bytes-=it->bytes;it=sources.erase(it);
        }else ++it;}
    for(auto it=sources.begin();it!=sources.end();++it)if(it->serial==model->info.serial&&it->model.lock()==model){
        output.model=model;output.parts=it->parts;sources.splice(sources.begin(),sources,it);return true;
    }
    std::shared_ptr<const ModelMaterialSource> parsed;
    if(!inspect_model_materials(model,a2fo_rs_native11_inspect_sod_all,parsed,error))return false;
    std::size_t bytes=sizeof(SourceEntry)+parsed->parts.size()*sizeof(MaterialPartSource);
    for(const auto& part:parsed->parts)bytes+=part.node_name.size()+part.authored_texture.size();
    if(bytes>8u*1024*1024){output=*parsed;return true;}
    while(!sources.empty()&&(sources.size()>=2048||source_bytes+bytes>8u*1024*1024)){
        source_bytes-=sources.back().bytes;sources.pop_back();}
    SourceEntry entry{model,model->info.serial,parsed->parts,bytes};
    sources.push_front(std::move(entry));source_bytes+=bytes;
    output=*parsed;return true;
}
std::shared_ptr<const ModelAsset> resolve_model(void*,std::uint32_t database,std::uint64_t serial){
    return a2fo::native11_assets::pin_model(database,serial);
}
std::shared_ptr<const TextureAsset> resolve_texture(void*,std::uint32_t texture){
    return a2fo::native11_textures::current(texture);
}
bool placement_unchanged(const Object& object) noexcept {
    if(object.instance>UINT32_MAX-0x84)return false;
    float raw[12]{},scale[3]{1,1,1};std::uint8_t active_scale=0;
    if(!read_local(nullptr,object.instance+0x44,raw,sizeof(raw))||
       !read_local(nullptr,object.instance+0x74,&active_scale,1)||
       (active_scale&&!read_local(nullptr,object.instance+0x28,scale,sizeof(scale))))return false;
    for(unsigned row=0;row<3;++row){
        if(raw[9+row]!=object.world[row*4+3])return false;
        for(unsigned col=0;col<3;++col)
            if(static_cast<float>(double(raw[col*3+row])*scale[col])!=object.world[row*4+col])return false;
    }
    return true;
}
}
namespace a2fo::native11_materials {
bool set_listener(Listener next) noexcept {
    Held held(lock);
    // Process-pinned callback ownership cannot change between a begin/end.
    if(active||listener.begin||!next.begin||!next.parts||!next.end)return false;
    listener=next;return true;
}
bool initialize(HMODULE image,bool requested,Log log) noexcept {
    logger.store(log);
    if(!requested){disable();return false;}
    if(!image||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
       !native11_capture::exact_engine_image(image)||!loaded_profile(image)){
        disable();report("Native11 material capture unavailable: exact render call/layout or ownership mismatch");return false;
    }
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    if(!installed){
        a2fo_native11_material_draw_original=base+0x22e780;
        if(!a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+call_rva),
            reinterpret_cast<void*>(&a2fo_native11_material_draw_tap),original.data(),original.size())){
            disable();report("Native11 material capture unavailable: checked render call refused");return false;
        }
        installed=true;
    }
    {Held held(lock);if(lifecycle==UINT64_MAX)return false;
        ++lifecycle;material_stream.clear();enabled=true;active=false;failed=false;failure_reason.clear();clear_records();image_owner=image;
        read_profile={static_cast<std::uint32_t>(base+0x2b3e2c),static_cast<std::uint32_t>(base+0x2bc56c),static_cast<std::uint32_t>(base+0x1968f0)};
    }
    report("Native11 material capture armed: per-instance render-time diffuse versions; no graphics takeover");return true;
}
void disable() noexcept {native11_animation_clock::disable();Held held(lock);material_stream.clear();enabled=false;active=false;failed=false;failure_reason.clear();clear_records();if(lifecycle!=UINT64_MAX)++lifecycle;}
void begin(void* view) noexcept {
    Held held(lock);clear_records();active=enabled&&lifecycle!=UINT64_MAX&&view;
    failed=false;failure_reason.clear();owner=GetCurrentThreadId();camera=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(view));frame_lifecycle=lifecycle;
}
void abort() noexcept {Held held(lock);active=false;clear_records();}
bool bind_frame(const Frame& frame,std::shared_ptr<const BoundMaterialCapture>& output,std::string& error) noexcept {
    try {
        std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> pinned;std::uint64_t ticket=0,epoch=0;
        {Held held(lock);
            if(!enabled||!active||failed||owner!=GetCurrentThreadId()||camera!=frame.camera||frame_lifecycle!=lifecycle||sealed_epoch==UINT64_MAX||
               (native11_nodes::requested()&&!native11_nodes::enabled())){
                error=failed&&!failure_reason.empty()?failure_reason:"material frame unavailable, incomplete or obsolete";
                active=false;clear_records();return false;
            }
            pinned=observations;ticket=lifecycle;epoch=++sealed_epoch;active=false;clear_records();
        }
        std::shared_ptr<const BoundMaterialCapture> pending;
        for(const auto& entry:pinned)if(native11_assets::pin_model(entry.second->database,entry.second->model_serial)!=entry.second->model){
            error="model changed before material frame sealing";return false;
        }
        {Held held(lock);if(!enabled||lifecycle!=ticket){error="material lifecycle changed before publication";return false;}
            if(!material_stream.update(frame,epoch,resolve_model,nullptr,pinned,error))return false;
            pending=material_stream.current();}
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="material frame allocation failure";}catch(...){}return false;}
}
bool current(const std::shared_ptr<const BoundMaterialCapture>& candidate) noexcept {
    if(!candidate)return false;
    std::uint64_t ticket=0;HMODULE image=nullptr;
    {
        Held held(lock);
        if(!enabled||!image_owner||material_stream.current()!=candidate)return false;
        ticket=lifecycle;image=image_owner;
    }
    if(!loaded_profile(image)||
       (native11_nodes::requested()&&!native11_nodes::owns_calls())||
       !candidate->resources_current(resolve_model,nullptr,resolve_texture,nullptr))return false;
    {
        Held held(lock);
        return enabled&&lifecycle==ticket&&image_owner==image&&
            material_stream.current()==candidate;
    }
}
}
static void observe_selection(void* instance,void* view,void* database,bool posed,
    unsigned native_x87_control=0) noexcept {
    try {
        std::uint64_t ticket=0;MaterialReadProfile profile{};HMODULE image=nullptr;
        {Held held(lock);if(!enabled||!active||failed||owner!=GetCurrentThreadId()||
             camera!=reinterpret_cast<std::uintptr_t>(view))return;
            ticket=lifecycle;profile=read_profile;image=image_owner;
        }
        Object object{};
        if(!a2fo::native11_capture::material_target(instance,view,object)){
            refusal("no owned main-view instance association");return;
        }
        bool valid=owns_call(image)&&(!posed||(a2fo::native11_nodes::owns_calls()&&
            object.database==reinterpret_cast<std::uintptr_t>(database)));
        auto model=valid?a2fo::native11_assets::pin_model(object.database,object.model_serial):nullptr;
        ModelMaterialSource source;std::string error;std::shared_ptr<const MaterialObservation> observed;
        valid=model&&source_for(model,source,error);
        if(valid&&posed&&!placement_unchanged(object)){valid=false;error="instance placement changed before node capture";}
        std::vector<PartState> states;
        if(valid&&posed)valid=capture_node_states(object,source,profile,nullptr,read_local,states,error);
        if(valid)valid=capture_material_observation(object,source,profile,
            nullptr,read_local,nullptr,resolve_texture,observed,error,posed?&states:nullptr);
        if(valid&&posed){
            if(valid){auto copy=std::make_shared<MaterialObservation>(*observed);
                copy->native_x87_control=native_x87_control;
                observed=std::move(copy);
            }
            if(valid)a2fo::native11_animation_clock::observe(object,
                static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(view)),&source,observed.get(),native_x87_control);
        }
        a2fo::native11_materials::Listener notify{};std::string rejected;
        {Held held(lock);if(!enabled||!active||lifecycle!=ticket||frame_lifecycle!=ticket)return;
            if(!valid||!commit_observation(observed)){
                failed=true;clear_records();
                failure_reason=!error.empty()?error:"material draw/model unavailable, duplicate event or frame budget exceeded";
                rejected=failure_reason;
            }else if(posed)notify=listener;
        }
        if(!rejected.empty())refusal(rejected.c_str());
        if(notify.parts)notify.parts(notify.context,view,object,observed);
    }catch(...){Held held(lock);if(active&&owner==GetCurrentThreadId()){failed=true;clear_records();}}
}
extern "C" void a2fo_native11_observe_materials(void* instance,void* view) noexcept {
    a2fo::native11_materials::Listener notify{};
    {Held held(lock);notify=listener;}
    if(notify.begin)notify.begin(notify.context,instance,view);
    if(!a2fo::native11_nodes::requested())observe_selection(instance,view,nullptr,false);
}
extern "C" void a2fo_native11_material_render_end() noexcept {
    a2fo::native11_materials::Listener notify{};
    {Held held(lock);notify=listener;}
    if(notify.end)notify.end(notify.context);
}
namespace a2fo::native11_materials {
void observe_nodes(void* instance,void* camera,void* database,unsigned native_x87_control) noexcept {
    observe_selection(instance,camera,database,true,native_x87_control);
}
}
