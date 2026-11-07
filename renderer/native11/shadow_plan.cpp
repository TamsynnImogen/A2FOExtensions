#include "shadow_plan.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace a2fo::native11 {
namespace {
struct V { double x,y,z; };
V sub(V a,V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V mul(V a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(V a,V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V cross(V a,V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
bool normalize(V& v) {
    const double n=std::sqrt(dot(v,v));
    if (!(n>1e-6) || !std::isfinite(n)) return false;
    v=mul(v,1/n);
    return true;
}
bool basis(V forward,V preferred_up,V& right,V& up) {
    right=cross(preferred_up,forward);
    if (!normalize(right)) return false;
    up=cross(forward,right);
    return true;
}
bool save(const double m[16],float out[16]) {
    for (int i=0;i<16;++i) {
        if (!std::isfinite(m[i]) || std::abs(m[i])>std::numeric_limits<float>::max()) return false;
        out[i]=static_cast<float>(m[i]);
    }
    return true;
}
bool perspective(V eye,V forward,V up,double near,double far,double focal,float out[16]) {
    V right{},actual_up{};
    if (!basis(forward,up,right,actual_up)) return false;
    if (!(near>0 && far>near && std::isfinite(focal))) return false;
    const double depth=far/(far-near);
    const double tx=-dot(right,eye),ty=-dot(actual_up,eye),tz=-dot(forward,eye);
    const double m[16]={
        focal*right.x,focal*right.y,focal*right.z,focal*tx,
        focal*actual_up.x,focal*actual_up.y,focal*actual_up.z,focal*ty,
        depth*forward.x,depth*forward.y,depth*forward.z,depth*(tz-near),
        forward.x,forward.y,forward.z,tz
    };
    return save(m,out);
}
bool range(const Light& light,double& near,double& far) {
    if (!std::isfinite(light.falloff_start)||!std::isfinite(light.falloff_range)||
        light.falloff_start<0||light.falloff_range<=0) return false;
    far=double(light.falloff_start)+light.falloff_range;
    near=far*0.001;
    return std::isfinite(far)&&near>0&&near<far&&
        static_cast<float>(near)>0;
}
bool position(const Light& light,V& p) {
    for (float v:light.position) if (!std::isfinite(v)) return false;
    p={light.position[0],light.position[1],light.position[2]};
    return true;
}
bool direction(const Light& light,V& d) {
    for (float v:light.direction) if (!std::isfinite(v)) return false;
    d={light.direction[0],light.direction[1],light.direction[2]};
    return normalize(d);
}
V choose_up(V forward) { return std::abs(forward.y)>0.99 ? V{0,0,-1}:V{0,1,0}; }
bool directional(V forward,const ShadowBounds& bounds,float out[16]) {
    V right{},up{};
    if (!basis(forward,choose_up(forward),right,up)) return false;
    const V center={
        double(bounds.low[0])+(double(bounds.high[0])-bounds.low[0])*0.5,
        double(bounds.low[1])+(double(bounds.high[1])-bounds.low[1])*0.5,
        double(bounds.low[2])+(double(bounds.high[2])-bounds.low[2])*0.5};
    double lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
    for (int mask=0;mask<8;++mask) {
        const V p={mask&1?bounds.high[0]:bounds.low[0],
                   mask&2?bounds.high[1]:bounds.low[1],
                   mask&4?bounds.high[2]:bounds.low[2]};
        const V delta=sub(p,center);
        const double q[3]={dot(right,delta),dot(up,delta),dot(forward,delta)};
        for (int j=0;j<3;++j) { lo[j]=std::min(lo[j],q[j]);hi[j]=std::max(hi[j],q[j]); }
    }
    double scale[3],offset[3];
    for (int j=0;j<3;++j) {
        const double extent=hi[j]-lo[j];
        const double pad=std::max(0.001,extent*0.001);
        lo[j]-=pad;hi[j]+=pad;
        scale[j]=(j==2?1.0:2.0)/(hi[j]-lo[j]);
        if (static_cast<float>(scale[j])==0) return false;
        offset[j]=(j==2?-lo[j]:-(hi[j]+lo[j])*0.5)*scale[j];
    }
    const double m[16]={
        scale[0]*right.x,scale[0]*right.y,scale[0]*right.z,offset[0]-scale[0]*dot(right,center),
        scale[1]*up.x,scale[1]*up.y,scale[1]*up.z,offset[1]-scale[1]*dot(up,center),
        scale[2]*forward.x,scale[2]*forward.y,scale[2]*forward.z,offset[2]-scale[2]*dot(forward,center),
        0,0,0,1
    };
    if (!save(m,out)) return false;
    // Rounded float coefficients must still contain every input corner.
    for (int mask=0;mask<8;++mask) {
        const double p[4]={mask&1?bounds.high[0]:bounds.low[0],
                           mask&2?bounds.high[1]:bounds.low[1],
                           mask&4?bounds.high[2]:bounds.low[2],1};
        double q[3]{};
        for (int r=0;r<3;++r) for (int c=0;c<4;++c) q[r]+=double(out[r*4+c])*p[c];
        if (!std::isfinite(q[0])||!std::isfinite(q[1])||!std::isfinite(q[2])||
            q[0]<-1.00001||q[0]>1.00001||q[1]<-1.00001||q[1]>1.00001||
            q[2]<-0.00001||q[2]>1.00001) return false;
    }
    return true;
}
} // namespace

std::uint32_t shadow_point_face(const float direction[3]) noexcept {
    if (!direction) return 0;
    double a[3]{};
    for (int i=0;i<3;++i) {
        if (!std::isfinite(direction[i])) return 0;
        a[i]=std::abs(double(direction[i]));
    }
    const int axis=a[0]>=a[1]&&a[0]>=a[2]?0:(a[1]>=a[2]?1:2);
    return std::uint32_t(2*axis+(direction[axis]<0));
}
bool plan_shadow_light(const Light& light,const ShadowBounds& bounds,ShadowLightPlan& output) noexcept {
    for (int i=0;i<3;++i)
        if (!std::isfinite(bounds.low[i])||!std::isfinite(bounds.high[i])||bounds.low[i]>bounds.high[i]) return false;
    ShadowLightPlan candidate{};
    V eye{},forward{};
    switch (light.type) {
    case LightType::directional:
        if (!direction(light,forward)||!directional(forward,bounds,candidate.world_to_clip[0])) return false;
        candidate.count=1;
        break;
    case LightType::spot: {
        double near,far;
        if (!position(light,eye)||!direction(light,forward)||!range(light,near,far)||
            !std::isfinite(light.cone_outer_degrees)||
            !(light.cone_outer_degrees>0&&light.cone_outer_degrees<180)) return false;
        const double focal=1/std::tan(double(light.cone_outer_degrees)*0.5*3.14159265358979323846/180);
        if (!perspective(eye,forward,choose_up(forward),near,far,focal,candidate.world_to_clip[0])) return false;
        candidate.count=1;
        break;
    }
    case LightType::point: {
        double near,far;
        if (!position(light,eye)||!range(light,near,far)) return false;
        const V axes[6]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        const V ups[6]={{0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0}};
        for (int i=0;i<6;++i)
            if (!perspective(eye,axes[i],ups[i],near,far,1,candidate.world_to_clip[i])) return false;
        candidate.count=6;
        break;
    }
    default: return false;
    }
    output=candidate;
    return true;
}
} // namespace a2fo::native11
