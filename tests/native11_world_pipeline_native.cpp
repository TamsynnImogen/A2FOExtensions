// Private-root, public-helper end-to-end test for the optional offscreen world pipeline.
#include "../core/native11_world_pipeline.hpp"
#include "../core/native11_light_classes.hpp"
#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/world_lighting.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo;
bool revoke_after_submit=false;
bool revoke_after_diagnostic_copy=false;
extern "C" void a2fo_native11_world_pipeline_after_diagnostic_copy() noexcept {
    if(revoke_after_diagnostic_copy){revoke_after_diagnostic_copy=false;native11_world_pipeline::invalidate_sources();}
}
extern "C" void a2fo_native11_world_pipeline_after_submit() noexcept {
    if(revoke_after_submit){revoke_after_submit=false;native11_world_pipeline::invalidate_sources();}
}
namespace {
void need(bool good,const char* label) {
    if (!good) { std::fprintf(stderr,"FAIL %s\n",label);std::exit(1); }
}
struct PrivateRoot {
    std::string path,dll,ini;
    explicit PrivateRoot(const char* source) {
        char temp[MAX_PATH]{};need(GetTempPathA(MAX_PATH,temp)>0,"temp path");
        char unique[MAX_PATH]{};need(GetTempFileNameA(temp,"n11",0,unique)!=0,"unique temp name");
        need(DeleteFileA(unique)!=0&&CreateDirectoryA(unique,nullptr)!=0,"private temp directory");
        path=unique;dll=path+"\\A2FONative11Renderer.dll";ini=path+"\\A2FORenderer.ini";
        need(CopyFileA(source,dll.c_str(),TRUE)!=0,"copy helper into private root");
        need(WritePrivateProfileStringA("Native11","OffscreenWorld","1",ini.c_str())!=0&&
             WritePrivateProfileStringA("Native11","WorldAmbient",".1 .1 .1",ini.c_str())!=0,
             "write private world configuration");
    }
    bool remove() {
        const bool removed=DeleteFileA(ini.c_str())&&DeleteFileA(dll.c_str())&&
            RemoveDirectoryA(path.c_str());
        if (removed) path.clear();
        return removed;
    }
    ~PrivateRoot() {
        if (!path.empty()) {
            DeleteFileA(ini.c_str());DeleteFileA(dll.c_str());RemoveDirectoryA(path.c_str());
        }
    }
};
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r) {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=b;bytes[19]=g;bytes[20]=r;return bytes;
}
struct Assets {
    std::shared_ptr<const native11::ModelAsset> model;
    std::shared_ptr<const native11::TextureAsset> white,blue;
    Assets() {
        auto m=std::make_shared<native11::ModelAsset>();m->info.database=7;m->info.serial=11;
        std::strcpy(m->info.name,"world.sod");m->bytes=native11_fixture::model("white.tga");
        m->info.byte_count=static_cast<std::uint32_t>(m->bytes.size());model=m;
        auto w=std::make_shared<native11::TextureAsset>();w->serial=31;w->texture=41;
        w->logical_name="white.tga";
        w->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(255,255,255));white=w;
        auto b=std::make_shared<native11::TextureAsset>();b->serial=32;b->texture=42;
        b->logical_name="blue.tga";
        b->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(255,0,0));blue=b;
    }
    static std::shared_ptr<const native11::ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial) {
        auto& a=*static_cast<Assets*>(raw);
        return a.model&&a.model->info.database==db&&a.model->info.serial==serial?a.model:nullptr;
    }
};
void pose(native11::Object& object,float scale,float z) {
    for (int i=0;i<16;++i) object.world[i]=(i%5)==0?1.f:0.f;
    object.world[0]=object.world[5]=object.world[10]=scale;object.world[11]=z;
}
std::shared_ptr<const native11::BoundMaterialCapture> materials(
    Assets& assets,std::uint64_t generation,bool all,bool mismatch=false) {
    auto input=std::make_unique<native11::Frame>();
    input->camera=1;input->generation=generation;input->object_count=all?2:1;
    for (int i=0;i<16;++i)input->view.world_to_clip[i]=(i%5)==0?1.f:0.f;
    input->view.viewport[2]=input->view.viewport[3]=32;
    std::map<std::uint32_t,std::shared_ptr<const native11::MaterialObservation>> observed;
    for (std::size_t i=0;i<input->object_count;++i) {
        auto& object=input->objects[i];object.instance=static_cast<std::uint32_t>(101+i);
        object.entity=static_cast<std::uint32_t>(201+i);object.database=7;object.model_serial=11;
        object.model_identity=native11::ModelIdentity::plain_database_name;
        object.placement_identity=native11::PlacementIdentity::authored_placement;
        object.shadow_only=i==1;std::strcpy(object.model_name,"world.sod");
        pose(object,i? .2f:1.f,i? .26f:(mismatch? .5f:.6f));
        auto part=std::make_shared<native11::MaterialObservation>();
        part->instance=object.instance;part->database=7;part->model_serial=11;
        part->model=assets.model;
        native11::MaterialBinding binding{};binding.authored_texture="white.tga";
        binding.texture=i?assets.blue:assets.white;binding.node_state_valid=true;
        part->parts.push_back(binding);observed.emplace(part->instance,part);
    }
    std::shared_ptr<const native11::BoundMaterialCapture> out;std::string error;
    need(native11::build_bound_material_capture(*input,20,Assets::resolve,&assets,observed,out,error),
         "owned material capture");
    need(out->frame().requires_part_states&&out->source_epoch()==20,"observed part state and source epoch");
    return out;
}
std::shared_ptr<const native11_world_lights::Snapshot> lighting(std::uint64_t generation,
    const std::shared_ptr<const native11::LightClassRecord>& record) {
    auto value=std::make_shared<native11_world_lights::Snapshot>();
    value->camera=1;value->frame_generation=generation;
    value->class_generation=native11_light_classes::generation();
    native11::PlacedLight source{};source.source_id=1;source.config=record->config;
    source.source_id=201;
    value->captured.sources.push_back(source);value->captured.class_owners.push_back(record);
    value->captured.members.push_back({201,101,static_cast<std::uint32_t>(record->class_id),record->version});
    const auto object_record=native11_light_classes::pin(0x4321);need(object_record!=nullptr,"owned object class");
    native11::PlacedLight disabled{};disabled.source_id=202;disabled.config=object_record->config;
    value->captured.sources.push_back(disabled);value->captured.class_owners.push_back(object_record);
    value->captured.members.push_back({202,102,static_cast<std::uint32_t>(object_record->class_id),object_record->version});
    const float ambient[3]{.1f,.1f,.1f};std::string error;
    need(native11::assemble_world_lighting(value->captured.sources,true,ambient,value->lighting,error)&&
         value->lighting.shadows.light_mask==1,"owned red world lighting");
    return value;
}
std::shared_ptr<const native11_world_lights::Snapshot> diagnostic_lighting(
    std::uint64_t generation,const std::shared_ptr<const native11::LightClassRecord>& owner) {
    auto value=std::make_shared<native11_world_lights::Snapshot>();
    value->camera=1;value->frame_generation=generation;
    value->class_generation=native11_light_classes::generation();
    for(const native11::WorldMember member:{native11::WorldMember{201,101,
            static_cast<std::uint32_t>(owner->class_id),owner->version},
            native11::WorldMember{202,102,static_cast<std::uint32_t>(owner->class_id),owner->version}}){
        value->captured.members.push_back(member);value->captured.class_owners.push_back(owner);
        native11::PlacedLight source{};source.source_id=member.entity;source.config=owner->config;
        value->captured.sources.push_back(source);
    }
    const float ambient[3]{.1f,.1f,.1f};std::string error;
    need(native11::assemble_world_lighting(value->captured.sources,true,ambient,
         value->lighting,error)&&value->lighting.lights.empty(),"owned diagnostic membership");
    return value;
}
struct Provider {
    std::shared_ptr<const native11::BoundMaterialCapture> all,mismatch;
    enum class Mode {incomplete,wrong_primary,valid,failed} mode=Mode::incomplete;
    bool reentry_refused=false;
    static bool candidates(void* raw,const native11::BoundMaterialCapture&,
        const native11_world_lights::Snapshot&,native11_world_pipeline::CandidateGeometry& out,std::string&) {
        auto& p=*static_cast<Provider*>(raw);
        if(p.mode==Mode::failed)return false;
        p.reentry_refused=!native11_world_pipeline::shutdown()&&
            !native11_world_pipeline::set_candidate_provider(candidates,&p);
        out.shadow_candidates={p.mode==Mode::wrong_primary?p.mismatch:p.all};
        out.absent_domains=p.mode==Mode::incomplete?native11::DomainEvidence{}:native11::DomainEvidence{true,true,true};
        return true;
    }
    static bool give(void* raw,const native11::BoundMaterialCapture&,
        const native11_world_lights::Snapshot&,native11_world_pipeline::Geometry& out,
        std::string&) {
        auto& p=*static_cast<Provider*>(raw);
        if(p.mode==Mode::failed)return false;
        p.reentry_refused=!native11_world_pipeline::shutdown()&&
            !native11_world_pipeline::set_geometry_provider(give,&p);
        out.all=p.mode==Mode::wrong_primary?p.mismatch:p.all;
        out.complete_casters=p.mode!=Mode::incomplete;
        out.complete_visibility=true;
        return true;
    }
};
int red(const std::vector<std::uint8_t>& image,unsigned x,unsigned y) {
    return image[(y*32+x)*4+2];
}
std::vector<std::uint8_t> pixels() {
    std::vector<std::uint8_t> bytes;std::uint32_t width=0,height=0;std::string error;
    need(native11_world_pipeline::readback(bytes,width,height,error)&&
         width==32&&height==32&&bytes.size()==32*32*4,"owned world readback");
    return bytes;
}
void check_status(native11_world_pipeline::State expected,std::uint64_t frame,
    std::uint64_t rendered,const char* name) {
    const auto state=native11_world_pipeline::status();
    need(state.state==expected&&state.frame==frame&&state.rendered_frames==rendered,name);
}
void run(const char* renderer_path,bool shared,bool candidate) {
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"helper initially unloaded");
    PrivateRoot root(renderer_path);
    if(shared)need(WritePrivateProfileStringA("Native11","ShareWorldGeometry","1",root.ini.c_str())!=0,"request API6 sharing");
    need(native11_world_pipeline::configure_from_roots({root.path},nullptr)&&
         native11_world_pipeline::requested(),"private root configuration");
    float configured_ambient[3]{};native11_world_pipeline::ambient(configured_ambient);
    need(configured_ambient[0]==.1f&&configured_ambient[1]==.1f&&
         configured_ambient[2]==.1f,"private ambient parsed");
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"configuration does not load GPU helper");
    native11_light_classes::configure(true,nullptr);
    native11_light_classes::dispatch_available(true);
    std::map<std::string,std::string> fields{{"lightSource","1"},{"lightType","directional"},
        {"lightColor","1 0 0"},{"lightDirection",".5 0 1"},
        {"lightIntensity",".6"},{"lightCastShadows","1"}};
    void* class_id=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234));
    need(native11_light_classes::observe_class(class_id,"private-sun.odf",fields)&&
         native11_light_classes::healthy(),"copied class producer");
    void* object_class=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4321));
    need(native11_light_classes::observe_class(object_class,"fixture-objects.odf",
         {{"lightSource","0"}}),"known diagnostic GameObject class");
    auto object_record=native11_light_classes::pin(0x4321);
    need(object_record&&!object_record->config.enabled,"pinned diagnostic GameObject class");
    auto record=native11_light_classes::pin(0x1234);
    need(record&&record->config.enabled&&record->version>0,"pinned real class version");
    Assets assets;
    auto visible=materials(assets,1,false),all=materials(assets,1,true);
    auto lights=lighting(1,record);
    auto diagnostic_lights=diagnostic_lighting(1,object_record);
    need(!native11_world_pipeline::on_frame(visible,diagnostic_lights),"no geometry provider refused");
    check_status(native11_world_pipeline::State::waiting_geometry,1,0,"waiting for geometry");
    const auto diagnostic=native11_world_pipeline::diagnostic_json();
    need(diagnostic.find("\"camera\":1")!=std::string::npos&&
         diagnostic.find("\"generation\":1")!=std::string::npos&&
         diagnostic.find("\"opaque_entity_id\":202")!=std::string::npos&&
         diagnostic.find("missing_game_object_geometry")!=std::string::npos&&
         diagnostic.find("unproven_game_features_absence")!=std::string::npos&&
         diagnostic.find("unproven_background_caster_absence")!=std::string::npos&&
         diagnostic.find("unproven_effect_caster_absence")!=std::string::npos&&
         diagnostic.find("\"observed_domain_ready\":false")!=std::string::npos,
         "owned missing-member diagnostic has all safety gates");
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"no-provider admission does not load helper");
    revoke_after_diagnostic_copy=true;
    need(native11_world_pipeline::diagnostic_json().empty()&&!revoke_after_diagnostic_copy,
         "source invalidation after diagnostic copy cannot return stale JSON");
    need(!native11_world_pipeline::on_frame(visible,diagnostic_lights)&&
         !native11_world_pipeline::diagnostic_json().empty(),"fresh diagnostic recovers after source revocation");
    need(native11_light_classes::observe_class(object_class,"fixture-objects.odf",
         {{"lightSource","0"}})&&native11_world_pipeline::diagnostic_json().empty(),
         "class generation change hides stale diagnostic");
    object_record=native11_light_classes::pin(0x4321);
    lights=lighting(1,record);
    const auto diagnostic_ticket=native11_world_pipeline::source_epoch();
    native11_world_pipeline::invalidate_sources();
    need(native11_world_pipeline::diagnostic_json().empty()&&
         !native11_world_pipeline::on_frame(visible,diagnostic_lights,diagnostic_ticket)&&
         native11_world_pipeline::diagnostic_json().empty(),
         "invalidation and stale ticket clear diagnostic");
    auto malformed_lights=std::make_shared<native11_world_lights::Snapshot>(*lights);
    malformed_lights->captured.members.clear();
    need(!native11_world_pipeline::on_frame(visible,malformed_lights)&&
         native11_world_pipeline::diagnostic_json().empty()&&
         native11_world_pipeline::status().reason.find("coverage audit unavailable")!=std::string::npos,
         "malformed membership gives bounded failure reason without stale report");
    Provider provider{all,materials(assets,1,true,true)};
    need((candidate?native11_world_pipeline::set_candidate_provider(Provider::candidates,&provider):native11_world_pipeline::set_geometry_provider(Provider::give,&provider)),"install checked provider");
    need(!native11_world_pipeline::on_frame(visible,lights),"incomplete provider refused");
    check_status(native11_world_pipeline::State::waiting_geometry,1,0,"incomplete geometry status");
    provider.mode=Provider::Mode::wrong_primary;
    need(!native11_world_pipeline::on_frame(visible,lights),"mismatched primary refused");
    check_status(native11_world_pipeline::State::waiting_geometry,1,0,"mismatch geometry status");
    need(provider.reentry_refused&&GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,
         "provider reentry refused before helper load");
    const auto expired_ticket=native11_world_pipeline::source_epoch();
    native11_world_pipeline::invalidate_sources();
    need(!native11_world_pipeline::on_frame(visible,lights,expired_ticket)&&GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,
         "expired native source ticket refuses before helper load");
    provider.mode=Provider::Mode::valid;
    need(native11_world_pipeline::on_frame(visible,lights),"valid complete world renders");
    check_status(native11_world_pipeline::State::rendered,1,1,"first rendered generation");
    need(GetModuleHandleA("A2FONative11Renderer.dll")!=nullptr,"private helper loaded for valid frame");
    auto first=pixels();
    need(red(first,16,16)>100&&red(first,20,16)>15&&red(first,20,16)<55&&
         red(first,11,16)>red(first,20,16)+60,
         "visible receiver and offscreen caster shadow pixels");
    need(!native11_world_pipeline::on_frame(visible,lights),"duplicate generation refused");
    need(native11_world_pipeline::status().rendered_frames==1&&pixels()==first,
         "duplicate frame preserves previous image");
    if(candidate){
        provider.mode=Provider::Mode::failed;native11_world_pipeline::invalidate_sources();
        need(!native11_world_pipeline::on_frame(visible,lights)&&
             native11_world_pipeline::status().state==native11_world_pipeline::State::waiting_geometry,
             "failed candidate provider cannot reuse prior assembled frame after revocation");
        std::vector<std::uint8_t> stale;std::uint32_t sw=0,sh=0;std::string why;
        need(!native11_world_pipeline::readback(stale,sw,sh,why),"failed candidate provider cannot expose old image");
        provider.mode=Provider::Mode::valid;
    }
    fields["lightDirection"]="-.5 0 1";
    need(native11_light_classes::observe_class(class_id,"private-sun.odf",fields),"new class version");
    record=native11_light_classes::pin(0x1234);
    need(record&&record->version>lights->captured.class_owners[0]->version,
         "new pinned class owner replaces old");
    visible=materials(assets,2,false);provider.all=materials(assets,2,true);
    provider.mismatch=materials(assets,2,true,true);lights=lighting(2,record);
    need(native11_world_pipeline::on_frame(visible,lights),"second generation renders");
    check_status(native11_world_pipeline::State::rendered,2,2,"second rendered generation");
    auto second=pixels();
    need(second!=first&&red(second,11,16)<red(second,20,16),"opposite light moves shadow");
    auto stale=lighting(3,record);
    need(native11_light_classes::observe_class(class_id,"private-sun.odf",fields),
         "class advances after snapshot");
    visible=materials(assets,3,false);provider.all=materials(assets,3,true);
    need(!native11_world_pipeline::on_frame(visible,stale),"stale class generation refused");
    check_status(native11_world_pipeline::State::waiting_sources,3,2,"stale source status");
    std::vector<std::uint8_t> rejected_pixels{9};std::uint32_t rejected_width=7,rejected_height=8;std::string rejected_error;
    need(!native11_world_pipeline::readback(rejected_pixels,rejected_width,rejected_height,rejected_error)&&
         rejected_pixels==std::vector<std::uint8_t>{9}&&rejected_width==7&&rejected_height==8,
         "stale class snapshot cannot expose pixels");
    record=native11_light_classes::pin(0x1234);
    visible=materials(assets,4,false);provider.all=materials(assets,4,true);lights=lighting(4,record);
    revoke_after_submit=true;
    need(!native11_world_pipeline::on_frame(visible,lights)&&!revoke_after_submit&&
         native11_world_pipeline::status().rendered_frames==2,
         "revocation after GPU submit prevents publication");
    need(!native11_world_pipeline::readback(rejected_pixels,rejected_width,rejected_height,rejected_error),
         "rendered but revoked frame cannot expose pixels");
    visible=materials(assets,5,false);provider.all=materials(assets,5,true);lights=lighting(5,record);
    need(native11_world_pipeline::on_frame(visible,lights)&&pixels()==second&&
         native11_world_pipeline::status().published_frame==5,
         "fresh frame recovers publication after revoked submit");
    need(native11_world_pipeline::shutdown(),"owner-thread helper shutdown");
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"private helper unloaded");
    need(WritePrivateProfileStringA("Native11","ShareWorldGeometry","bogus",root.ini.c_str())!=0,
         "write malformed sharing option");
    need(!native11_world_pipeline::configure_from_roots({root.path},nullptr)&&
         native11_world_pipeline::status().reason=="ShareWorldGeometry must be 0 or 1"&&
         GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"invalid sharing refuses helper load");
    need(WritePrivateProfileStringA("Native11","ShareWorldGeometry",shared?"1":"0",root.ini.c_str())!=0,
         "restore sharing option");
    need(WritePrivateProfileStringA("Native11","OffscreenWorld","bogus",root.ini.c_str())!=0,
         "write invalid private setting");
    need(!native11_world_pipeline::configure_from_roots({root.path},nullptr),
         "invalid setting refused");
    need((candidate?native11_world_pipeline::set_candidate_provider(Provider::candidates,&provider):native11_world_pipeline::set_geometry_provider(Provider::give,&provider)),
         "provider may be set while configuration unavailable");
    record=native11_light_classes::pin(0x1234);visible=materials(assets,6,false);
    provider.all=materials(assets,6,true);lights=lighting(6,record);
    need(!native11_world_pipeline::on_frame(visible,lights)&&
         native11_world_pipeline::status().state==native11_world_pipeline::State::unavailable&&
         GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,
         "configured gate prevents helper load after invalid setting");
    need(native11_world_pipeline::shutdown(),"final pipeline shutdown");
    native11_light_classes::disable();
    need(root.remove(),"private DLL, INI, and directory removed");
}
}
int main(int argc,char** argv) {
    need(argc==2||(argc==3&&(std::strcmp(argv[2],"--shared")==0||std::strcmp(argv[2],"--candidate")==0)),"usage: native11_world_pipeline_native.exe RENDERER_DLL [--shared]");
    run(argv[1],argc==3&&std::strcmp(argv[2],"--shared")==0,argc==3&&std::strcmp(argv[2],"--candidate")==0);std::puts("PASS native11 owned world pipeline private helper");
}
