#include "../renderer/native11/world_lighting.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool ok,const char* name) {
    if (!ok) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); }
}
PlacedLight placed(std::uint64_t id,LightType type=LightType::point) {
    PlacedLight p{};p.source_id=id;p.config.enabled=true;p.config.light.type=type;
    p.config.light.falloff_start=100;p.config.light.falloff_range=5;
    return p;
}
void reject(const std::vector<PlacedLight>& sources,bool complete,const float ambient[3],const char* name) {
    LightingFrame output{};
    output.lights.push_back(Light{});output.source_ids.push_back(77);
    output.ambient[0]=8;output.shadows.light_mask=3;
    std::string error="old";
    need(!assemble_world_lighting(sources,complete,ambient,output,error),name);
    need(output.lights.size()==1&&output.source_ids.size()==1&&output.source_ids[0]==77&&
         output.ambient[0]==8&&output.shadows.light_mask==3,"transactional failure");
    need(!error.empty()&&error!="old","failure diagnostic");
}
}
int main() {
    const float ambient[3]{0.1f,0.2f,0.3f};
    LightingFrame frame{};std::string error="old";
    need(assemble_world_lighting({},true,ambient,frame,error)&&frame.lights.empty()&&
         frame.source_ids.empty()&&frame.ambient[2]==0.3f&&error.empty(),"empty complete world");
    reject({},false,ambient,"incomplete source list");
    reject({},true,nullptr,"null ambient");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float bad_ambient[3]{0,nan,0};
    reject({},true,bad_ambient,"nonfinite ambient");
    const float negative_ambient[3]{0,-1,0};
    reject({},true,negative_ambient,"negative ambient");
    auto point=placed(12);point.local_to_world[3]=10;point.local_to_world[7]=20;
    point.local_to_world[11]=30;point.config.cast_shadows=false;
    auto spot=placed(4,LightType::spot);spot.local_to_world[0]=0;
    spot.local_to_world[2]=1;spot.local_to_world[8]=-1;spot.local_to_world[10]=0;
    spot.local_to_world[3]=7;spot.local_to_world[7]=8;spot.local_to_world[11]=9;
    spot.config.light.direction[0]=1;spot.config.light.direction[1]=0;
    spot.config.light.direction[2]=0;
    auto disabled=placed(8);disabled.config.enabled=false;
    disabled.local_to_world[0]=nan;
    auto dark=placed(10);dark.config.light.intensity=0;
    auto black=placed(11);black.config.light.colour[0]=0;
    black.config.light.colour[1]=0;black.config.light.colour[2]=0;
    std::vector<PlacedLight> input{point,black,disabled,spot,dark};
    need(assemble_world_lighting(input,true,ambient,frame,error)&&frame.lights.size()==2&&
         frame.source_ids==std::vector<std::uint64_t>({4,12})&&error.empty(),
         "sorted active lights and skipped dark sources");
    need(frame.lights[0].type==LightType::spot&&frame.lights[0].position[0]==7&&
         frame.lights[0].position[1]==8&&frame.lights[0].position[2]==9&&
         frame.lights[0].direction[0]==0&&frame.lights[0].direction[2]==-1,
         "rotated and moved spot");
    need(frame.lights[1].position[0]==10&&frame.lights[1].position[1]==20&&
         frame.lights[1].position[2]==30&&frame.shadows.light_mask==1&&
         frame.shadows.resolution==512&&frame.shadows.depth_bias==0.001f,
         "point position and mixed shadows");
    input.clear();
    need(frame.source_ids[0]==4&&frame.lights[0].position[0]==7,"frame owns data");
    auto directional=placed(1,LightType::directional);
    directional.local_to_world[3]=99;directional.local_to_world[7]=98;
    directional.local_to_world[0]=2;directional.local_to_world[5]=3;
    need(assemble_world_lighting({directional},true,ambient,frame,error)&&
         frame.lights[0].position[0]==0&&frame.lights[0].position[1]==0&&
         frame.lights[0].direction[1]==-1,"directional ignores translation");
    auto duplicate=placed(2);duplicate.config.enabled=false;
    reject({duplicate,duplicate},true,ambient,"duplicate disabled IDs");
    duplicate.source_id=0;reject({duplicate},true,ambient,"zero disabled ID");
    std::vector<PlacedLight> many(4097);reject(many,true,ambient,"input cap");
    many.clear();for (std::uint64_t id=1;id<=9;++id) many.push_back(placed(id));
    reject(many,true,ambient,"active cap");
    auto bad=placed(1);bad.local_to_world[12]=1;reject({bad},true,ambient,"nonaffine pose");
    bad=placed(1);bad.local_to_world[0]=0;reject({bad},true,ambient,"singular pose");
    bad=placed(1);bad.local_to_world[3]=nan;reject({bad},true,ambient,"nonfinite pose");
    bad=placed(1);bad.config.light.intensity=0;bad.local_to_world[0]=0;
    reject({bad},true,ambient,"dark enabled pose still validated");
    bad=placed(1);bad.config.light.intensity=0;bad.config.light.colour[0]=nan;
    reject({bad},true,ambient,"zero intensity cannot mask NaN colour");
    bad=placed(1);bad.config.light.colour[0]=0;bad.config.light.colour[1]=0;
    bad.config.light.colour[2]=0;bad.config.light.type=static_cast<LightType>(99);
    reject({bad},true,ambient,"black source cannot mask unknown type");
    bad=placed(1,LightType::spot);bad.config.light.intensity=0;
    bad.config.light.cone_inner_degrees=70;bad.config.light.cone_outer_degrees=60;
    reject({bad},true,ambient,"zero intensity cannot mask bad cone");
    bad=placed(1);bad.config.light.colour[0]=nan;reject({bad},true,ambient,"NaN colour");
    bad=placed(1);bad.config.light.intensity=nan;reject({bad},true,ambient,"NaN intensity");
    bad=placed(1);bad.config.light.colour[0]=3e38f;bad.config.light.intensity=2;
    reject({bad},true,ambient,"energy overflow");
    bad=placed(1);bad.config.light.falloff_range=0;reject({bad},true,ambient,"invalid local range");
    bad=placed(1,LightType::spot);bad.config.light.cone_inner_degrees=70;
    bad.config.light.cone_outer_degrees=60;reject({bad},true,ambient,"invalid cone");
    bad=placed(1,LightType::directional);bad.config.light.direction[0]=0;
    bad.config.light.direction[1]=0;bad.config.light.direction[2]=0;
    reject({bad},true,ambient,"zero direction");
    bad=placed(1,LightType::spot);bad.local_to_world[0]=0;
    bad.local_to_world[1]=1;bad.local_to_world[4]=1;bad.local_to_world[5]=0;
    bad.local_to_world[10]=1;bad.config.light.direction[0]=1;
    bad.config.light.direction[1]=0;bad.config.light.direction[2]=0;
    need(assemble_world_lighting({bad},true,ambient,frame,error)&&
         frame.lights[0].direction[0]==0&&frame.lights[0].direction[1]==1,
         "transformed direction normalized");
    std::puts("native11 world lighting: PASS");
}
