#include "../renderer/native11/shadow_plan.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace a2fo::native11;
namespace {
void need(bool b,const char* name) { if (!b) { std::fprintf(stderr,"FAIL %s\n",name);std::exit(1); } }
struct Clip { double x,y,z,w; };
Clip project(const float m[16],double x,double y,double z) {
    return {double(m[0])*x+double(m[1])*y+double(m[2])*z+m[3],
            double(m[4])*x+double(m[5])*y+double(m[6])*z+m[7],
            double(m[8])*x+double(m[9])*y+double(m[10])*z+m[11],
            double(m[12])*x+double(m[13])*y+double(m[14])*z+m[15]};
}
void inside(Clip p,const char* name) {
    need(p.w>0&&p.x>=-p.w-1e-4&&p.x<=p.w+1e-4&&
         p.y>=-p.w-1e-4&&p.y<=p.w+1e-4&&p.z>=-1e-4&&p.z<=p.w+1e-4,name);
}
void unchanged(const Light& l,const ShadowBounds& b,const char* name) {
    ShadowLightPlan p{};p.count=77;
    std::fill(&p.world_to_clip[0][0],&p.world_to_clip[0][0]+96,7.25f);
    const auto before=p;
    need(!plan_shadow_light(l,b,p),name);
    need(std::memcmp(&p,&before,sizeof(p))==0,"failure output atomicity");
}
}
int main() {
    ShadowBounds box{{-2,-1,-3},{4,5,7}};
    Light l{};l.direction[0]=1;l.direction[1]=-2;l.direction[2]=3;
    ShadowLightPlan p{};
    need(plan_shadow_light(l,box,p)&&p.count==1,"directional plan");
    for (int mask=0;mask<8;++mask) {
        auto q=project(p.world_to_clip[0],mask&1?box.high[0]:box.low[0],
            mask&2?box.high[1]:box.low[1],mask&4?box.high[2]:box.low[2]);
        inside(q,"directional corner");
    }
    box={{10,20,30},{10,20,30}};
    need(plan_shadow_light(l,box,p),"flat directional bounds");
    inside(project(p.world_to_clip[0],10,20,30),"flat directional point");
    l.type=LightType::spot;l.position[0]=10;l.position[1]=20;l.position[2]=30;
    l.direction[0]=1;l.direction[1]=0;l.direction[2]=0;
    l.falloff_start=1;l.falloff_range=9;l.cone_outer_degrees=60;
    need(plan_shadow_light(l,box,p)&&p.count==1,"translated spot");
    const auto center=project(p.world_to_clip[0],15,20,30);
    inside(center,"spot center");
    need(std::abs(center.x/center.w)<1e-5&&std::abs(center.y/center.w)<1e-5,"spot axis");
    inside(project(p.world_to_clip[0],15,20,30+5/std::sqrt(3.0)),"spot rim");
    need(project(p.world_to_clip[0],10.02,20,30).z>0,"spot positive depth");
    l.type=LightType::point;
    need(plan_shadow_light(l,box,p)&&p.count==6,"point plan");
    const int dirs[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (int i=0;i<6;++i) {
        const auto q=project(p.world_to_clip[i],10+5*dirs[i][0],20+5*dirs[i][1],30+5*dirs[i][2]);
        inside(q,"point face center");
        need(std::abs(q.x/q.w)<1e-5&&std::abs(q.y/q.w)<1e-5,"point face axis");
        float d[3]={float(dirs[i][0]),float(dirs[i][1]),float(dirs[i][2])};
        need(shadow_point_face(d)==std::uint32_t(i),"face selection");
    }
    const float tie[3]={-1,1,1},zero[3]={0,0,0};
    need(shadow_point_face(tie)==1&&shadow_point_face(zero)==0,"face ties and zero");
    box={{-2,-1,-3},{4,5,7}};
    l.type=LightType::directional;l.direction[0]=l.direction[1]=l.direction[2]=0;
    unchanged(l,box,"zero direction");
    l.direction[0]=std::numeric_limits<float>::quiet_NaN();unchanged(l,box,"NaN direction");
    l.direction[0]=std::numeric_limits<float>::infinity();unchanged(l,box,"infinite direction");
    l.direction[0]=1;
    box.low[0]=5;unchanged(l,box,"inverted bounds");
    box.low[0]=std::numeric_limits<float>::quiet_NaN();unchanged(l,box,"NaN bounds");
    box.low[0]=-2;
    l.type=LightType::spot;l.cone_outer_degrees=180;unchanged(l,box,"invalid cone");
    l.cone_outer_degrees=60;l.falloff_start=0;l.falloff_range=0;unchanged(l,box,"zero far");
    l.falloff_range=10;l.position[0]=std::numeric_limits<float>::infinity();unchanged(l,box,"infinite position");
    l.position[0]=0;l.type=LightType::point;l.falloff_start=-1;unchanged(l,box,"negative falloff");
    l.type=static_cast<LightType>(99);unchanged(l,box,"unknown light type");
    std::puts("native11 shadow plan: PASS");
}
