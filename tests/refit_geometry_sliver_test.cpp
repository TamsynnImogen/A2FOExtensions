// Host test for the sliver-tolerant fallback of check_geometry_parity2.
// Identity view/projection, viewport 100x100: NDC (x,y) -> ((x+1)*50,(1-y)*50).
#include "../renderer/refit/geometry_parity.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace a2fo::refit;
namespace {
constexpr std::array<float,12> world={1,0,0,0,1,0,0,0,1,0,0,0};
constexpr std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
constexpr std::array<float,4> viewport={0,0,100,100};
ProjectedTriangle2 project(const AuthoredTriangle2& t){
    ProjectedTriangle2 p{};
    for(unsigned i=0;i<3;++i)p[i]={(t[i].x+1)*50.f,(1-t[i].y)*50.f,t[i].z,t[i].u,t[i].v,1.f};
    return p;
}
// Solid triangles of about 12.5 px^2 and slivers of about 1e-4 px^2. Screen
// winding with y flipped: these authored corners give negative signed area.
AuthoredTriangle2 solid(float x){return {{{x,0,.5f,0,0},{x+.1f,0,.5f,1,0},{x,.1f,.5f,0,1}}};}
AuthoredTriangle2 sliver(float x){return {{{x,.5f,.5f,0,0},{x+.1f,.5f,.5f,1,0},{x+.05f,.500004f,.5f,.5f,1}}};}
AuthoredTriangle2 flipped(AuthoredTriangle2 t){std::swap(t[1],t[2]);return t;}
int failures=0;
void expect(bool want,const std::vector<AuthoredTriangle2>& authored,const std::vector<ProjectedTriangle2>& native,
            GeometryCull2 cull,const char* label,std::uint32_t want_visible=0){
    std::uint32_t visible=999;std::string error="unset";
    const bool got=check_geometry_parity2(authored,world,identity,identity,viewport,cull,native,visible,error);
    const bool ok=got==want&&(want?error.empty()&&visible==want_visible:visible==999&&!error.empty());
    if(!ok){++failures;std::cerr<<label<<": got="<<got<<" visible="<<visible<<" error="<<error<<'\n';}
}
}
int main(){
    // Facing convention for this fixture: solid() projects with negative area.
    const auto a=project(solid(-.8f));
    const double signed_area=(double(a[1].x)-a[0].x)*(double(a[2].y)-a[0].y)-(double(a[1].y)-a[0].y)*(double(a[2].x)-a[0].x);
    const auto front=signed_area<0?GeometryCull2::Clockwise:GeometryCull2::CounterClockwise;
    const std::vector<AuthoredTriangle2> base{solid(-.8f),solid(-.4f),solid(0.f),solid(.4f)};
    std::vector<ProjectedTriangle2> native;for(const auto& t:base)native.push_back(project(t));
    {const auto s=project(sliver(0));const double ar=std::abs((double(s[1].x)-s[0].x)*(double(s[2].y)-s[0].y)-(double(s[1].y)-s[0].y)*(double(s[2].x)-s[0].x))/2;
     if(!(ar>1e-8&&ar<geometry_sliver_area2)){std::cerr<<"fixture sliver area "<<ar<<'\n';return 1;}}

    expect(true,base,native,front,"strict exact match",4);
    // Expected sliver absent natively (live rom_talon case).
    auto with_sliver=base;with_sliver.push_back(sliver(-.2f));
    expect(true,with_sliver,native,front,"expected sliver absent natively",4);
    // Same sliver present natively: strict path still succeeds with all 5.
    auto native5=native;native5.push_back(project(sliver(-.2f)));
    expect(true,with_sliver,native5,front,"expected sliver present natively",5);
    // A sliver this projection culls (opposite facing) but the engine drew.
    auto with_culled=base;with_culled.push_back(flipped(sliver(-.2f)));
    auto native_flip=native;native_flip.push_back(project(flipped(sliver(-.2f))));
    expect(true,with_culled,native_flip,front,"culled sliver drawn natively",5);
    // Missing a SOLID triangle must still fail.
    auto native3=native;native3.pop_back();
    expect(false,base,native3,front,"missing solid triangle");
    expect(false,with_sliver,native3,front,"missing solid with expected sliver present");
    // A native sliver with no authored counterpart must fail.
    auto native_extra=native;native_extra.push_back(project(sliver(.7f)));
    expect(false,with_sliver,native_extra,front,"unmatched native sliver");
    // Two native copies of one authored sliver must fail (one-to-one).
    auto native_dup=native;native_dup.push_back(project(sliver(-.2f)));native_dup.push_back(project(sliver(-.2f)));
    expect(false,with_sliver,native_dup,front,"duplicate native sliver");
    // A solid triangle with a wrong UV still fails even when slivers exist.
    auto native_uv=native;native_uv[1][0].u+=.5f;
    expect(false,with_sliver,native_uv,front,"solid uv mismatch with sliver present");
    // A solid triangle with a different reciprocal-W scale still fails.
    auto native_rhw=native;for(auto& v:native_rhw[2])v.rhw=2.f;
    expect(false,with_sliver,native_rhw,front,"solid rhw scale mismatch with sliver present");
    if(failures)return 1;
    std::cout<<"sliver fallback: absent, present, culled-drawn, solid-missing, unmatched, duplicate, uv, rhw verified\n";
    return 0;
}
