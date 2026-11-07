#include "../renderer/native11/primitive_service.hpp"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* label){if(!yes){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
// i686 x87 may keep a float literal at excess precision in an expression.
// Compare stored wire and pinned-source representations to prove exact transport.
bool same_float_bits(const float& wire,const float& source){return std::memcmp(&wire,&source,sizeof(float))==0;}
std::shared_ptr<PrimitiveFrame> sample(){
    auto p=std::make_shared<PrimitiveFrame>();p->generation=7;p->source_epoch=3;
    PrimitiveMesh mesh{};mesh.id=11;mesh.vertices.resize(3);mesh.indices={0,1,2};
    p->meshes.push_back(std::move(mesh));
    PrimitiveTexture texture{};texture.id=12;texture.width=texture.height=1;
    texture.bgra={1,2,3,4};p->textures.push_back(std::move(texture));
    PrimitiveDraw draw{};draw.mesh=11;draw.texture=12;
    draw.surface.flags=two_sided|cutout;draw.surface.layer=4;
    draw.surface.depth_mode=DepthMode::read_only;
    draw.surface.sampler=Sampling::point_wrap;
    draw.surface.shadow_only=true;draw.surface.casts_shadow=false;
    draw.surface.receives_shadow=false;draw.surface.fog_enabled=false;
    draw.surface.alpha_cutoff=.3f;draw.surface.uv_transform[2]=.25f;
    p->draws.push_back(draw);
    Light light{};light.type=LightType::spot;light.intensity=.4f;p->lights.push_back(light);
    p->settings.ordering=DrawOrdering::opaque_then_alpha;
    p->settings.clear_colour_target=false;p->settings.clear_depth_target=false;
    p->settings.shadows.light_mask=1;p->settings.shadows.resolution=256;
    p->settings.shadows.depth_bias=.02f;p->settings.shadows.filter=ShadowFilter::pcf3;
    p->settings.fog.enabled=true;p->settings.fog.colour[0]=.2f;
    p->settings.fog.start=1;p->settings.fog.end=5;p->settings.fog.camera_position[2]=2;
    p->settings.ambient[0]=.7f;p->settings.clear_colour[3]=.8f;
    p->settings.viewport[2]=32;p->settings.viewport[3]=32;
    return p;
}
void assert_bound(const EncodedPrimitiveFrame& e){
    need(e.packet&&e.frame.struct_size==sizeof(e.frame),"packet owner and frame size");
    need(e.frame.meshes==e.meshes.data()&&e.frame.textures==e.textures.data()&&
         e.frame.draws==e.draws.data()&&e.frame.lights==e.lights.data(),"moved descriptors rebound");
    need(e.meshes[0].vertices==reinterpret_cast<const A2FO_N11_Vertex*>(e.packet->meshes[0].vertices.data())&&
         e.meshes[0].indices==e.packet->meshes[0].indices.data()&&
         e.textures[0].bgra==e.packet->textures[0].bgra.data(),"borrowed bytes pinned by owner");
    need(e.frame.generation==7&&e.frame.source_epoch==3&&e.frame.mesh_count==1&&
         e.frame.texture_count==1&&e.frame.draw_count==1&&e.frame.light_count==1,"wire counts");
    need(e.frame.flags==(A2FO_N11_PRIMITIVE_LOAD_COLOUR|A2FO_N11_PRIMITIVE_LOAD_DEPTH)&&
         e.frame.ordering==1&&e.frame.shadow_mask==1&&e.frame.shadow_resolution==256&&
         same_float_bits(e.frame.shadow_bias,e.packet->settings.shadows.depth_bias)&&
         e.frame.shadow_filter==2,"view and shadow mapping");
    need(e.frame.fog_enabled==1&&
         same_float_bits(e.frame.fog_colour[0],e.packet->settings.fog.colour[0])&&
         same_float_bits(e.frame.fog_start,e.packet->settings.fog.start)&&
         same_float_bits(e.frame.fog_end,e.packet->settings.fog.end)&&
         same_float_bits(e.frame.camera_position[2],e.packet->settings.fog.camera_position[2])&&
         same_float_bits(e.frame.ambient[0],e.packet->settings.ambient[0])&&
         same_float_bits(e.frame.clear_colour[3],e.packet->settings.clear_colour[3])&&
         same_float_bits(e.frame.viewport[2],e.packet->settings.viewport[2]),"fog and frame mapping");
    const auto& d=e.draws[0];
    need(d.mesh==11&&d.texture==12&&d.flags==(two_sided|cutout)&&d.layer==4&&
         d.depth_mode==1&&d.sampling==3&&d.options==15&&
         same_float_bits(d.alpha_cutoff,e.packet->draws[0].surface.alpha_cutoff)&&
         same_float_bits(d.uv_transform[2],e.packet->draws[0].surface.uv_transform[2])&&
         d.reserved==0,"surface mapping");
    need(e.lights[0].type==3&&
         same_float_bits(e.lights[0].intensity,e.packet->lights[0].intensity),"light mapping");
}
void run(){
    auto owned=sample();std::string error;EncodedPrimitiveFrame encoded;
    need(encode_primitive_frame(owned,encoded,error)&&error.empty(),"encode sample");
    assert_bound(encoded);owned.reset();assert_bound(encoded);
    EncodedPrimitiveFrame moved(std::move(encoded));assert_bound(moved);
    EncodedPrimitiveFrame reassigned;reassigned=std::move(moved);assert_bound(reassigned);
    const auto prior_owner=reassigned.packet;
    need(!encode_primitive_frame(nullptr,reassigned,error)&&reassigned.packet==prior_owner,
         "null failure transactional");
    auto invalid=sample();invalid->settings.draw_count=1;
    need(!encode_primitive_frame(invalid,reassigned,error)&&reassigned.packet==prior_owner,
         "borrowed draw pointer/count refused transactionally");
    invalid->settings.draw_count=0;invalid->draws[0].surface.mesh=99;
    need(!encode_primitive_frame(invalid,reassigned,error)&&reassigned.packet==prior_owner,
         "foreign GPU handle refused transactionally");
    std::puts("PASS owned API7 primitive encoding, full mapping, move pinning and transactional refusal");
}
}
int main(){run();}
