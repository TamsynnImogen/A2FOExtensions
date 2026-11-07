#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/scene.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
struct Store { std::shared_ptr<const ModelAsset> model; };
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial) {
    auto model=static_cast<Store*>(raw)->model;
    return model&&model->info.database==db&&model->info.serial==serial?model:nullptr;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t serial,std::uint8_t byte) {
    auto t=std::make_shared<TextureAsset>();t->serial=serial;t->texture=static_cast<std::uint32_t>(serial);
    t->logical_name="selected.dds";
    t->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{byte});return t;
}
struct Textures {std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> current;};
std::shared_ptr<const TextureAsset> current_texture(void* raw,std::uint32_t pointer) {
    auto& values=static_cast<Textures*>(raw)->current;
    const auto found=values.find(pointer);return found==values.end()?nullptr:found->second;
}
struct Packet { A2FO_N11_LiveMaterial value{};std::string id;unsigned calls=0;bool reject=false; };
std::int32_t A2FO_N11_CALL packet_sink(void* raw,const A2FO_N11_LiveMaterial* value) {
    auto& out=*static_cast<Packet*>(raw);++out.calls;out.value=*value;
    if(value->texture_identifier_length)out.id.assign(
        reinterpret_cast<const char*>(value->texture_identifier),value->texture_identifier_length);
    return out.reject?0:1;
}
struct Bytes { std::vector<std::uint8_t> value;unsigned calls=0; };
std::int32_t A2FO_N11_CALL bytes_sink(void* raw,const std::uint8_t* bytes,std::uint32_t n) {
    auto& out=*static_cast<Bytes*>(raw);++out.calls;out.value.assign(bytes,bytes+n);return 1;
}
A2FO_N11_MaterialSeed seed(std::uint32_t part,std::uint32_t node,std::uint32_t group,const char* authored) {
    A2FO_N11_MaterialSeed s{};s.struct_size=sizeof(s);s.part_index=part;s.node_index=node;s.group_index=group;
    s.authored_texture=reinterpret_cast<const std::uint8_t*>(authored);
    s.authored_texture_length=std::strlen(authored);s.colour[0]=.2f;s.colour[1]=.3f;
    s.colour[2]=.4f;s.colour[3]=.5f;s.emissive[0]=.6f;s.flags=two_sided|cutout;return s;
}
bool invoke(const BoundMaterialCapture& bound,const std::string& id,const A2FO_N11_MaterialSeed& s,Packet& out) {
    return BoundMaterialCapture::resolve_live_material(const_cast<BoundMaterialCapture*>(&bound),
        reinterpret_cast<const std::uint8_t*>(id.data()),id.size(),&s,packet_sink,&out)==1;
}
}
int main() {
    auto model=std::make_shared<ModelAsset>();model->info.database=7;model->info.serial=11;
    std::strcpy(model->info.name,"ship.sod");model->bytes={1,2};model->info.byte_count=2;
    Store store{model};auto frame=std::make_unique<Frame>();frame->object_count=2;
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations;
    for(unsigned i=0;i<2;++i) {
        auto& object=frame->objects[i];object.instance=100+i;object.database=7;object.model_serial=11;
        object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(object.model_name,"ship.sod");
        auto observation=std::make_shared<MaterialObservation>();observation->instance=object.instance;
        observation->database=7;observation->model_serial=11;observation->model=model;
        MaterialBinding changed{};changed.authored_texture="authored.dds";
        changed.texture=i?texture(32,8):texture(31,9);changed.node_index=2;changed.group_index=4;
        changed.node_state_valid=true;
        MaterialBinding white{};white.authored_texture="old.dds";white.node_index=2;white.group_index=5;
        white.node_state_valid=true;
        MaterialBinding new_texture{};new_texture.texture=texture(40+i,static_cast<std::uint8_t>(6+i));
        new_texture.node_index=3;new_texture.group_index=0;new_texture.node_state_valid=true;
        observation->parts={changed,white,new_texture};observations.emplace(object.instance,observation);
    }
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    assert(build_bound_material_capture(*frame,19,resolve,&store,observations,bound,error));
    const std::string left=bound->frame().objects[0].model_name,right=bound->frame().objects[1].model_name;
    store.model.reset();observations.clear();model.reset();
    auto s=seed(0,2,4,"authored.dds");Packet a,b;
    assert(invoke(*bound,left,s,a)&&invoke(*bound,right,s,b));
    assert(a.calls==1&&b.calls==1&&a.id=="n11:texture:31"&&b.id=="n11:texture:32");
    assert(a.value.struct_size==sizeof(A2FO_N11_LiveMaterial)&&a.value.flags==s.flags&&
        a.value.colour[0]==s.colour[0]&&a.value.emissive[0]==s.emissive[0]&&
        a.value.sampler==0&&a.value.depth_mode==0&&a.value.alpha_cutoff==.5f&&
        a.value.uv_transform[0]==1&&a.value.uv_transform[4]==1);
    Bytes pixels;assert(BoundMaterialCapture::read_asset(const_cast<BoundMaterialCapture*>(bound.get()),2,
        reinterpret_cast<const std::uint8_t*>(a.id.data()),a.id.size(),16,bytes_sink,&pixels)==1);
    assert(pixels.calls==1&&pixels.value==std::vector<std::uint8_t>({9}));
    s=seed(1,2,5,"old.dds");Packet white;
    assert(invoke(*bound,left,s,white)&&white.calls==1&&white.id.empty()&&
        white.value.texture_identifier_length==0);
    s=seed(2,3,0,"");Packet added;
    assert(invoke(*bound,left,s,added)&&added.id=="n11:texture:40");
    Packet bad;s.node_index=4;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.node_index=3;s.group_index=1;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.group_index=0;s.authored_texture_length=1;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s=seed(0,2,4,"authored.dds");s.struct_size--;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.struct_size++;s.part_index=99;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.part_index=0;s.flags=16;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.flags=alpha_blend|additive;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.flags=0;s.colour[0]=std::nanf("");assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.colour[0]=.2f;s.emissive[0]=-1;assert(!invoke(*bound,left,s,bad)&&bad.calls==0);
    s.emissive[0]=0;bad.reject=true;assert(!invoke(*bound,left,s,bad)&&bad.calls==1);
    assert(!invoke(*bound,"wrong",s,a));
    Object exported{};std::shared_ptr<const MaterialObservation> out;
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> roundtrip;
    for(auto instance:{100u,101u}) {
        assert(bound->export_instance(instance,exported,out,error));
        assert(out->parts[0].node_index==2&&out->parts[0].group_index==4&&
            out->parts[2].node_index==3&&out->parts[2].group_index==0);
        roundtrip.emplace(instance,out);store.model=out->model;
    }
    std::shared_ptr<const BoundMaterialCapture> rebuilt;
    assert(build_bound_material_capture(*frame,19,resolve,&store,roundtrip,rebuilt,error));
    assert(bound->same_resources(*rebuilt)&&bound->same_instance_contents(100,*rebuilt));
    Textures versions;
    for(const auto& entry:roundtrip)for(const auto& part:entry.second->parts)
        if(part.texture)versions.current.emplace(part.texture->texture,part.texture);
    assert(bound->resources_current(resolve,&store,current_texture,&versions));
    auto retained_model=store.model;store.model.reset();
    assert(!bound->resources_current(resolve,&store,current_texture,&versions));store.model=retained_model;
    auto previous=versions.current.at(31);versions.current[31]=texture(31,7);
    assert(!bound->resources_current(resolve,&store,current_texture,&versions));versions.current[31]=previous;
    assert(bound->resources_current(resolve,&store,current_texture,&versions));
    std::shared_ptr<const BoundMaterialCapture> next_epoch;
    assert(build_bound_material_capture(*frame,20,resolve,&store,roundtrip,next_epoch,error));
    // A fresh cache epoch does not change the immutable resource/content owners.
    assert(bound->source_epoch()!=next_epoch->source_epoch()&&
        bound->same_resources(*next_epoch)&&bound->same_instance_contents(100,*next_epoch));
    auto duplicate=std::make_shared<MaterialObservation>(*roundtrip.at(100));
    duplicate->parts[1].node_index=2;duplicate->parts[1].group_index=4;
    roundtrip[100]=duplicate;
    assert(!build_bound_material_capture(*frame,19,resolve,&store,roundtrip,rebuilt,error)&&!error.empty());
    std::puts("native11 live material snapshot test passed");
}
