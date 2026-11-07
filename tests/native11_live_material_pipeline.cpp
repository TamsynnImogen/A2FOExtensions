// Private-helper, headless Core -> WorldRenderSession -> API11 material preview.
#include "../core/native11_world_pipeline.hpp"
#include "../core/native11_preview_export.hpp"
#include "../core/native11_light_classes.hpp"
#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo;
namespace {
void need(bool good,const char* why){if(!good){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct PrivateRoot {
    std::string path,dll,ini;
    explicit PrivateRoot(const char* source){
        char temp[MAX_PATH]{};need(GetTempPathA(MAX_PATH,temp)>0,"private temp path");
        char unique[MAX_PATH]{};need(GetTempFileNameA(temp,"n11",0,unique)!=0,"unique private root");
        need(DeleteFileA(unique)&&CreateDirectoryA(unique,nullptr),"private root created");
        path=unique;dll=path+"\\A2FONative11Renderer.dll";ini=path+"\\A2FORenderer.ini";
        need(CopyFileA(source,dll.c_str(),TRUE)!=0,"private helper copied");
        need(WritePrivateProfileStringA("Native11","OffscreenWorld","1",ini.c_str())&&
             WritePrivateProfileStringA("Native11","LiveMaterials","1",ini.c_str())&&
             WritePrivateProfileStringA("Native11","LiveMaterialPreview","1",ini.c_str()),
             "private material preview configured");
    }
    bool remove(){const bool okay=DeleteFileA(ini.c_str())&&DeleteFileA(dll.c_str())&&
        RemoveDirectoryA(path.c_str());if(okay)path.clear();return okay;}
    ~PrivateRoot(){if(!path.empty()){DeleteFileA(ini.c_str());DeleteFileA(dll.c_str());RemoveDirectoryA(path.c_str());}}
};
std::vector<std::uint8_t> tga(std::uint8_t blue,std::uint8_t green,std::uint8_t red){
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=blue;bytes[19]=green;bytes[20]=red;return bytes;
}
struct Assets {
    std::shared_ptr<const native11::ModelAsset> model;
    std::shared_ptr<const native11::TextureAsset> white,red,blue;
    std::shared_ptr<const native11::TextureAsset> current_white,current_red,current_blue;
    unsigned validator_calls=0,invalidate_at=0;
    Assets(){
        auto m=std::make_shared<native11::ModelAsset>();m->info.database=7;m->info.serial=11;
        std::strcpy(m->info.name,"ship.sod");m->bytes=native11_fixture::model("white.tga",true);
        m->info.byte_count=static_cast<std::uint32_t>(m->bytes.size());
        const float scale=1.f;std::memcpy(&m->info.parser_scalar_bits,&scale,4);
        m->info.parser_native_x87_control=native11::encode_native_x87(0x037f);model=m;
        white=texture(41,31,"white.tga",255,255,255);
        red=texture(42,32,"red.tga",0,0,255);
        blue=texture(43,33,"blue.tga",255,0,0);
        current_white=white;current_red=red;current_blue=blue;
    }
    static std::shared_ptr<const native11::TextureAsset> texture(std::uint32_t address,
        std::uint64_t serial,const char* name,std::uint8_t b,std::uint8_t g,std::uint8_t r){
        auto result=std::make_shared<native11::TextureAsset>();
        result->serial=serial;result->texture=address;result->logical_name=name;
        result->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(b,g,r));return result;
    }
    static std::shared_ptr<const native11::ModelAsset> resolve_model(void* raw,
        std::uint32_t db,std::uint64_t serial){
        auto& a=*static_cast<Assets*>(raw);
        return a.model&&db==a.model->info.database&&serial==a.model->info.serial?a.model:nullptr;
    }
    static std::shared_ptr<const native11::TextureAsset> resolve_texture(void* raw,std::uint32_t id){
        const auto& a=*static_cast<Assets*>(raw);
        return id==41?a.current_white:id==42?a.current_red:id==43?a.current_blue:nullptr;
    }
};
Assets* active_assets=nullptr;
bool validate(const std::shared_ptr<const native11::BoundMaterialCapture>& packet) noexcept {
    if(!packet||!active_assets)return false;
    auto& a=*active_assets;++a.validator_calls;
    if(a.invalidate_at&&a.validator_calls==a.invalidate_at)
        native11_world_pipeline::invalidate_sources();
    return packet->resources_current(Assets::resolve_model,&a,Assets::resolve_texture,&a);
}
void transform(native11::Object& object,float x){
    for(unsigned k=0;k<16;++k)object.world[k]=k%5==0?1.f:0.f;
    object.world[0]=object.world[5]=.58f;object.world[3]=x;
}
std::shared_ptr<const native11::BoundMaterialCapture> capture(Assets& a,
    std::uint64_t generation,std::uint64_t source_epoch){
    auto frame=std::make_unique<native11::Frame>();frame->camera=1;
    frame->generation=generation;frame->object_count=2;
    for(unsigned k=0;k<16;++k)frame->view.world_to_clip[k]=k%5==0?1.f:0.f;
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    std::map<std::uint32_t,std::shared_ptr<const native11::MaterialObservation>> observations;
    for(unsigned k=0;k<2;++k){
        auto& object=frame->objects[k];object.instance=101+k;object.entity=201+k;
        object.database=7;object.model_serial=11;
        object.model_identity=native11::ModelIdentity::plain_database_name;
        object.placement_identity=native11::PlacementIdentity::authored_placement;
        std::strcpy(object.model_name,"ship.sod");transform(object,k? .48f:-.48f);
        auto observed=std::make_shared<native11::MaterialObservation>();
        observed->instance=object.instance;observed->database=7;observed->model_serial=11;
        observed->model=a.model;observed->native_x87_control=native11::encode_native_x87(0x037f);
        native11::MaterialBinding part;part.authored_texture="white.tga";
        part.texture=k?a.blue:a.red;part.node_index=0;part.group_index=0;
        part.node_state_valid=true;observed->parts.push_back(part);
        observations.emplace(object.instance,observed);
    }
    std::shared_ptr<const native11::BoundMaterialCapture> output;std::string error;
    need(native11::build_bound_material_capture(*frame,source_epoch,Assets::resolve_model,&a,
        observations,output,error),"sealed two-actor material packet");
    need(output->frame().requires_part_states&&output->common_native_x87_control()!=0,
         "owned part states and load control retained");
    return output;
}
std::vector<std::uint8_t> preview(){
    std::vector<std::uint8_t> bytes;std::uint32_t width=0,height=0;std::string error;
    need(native11_world_pipeline::readback_material_preview(bytes,width,height,error)&&
         width==32&&height==32&&bytes.size()==32*32*4,"private material preview readback");
    return bytes;
}
std::vector<std::uint8_t> private_file(const std::string& path){
    HANDLE handle=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,nullptr);need(handle!=INVALID_HANDLE_VALUE,"open private preview export");
    const DWORD length=GetFileSize(handle,nullptr);
    need(length!=INVALID_FILE_SIZE&&length<1024*1024,"bounded private preview export");
    std::vector<std::uint8_t> bytes(length);DWORD got=0;
    need(ReadFile(handle,bytes.data(),length,&got,nullptr)&&got==length,
         "complete private preview export read");
    CloseHandle(handle);return bytes;
}
struct ExportContext {
    Assets* assets;
    std::shared_ptr<const native11::BoundMaterialCapture> packet;
    std::uint32_t ticket;
    static bool read(void*,std::vector<std::uint8_t>& bytes,std::uint32_t& width,
        std::uint32_t& height,std::string& error){
        return native11_world_pipeline::readback_material_preview(bytes,width,height,error);
    }
    static bool current(void* raw) noexcept {
        const auto& held=*static_cast<ExportContext*>(raw);
        const auto status=native11_world_pipeline::material_status();
        return held.ticket&&held.ticket==native11_world_pipeline::source_epoch()&&
            status.state==native11_world_pipeline::MaterialState::previewed&&
            status.frame==held.packet->frame().generation&&
            held.packet->resources_current(Assets::resolve_model,held.assets,
                Assets::resolve_texture,held.assets);
    }
};
void export_first_preview(const std::string& root,
    const std::shared_ptr<const native11::BoundMaterialCapture>& packet,Assets& assets){
    const auto directory=root+"\\export";std::string error;
    need(native11_preview_export::configure_at(directory,1000,nullptr,error),
         "private owned preview exporter configured");
    const auto material=native11_world_pipeline::material_status();
    native11::PreviewExportStatus status;status.state="frame";
    status.reason="synthetic owned material preview";status.frame=material.frame;
    status.source_epoch=material.source_epoch;status.source_ticket=material.source_ticket;
    status.objects=packet->frame().object_count;
    ExportContext context{&assets,packet,native11_world_pipeline::source_epoch()};
    native11_preview_export::sample(status,ExportContext::read,ExportContext::current,&context,0);
    const auto bmp=private_file(directory+"\\frame-a.bmp");
    const auto raw=private_file(directory+"\\status.json");
    const std::string json(raw.begin(),raw.end());
    need(json.find("\"state\":\"frame\"")!=std::string::npos&&
         json.find("\"slot\":\"a\"")!=std::string::npos&&
         json.find("\"frame\":\"1\"")!=std::string::npos&&
         bmp.size()==54+32*32*3&&bmp[0]=='B'&&bmp[1]=='M',
         "actual GPU preview exported to owned BMP and status");
    unsigned red=0,blue=0;
    for(std::size_t k=54;k+2<bmp.size();k+=3){
        if(bmp[k+2]>bmp[k]+50&&bmp[k+2]>100)++red;
        if(bmp[k]>bmp[k+2]+50&&bmp[k]>100)++blue;
    }
    need(red>5&&blue>5,"exported BMP preserves both selected actor colours");
    need(native11_preview_export::stop()&&!native11_preview_export::requested(),
         "private exporter stopped before pipeline continues");
    for(const auto leaf:{"frame-a.bmp","frame-b.bmp","status.json","capture.lock",
                         "write.tmp",".a2fo-preview-v1"}){
        const auto path=directory+"\\"+leaf;
        if(GetFileAttributesA(path.c_str())!=INVALID_FILE_ATTRIBUTES)
            need(DeleteFileA(path.c_str())!=0,"delete exact private export file");
    }
    need(RemoveDirectoryA(directory.c_str())!=0,"remove owned export directory");
}
std::int32_t A2FO_N11_CALL copy_bytes(void* raw,const std::uint8_t* bytes,
    std::uint32_t count) noexcept {
    try {auto& out=*static_cast<std::vector<std::uint8_t>*>(raw);
        out.assign(bytes,bytes+count);return 1;
    }catch(...){return 0;}
}
void selected_colours(const std::vector<std::uint8_t>& bytes){
    unsigned red=0,blue=0;
    for(std::size_t k=0;k<bytes.size();k+=4){
        if(bytes[k+2]>bytes[k]+50&&bytes[k+2]>100)++red;
        if(bytes[k]>bytes[k+2]+50&&bytes[k]>100)++blue;
    }
    need(red>5&&blue>5,"two actors submit distinct selected red and blue textures");
}
void no_world_image(){
    std::vector<std::uint8_t> retained{9};std::uint32_t width=7,height=8;std::string error;
    need(!native11_world_pipeline::readback(retained,width,height,error)&&
         retained==std::vector<std::uint8_t>{9}&&width==7&&height==8,
         "material preview does not publish a complete world image");
}
struct CompleteWorldProvider {
    std::shared_ptr<const native11::BoundMaterialCapture> all;
    static bool provide(void* raw,const native11::BoundMaterialCapture&,
        const native11_world_lights::Snapshot&,native11_world_pipeline::Geometry& output,
        std::string&){
        const auto& source=*static_cast<CompleteWorldProvider*>(raw);
        output.all=source.all;output.complete_casters=true;output.complete_visibility=true;
        return true;
    }
};
std::shared_ptr<const native11_world_lights::Snapshot> complete_lighting(
    std::uint64_t generation,const std::shared_ptr<const native11::LightClassRecord>& owner){
    auto result=std::make_shared<native11_world_lights::Snapshot>();
    result->camera=1;result->frame_generation=generation;
    result->class_generation=native11_light_classes::generation();
    for(const auto member:{native11::WorldMember{201,101,static_cast<std::uint32_t>(owner->class_id),owner->version},
                           native11::WorldMember{202,102,static_cast<std::uint32_t>(owner->class_id),owner->version}}){
        result->captured.members.push_back(member);
        result->captured.class_owners.push_back(owner);
        native11::PlacedLight source{};source.source_id=member.entity;source.config=owner->config;
        result->captured.sources.push_back(source);
    }
    const float ambient[3]{.1f,.1f,.1f};std::string error;
    need(native11::assemble_world_lighting(result->captured.sources,true,ambient,
        result->lighting,error)&&result->lighting.lights.empty(),
        "complete synthetic world membership with disabled lights");
    return result;
}
void run(const char* renderer_path){
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"helper initially unloaded");
    PrivateRoot root(renderer_path);Assets assets;active_assets=&assets;
    need(native11_world_pipeline::configure_from_roots({root.path},nullptr)&&
         native11_world_pipeline::requested(),"material preview configuration admitted");
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"configuration is startup only");
    need(native11_world_pipeline::set_material_validator(validate),"resource validator registered");
    auto first=capture(assets,1,20);
    need(!native11_world_pipeline::on_frame(first,nullptr),"no complete world geometry available");
    auto status=native11_world_pipeline::material_status();
    need(status.state==native11_world_pipeline::MaterialState::previewed&&status.frame==1&&
         status.source_epoch==20&&status.submitted_frames==1&&
         native11_world_pipeline::status().state!=native11_world_pipeline::State::rendered,
         "API11 material preview independent of full world readiness");
    need(GetModuleHandleA("A2FONative11Renderer.dll")!=nullptr,"private API11 helper loaded");
    selected_colours(preview());no_world_image();
    export_first_preview(root.path,first,assets);
    need(!native11_world_pipeline::on_frame(first,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::unavailable,
         "same frame material replay refused");
    auto second=capture(assets,2,20);
    need(!native11_world_pipeline::on_frame(second,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::previewed,
         "next material frame reuses immutable resource epoch");
    assets.current_blue=Assets::texture(43,34,"blue.tga",255,0,0);
    std::vector<std::uint8_t> retained_blue;
    constexpr char old_blue_id[]="n11:texture:33";
    need(native11::BoundMaterialCapture::read_asset(
             const_cast<native11::BoundMaterialCapture*>(first.get()),2,
             reinterpret_cast<const std::uint8_t*>(old_blue_id),sizeof(old_blue_id)-1,
             64,copy_bytes,&retained_blue)==1&&retained_blue==tga(255,0,0),
         "retained snapshot keeps original selected texture bytes immutable");
    std::vector<std::uint8_t> bytes{7};std::uint32_t width=4,height=5;std::string error;
    need(!native11_world_pipeline::readback_material_preview(bytes,width,height,error)&&
         bytes==std::vector<std::uint8_t>{7}&&width==4&&height==5,
         "retired texture owner revokes previously published preview");
    auto stale=capture(assets,3,21);
    need(!native11_world_pipeline::on_frame(stale,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::waiting_sources,
         "stale retained texture cannot submit after address reuse");
    assets.blue=assets.current_blue;
    auto fresh=capture(assets,4,21);
    need(!native11_world_pipeline::on_frame(fresh,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::previewed,
         "fresh owner and source epoch recover material preview");
    const auto recovered=preview();need(recovered!=std::vector<std::uint8_t>{7},"fresh preview pixels copied");
    native11_world_pipeline::invalidate_sources();
    need(native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::waiting_sources&&
         !native11_world_pipeline::readback_material_preview(bytes,width,height,error),
         "source ticket revokes preview publication");
    auto interrupted=capture(assets,5,21);
    assets.invalidate_at=assets.validator_calls+5;
    need(!native11_world_pipeline::on_frame(interrupted,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::unavailable&&
         !native11_world_pipeline::readback_material_preview(bytes,width,height,error),
         "source invalidation during GPU preparation prevents publication");
    assets.invalidate_at=0;
    auto recovered_frame=capture(assets,6,21);
    need(!native11_world_pipeline::on_frame(recovered_frame,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::previewed,
         "fresh frame recovers after mid-submission revocation");
    selected_colours(preview());no_world_image();
    native11_light_classes::configure(true,nullptr);
    native11_light_classes::dispatch_available(true);
    void* class_id=reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x4321));
    need(native11_light_classes::observe_class(class_id,"synthetic-world-objects.odf",
         {{"lightSource","0"}}),"synthetic complete-world class owner");
    const auto class_owner=native11_light_classes::pin(0x4321);
    need(class_owner&&!class_owner->config.enabled,"disabled synthetic light class pinned");
    CompleteWorldProvider provider;
    provider.all=capture(assets,7,21);
    need(native11_world_pipeline::set_geometry_provider(CompleteWorldProvider::provide,&provider),
         "synthetic complete-world provider registered");
    const auto full_lights=complete_lighting(7,class_owner);
    need(native11_world_pipeline::on_frame(provider.all,full_lights)&&
         native11_world_pipeline::status().state==native11_world_pipeline::State::rendered,
         "checked synthetic complete world renders after material preview");
    std::vector<std::uint8_t> world_image;std::uint32_t world_width=0,world_height=0;
    need(native11_world_pipeline::readback(world_image,world_width,world_height,error)&&
         world_width==32&&world_height==32&&world_image.size()==32*32*4,
         "complete world image published on shared private target");
    bytes={7};width=4;height=5;
    need(!native11_world_pipeline::readback_material_preview(bytes,width,height,error)&&
         bytes==std::vector<std::uint8_t>{7}&&width==4&&height==5,
         "full-world render revokes previous material preview readback");
    auto preview_only=capture(assets,8,21);
    need(!native11_world_pipeline::on_frame(preview_only,nullptr)&&
         native11_world_pipeline::material_status().state==native11_world_pipeline::MaterialState::previewed&&
         native11_world_pipeline::status().state!=native11_world_pipeline::State::rendered,
         "next observed material frame previews without complete world sources");
    selected_colours(preview());no_world_image();
    need(native11_world_pipeline::shutdown(),"owner-thread API11 session shutdown");
    native11_light_classes::disable();
    need(GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,"private helper unloaded");
    need(WritePrivateProfileStringA("Native11","LiveMaterials","bogus",root.ini.c_str())&&
         !native11_world_pipeline::configure_from_roots({root.path},nullptr)&&
         GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,
         "malformed material option rejected before helper load");
    need(native11_world_pipeline::shutdown(),"invalid configuration shutdown");
    need(WritePrivateProfileStringA("Native11","LiveMaterials","1",root.ini.c_str())&&
         WritePrivateProfileStringA("Native11","LiveMaterialPreview","bogus",root.ini.c_str())&&
         !native11_world_pipeline::configure_from_roots({root.path},nullptr)&&
         GetModuleHandleA("A2FONative11Renderer.dll")==nullptr,
         "malformed preview option rejected before helper load");
    need(native11_world_pipeline::shutdown(),"final pipeline shutdown");
    active_assets=nullptr;need(root.remove(),"private helper, INI and root removed");
}
}
int main(int argc,char** argv){
    need(argc==2,"usage: native11_live_material_pipeline.exe HELPER_DLL");
    run(argv[1]);std::puts("PASS native11 API11 private live material pipeline");
}
