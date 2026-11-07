// Host test for refit::select_opaque_group2 (two-group legacy meshes).
#include "../renderer/refit/mesh_scope.hpp"
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

using namespace a2fo;
namespace {
native11::MaterialObservation two_group(){
    auto model=std::make_shared<native11::ModelAsset>();
    model->info.database=3;model->info.serial=4;
    native11::MaterialObservation s{};
    s.instance=2;s.database=3;s.model_serial=4;s.native_x87_control=0x1234;s.model=model;
    native11::MaterialBinding team{};
    team.native_node=0x1000;team.node_index=5;team.group_index=0;team.source_triangles=24;
    team.node_state_valid=true;team.authored_texture="saber.dds";
    auto base=team;base.group_index=1;base.source_triangles=734;
    auto other=team;other.native_node=0x2000;other.node_index=6;
    s.parts={team,base,other};
    return s;
}
void rejected(const native11::MaterialObservation& s,std::uint32_t node,std::uint32_t count,std::uint32_t group){
    refit::MeshSelection2 out{};out.triangles=77;std::string error="old";
    assert(!refit::select_opaque_group2(s,node,count,group,out,error));
    assert(!error.empty()&&out.triangles==77&&!out.observation);
}
}
int main(){
    const auto s=two_group();
    for(std::uint32_t g:{0u,1u}){
        refit::MeshSelection2 out{};std::string error="old";
        assert(refit::select_opaque_group2(s,0x1000,2,g,out,error)&&error.empty());
        assert(out.observation&&out.observation->parts.size()==1&&out.group_index==g&&
               out.node_index==5&&out.triangles==(g?734u:24u)&&
               out.observation->parts[0].group_index==g&&out.observation->native_x87_control==0x1234);
    }
    // Single-group admission is unchanged: the old selector still refuses two groups.
    {refit::MeshSelection2 out{};std::string error;
     assert(!refit::select_opaque_mesh2(s,0x1000,2,out,error)&&!error.empty());}
    rejected(s,0x1000,1,0);            // only exactly two groups
    rejected(s,0x1000,3,0);
    rejected(s,0x1000,2,2);            // index out of range
    rejected(s,0,2,0);
    rejected(s,0x3000,2,0);            // node unobserved
    auto bad=s;bad.parts.erase(bad.parts.begin()+1);rejected(bad,0x1000,2,0); // missing group 1 binding
    bad=s;bad.parts.push_back(bad.parts[0]);rejected(bad,0x1000,2,1);          // duplicate (node,group)
    bad=s;bad.parts[1].group_index=2;rejected(bad,0x1000,2,0);                 // binding out of range
    bad=s;bad.parts[1].node_index=9;rejected(bad,0x1000,2,0);                  // groups disagree on node
    bad=s;bad.parts[0].node_state.visible=false;rejected(bad,0x1000,2,0);
    {   // a hidden sibling does not block the visible group
        refit::MeshSelection2 out{};std::string error;
        assert(refit::select_opaque_group2(bad,0x1000,2,1,out,error));
    }
    bad=s;bad.parts[1].node_state_valid=false;rejected(bad,0x1000,2,1);
    bad=s;bad.parts[1].node_state.local_to_model[0]=std::numeric_limits<float>::infinity();rejected(bad,0x1000,2,1);
    bad=s;bad.parts[1].source_triangles=0;rejected(bad,0x1000,2,1);
    bad=s;bad.parts[1].source_triangles=21846;rejected(bad,0x1000,2,1);
    for(std::uint32_t flag:{2u,4u,8u}){bad=s;bad.parts[1].source_flags=flag;rejected(bad,0x1000,2,1);}
    bad=s;bad.parts[1].node_index=UINT32_MAX;bad.parts[0].node_index=UINT32_MAX;rejected(bad,0x1000,2,1);
    bad=s;bad.instance=0;rejected(bad,0x1000,2,0);
    bad=s;bad.model.reset();rejected(bad,0x1000,2,0);
    auto other=std::make_shared<native11::ModelAsset>(*s.model);other->info.serial=9;
    bad=s;bad.model=other;rejected(bad,0x1000,2,0);
    return 0;
}
