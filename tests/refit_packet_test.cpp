#include "../renderer/refit/packet.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

using namespace a2fo;
namespace {
bool resolve(void*,const std::shared_ptr<const native11::TextureAsset>& asset,
             std::string& name,std::uint32_t& revision,std::string&) {
    name="uploaded_"+std::to_string(asset->serial);revision=7;return true;
}
bool counted_resolve(void* context,const std::shared_ptr<const native11::TextureAsset>& asset,
                     std::string& name,std::uint32_t& revision,std::string& error) {
    ++*static_cast<unsigned*>(context);return resolve(nullptr,asset,name,revision,error);
}
bool bad_name(void* context,const std::shared_ptr<const native11::TextureAsset>&,
              std::string& name,std::uint32_t& revision,std::string&) {
    name=*static_cast<const std::string*>(context);revision=1;return true;
}
struct Fixture {
    native11::Object object{};
    native11::MaterialObservation observation{};
    refit::CameraView2 camera{};
    refit::Presentation2 presentation{};
    std::shared_ptr<native11::ModelAsset> model=std::make_shared<native11::ModelAsset>();
    std::shared_ptr<native11::TextureAsset> texture=std::make_shared<native11::TextureAsset>();
    std::vector<A2FORefitLight> lights;
    Fixture() {
        object.entity=1;object.instance=2;object.database=3;object.model_serial=4;
        object.model_identity=native11::ModelIdentity::plain_database_name;
        object.placement_identity=native11::PlacementIdentity::authored_placement;
        std::strcpy(object.model_name,"ship.sod");
        const float world[16]={0,-1,0,10, 1,0,0,20, 0,0,1,30, 0,0,0,1};
        std::memcpy(object.world,world,sizeof(world));
        model->info.database=3;model->info.serial=4;model->info.byte_count=3;
        std::strcpy(model->info.name,"ship.sod");model->bytes={1,2,3};
        observation.instance=2;observation.database=3;observation.model_serial=4;
        observation.native_x87_control=native11::encode_native_x87(0x037f);
        observation.model=model;
        texture->serial=6;texture->texture=8;
        texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{1});
        native11::MaterialBinding a{};a.node_index=1;a.group_index=2;a.node_state_valid=true;
        a.authored_texture="hull.tga";a.texture=texture;
        a.node_state.local_to_model[3]=2;a.node_state.local_to_model[7]=3;
        a.node_state.local_to_model[11]=4;
        observation.parts.push_back(a);
        auto b=a;b.group_index=3;b.texture.reset();b.authored_texture.clear();
        observation.parts.push_back(b);
        auto hidden=a;hidden.node_index=2;hidden.group_index=0;hidden.node_state.visible=false;
        observation.parts.push_back(hidden);
        const float view[16]={1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
        std::memcpy(camera.world_to_clip,view,sizeof(view));
        camera.viewport[0]=11;camera.viewport[1]=12;camera.viewport[2]=640;camera.viewport[3]=480;
        camera.eye[0]=1;camera.eye[1]=2;camera.eye[2]=3;camera.time=4;
        presentation.flags=A2FO_REFIT_TEAM;presentation.team[0]=.25f;
        A2FORefitLight light{};light.kind=A2FO_REFIT_POINT;light.range=10;light.intensity=2;
        lights.push_back(light);
    }
    bool build(std::shared_ptr<const refit::Packet2>& out,std::string& error) {
        return refit::build_packet2(object,observation,camera,presentation,10,11,3,12,13,
            lights,nullptr,resolve,out,error);
    }
};
}
int main() {
    Fixture f;std::shared_ptr<const refit::Packet2> packet;std::string error;
    assert(f.build(packet,error));
    const auto& frame=packet->frame();
    assert(frame.size==sizeof(A2FORefitFrame2)&&frame.flags==A2FO_REFIT_FRAME_DIRECT);
    assert(frame.source_epoch==12&&frame.serial==13&&frame.x==11&&frame.y==12);
    assert(frame.part_count==2&&frame.light_count==1);
    assert(frame.view[1]==5&&frame.view[4]==2&&frame.projection[0]==1&&frame.projection[5]==1);
    assert(frame.parts[0].world[9]==7&&frame.parts[0].world[10]==22&&frame.parts[0].world[11]==34);
    assert(frame.parts[0].flags==A2FO_REFIT_TEAM&&frame.parts[0].team[0]==.25f);
    assert(std::string(frame.parts[0].texture_name,frame.parts[0].texture_name_length)=="uploaded_6");
    assert(frame.parts[0].texture_revision==7&&frame.parts[1].texture_name_length==0&&
           frame.parts[1].texture_revision==0);
    auto alpha_texture=std::make_shared<native11::TextureAsset>(*f.texture);
    alpha_texture->flags=native11::texture_has_alpha;
    f.observation.parts[0].texture=alpha_texture;
    std::shared_ptr<const refit::Packet2> alpha_packet;
    assert(f.build(alpha_packet,error)&&alpha_packet->frame().part_count==2&&
           std::string(alpha_packet->frame().parts[0].texture_name,
                       alpha_packet->frame().parts[0].texture_name_length)=="uploaded_6");
    auto unknown_texture=std::make_shared<native11::TextureAsset>(*alpha_texture);
    unknown_texture->flags=4;
    f.observation.parts[0].texture=unknown_texture;
    auto alpha_prior=alpha_packet;
    assert(!f.build(alpha_packet,error)&&!error.empty()&&alpha_packet==alpha_prior);
    f.observation.parts[0].texture=f.texture;
    f.observation.parts[2].texture.reset();
    unsigned resolver_calls=0;std::shared_ptr<const refit::Packet2> hidden_packet;
    assert(refit::build_packet2(f.object,f.observation,f.camera,f.presentation,
        10,11,3,12,13,f.lights,&resolver_calls,counted_resolve,hidden_packet,error));
    assert(resolver_calls==1&&hidden_packet->frame().part_count==2&&
           f.observation.parts[2].authored_texture=="hull.tga");
    f.observation.parts[0].node_state.local_to_model[3]=99;
    f.lights[0].intensity=99;f.camera.world_to_clip[1]=99;
    assert(std::string(frame.parts[0].texture_name,frame.parts[0].texture_name_length)=="uploaded_6");
    assert(frame.parts[0].world[9]==7&&frame.lights[0].intensity==2&&frame.view[4]==2);
    auto old=packet;
    f.observation.parts[0].node_state_valid=false;
    assert(!f.build(packet,error)&&!error.empty()&&packet==old);
    f.observation.parts[0].node_state_valid=true;
    f.observation.parts[0].node_state.local_to_model[3]=2;
    f.object.database=8;assert(!f.build(packet,error)&&packet==old);f.object.database=3;
    f.observation.parts[0].texture.reset();assert(!f.build(packet,error)&&packet==old);
    f.observation.parts[0].texture=f.texture;
    f.object.world[0]=std::numeric_limits<float>::infinity();
    assert(!f.build(packet,error)&&packet==old);f.object.world[0]=0;
    f.object.world[12]=1;assert(!f.build(packet,error)&&packet==old);f.object.world[12]=0;
    f.camera.viewport[2]=1.5f;assert(!f.build(packet,error)&&packet==old);
    f.camera.viewport[2]=640;f.presentation.alpha=.5f;
    assert(!f.build(packet,error)&&packet==old);
    f.presentation.alpha=1;
    f.presentation.team[0]=-1;
    assert(!f.build(packet,error)&&packet==old);
    f.presentation.team[0]=.25f;
    f.presentation.self_illum=2e12f;
    assert(!f.build(packet,error)&&packet==old);
    f.presentation.self_illum=0;
    f.camera.viewport[0]=4294967296.0f;
    assert(!f.build(packet,error)&&packet==old);
    f.camera.viewport[0]=11;
    f.camera.time=2e12f;
    assert(!f.build(packet,error)&&packet==old);
    f.camera.time=4;
    std::string invalid_name="uploaded:6";
    assert(!refit::build_packet2(f.object,f.observation,f.camera,f.presentation,
        10,11,3,12,13,f.lights,&invalid_name,bad_name,packet,error)&&packet==old);
    invalid_name.assign(256,'a');
    assert(!refit::build_packet2(f.object,f.observation,f.camera,f.presentation,
        10,11,3,12,13,f.lights,&invalid_name,bad_name,packet,error)&&packet==old);
    f.lights.resize(33,f.lights[0]);
    assert(!f.build(packet,error)&&packet==old);
    f.lights.resize(1);
    f.lights[0].range=0;
    assert(!f.build(packet,error)&&packet==old);
    f.lights[0].range=10;
    f.lights[0].colour[0]=-1;
    assert(!f.build(packet,error)&&packet==old);
    f.lights[0].colour[0]=0;
    f.lights[0].kind=A2FO_REFIT_DIRECTIONAL;
    assert(!f.build(packet,error)&&packet==old);
    f.lights[0].direction[0]=1;
    f.lights.resize(5,f.lights[0]);
    assert(!f.build(packet,error)&&packet==old);
    f.lights.resize(1);
    f.lights[0].direction[0]=0;
    f.lights[0].kind=A2FO_REFIT_POINT;
    f.object.world[0]=0;f.object.world[1]=0;f.object.world[2]=0;
    assert(!f.build(packet,error)&&packet==old);
    f.object.world[1]=-1;
    f.observation.parts[1].group_index=2;
    assert(!f.build(packet,error)&&packet==old);
    f.observation.parts[1].group_index=3;
    f.observation.parts[2].node_state_valid=false;
    assert(!f.build(packet,error)&&packet==old);
    f.observation.parts[2].node_state_valid=true;
    auto keyed=std::make_shared<native11::TextureAsset>(*f.texture);
    keyed->colour_key=true;f.observation.parts[0].texture=keyed;
    assert(!f.build(packet,error)&&packet==old);
    f.observation.parts[0].texture=f.texture;
    Fixture second;
    second.object.instance=20;second.observation.instance=20;
    second.texture->serial=9;
    assert(second.build(packet,error));
    assert(std::string(packet->frame().parts[0].texture_name,
        packet->frame().parts[0].texture_name_length)=="uploaded_9");
    assert(std::string(old->frame().parts[0].texture_name,
        old->frame().parts[0].texture_name_length)=="uploaded_6");
    second.object.placement_identity=native11::PlacementIdentity::craft_runtime_scale;
    assert(second.build(packet,error));
    second.object.runtime_scale[0]=0;
    assert(!second.build(packet,error));
    return 0;
}
