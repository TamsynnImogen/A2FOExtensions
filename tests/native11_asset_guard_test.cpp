#include "../core/native11_assets.hpp"
#include "../core/native11_asset_profile.hpp"
#include "../core/native11_capture.hpp"
#include "../core/native11_capture_scale_profile.hpp"
#include "../core/native11_fofs_stream.hpp"
#include "../core/native11_fofs_stream_profile.hpp"
#include "../core/native11_world_lights.hpp"
#include "../core/native11_world_light_profile.hpp"
#include "../core/native11_craft_view_profile.hpp"
#include "../core/native11_craft_preparation_profile.hpp"
#include "../core/native11_craft_visibility_base_profile.hpp"
#include "../core/native11_borg_material_profile.hpp"
#include "../core/native11_light_classes.hpp"
#include "../core/native11_world_pipeline.hpp"
#include "native11_model_fixture.hpp"
#include <algorithm>
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace {
unsigned patches=0, logs=0;
bool allow_patch=false;
unsigned before_publish_mode=0;
std::uint32_t retire_before_publish_db=0;
HMODULE capture_image=nullptr;
struct TemplateState {
    std::shared_ptr<const a2fo::native11::ModelAsset> model;
    std::shared_ptr<const a2fo::native11::AuthoredPoseData> authored;
    std::shared_ptr<const a2fo::native11::ModelNodeTemplateData> data;
    bool available=true;
};
TemplateState* active_template_state=nullptr;
struct MaterialState {
    std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> baseline;
    std::shared_ptr<const a2fo::native11::TextureAsset> original,alternate;
    bool available=true,textures_available=true;
};
MaterialState* active_material_state=nullptr;
std::shared_ptr<const a2fo::native11::ModelMaterialBaseline> material_source(
    void* context,const std::shared_ptr<const a2fo::native11::ModelAsset>& model){
    const auto& state=*static_cast<MaterialState*>(context);
    return state.available&&state.baseline&&state.baseline->model==model?
        std::make_shared<const a2fo::native11::ModelMaterialBaseline>(*state.baseline):nullptr;
}
std::shared_ptr<const a2fo::native11::TextureAsset> borg_texture_source(void* context,std::uint32_t texture){
    const auto& state=*static_cast<MaterialState*>(context);
    if(!state.textures_available)return nullptr;
    if(state.original&&state.original->texture==texture)return state.original;
    return state.alternate&&state.alternate->texture==texture?state.alternate:nullptr;
}
DWORD WINAPI foreign_borg_read(void* context){
    *static_cast<bool*>(context)=bool(a2fo::native11_capture::latest_world_borg_materials());return 0;
}
std::shared_ptr<const a2fo::native11::ModelNodeTemplate> template_source(
    void* context,const std::shared_ptr<const a2fo::native11::ModelAsset>& model){
    const auto& state=*static_cast<TemplateState*>(context);
    if(!state.available||model!=state.model)return nullptr;
    // Test double for a completed-load owner; this fixture does not prove a
    // real load-template callback or the native game's model selection.
    auto wrapper=std::make_shared<a2fo::native11::ModelNodeTemplate>();
    wrapper->model=state.model;wrapper->authored=state.authored;wrapper->data=state.data;
    return wrapper;
}
struct ForeignVisibilityRead {
    std::shared_ptr<const a2fo::native11_capture::WorldVisibilitySnapshot> result;
};
DWORD WINAPI foreign_visibility_read(void* context){
    static_cast<ForeignVisibilityRead*>(context)->result=a2fo::native11_capture::latest_world_visibility();
    return 0;
}
void log(const char*);
DWORD WINAPI disable_capture_worker(void* restart) {
    a2fo::native11_capture::disable();
    return restart&&!a2fo::native11_capture::initialize(capture_image,true,log)?1:0;
}
void log(const char*) { ++logs; }
void need(bool value,const char* message) {
    if(!value){std::fprintf(stderr,"FAIL %s (winerr=%lu)\n",message,GetLastError());std::exit(1);}
}
void word(void* object,std::size_t offset,std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(object)+offset,&value,4);
}
std::uint32_t address(const void* p) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}
std::uint32_t loaded_word(const std::uint8_t* p) {
    std::uint32_t value=0;std::memcpy(&value,p,4);return value;
}
void mapped_word(std::uint8_t* base,std::uint32_t rva,std::uint32_t expected,std::uint32_t replacement) {
    auto* p=base+rva;
    need(loaded_word(p)==expected,"private loader original word");
    DWORD old=0;need(VirtualProtect(p,4,PAGE_READWRITE,&old)!=0,"private loader word writable");
    std::memcpy(p,&replacement,4);
    DWORD ignored=0;need(VirtualProtect(p,4,old,&ignored)!=0,"private loader word protected");
}
void mapped_bytes(void* place,const void* source,std::size_t size) {
    DWORD old=0,ignored=0;
    need(VirtualProtect(place,size,PAGE_EXECUTE_READWRITE,&old)!=0,"private image bytes writable");
    std::memcpy(place,source,size);
    need(FlushInstructionCache(GetCurrentProcess(),place,size)!=0,"private image cache flushed");
    need(VirtualProtect(place,size,old,&ignored)!=0,"private image protection restored");
}
template<class F> void altered_byte(std::uint8_t* p,F&& check,DWORD writable=PAGE_READWRITE) {
    DWORD old=0;need(VirtualProtect(p,1,writable,&old)!=0,"private image byte writable");
    const auto original=*p;*p^=1;
    check();
    *p=original;
    DWORD ignored=0;need(VirtualProtect(p,1,old,&ignored)!=0,"private image byte protected");
}
struct SinkState {
    std::uint32_t database=0;
    std::uint64_t serial=0;
    std::vector<std::uint8_t> bytes;
    bool reentered=false;
};
std::int32_t A2FO_N11_CALL sink(void* context,const std::uint8_t* bytes,std::uint32_t count) {
    auto& state=*static_cast<SinkState*>(context);
    a2fo::native11::ModelAssetInfo info{};
    state.reentered=a2fo::native11_assets::model_info(state.database,info)&&info.serial==state.serial;
    state.bytes.assign(bytes,bytes+count);
    return 1;
}
}
extern "C" void a2fo_native11_capture_before_publish() noexcept {
    if(before_publish_mode==5){before_publish_mode=0;
        if(active_material_state)active_material_state->available=false;
        return;}
    if(!before_publish_mode)return;
    if(before_publish_mode==4){before_publish_mode=0;
        if(active_template_state)active_template_state->available=false;
        return;
    }
    if(before_publish_mode==3){before_publish_mode=0;
        need(a2fo::native11_assets::retire_model(retire_before_publish_db),
             "model retired after association binding and before publication");return;
    }
    const bool restart=before_publish_mode==2;before_publish_mode=0;
    HANDLE worker=CreateThread(nullptr,0,disable_capture_worker,restart?reinterpret_cast<void*>(1):nullptr,0,nullptr);
    need(worker&&WaitForSingleObject(worker,5000)==WAIT_OBJECT_0,"concurrent disable completed before publication");
    DWORD result=1;need(GetExitCodeThread(worker,&result)&&result==0,"optional reenable completed before publication");
    CloseHandle(worker);
}
// The checked patch is observed, never written; neither patched call nor game code runs.
extern "C" bool a2fo_rs_windows_patch_call(void* site,void* target,
    const std::uint8_t* expected,std::size_t length) {
    const std::uint8_t signature[]{0xe8,0x0e,0x2c,0x01,0x00};
    const std::uint8_t entity_signature[]{0x8b,0x11,0xff,0x52,0x14};
    if(length==5&&expected&&!std::memcmp(expected,entity_signature,5))return allow_patch;
    ++patches;
    need(site&&target&&expected&&length==sizeof(signature)&&
         std::memcmp(expected,signature,sizeof(signature))==0,"model call patch contract");
    return allow_patch;
}
int main(int argc,char** argv) {
    using namespace a2fo::native11_assets;
    const bool world_views=argc==3&&std::strcmp(argv[2],"--world-views")==0;
    const bool world_borg=argc==3&&std::strcmp(argv[2],"--world-borg")==0;
    const bool world_visibility=world_borg||(argc==3&&std::strcmp(argv[2],"--world-visibility")==0);
    const bool world_preparations=world_visibility||
        (argc==3&&std::strcmp(argv[2],"--world-preparations")==0);
    const bool world_models=world_views||world_preparations||
        (argc==3&&std::strcmp(argv[2],"--world-models")==0);
    need(!initialize(GetModuleHandleA(nullptr),false,log)&&patches==0,"default disabled");
    need(!initialize(GetModuleHandleA(nullptr),true,log)&&patches==0&&logs==1,
         "unreviewed executable rejected before patch");
    if(argc==2||argc==3) {
        HMODULE image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        need(image!=nullptr,"map private pinned executable without resolving imports");
        auto* base=reinterpret_cast<std::uint8_t*>(image);
        namespace p=a2fo::native11_assets::profile;
        const auto numeric=address(base),delta=numeric-p::preferred_base;
        const auto first=p::slots.front();
        if(delta&&loaded_word(base+first[0])==p::preferred_base+first[1]) {
            need(!initialize(image,true,log)&&patches==0,"unrelocated inspection image rejected");
            for(const auto& region:p::regions) {
                need(std::memcmp(base+region.rva,region.bytes,region.size)==0,
                     "private preferred-base profile matches");
                for(std::size_t i=0;i<region.relocation_count;++i) {
                    const auto offset=region.relocations[i];
                    const auto original=loaded_word(region.bytes+offset);
                    mapped_word(base,region.rva+offset,original,original+delta);
                }
            }
            for(const auto& slot:p::slots)
                mapped_word(base,slot[0],p::preferred_base+slot[1],numeric+slot[1]);
            std::puts("Private image: guarded HIGHLOW and slot relocations emulated");
        } else {
            need(loaded_word(base+first[0])==numeric+first[1],
                 "mapped inspection image has loader-relative slot");
        }
        HMODULE fleet=nullptr;
        std::array<std::uint8_t,6> original_fopen{};
        std::vector<std::pair<std::uint8_t*,std::uint32_t>> fleet_fixups;
        if(argc==3&&!world_models){
            namespace f=a2fo::native11_fofs_stream::profile;
            fleet=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
            need(fleet&&a2fo::native11_capture::exact_image_file(fleet,f::fleet_sha256.data()),
                 "exact Fleet DLL hash pinned before stream redirect");
            auto* loaded=reinterpret_cast<std::uint8_t*>(fleet);
            const auto fleet_delta=address(loaded)-f::fleet_preferred_base;
            for(std::size_t i=0;i<f::fopen_callback.relocation_count;++i){
                const auto offset=f::fopen_callback.relocations[i];
                const auto expected=loaded_word(f::fopen_callback.bytes+offset);
                const auto actual=loaded_word(loaded+f::fopen_callback.rva+offset);
                need(actual==expected||actual==expected+fleet_delta,
                     "Fleet fopen callback HIGHLOW original or relocated");
                if(actual==expected&&fleet_delta){
                    fleet_fixups.emplace_back(loaded+f::fopen_callback.rva+offset,actual);
                    const auto relocated=expected+fleet_delta;
                    mapped_bytes(loaded+f::fopen_callback.rva+offset,&relocated,4);
                }
            }
            auto* site=base+f::open_call_rva;
            std::memcpy(original_fopen.data(),site,original_fopen.size());
            need(original_fopen[0]==0xff&&original_fopen[1]==0x15,
                 "private Root fopen import call present");
            std::array<std::uint8_t,6> redirected{{0xe8,0,0,0,0,0x90}};
            const auto target=address(loaded+f::fopen_callback.rva)-address(site)-5;
            std::memcpy(redirected.data()+1,&target,4);mapped_bytes(site,redirected.data(),redirected.size());
            need(a2fo::native11_fofs_stream::qualify(image,fleet),
                 "checked Fleet fopen redirect qualified without native execution");
        }
        auto initialize_assets=[&](){return initialize(image,true,log,fleet);};
        allow_patch=true;
        for(const auto& region:p::regions) {
            for(const auto offset:{std::size_t{0},region.size-1})
                altered_byte(base+region.rva+offset,[&]{
                    need(!initialize_assets()&&patches==0,
                         "profile boundary mutation rejected before patch");
                });
            for(std::size_t i=0;i<region.relocation_count;++i)
                altered_byte(base+region.rva+region.relocations[i],[&]{
                    need(!initialize_assets()&&patches==0,
                         "normalized HIGHLOW mutation rejected before patch");
                });
        }
        for(const auto& slot:p::slots)
            altered_byte(base+slot[0],[&]{
                need(!initialize_assets()&&patches==0,
                     "loaded vtable slot mutation rejected before patch");
            });
        allow_patch=false;
        need(!initialize_assets()&&patches==1,"checked patch refusal leaves capture disabled");
        a2fo::native11::ModelAssetInfo disabled_info{};
        need(!model_info(0x1234,disabled_info),"failed patch cannot publish metadata");
        allow_patch=true;
        need(initialize_assets()&&patches==2,"exact image retries checked mocked patch");
        need(a2fo_native11_read_database_original==reinterpret_cast<std::uintptr_t>(base)+0x23fa60,
             "checked call continuation seeded");
        need(!owns_load_call(image),"mock success without installed bytes is not load-boundary ownership");
        need(!initialize_assets()&&!owns_load_call(image),
             "mocked install cannot reactivate without owned model CALL");
        const auto call_operand=loaded_word(base+0x22ce4e);
        const auto owned_operand=static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(&a2fo_native11_database_read_tap)-numeric-0x22ce52u);
        mapped_word(base,0x22ce4e,call_operand,owned_operand);
        need(initialize_assets(),"owned model CALL permits reactivation");
        need(owns_load_call(image)&&!owns_load_call(GetModuleHandleA(nullptr)),
             "exact installed CALL and module ownership checked without execution");
        altered_byte(base+0x22ce4e,[&]{need(!owns_load_call(image),"foreign load CALL rejected");});
        const auto prepublish_epoch=world_models?generation():0;
        if(!fleet&&!world_models)mapped_word(base,0x22ce4e,owned_operand,call_operand);
        std::array<std::uint8_t,0x40> database{};
        std::array<std::uint8_t,0x20> stream{};
        auto source=native11_fixture::model();const auto original=source;
        char name[]="fed_venture.sod";
        const auto db=address(database.data());
        word(database.data(),0,numeric+0x2bc56c);
        word(database.data(),0x38,address(name));
        word(stream.data(),0,numeric+0x2bc874);
        word(stream.data(),0x14,address(source.data()));
        word(stream.data(),0x18,address(source.data()+source.size()));
        word(stream.data(),0x1c,address(source.data()+2));
        a2fo_native11_observe_database(database.data(),stream.data(),0x7fc01234u,name,0x1027fu);
        a2fo::native11::ModelAssetInfo info{};
        need(model_info(db,info)&&info.database==db&&info.byte_count==source.size()&&
             info.parser_scalar_bits==0x7fc01234u&&info.parser_native_x87_control==0x1027fu&&
             std::strcmp(info.name,name)==0&&info.serial,
             "manual observation publishes actual bytes and raw scalar bits");
        if(world_models){
            need(generation()>prepublish_epoch&&pin_current_model(db,name)!=nullptr&&
                 !pin_current_model(db,"wrong.sod"),
                 "owned model publication advances generation and requires exact copied name");
            altered_byte(base+0x22ce4e,[&]{
                need(generation()==0&&!pin_current_model(db,name),
                     "wrong model CALL removes source generation and current pin");
            });
        }
        std::fill(source.begin(),source.end(),99);
        SinkState result{db,info.serial,{},false};
        need(read_model(db,info.serial,sink,&result)&&result.reentered&&
             result.bytes==original,
             "owned bytes survive source change and reentrant sink reads metadata");
        auto frame=std::make_unique<a2fo::native11::Frame>();
        frame->object_count=1;frame->camera=1;frame->generation=1;
        auto& object=frame->objects[0];object.database=db;object.model_serial=info.serial;
        object.model_identity=a2fo::native11::ModelIdentity::plain_database_name;
        std::strcpy(object.model_name,name);
        std::shared_ptr<const a2fo::native11::BoundCapture> bound;std::string error;
        need(bind_frame(*frame,bound,error)&&bound->models().size()==1,"core binds recorded model version under lock");
        const auto retained=bound;
        // Also exercise the actual core scene -> serial -> store binding. Only
        // guarded loader fixups are emulated in this non-executed image.
        namespace scene_profile=a2fo::native11_capture::profile;
        if(delta){
            for(const auto& region:scene_profile::regions)
                for(std::size_t i=0;i<region.relocation_count;++i){const auto offset=region.relocations[i];
                    const auto original=loaded_word(region.bytes+offset);
                    if(loaded_word(base+region.rva+offset)==original)
                        mapped_word(base,region.rva+offset,original,original+delta);
                }
            for(const auto& slot:std::array<std::array<std::uint32_t,2>,3>{{{{0x2b3e38,0xcb6b0}},{{0x2b3e40,0xcb0e0}},{{0x2bc56c,0x2209b0}}}})
                if(loaded_word(base+slot[0])==p::preferred_base+slot[1])
                    mapped_word(base,slot[0],p::preferred_base+slot[1],numeric+slot[1]);
        }
        std::vector<std::pair<std::uint8_t*,std::uint32_t>> world_fixups;
        std::vector<std::pair<std::uint8_t*,std::uint32_t>> view_fixups;
        std::vector<std::pair<std::uint8_t*,std::vector<std::uint8_t>>> view_globals;
        std::vector<std::pair<std::uint8_t*,std::uint32_t>> preparation_fixups;
        std::vector<std::pair<std::uint8_t*,std::vector<std::uint8_t>>> preparation_globals;
        std::vector<std::pair<std::uint8_t*,std::uint32_t>> visibility_fixups;
        std::vector<std::pair<std::uint8_t*,std::vector<std::uint8_t>>> visibility_globals;
        std::uint8_t* world_block=nullptr;
        std::uint32_t old_world_list=0;
        std::array<std::uint8_t,0x500> class_storage{};
        std::array<std::uint8_t,0x300> race_storage{};
        std::array<std::uint8_t,0xd0> first_systems{},second_systems{};
        if(world_models){
            namespace wp=a2fo::native11_world_lights::profile;
            for(const auto& region:wp::regions)
                for(std::size_t i=0;i<region.relocation_count;++i){
                    const auto offset=region.relocations[i];
                    const auto expected=loaded_word(region.bytes+offset);
                    auto* place=base+region.rva+offset;
                    const auto actual=loaded_word(place);
                    need(actual==expected||actual==expected+delta,
                         "world light HIGHLOW original or relocated");
                    if(actual==expected&&delta){
                        world_fixups.emplace_back(place,actual);
                        mapped_word(base,region.rva+offset,actual,actual+delta);
                    }
                }
            if(world_views){
                namespace vp=a2fo::native11_animation_clock::view_profile;
                for(const auto& region:vp::regions)
                    for(std::size_t i=0;i<region.relocation_count;++i){
                        const auto offset=region.relocations[i];
                        const auto expected=loaded_word(region.bytes+offset);
                        auto* place=base+region.rva+offset;
                        const auto actual=loaded_word(place);
                        need(actual==expected||actual==expected+delta,
                             "Craft view HIGHLOW original or relocated");
                        if(actual==expected&&delta){
                            view_fixups.emplace_back(place,actual);
                            const auto relocated=expected+delta;
                            mapped_bytes(place,&relocated,4);
                        }
                    }
                for(const auto& slot:vp::slots){
                    auto* place=base+slot[0];const auto actual=loaded_word(place);
                    const auto expected=vp::preferred_base+slot[1];
                    need(actual==expected||actual==numeric+slot[1],
                         "Craft view vtable slot original or relocated");
                    if(actual==expected&&delta){
                        view_fixups.emplace_back(place,actual);
                        const auto relocated=numeric+slot[1];
                        mapped_bytes(place,&relocated,4);
                    }
                }
            }
            if(world_preparations){
                namespace cp=a2fo::native11_craft_preparation::profile;
                for(const auto& region:cp::logical_regions)
                    for(std::size_t i=0;i<region.relocation_count;++i){
                        const auto offset=region.relocations[i];
                        const auto expected=loaded_word(region.bytes+offset);
                        auto* place=base+region.rva+offset;
                        const auto actual=loaded_word(place);
                        need(actual==expected||actual==expected+delta,
                             "Craft preparation HIGHLOW original or relocated");
                        if(actual==expected&&delta){
                            preparation_fixups.emplace_back(place,actual);
                            const auto relocated=expected+delta;
                            mapped_bytes(place,&relocated,4);
                        }
                    }
                for(const auto& slot:{cp::logical_slots[0],cp::slots[2]}){
                    auto* place=base+slot[0];const auto actual=loaded_word(place);
                    const auto expected=cp::preferred_base+slot[1];
                    need(actual==expected||actual==numeric+slot[1],
                         "Craft preparation vtable slot original or relocated");
                    if(actual==expected&&delta){
                        preparation_fixups.emplace_back(place,actual);
                        const auto relocated=numeric+slot[1];
                        mapped_bytes(place,&relocated,4);
                    }
                }
            }
            if(world_visibility){
                namespace cp=a2fo::native11_craft_visibility_base::profile;
                for(const auto& region:cp::regions)
                    for(std::size_t i=0;i<region.relocation_count;++i){
                        const auto offset=region.relocations[i];
                        const auto expected=loaded_word(region.bytes+offset);
                        auto* place=base+region.rva+offset;
                        const auto actual=loaded_word(place);
                        need(actual==expected||actual==expected+delta,
                             "base visibility HIGHLOW original or relocated");
                        if(actual==expected&&delta){
                            visibility_fixups.emplace_back(place,actual);
                            const auto relocated=expected+delta;
                            mapped_bytes(place,&relocated,4);
                        }
                    }
                for(const auto& slot:cp::slots){
                    auto* place=base+slot[0];const auto actual=loaded_word(place);
                    const auto expected=cp::preferred_base+slot[1];
                    need(actual==expected||actual==numeric+slot[1],
                         "base visibility vtable slot original or relocated");
                    if(actual==expected&&delta){
                        visibility_fixups.emplace_back(place,actual);
                        const auto relocated=numeric+slot[1];
                        mapped_bytes(place,&relocated,4);
                    }
                }
            }
            world_block=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,
                MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
            need(world_block!=nullptr,"private world list allocation");
            const auto container=address(world_block+0x100);
            const auto sentinel=address(world_block+0x200);
            const auto first=address(world_block+0x220);
            const auto second=address(world_block+0x240);
            old_world_list=loaded_word(base+0x361084);
            mapped_bytes(base+0x361084,&container,4);
            word(world_block+0x104,0,sentinel);
            word(world_block+0x200,0,first);word(world_block+0x200,4,second);
            word(world_block+0x220,0,second);word(world_block+0x220,4,sentinel);
            word(world_block+0x240,0,sentinel);word(world_block+0x240,4,first);
            a2fo::native11_light_classes::configure(true,log);
            a2fo::native11_light_classes::dispatch_available(true);
            need(a2fo::native11_light_classes::observe_class(
                class_storage.data(),"fixture.odf",{}),
                "synthetic class published for complete world list");
        }
        need(a2fo::native11_capture::initialize(image,true,log),"scene observer installed through mocked checked call");
        capture_image=image;
        std::array<std::uint8_t,0x100> instance{};
        std::array<std::uint8_t,0x220> entity{};
        std::array<std::uint8_t,0x100> second_instance{};
        std::array<std::uint8_t,0x220> second_entity{};
        std::array<std::uint8_t,0x1c0> camera{};
        word(instance.data(),0,numeric+0x2b3e2c);word(instance.data(),0x80,db);
        word(entity.data(),4,address(instance.data()));
        const float world[12]{1,0,0,0,1,0,0,0,1,0,0,0};
        const float screen[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        const float viewport[4]{0,0,128,128};
        std::memcpy(instance.data()+0x44,world,sizeof(world));
        if(world_models){
            const auto klass=address(class_storage.data());
            word(entity.data(),0x40,klass);word(second_entity.data(),0x40,klass);
            word(second_entity.data(),4,address(second_instance.data()));
            word(second_instance.data(),0,numeric+0x2b3e2c);
            word(second_instance.data(),0x80,db);
            std::memcpy(second_instance.data()+0x44,world,sizeof(world));
            const float second_translation=.25f;
            std::memcpy(second_instance.data()+0x44+9*sizeof(float),&second_translation,4);
            const float runtime_scale[3]{2,3,4};
            std::memcpy(instance.data()+0x28,runtime_scale,sizeof(runtime_scale));
            instance[0x74]=1;
            word(world_block+0x220,8,address(entity.data()));
            word(world_block+0x240,8,address(second_entity.data()));
            need(a2fo::native11_world_lights::initialize(image,true,log),
                 "main-thread checked world light reader armed");
        }
        if(world_views){
            auto save_global=[&](std::uint32_t rva,const void* value,std::size_t length){
                auto* place=base+rva;
                std::vector<std::uint8_t> original_bytes(length);
                std::memcpy(original_bytes.data(),place,length);
                view_globals.emplace_back(place,std::move(original_bytes));
                mapped_bytes(place,value,length);
            };
            auto* engine=world_block+0x400;
            auto* game=world_block+0x600;
            auto* settings=world_block+0x700;
            word(engine,0xfc,address(camera.data()));word(engine,0xe8,0);
            word(game,0x80,address(settings));game[0x84]=0;
            word(settings,0x40,1);
            const auto engine_pointer=address(engine),game_pointer=address(game);
            const std::uint32_t mode=1;const std::uint8_t skip_near=0;
            const float max_distance=10.f;
            save_global(0x3ad508,&engine_pointer,4);
            save_global(0x36b5ac,&game_pointer,4);
            save_global(0x36b610,&mode,4);
            save_global(0x3637b5,&skip_near,1);
            save_global(0x2fcac0,&max_distance,4);
            const float radius=1.f;
            std::memcpy(instance.data()+0x40,&radius,4);
            std::memcpy(second_instance.data()+0x40,&radius,4);
            instance[0x84]=0;second_instance[0x84]=0;
            const float first_position[3]{3,4,0},second_position[3]{6,8,0};
            const float camera_position[3]{0,0,0};
            std::memcpy(entity.data()+0xac,first_position,sizeof(first_position));
            std::memcpy(second_entity.data()+0xac,second_position,sizeof(second_position));
            std::memcpy(camera.data()+0xe4,camera_position,sizeof(camera_position));
        }
        if(world_preparations){
            const auto klass=address(class_storage.data());
            const auto race=address(race_storage.data());
            const auto first_system=address(first_systems.data());
            const auto second_system=address(second_systems.data());
            word(entity.data(),0,numeric+0x2b3cdc);
            word(second_entity.data(),0,numeric+0x2b3cdc);
            word(instance.data(),0x98,klass);word(second_instance.data(),0x98,klass);
            word(class_storage.data(),0x84,1);
            const float divisor=2.f,current=50.f,maximum=100.f;
            std::memcpy(race_storage.data()+0x288,&divisor,4);
            const std::uint32_t systems[]{first_system,second_system};
            for(unsigned k=0;k<2;++k){
                auto* logical=k?second_entity.data():entity.data();
                word(logical,0xfc,race);word(logical,0x1e0,systems[k]);
                logical[0x1bf]=0;
                std::memcpy(logical+0x1dc,&current,4);
                std::memcpy(logical+0x1c4,&maximum,4);
            }
            const std::uint8_t states[]{0,1,2,255,0};
            const std::size_t offsets[]{0,0x30,0x60,0x90,0xc0};
            for(auto* system:{first_systems.data(),second_systems.data()})
                for(unsigned k=0;k<5;++k)system[offsets[k]]=states[k];
            auto* place=base+0x340bac;
            std::vector<std::uint8_t> original_bytes(4);
            std::memcpy(original_bytes.data(),place,4);
            preparation_globals.emplace_back(place,std::move(original_bytes));
            mapped_bytes(place,&race,4);
        }
        TemplateState template_state;
        MaterialState material_state;
        std::array<std::uint32_t,2> borg_table{};
        std::array<std::array<std::uint8_t,0x140>,12> native_nodes{};
        std::array<std::array<std::uint8_t,8>,12> native_vtables{};
        std::array<std::array<char,40>,12> native_names{};
        if(world_visibility){
            auto* synthetic_engine=world_block+0x400;
            auto* synthetic_context=world_block+0x800;
            word(synthetic_engine,0x104,address(synthetic_context));
            word(synthetic_context,0x40,1);
            const auto synthetic_engine_address=address(synthetic_engine);
            auto* global=base+0x3ad508;
            std::vector<std::uint8_t> original_bytes(4);
            std::memcpy(original_bytes.data(),global,4);
            visibility_globals.emplace_back(global,std::move(original_bytes));
            mapped_bytes(global,&synthetic_engine_address,4);
            auto authored=std::make_shared<a2fo::native11::AuthoredPoseData>();
            auto data=std::make_shared<a2fo::native11::ModelNodeTemplateData>();
            template_state.model=pin_current_model(db,name);
            need(template_state.model!=nullptr,"test template binds initial owned model version");
            template_state.authored=authored;template_state.data=data;
            const std::array<const char*,12> names{{"root","hardpoints","damage","d0","d1","lights",
                "crew","life","weapons","engines","sensors","assimilation"}};
            data->hardpoints=1;data->damage=2;data->lights=5;
            data->nodes.resize(names.size());authored->nodes.resize(names.size());
            for(std::size_t k=0;k<names.size();++k){
                auto& node=data->nodes[k];auto& pose=authored->nodes[k];
                node.address=address(native_nodes[k].data());
                node.vtable=address(native_vtables[k].data());
                node.type_function=0x81000000u+static_cast<std::uint32_t>(k);
                node.name_pointer=address(native_names[k].data());
                std::strcpy(native_names[k].data(),names[k]);
                node.name=names[k];pose.name=node.name;
                node.parent=k==0?UINT32_MAX:(k==3||k==4?2:0);pose.parent=node.parent;
                node.flags=k==3||k==4?0x81u:0x80u;
                word(native_nodes[k].data(),0,node.vtable);
                word(native_nodes[k].data(),4,db);
                word(native_nodes[k].data(),8,node.name_pointer);
                word(native_nodes[k].data(),0xbc,0xff);
                word(native_vtables[k].data(),4,node.type_function);
            }
            auto link=[&](unsigned parent,unsigned child,unsigned sibling){
                auto& node=data->nodes[child];node.parent=parent;
                node.parent_address=data->nodes[parent].address;
                node.next_sibling=sibling==UINT32_MAX?UINT32_MAX:sibling;
                node.sibling_address=sibling==UINT32_MAX?0:data->nodes[sibling].address;
                authored->nodes[child].parent=parent;
            };
            data->nodes[0].first_child=1;data->nodes[0].child_address=data->nodes[1].address;
            link(0,1,2);link(0,2,5);link(0,5,6);
            for(unsigned k=6;k<11;++k)link(0,k,k+1);
            link(0,11,UINT32_MAX);
            data->nodes[2].first_child=3;data->nodes[2].child_address=data->nodes[3].address;
            link(2,3,4);link(2,4,UINT32_MAX);
            data->root_address=data->nodes[0].address;
            for(const auto& node:data->nodes){
                word(reinterpret_cast<void*>(std::uintptr_t{node.address}),0x18,node.parent_address);
                word(reinterpret_cast<void*>(std::uintptr_t{node.address}),0x1c,node.child_address);
                word(reinterpret_cast<void*>(std::uintptr_t{node.address}),0x20,node.sibling_address);
            }
            word(database.data(),0x3c,data->root_address);
            word(database.data(),0x2c,data->nodes[1].address);
            word(database.data(),0x30,data->nodes[2].address);
            word(database.data(),0x34,data->nodes[5].address);
            word(class_storage.data(),0x1dc,db);
            word(class_storage.data(),0x408,data->nodes[2].address);
            word(class_storage.data(),0x420,data->nodes[11].address);
            const std::size_t offsets[]{0x40c,0x418,0x410,0x41c,0x414};
            for(unsigned k=0;k<5;++k)word(class_storage.data(),offsets[k],data->nodes[6+k].address);
            instance[0x90]=0;second_instance[0x90]=0;
            for(auto* synthetic_instance:{instance.data(),second_instance.data()})
                for(const auto offset:{0x9cu,0x9du,0x9eu,0x9fu,0xa0u,0xa1u,0xa4u})
                    synthetic_instance[offset]=0xff;
            active_template_state=&template_state;
            a2fo::native11_capture::set_world_template_source_for_testing(template_source,&template_state);
            if(world_borg){
                namespace bp=a2fo::native11_borg_material::profile;
                for(const auto& region:bp::regions)for(std::size_t k=0;k<region.relocation_count;++k){
                    const auto offset=region.relocations[k];auto* place=base+region.rva+offset;
                    const auto expected=loaded_word(region.bytes+offset),actual=loaded_word(place);
                    need(actual==expected||actual==expected+delta,"original or relocated Borg operation source");
                    if(actual==expected&&delta){std::vector<std::uint8_t> saved(4);std::memcpy(saved.data(),place,4);
                        visibility_globals.emplace_back(place,std::move(saved));const auto relocated=expected+delta;
                        mapped_bytes(place,&relocated,4);}
                }
                auto make_texture=[](std::uint32_t token,std::uint64_t serial,const char* texture_name){
                    auto texture=std::make_shared<a2fo::native11::TextureAsset>();
                    texture->texture=token;texture->serial=serial;texture->logical_name=texture_name;
                    texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(4,0x7f);return texture;
                };
                material_state.original=make_texture(0x50000001,1,"base.tga");
                material_state.alternate=make_texture(0x50000002,2,"borg.tga");
                auto& mesh=data->nodes[3];mesh.type_function=numeric+0x1968f0;
                authored->nodes[3].kind=1;word(native_vtables[3].data(),4,mesh.type_function);
                word(native_nodes[3].data(),0x13c,material_state.alternate->texture);
                // Poison current slots: material decisions must use owned load
                // state, independently of the last draw's shared mesh values.
                word(native_nodes[3].data(),0x114,1);word(native_nodes[3].data(),0x11c,UINT32_MAX);
                auto materials=std::make_shared<a2fo::native11::ModelMaterialBaselineData>();
                a2fo::native11::ModelMeshMaterialBaseline row;row.node_index=3;row.node_address=mesh.address;
                row.slot_vector=0x50000010;row.slots={material_state.original,nullptr,nullptr,material_state.original};
                row.borg_alternate=material_state.alternate;materials->meshes.push_back(row);
                material_state.baseline=std::make_shared<const a2fo::native11::ModelMaterialBaseline>(
                    a2fo::native11::ModelMaterialBaseline{template_state.model,authored,data,materials});
                borg_table={{mesh.address,material_state.original->texture}};
                word(class_storage.data(),0,address(native_vtables[0].data()));
                word(class_storage.data(),0x1d8,db);word(class_storage.data(),0x424,1);
                word(class_storage.data(),0x428,address(borg_table.data()));
                active_material_state=&material_state;
                a2fo::native11_capture::set_world_material_sources_for_testing(
                    material_source,&material_state,borg_texture_source,&material_state);
            }
        }
        std::memcpy(camera.data()+0x160,screen,sizeof(screen));
        std::memcpy(camera.data()+0x1b0,viewport,sizeof(viewport));
        auto capture_scene=[&]{
            const bool owned=a2fo::native11_capture::begin(camera.data(),1);need(owned,"synthetic core view begin");
            a2fo_native11_observe_entity(entity.data(),camera.data());
            a2fo::native11_capture::end(camera.data(),owned);
        };
        capture_scene();
        const auto observed=a2fo::native11_capture::latest_bound();
        need(observed&&observed->frame().object_count==1&&observed->frame().objects[0].model_serial==info.serial,
             "actual core view pins version at observation and binds at frame end");
        std::shared_ptr<const a2fo::native11_capture::WorldModelSnapshot> world_snapshot;
        if(world_models){
            world_snapshot=a2fo::native11_capture::latest_world_models();
            need(world_snapshot&&world_snapshot->associations.members.size()==2&&
                 world_snapshot->world->captured.members.size()==2&&
                 world_snapshot->associations.members[0].member.entity==address(entity.data())&&
                 world_snapshot->associations.members[1].member.entity==address(second_entity.data())&&
                 world_snapshot->associations.members[0].status==
                     a2fo::native11::WorldModelStatus::bound_current_database&&
                 world_snapshot->associations.members[1].status==
                     a2fo::native11::WorldModelStatus::bound_current_database&&
                 world_snapshot->associations.members[0].model==pin_current_model(db,name)&&
                 world_snapshot->associations.members[1].model==pin_current_model(db,name)&&
                 world_snapshot->associations.members[0].model->info.serial==info.serial&&
                 world_snapshot->associations.members[1].native_placement[9]==.25f&&
                 world_snapshot->associations.members[0].runtime_scale[0]==2.f&&
                 world_snapshot->associations.members[0].native_placement[0]==1.f&&
                 observed->frame().object_count==1,
                 "complete ordered world model roster includes unobserved second object without geometry claim");
            if(world_views){
                const auto view=a2fo::native11_capture::latest_world_views();
                need(view&&view->models==world_snapshot&&view->views.members.size()==2&&
                     view->views.members[0].status==a2fo::native11::WorldViewStatus::captured&&
                     view->views.members[1].status==a2fo::native11::WorldViewStatus::captured&&
                     view->views.members[0].member.entity==address(entity.data())&&
                     view->views.members[1].member.entity==address(second_entity.data())&&
                     view->views.members[0].view.result.distance==5.f&&
                     view->views.members[1].view.result.distance==10.f&&
                     view->models->associations.members[1].native_placement[9]==.25f,
                     "world views pin same ordered model source and fresh policy inputs");
                namespace vp=a2fo::native11_animation_clock::view_profile;
                altered_byte(base+vp::regions[0].rva,[&]{
                    need(!a2fo::native11_capture::latest_world_views()&&
                         a2fo::native11_capture::latest_world_models()==world_snapshot,
                         "view profile loss revokes only view getter");
                });
                // Real startup can change these globals after source admission.
                // Sampling must use the live values instead of file defaults.
                const float runtime_fade[3]{.9f,.99f,.9f};
                const float original_fade[3]{.5f,2.f,.875f};
                mapped_bytes(base+0x30444c,runtime_fade,sizeof(runtime_fade));
                capture_scene();
                const auto configured_view=a2fo::native11_capture::latest_world_views();
                need(configured_view&&configured_view->views.members[0].view.input.near_fade_min==.9f&&
                     configured_view->views.members[0].view.input.near_fade_max==.99f&&
                     configured_view->views.members[0].view.input.near_fade_amount==.9f,
                     "configured runtime fade globals remain available and are copied into world views");
                const float invalid_fade[3]{.99f,.9f,.9f};
                mapped_bytes(base+0x30444c,invalid_fade,sizeof(invalid_fade));
                capture_scene();
                const auto invalid_view=a2fo::native11_capture::latest_world_views();
                need(invalid_view&&invalid_view->views.members[0].status==a2fo::native11::WorldViewStatus::view_unavailable,
                     "invalid live fade range stays unavailable without accepting default values");
                mapped_bytes(base+0x30444c,original_fade,sizeof(original_fade));
                const float moved_camera=1.f,moved_second=5.f;
                std::memcpy(camera.data()+0xe4,&moved_camera,4);
                std::memcpy(second_entity.data()+0xac,&moved_second,4);
                capture_scene();
                const auto fresh_view=a2fo::native11_capture::latest_world_views();
                need(fresh_view&&fresh_view->views.members[0].view.input.camera[0]==1.f&&
                     fresh_view->views.members[1].view.input.position[0]==5.f&&
                     fresh_view->views.members[0].view.result.distance!=5.f,
                     "new frame samples fresh camera and world-member logical position");
                auto* engine=world_block+0x400;
                word(engine,0xfc,address(camera.data())+4);
                capture_scene();
                const auto unmatched=a2fo::native11_capture::latest_world_views();
                need(unmatched&&a2fo::native11_capture::latest_world_models()&&
                     unmatched->views.members.size()==2&&
                     unmatched->views.members[0].status==a2fo::native11::WorldViewStatus::view_unavailable&&
                     unmatched->views.members[1].status==a2fo::native11::WorldViewStatus::view_unavailable,
                     "camera mismatch leaves explicit unavailable rows without geometry claim");
                word(engine,0xfc,address(camera.data()));
                capture_scene();
                need(a2fo::native11_capture::latest_world_views()!=nullptr,
                     "restored active camera recovers fresh view rows");
            }
            if(world_preparations){
                const auto preparation=a2fo::native11_capture::latest_world_preparations();
                const std::array<bool,5> hidden{{false,true,true,true,false}};
                need(preparation&&preparation->models==world_snapshot&&
                     preparation->preparations.members.size()==2&&
                     preparation->preparations.members[0].status==
                         a2fo::native11::WorldPreparationStatus::captured&&
                     preparation->preparations.members[1].status==
                         a2fo::native11::WorldPreparationStatus::captured&&
                     preparation->preparations.members[0].preparation.model==pin_current_model(db,name)&&
                     preparation->preparations.members[1].preparation.model==pin_current_model(db,name)&&
                     preparation->preparations.members[0].preparation.state.systems_hidden==hidden&&
                     preparation->preparations.members[1].preparation.state.systems_hidden==hidden&&
                     preparation->preparations.members[0].preparation.state.borg_variant&&
                     preparation->preparations.members[1].preparation.state.borg_variant&&
                     preparation->preparations.members[0].preparation.state.hull_fraction==.25f&&
                     preparation->preparations.members[1].preparation.state.hull_fraction==.25f&&
                     observed->frame().object_count==1,
                     "world preparation pins both logical actors including unobserved second member");
                altered_byte(base+0xcb390+8,[&]{
                    need(!a2fo::native11_capture::latest_world_preparations()&&
                         a2fo::native11_capture::latest_world_models()==world_snapshot,
                         "setup body loss revokes only world preparation getter");
                });
                altered_byte(base+0xaf100,[&]{
                    need(!a2fo::native11_capture::latest_world_preparations()&&
                         a2fo::native11_capture::latest_world_models()==world_snapshot,
                         "hull getter loss revokes world preparation getter");
                });
                need(a2fo::native11_capture::latest_world_preparations()==preparation,
                     "restored preparation profile retains immutable current frame");
                first_systems[0x30]=0;
                capture_scene();
                const auto fresh=a2fo::native11_capture::latest_world_preparations();
                need(fresh&&fresh!=preparation&&
                     !fresh->preparations.members[0].preparation.state.systems_hidden[1]&&
                     fresh->preparations.members[1].preparation.state.systems_hidden[1]&&
                     preparation->preparations.members[0].preparation.state.systems_hidden[1],
                     "new frame copies fresh logical systems while old owner remains immutable");
            }
            if(world_borg){
                const auto operations=a2fo::native11_capture::latest_world_borg_materials();
                const auto preparation=a2fo::native11_capture::latest_world_preparations();
                need(operations&&operations->preparations==preparation&&operations->materials.members.size()==2&&
                     operations->materials.members[0].status==a2fo::native11::WorldBorgMaterialStatus::correlated&&
                     operations->materials.members[1].status==a2fo::native11::WorldBorgMaterialStatus::correlated&&
                     operations->materials.members[0].writes.size()==1&&operations->materials.members[0].writes[0].slot==0&&
                     operations->materials.members[0].writes[0].texture==material_state.alternate->texture&&
                     operations->materials.members[1].writes.size()==1&&observed->frame().object_count==1,
                     "Core correlates original operations for observed and unobserved actors without current slots");
                need(operations->materials.members[0].route_valid&&
                     operations->materials.members[0].route.selector_reached,
                     "Core material operations retain qualified base Craft route");
                word(class_storage.data(),0x1dc,0);
                need(!a2fo::native11_capture::latest_world_borg_materials()&&
                     a2fo::native11_capture::latest_world_preparations()==preparation,
                     "changed primary database revokes route without revoking logical preparation");
                word(class_storage.data(),0x1dc,db);
                need(a2fo::native11_capture::latest_world_borg_materials()==operations,
                     "exact captured route restored for current frame");
                borg_table[1]=material_state.alternate->texture;
                need(!a2fo::native11_capture::latest_world_borg_materials(),
                     "in-place material table row change revokes current snapshot");
                borg_table[1]=material_state.original->texture;
                word(class_storage.data(),0x424,0);
                need(!a2fo::native11_capture::latest_world_borg_materials(),
                     "in-place material table count change revokes current snapshot");
                word(class_storage.data(),0x424,1);word(class_storage.data(),0x428,1);
                need(!a2fo::native11_capture::latest_world_borg_materials(),
                     "unreadable replacement material table rejected");
                word(class_storage.data(),0x428,address(borg_table.data()));
                word(native_nodes[3].data(),0x13c,material_state.original->texture);
                need(!a2fo::native11_capture::latest_world_borg_materials(),
                     "in-place Borg alternate change revokes current snapshot");
                word(native_nodes[3].data(),0x13c,material_state.alternate->texture);
                need(a2fo::native11_capture::latest_world_borg_materials()==operations,
                     "unchanged table, alternate and route retain immutable snapshot");
                const auto data=material_state.baseline->data;
                auto replacement=std::make_shared<a2fo::native11::ModelMaterialBaseline>(*material_state.baseline);
                replacement->data=std::make_shared<const a2fo::native11::ModelMaterialBaselineData>(*data);
                material_state.baseline=replacement;
                need(!a2fo::native11_capture::latest_world_borg_materials()&&
                     a2fo::native11_capture::latest_world_preparations()==preparation,
                     "baseline owner replacement revokes only material operations");
                replacement->data=data;
                need(a2fo::native11_capture::latest_world_borg_materials()==operations,
                     "fresh wrapper with original baseline tuple remains valid");
                material_state.textures_available=false;
                need(!a2fo::native11_capture::latest_world_borg_materials(),"retired texture revokes material operations");
                material_state.textures_available=true;
                const auto original=material_state.original;
                auto reused=std::make_shared<a2fo::native11::TextureAsset>(*original);++reused->serial;
                material_state.original=reused;
                need(!a2fo::native11_capture::latest_world_borg_materials(),"same texture address with new version is rejected");
                material_state.original=original;
                altered_byte(base+0xbfb70,[&]{
                    need(!a2fo::native11_capture::latest_world_borg_materials()&&
                         a2fo::native11_capture::latest_world_preparations()==preparation,
                         "named operation code loss revokes only candidate material snapshot");
                },PAGE_EXECUTE_READWRITE);
                bool foreign=true;HANDLE worker=CreateThread(nullptr,0,foreign_borg_read,&foreign,0,nullptr);
                need(worker&&WaitForSingleObject(worker,5000)==WAIT_OBJECT_0,"foreign material getter worker completes");
                CloseHandle(worker);need(!foreign,"foreign thread cannot obtain world material operations");
                const auto old_data=operations->materials.members[0].baseline->data;
                before_publish_mode=5;capture_scene();
                need(!a2fo::native11_capture::latest_world_borg_materials()&&
                     a2fo::native11_capture::latest_world_preparations(),"late baseline loss discards only candidate materials");
                material_state.available=true;
                need(!a2fo::native11_capture::latest_world_borg_materials(),"discarded material publication does not resurrect");
                capture_scene();need(a2fo::native11_capture::latest_world_borg_materials()&&
                     operations->materials.members[0].baseline->data==old_data,"fresh frame republishes without mutating retained snapshot");
                auto non_borg_race=race_storage;
                word(second_entity.data(),0xfc,address(non_borg_race.data()));capture_scene();
                const auto independent=a2fo::native11_capture::latest_world_borg_materials();
                need(independent&&independent->materials.members[0].writes.size()==1&&
                     independent->materials.members[1].writes.size()==2&&
                     independent->materials.members[1].writes[0].slot==3&&
                     independent->materials.members[1].writes[0].texture==material_state.original->texture&&
                     operations->materials.members[1].writes.size()==1,
                     "fresh per-actor Borg race chooses independent operations while retained frame stays immutable");
                word(second_entity.data(),0xfc,address(race_storage.data()));capture_scene();
            }
            if(world_visibility){
                const auto visibility=a2fo::native11_capture::latest_world_visibility();
                const auto preparation=a2fo::native11_capture::latest_world_preparations();
                need(visibility&&preparation&&visibility->preparations==preparation&&
                     visibility->preparations->models==a2fo::native11_capture::latest_world_models()&&
                     visibility->visibility.members.size()==2&&
                     visibility->visibility.members[0].status==
                         a2fo::native11::WorldPreparedVisibilityStatus::captured&&
                     visibility->visibility.members[1].status==
                         a2fo::native11::WorldPreparedVisibilityStatus::captured&&
                     visibility->visibility.members[0].capture.preparation&&
                     visibility->visibility.members[1].capture.preparation&&
                     visibility->visibility.members[0].capture.baseline->data==template_state.data&&
                     visibility->visibility.members[1].capture.baseline->data==template_state.data&&
                     observed->frame().object_count==1,
                     "base visibility pins same frame/preparation/template tuple for observed and unobserved actors");
                namespace vp=a2fo::native11_craft_visibility_base::profile;
                // CB0E0 and parent setup CB390 share a page. Removing execute
                // permission intentionally revokes both admission domains.
                altered_byte(base+vp::regions[0].rva,[&]{
                    need(!a2fo::native11_capture::latest_world_visibility()&&
                         !a2fo::native11_capture::latest_world_preparations(),
                         "non-executable shared code page revokes child and parent");
                });
                altered_byte(base+vp::regions[0].rva,[&]{
                    const auto child=a2fo::native11_capture::latest_world_visibility();
                    const auto parent=a2fo::native11_capture::latest_world_preparations();
                    need(!child,"base visibility code loss revokes visibility getter");
                    need(parent==preparation,"base visibility code loss preserves logical preparation getter");
                },PAGE_EXECUTE_READWRITE);
                auto* synthetic_engine=world_block+0x400;
                auto* synthetic_context=world_block+0x800;
                word(synthetic_context,0x40,0);
                need(!a2fo::native11_capture::latest_world_visibility()&&
                     a2fo::native11_capture::latest_world_preparations()==preparation,
                     "context quality drift revokes visibility getter");
                word(synthetic_context,0x40,1);
                word(synthetic_engine,0x104,0);
                need(!a2fo::native11_capture::latest_world_visibility(),
                     "context pointer drift revokes visibility getter");
                word(synthetic_engine,0x104,address(synthetic_context));
                const auto original_data=template_state.data;
                template_state.data=std::make_shared<a2fo::native11::ModelNodeTemplateData>(*original_data);
                need(!a2fo::native11_capture::latest_world_visibility()&&
                     a2fo::native11_capture::latest_world_preparations()==preparation,
                     "template data owner loss revokes only visibility getter");
                template_state.data=original_data;
                template_state.available=false;
                need(!a2fo::native11_capture::latest_world_visibility()&&
                     a2fo::native11_capture::latest_world_preparations()==preparation,
                     "missing current template revokes visibility getter");
                template_state.available=true;
                need(a2fo::native11_capture::latest_world_visibility()==visibility,
                     "restored source tuple retains current immutable visibility frame");
                ForeignVisibilityRead foreign;
                HANDLE worker=CreateThread(nullptr,0,foreign_visibility_read,&foreign,0,nullptr);
                need(worker&&WaitForSingleObject(worker,5000)==WAIT_OBJECT_0,
                     "foreign visibility getter worker completed");
                CloseHandle(worker);
                need(!foreign.result&&a2fo::native11_capture::latest_world_visibility()==visibility,
                     "foreign thread cannot obtain current base visibility");
                const float second_hull=75.f;
                std::memcpy(second_entity.data()+0x1dc,&second_hull,4);
                capture_scene();
                const auto independent=a2fo::native11_capture::latest_world_visibility();
                need(independent&&independent!=visibility&&
                     independent->visibility.members[0].capture.input.hull_fraction==.25f&&
                     independent->visibility.members[1].capture.input.hull_fraction==.375f&&
                     !independent->visibility.members[0].capture.input.system_hidden[1]&&
                     independent->visibility.members[1].capture.input.system_hidden[1]&&
                     independent->visibility.members[0].policy.flags[7]==0x80&&
                     independent->visibility.members[1].policy.flags[7]==0x82&&
                     visibility->visibility.members[1].capture.input.hull_fraction==.25f,
                     "new frame evaluates independent logical systems and hull without rewriting old owner");
                const float original_hull=50.f;
                std::memcpy(second_entity.data()+0x1dc,&original_hull,4);
                first_systems[0x30]=1;
                capture_scene();
                need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                     "restored logical inputs recover fresh visibility capture");
                before_publish_mode=4;capture_scene();
                need(!a2fo::native11_capture::latest_world_visibility()&&
                     a2fo::native11_capture::latest_world_preparations()!=nullptr,
                     "template loss before publication discards only visibility");
                template_state.available=true;
                need(!a2fo::native11_capture::latest_world_visibility(),
                     "discarded visibility does not resurrect on template restoration");
                capture_scene();
                need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                     "fresh frame after template restoration republishes visibility");
            }
            // The view cases above publish newer frames; compare restoration
            // with the current owner rather than the initial historical frame.
            world_snapshot=a2fo::native11_capture::latest_world_models();
            altered_byte(base+scene_profile::regions[0].rva,[&]{
                need(!a2fo::native11_capture::latest_world_models(),
                     "loaded placement profile loss revokes published association getter");
                if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                           "model profile loss revokes world preparation getter");
                if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                         "model profile loss revokes base visibility getter");
            });
            altered_byte(base+0x22ce4e,[&]{
                need(!a2fo::native11_capture::latest_world_models(),
                     "model load owner loss revokes published association getter");
                if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                           "model load owner loss revokes world preparation getter");
                if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                         "model load owner loss revokes base visibility getter");
            });
            need(a2fo::native11_capture::latest_world_models()==world_snapshot,
                 "restored profiles retain immutable historical association");
            a2fo::native11_world_pipeline::invalidate_sources();
            need(!a2fo::native11_capture::latest_world_models(),
                 "source ticket change revokes world model getter");
            if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                                "source ticket change revokes world view getter");
            if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                       "source ticket change revokes world preparation getter");
            if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                     "source ticket change revokes base visibility getter");
            capture_scene();
            need(a2fo::native11_capture::latest_world_models()!=nullptr,
                 "fresh source ticket recovers world model snapshot");
            if(world_preparations)need(a2fo::native11_capture::latest_world_preparations()!=nullptr,
                                       "fresh source ticket recovers world preparation snapshot");
            if(world_visibility)need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                                     "fresh source ticket recovers base visibility snapshot");
            const auto class_before=a2fo::native11_light_classes::generation();
            need(a2fo::native11_light_classes::observe_class(
                class_storage.data(),"fixture.odf",{})&&
                a2fo::native11_light_classes::generation()>class_before&&
                !a2fo::native11_capture::latest_world_models(),
                "class generation change revokes world model getter");
            if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                                "class generation change revokes world view getter");
            if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                       "class generation change revokes world preparation getter");
            if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                     "class generation change revokes base visibility getter");
            capture_scene();
            need(a2fo::native11_capture::latest_world_models()!=nullptr,
                 "new class generation recovers world model snapshot");
            if(world_preparations)need(a2fo::native11_capture::latest_world_preparations()!=nullptr,
                                       "new class generation recovers world preparation snapshot");
            if(world_visibility)need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                                     "new class generation recovers base visibility snapshot");
            a2fo::native11_world_lights::disable();
            need(!a2fo::native11_capture::latest_world_models(),
                 "world-light disable revokes world model getter");
            if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                                "world-light disable revokes world view getter");
            if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                       "world-light disable revokes world preparation getter");
            if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                     "world-light disable revokes base visibility getter");
            need(a2fo::native11_world_lights::initialize(image,true,log)&&
                 !a2fo::native11_capture::latest_world_models(),
                 "world-light reinitialize does not revive old snapshot");
            capture_scene();
            need(a2fo::native11_capture::latest_world_models()!=nullptr,
                 "fresh world-light lifecycle captures new association");
            if(world_preparations)need(a2fo::native11_capture::latest_world_preparations()!=nullptr,
                                       "fresh world-light lifecycle captures new preparation");
            if(world_visibility)need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                                     "fresh world-light lifecycle captures new base visibility");
        }
        before_publish_mode=1;capture_scene();
        need(!a2fo::native11_capture::latest_bound(),"disable racing frame end prevents late publication");
        if(world_models)need(!a2fo::native11_capture::latest_world_models(),
                             "disable racing frame end prevents world model publication");
        if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                            "disable racing frame end prevents world view publication");
        if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                   "disable racing frame end prevents world preparation publication");
        if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                 "disable racing frame end prevents base visibility publication");
        need(a2fo::native11_capture::initialize(image,true,log)&&!a2fo::native11_capture::latest_bound(),
             "reenable cannot resurrect discarded in-flight capture");
        before_publish_mode=2;capture_scene();
        need(!a2fo::native11_capture::latest_bound(),"disable then reenable before publication rejects old lifecycle");
        if(world_models)need(!a2fo::native11_capture::latest_world_models(),
                             "rearm during frame end cannot publish old world association");
        if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                            "rearm during frame end cannot publish old world view");
        if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                   "rearm during frame end cannot publish old world preparation");
        if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                 "rearm during frame end cannot publish old base visibility");
        capture_scene();
        need(a2fo::native11_capture::latest_bound()!=nullptr,"fresh lifecycle can publish after restart");
        if(world_models)need(a2fo::native11_capture::latest_world_models()!=nullptr,
                             "fresh capture lifecycle publishes world association");
        if(world_views)need(a2fo::native11_capture::latest_world_views()!=nullptr,
                            "fresh capture lifecycle publishes world views");
        if(world_preparations)need(a2fo::native11_capture::latest_world_preparations()!=nullptr,
                                   "fresh capture lifecycle publishes world preparations");
        if(world_visibility)need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                                 "fresh capture lifecycle publishes base visibility");
        a2fo::native11_capture::disable();
        if(world_models)need(!a2fo::native11_capture::latest_world_models(),
                             "capture disable revokes world association");
        if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                            "capture disable revokes world views");
        if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                   "capture disable revokes world preparations");
        if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                 "capture disable revokes base visibility");
        const auto pinned=pin_model(db,result.serial);
        need(pinned&&pinned->bytes==original,"retirement fixture pins current model owner");
        const auto before_retire_epoch=world_models?generation():0;
        bool retired=false;
        if(world_models){
            need(a2fo::native11_capture::initialize(image,true,log),"rearm model retirement timing case");
            capture_scene();need(a2fo::native11_capture::latest_world_models()!=nullptr,
                                 "live association exists before timed retirement");
            if(world_views)need(a2fo::native11_capture::latest_world_views()!=nullptr,
                                "live world views exist before timed retirement");
            if(world_preparations)need(a2fo::native11_capture::latest_world_preparations()!=nullptr,
                                       "live world preparations exist before timed retirement");
            if(world_visibility)need(a2fo::native11_capture::latest_world_visibility()!=nullptr,
                                     "live base visibility exists before timed retirement");
            retire_before_publish_db=db;before_publish_mode=3;capture_scene();retired=true;
            need(!a2fo::native11_capture::latest_world_models(),
                 "retirement between binding and publication cannot publish world association");
            if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                                "retirement between binding and publication cannot publish world views");
            if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                       "retirement between binding and publication cannot publish world preparations");
            if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                     "retirement between binding and publication cannot publish base visibility");
        }else retired=retire_model(db);
        need(retired&&!retire_model(db)&&!retire_model(db+4),
             "captured model retires once; absent database retirement is inert");
        if(world_models)need(generation()>before_retire_epoch&&
                             !pin_current_model(db,name)&&
                             !a2fo::native11_capture::latest_world_models(),
                             "model retirement advances generation and revokes world association");
        if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                            "model retirement revokes world view getter");
        if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                   "model retirement revokes world preparation getter");
        if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                 "model retirement revokes base visibility getter");
        const auto after_retire_epoch=world_models?generation():0;
        need(!model_info(db,info)&&!pin_model(db,result.serial)&&
             !read_model(db,result.serial,sink,&result)&&pinned->bytes==original,
             "retirement hides current info, pin and read while held owner remains readable");
        a2fo_native11_observe_database(database.data(),stream.data(),0x7fc01234u,name,0x1027fu);
        a2fo::native11::ModelAssetInfo after_retire{};
        need(model_info(db,after_retire)&&after_retire.serial>result.serial&&
             pin_model(db,after_retire.serial)&&!pin_model(db,result.serial),
             "reused database pointer receives a fresh immutable model version");
        if(world_models)need(generation()>after_retire_epoch&&
                             pin_current_model(db,name)==pin_model(db,after_retire.serial)&&
                             !a2fo::native11_capture::latest_world_models(),
                             "reused model owner is current but cannot revive old world association");
        word(stream.data(),0,numeric+0x2bc874+4);
        a2fo_native11_observe_database(database.data(),stream.data(),0,name);
        need(!model_info(db,info)&&!read_model(db,result.serial,sink,&result),
             "invalid replacement retires old binding");
        need(!bind_frame(*frame,bound,error)&&bound==retained,"stale frame binding rejected atomically");
        SinkState retained_result{};
        const auto* opaque=bound->frame().objects[0].model_name;
        need(a2fo::native11::BoundCapture::read_asset(const_cast<a2fo::native11::BoundCapture*>(bound.get()),1,
             reinterpret_cast<const std::uint8_t*>(opaque),std::strlen(opaque),64u*1024*1024,sink,&retained_result)==1&&
             retained_result.bytes==original,"retained owner survives registry invalidation");
        word(stream.data(),0,numeric+0x2bc874);
        word(stream.data(),0x1c,address(source.data())+source.size()+1);
        a2fo_native11_observe_database(database.data(),stream.data(),0,name);
        need(!model_info(db,info),"out-of-span parser cursor rejected");
        word(stream.data(),0x1c,address(source.data()+1));
        word(database.data(),0,numeric+0x2bc56c+4);
        a2fo_native11_observe_database(database.data(),stream.data(),0,name);
        need(!model_info(db,info),"wrong database class rejected");
        word(database.data(),0,numeric+0x2bc56c);
        char wrong[]="other.sod";
        word(database.data(),0x38,address(wrong));
        a2fo_native11_observe_database(database.data(),stream.data(),0,name);
        need(!model_info(db,info),"database name mismatch rejected");
        word(database.data(),0x38,address(name));
        a2fo_native11_observe_database(database.data(),stream.data(),0x80000001u,name);
        need(model_info(db,info)&&info.serial>result.serial&&
             info.parser_scalar_bits==0x80000001u,"fresh binding receives new serial");
        need(!read_model(db,result.serial,sink,&result),"stale serial rejected after reuse");
        if(fleet){
            namespace f=a2fo::native11_fofs_stream::profile;
            auto* callback=reinterpret_cast<std::uint8_t*>(fleet)+f::fopen_callback.rva;
            const auto prior_serial=info.serial;
            altered_byte(callback,[&]{
                need(!owns_load_call(image),"mutated Fleet callback revokes source ownership");
                a2fo_native11_observe_database(database.data(),stream.data(),0x80000001u,name);
                need(!model_info(db,info),"reused database invalidated before FOFS refusal");
            });
            a2fo_native11_observe_database(database.data(),stream.data(),0x80000001u,name);
            need(model_info(db,info)&&info.serial>prior_serial,
                 "restored Fleet callback permits fresh database publication");
        }
        disable();
        if(world_models)need(generation()==0&&!pin_current_model(db,name)&&
                             !a2fo::native11_capture::latest_world_models(),
                             "asset disable removes current source generation and association");
        if(world_views)need(!a2fo::native11_capture::latest_world_views(),
                            "asset disable removes current world views");
        if(world_preparations)need(!a2fo::native11_capture::latest_world_preparations(),
                                   "asset disable removes current world preparations");
        if(world_visibility)need(!a2fo::native11_capture::latest_world_visibility(),
                                 "asset disable removes current base visibility");
        need(!model_info(db,info)&&!read_model(db,info.serial,sink,&result),
             "disable clears binding and blocks reads");
        need(!bind_frame(*frame,bound,error)&&bound==retained,"disabled capture cannot bind and preserves caller owner");
        need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
             !GetModuleHandleA("D3DX81ab.dll"),"static mapping resolved no graphics imports");
        if(fleet){
            namespace f=a2fo::native11_fofs_stream::profile;
            auto* site=base+f::open_call_rva;
            auto* callback=reinterpret_cast<std::uint8_t*>(fleet)+f::fopen_callback.rva;
            need(initialize_assets()&&owns_load_call(image),
                 "disabled Fleet model observer reactivates with both calls owned");
            disable();
            altered_byte(site+1,[&]{
                need(!initialize_assets()&&!owns_load_call(image),
                     "foreign Root fopen target refuses reactivation");
            });
            need(initialize_assets()&&owns_load_call(image),"restored Root fopen route recovers");
            disable();
            altered_byte(callback,[&]{
                need(!initialize_assets()&&!owns_load_call(image),
                     "mutated Fleet callback refuses reactivation");
            });
            need(initialize_assets()&&owns_load_call(image),"restored Fleet callback recovers");
            disable();
            need(!initialize(image,true,log,nullptr)&&!owns_load_call(image),
                 "missing Fleet owner refuses reactivation");
            need(!initialize(image,true,log,GetModuleHandleA(nullptr))&&!owns_load_call(image),
                 "wrong Fleet owner refuses reactivation");
            need(initialize_assets()&&owns_load_call(image),"correct Fleet owner recovers");
            disable();
            mapped_bytes(site,original_fopen.data(),original_fopen.size());
            mapped_word(base,0x22ce4e,owned_operand,call_operand);
            for(const auto& fix:fleet_fixups)mapped_bytes(fix.first,&fix.second,4);
            // qualify() pins Fleet for process lifetime; no unload assertion.
            FreeLibrary(fleet);
        }
        if(world_models){
            a2fo::native11_world_lights::disable();
            a2fo::native11_light_classes::disable();
            if(world_visibility){
                a2fo::native11_capture::set_world_template_source_for_testing(nullptr,nullptr);
                active_template_state=nullptr;
            }
            if(world_borg){a2fo::native11_capture::set_world_material_sources_for_testing(nullptr,nullptr,nullptr,nullptr);
                active_material_state=nullptr;}
            for(auto it=visibility_globals.rbegin();it!=visibility_globals.rend();++it)
                mapped_bytes(it->first,it->second.data(),it->second.size());
            for(auto it=visibility_fixups.rbegin();it!=visibility_fixups.rend();++it)
                mapped_bytes(it->first,&it->second,4);
            for(auto it=view_globals.rbegin();it!=view_globals.rend();++it)
                mapped_bytes(it->first,it->second.data(),it->second.size());
            for(auto it=view_fixups.rbegin();it!=view_fixups.rend();++it)
                mapped_bytes(it->first,&it->second,4);
            for(auto it=preparation_globals.rbegin();it!=preparation_globals.rend();++it)
                mapped_bytes(it->first,it->second.data(),it->second.size());
            for(auto it=preparation_fixups.rbegin();it!=preparation_fixups.rend();++it)
                mapped_bytes(it->first,&it->second,4);
            mapped_bytes(base+0x361084,&old_world_list,4);
            for(const auto& fix:world_fixups)mapped_bytes(fix.first,&fix.second,4);
            mapped_word(base,0x22ce4e,owned_operand,call_operand);
            need(VirtualFree(world_block,0,MEM_RELEASE)!=0,"private world list released");
        }
        std::puts("PASS native11 asset guard, mock install, owned observation and invalidation");
    } else need(argc==1,"usage: native11_asset_guard_test.exe [PRIVATE_PINNED_ENGINE_EXE]");
    disable();
    std::puts("PASS native11 asset default-off and unreviewed-executable guard");
}
