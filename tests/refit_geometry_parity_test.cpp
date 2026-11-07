#include "../renderer/refit/geometry_parity.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo::refit;
namespace {
const std::array<float,12> identity_world={1,0,0,0,1,0,0,0,1,0,0,0};
const std::array<float,16> identity4={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
const std::array<float,16> perspective={1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1};
const std::array<float,4> viewport={10,20,100,80};
const AuthoredTriangle2 face={{{-1,-1,1,0,0},{1,-1,1,1,0},{0,1,1,0.5f,1}}};
const ProjectedTriangle2 projected={{{35,80,0.5f,0,0,0.5f},
                                    {85,80,0.5f,1,0,0.5f},
                                    {60,40,0.5f,0.5f,1,0.5f}}};
void passes(const std::vector<AuthoredTriangle2>& authored,
            const std::array<float,12>& world,GeometryCull2 cull,
            const std::vector<ProjectedTriangle2>& native,std::uint32_t count){
    std::uint32_t output=999;std::string error="old";
    assert(check_geometry_parity2(authored,world,identity4,perspective,viewport,
                                  cull,native,output,error));
    assert(output==count&&error.empty());
}
void rejects(const std::vector<AuthoredTriangle2>& authored,
             const std::array<float,12>& world,
             const std::array<float,16>& view,
             const std::array<float,16>& projection,
             const std::array<float,4>& vp,GeometryCull2 cull,
             const std::vector<ProjectedTriangle2>& native){
    std::uint32_t output=999;std::string error="old";
    assert(!check_geometry_parity2(authored,world,view,projection,vp,cull,
                                   native,output,error));
    assert(output==999&&!error.empty());
}
void rejects(const std::vector<AuthoredTriangle2>& authored,
             GeometryCull2 cull,const std::vector<ProjectedTriangle2>& native){
    rejects(authored,identity_world,identity4,perspective,viewport,cull,native);
}
}
int main(){
    passes({face},identity_world,GeometryCull2::None,{projected},1);
    auto scaled_rhw=projected;
    for(auto& vertex:scaled_rhw)vertex.rhw*=2;
    passes({face},identity_world,GeometryCull2::None,{scaled_rhw},1);
    auto varied_w=face;varied_w[2].z=2;
    auto varied_projected=projected;
    varied_projected[2].y=46.666668f;
    varied_projected[2].z=2.f/3.f;
    varied_projected[2].rhw=1.f/3.f;
    passes({varied_w},identity_world,GeometryCull2::None,{varied_projected},1);
    scaled_rhw=varied_projected;
    for(auto& vertex:scaled_rhw)vertex.rhw*=2;
    passes({varied_w},identity_world,GeometryCull2::None,{scaled_rhw},1);
    scaled_rhw[2].rhw*=1.01f;
    rejects({varied_w},GeometryCull2::None,{scaled_rhw});
    for(float invalid:{0.f,-0.5f,std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}){
        auto bad_rhw=varied_projected;bad_rhw[0].rhw=invalid;
        rejects({varied_w},GeometryCull2::None,{bad_rhw});
    }
    passes({face},identity_world,GeometryCull2::Clockwise,{projected},1);
    passes({face},identity_world,GeometryCull2::CounterClockwise,{},0);
    auto rotated=projected;
    std::rotate(rotated.begin(),rotated.begin()+1,rotated.end());
    passes({face},identity_world,GeometryCull2::None,{rotated},1);
    passes({face,face},identity_world,GeometryCull2::None,{rotated,projected},2);
    rejects({face,face},GeometryCull2::None,{projected}); // multiplicity
    rejects({face},GeometryCull2::None,{projected,projected}); // extra
    rejects({face},GeometryCull2::None,{}); // source-budget reduction
    auto changed=projected;changed[1].u+=0.001f;
    rejects({face},GeometryCull2::None,{changed});
    changed=projected;changed[1].z+=0.001f;
    rejects({face},GeometryCull2::None,{changed});
    changed=projected;changed[1].x+=0.01f;
    rejects({face},GeometryCull2::None,{changed});
    changed=projected;std::swap(changed[1],changed[2]);
    rejects({face},GeometryCull2::None,{changed}); // reversed winding
    changed=projected;changed[1].x+=0.001f;
    passes({face},identity_world,GeometryCull2::None,{changed},1);

    auto translated=identity_world;translated[9]=0.2f;
    auto raised_view=identity4;raised_view[13]=0.5f;
    auto offset=projected;
    for(auto& vertex:offset){vertex.x+=5;vertex.y-=10;}
    std::uint32_t offset_count=999;std::string offset_error="old";
    assert(check_geometry_parity2({face},translated,raised_view,perspective,viewport,
                                  GeometryCull2::None,{offset},offset_count,offset_error));
    assert(offset_count==1&&offset_error.empty());

    auto mirrored=identity_world;mirrored[0]=-1;
    auto reflected=projected;
    reflected[0].x=85;reflected[1].x=35;
    passes({face},mirrored,GeometryCull2::CounterClockwise,{reflected},1);
    passes({face},mirrored,GeometryCull2::Clockwise,{},0);
    rejects({face},mirrored,identity4,perspective,viewport,
            GeometryCull2::CounterClockwise,{projected});

    auto outside=face;outside[0].x=-3;
    rejects({outside},GeometryCull2::None,{});
    auto behind=face;behind[0].z=-2;
    rejects({behind},GeometryCull2::None,{});
    auto degenerate=face;degenerate[2]=degenerate[1];
    rejects({degenerate},GeometryCull2::None,{});
    changed=projected;changed[0].x=changed[1].x;
    changed[0].y=changed[1].y;
    rejects({face},GeometryCull2::None,{changed});
    outside=face;outside[0].u=std::numeric_limits<float>::quiet_NaN();
    rejects({outside},GeometryCull2::None,{});
    changed=projected;changed[0].z=std::numeric_limits<float>::infinity();
    rejects({face},GeometryCull2::None,{changed});
    auto bad_world=identity_world;bad_world[0]=0;
    rejects({face},bad_world,identity4,perspective,viewport,GeometryCull2::None,{projected});
    auto bad_view=identity4;bad_view[1]=std::numeric_limits<float>::infinity();
    rejects({face},identity_world,bad_view,perspective,viewport,GeometryCull2::None,{projected});
    bad_view=identity4;bad_view[0]=0;
    rejects({face},identity_world,bad_view,perspective,viewport,GeometryCull2::None,{projected});
    auto bad_vp=viewport;bad_vp[2]=0;
    rejects({face},identity_world,identity4,perspective,bad_vp,GeometryCull2::None,{projected});
    rejects({face},identity_world,identity4,perspective,viewport,
            static_cast<GeometryCull2>(3),{projected});
    rejects(std::vector<AuthoredTriangle2>(2049,face),GeometryCull2::None,{});
    rejects({face},GeometryCull2::None,std::vector<ProjectedTriangle2>(2049,projected));
}
