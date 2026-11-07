#include "../renderer/refit/mesh_scope.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace a2fo;
namespace {
struct Fixture {
    std::shared_ptr<native11::ModelAsset> model=std::make_shared<native11::ModelAsset>();
    std::shared_ptr<native11::TextureAsset> texture=std::make_shared<native11::TextureAsset>();
    native11::MaterialObservation source{};
    Fixture(){
        model->info.database=3;model->info.serial=4;
        texture->serial=7;texture->texture=8;
        texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(
            std::vector<std::uint8_t>{1,2,3});
        source.instance=2;source.database=3;source.model_serial=4;
        source.native_x87_control=0x1234;source.model=model;
        native11::MaterialBinding part{};
        part.native_node=0x1000;part.node_index=5;part.group_index=0;
        part.source_triangles=12;part.source_flags=1;
        part.node_state_valid=true;part.authored_texture="hull.dds";part.texture=texture;
        part.node_state.local_to_model[3]=6;
        source.parts.push_back(part);
        auto other=part;other.native_node=0x2000;other.node_index=6;
        other.authored_texture="other.dds";source.parts.push_back(other);
    }
};
void rejected(const native11::MaterialObservation& source,std::uint32_t node,
              std::uint32_t groups,const refit::MeshSelection2& prior){
    refit::MeshSelection2 result=prior;
    std::string error="old";
    assert(!refit::select_opaque_mesh2(source,node,groups,result,error));
    assert(!error.empty()&&result.observation==prior.observation&&
           result.triangles==prior.triangles&&result.node_index==prior.node_index&&
           result.group_index==prior.group_index);
}
native11::ModelAsset header(const char* magic,float version){
    native11::ModelAsset model{};
    model.bytes.assign(magic,magic+10);
    std::uint32_t bits=0;static_assert(sizeof(bits)==sizeof(version));
    std::memcpy(&bits,&version,sizeof(bits));
    for(unsigned i=0;i<4;++i)model.bytes.push_back(static_cast<std::uint8_t>(bits>>(8*i)));
    return model;
}
}
int main(){
    for(const char* magic:{"Storm3D_SW","StarTrekDB"}){
        assert(refit::mesh_model_format_supported2(header(magic,1.7f)));
        assert(refit::mesh_model_format_supported2(header(magic,1.93f)));
        assert(refit::mesh_model_format_supported2(header(magic,1.8f)));
        assert(!refit::mesh_model_format_supported2(header(magic,1.69f)));
        assert(!refit::mesh_model_format_supported2(header(magic,1.94f)));
        assert(!refit::mesh_model_format_supported2(header(
            magic,std::numeric_limits<float>::quiet_NaN())));
    }
    auto format=header("Storm3D_SW",1.8f);
    format.bytes.pop_back();assert(!refit::mesh_model_format_supported2(format));
    format.bytes.resize(13);assert(!refit::mesh_model_format_supported2(format));
    format=header("WrongMagic",1.8f);
    assert(!refit::mesh_model_format_supported2(format));
    format=header("StarTrekDB",1.8f);format.bytes.push_back(0xff);
    assert(refit::mesh_model_format_supported2(format)); // Header gate is not a full parser.
    Fixture fixture;
    auto& source=fixture.source;
    refit::MeshSelection2 selected{};
    std::string error="old";
    assert(refit::select_opaque_mesh2(source,0x1000,1,selected,error)&&error.empty());
    assert(selected.observation&&selected.observation->parts.size()==1&&
           selected.triangles==12&&selected.node_index==5&&selected.group_index==0&&
           selected.observation->native_x87_control==0x1234&&
           selected.observation->parts[0].source_flags==1);
    assert(selected.observation->parts[0].texture==fixture.texture&&
           selected.observation->model==fixture.model);
    source.parts[0].authored_texture="changed.dds";
    source.parts[0].node_state.local_to_model[3]=99;
    source.parts[0].source_triangles=8;
    source.parts[1].authored_texture="changed_other.dds";
    assert(selected.observation->parts[0].authored_texture=="hull.dds"&&
           selected.observation->parts[0].node_state.local_to_model[3]==6&&
           selected.observation->parts[0].source_triangles==12&&
           selected.observation->parts.size()==1);
    std::weak_ptr<const native11::ModelAsset> pinned_model=fixture.model;
    std::weak_ptr<const native11::TextureAsset> pinned_texture=fixture.texture;
    source.model.reset();source.parts.clear();fixture.model.reset();fixture.texture.reset();
    assert(!pinned_model.expired()&&!pinned_texture.expired()&&
           selected.observation->model==pinned_model.lock()&&
           selected.observation->parts[0].texture==pinned_texture.lock());

    Fixture fresh;
    const auto baseline=fresh.source;
    rejected(baseline,0,1,selected);
    rejected(baseline,0x3000,1,selected);
    rejected(baseline,0x1000,0,selected);
    rejected(baseline,0x1000,2,selected);
    auto bad=baseline;bad.parts.push_back(bad.parts[0]);
    bad.parts.back().node_state.visible=false;
    rejected(bad,0x1000,1,selected); // Even hidden duplicates are ambiguous.
    bad=baseline;bad.parts[0].group_index=1;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].node_index=UINT32_MAX;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].node_state_valid=false;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].node_state.visible=false;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].node_state.local_to_model[0]=
        std::numeric_limits<float>::infinity();rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].source_triangles=0;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].source_triangles=21846;rejected(bad,0x1000,1,selected);
    for(std::uint32_t flag:{2u,4u,8u}){
        bad=baseline;bad.parts[0].source_flags=flag;rejected(bad,0x1000,1,selected);
    }
    bad=baseline;bad.instance=0;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.database=0;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.model_serial=0;rejected(bad,0x1000,1,selected);
    bad=baseline;bad.model.reset();rejected(bad,0x1000,1,selected);
    auto other_model=std::make_shared<native11::ModelAsset>(*fresh.model);
    other_model->info.database=9;bad=baseline;bad.model=other_model;
    rejected(bad,0x1000,1,selected);
    other_model->info.database=3;other_model->info.serial=9;
    rejected(bad,0x1000,1,selected);
    bad=baseline;bad.parts[0].source_triangles=21845;bad.parts[0].source_flags=0;
    assert(refit::select_opaque_mesh2(bad,0x1000,1,selected,error)&&error.empty()&&
           selected.triangles==21845&&selected.observation->parts.size()==1);
}
