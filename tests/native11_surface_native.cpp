// Private HWND-free GPU fixture for the opt-in surface stages. No engine entry.
// Compile with -DNATIVE11_SURFACE_STAGE=N to stop at a particular stage.
#include "../renderer/native11/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
using namespace a2fo::native11;
#ifndef NATIVE11_SURFACE_STAGE
#define NATIVE11_SURFACE_STAGE 33
#endif
namespace {
void need(bool value,const char* what){if(!value){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
void ok(HRESULT hr,const char* what){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s hr=%08lx\n",what,static_cast<unsigned long>(hr));std::exit(1);}}
void check_near(int value,int expected,int tolerance,const char* what){
    if(std::abs(value-expected)>tolerance){std::fprintf(stderr,"FAIL %s got=%d expected=%d\n",what,value,expected);std::exit(1);}
}
struct Image {std::vector<std::uint8_t> bytes;unsigned width=0,height=0;
    int at(unsigned x,unsigned y,unsigned channel=2)const{return bytes[(std::size_t(y)*width+x)*4+channel];}
};
Image read(Renderer& r){Image out;ok(r.readback(out.bytes,out.width,out.height),"readback");
    need(out.width==32&&out.height==32&&out.bytes.size()==4096,"readback dimensions");return out;}
std::uint32_t mesh(Renderer& r,float z,float scale=1,float u=0,float v=0){
    const Vertex vertices[3]={{{-scale,-scale,z},{0,0,-1},{u,v}},{{3*scale,-scale,z},{0,0,-1},{u,v}},
        {{-scale,3*scale,z},{0,0,-1},{u,v}}};
    const std::uint32_t indices[3]{0,1,2};std::uint32_t handle=0;
    ok(r.upload_mesh({vertices,3,indices,3},handle),"mesh upload");return handle;
}
[[maybe_unused]] std::uint32_t texture(Renderer& r,const std::vector<std::uint8_t>& bgra,unsigned width,unsigned height){
    std::uint32_t handle=0;ok(r.upload_texture({bgra.data(),width,height,width*4,bgra.size()},handle),"texture upload");return handle;
}
Draw draw(std::uint32_t mesh_handle,float red,float green,float blue,float alpha=1){
    Draw d{};d.mesh=mesh_handle;d.flags=two_sided;
    d.colour[0]=red;d.colour[1]=green;d.colour[2]=blue;d.colour[3]=alpha;return d;
}
FrameView frame(Draw* draws,unsigned count){FrameView f{};f.draws=draws;f.draw_count=count;
    f.ambient[0]=f.ambient[1]=f.ambient[2]=1;
    f.clear_colour[0]=f.clear_colour[1]=f.clear_colour[2]=0;return f;
}
void unchanged(Renderer& r,const Image& previous,const FrameView& bad,const char* what){
    need(r.render(bad,false)==E_INVALIDARG,what);need(read(r).bytes==previous.bytes,"invalid render preserves colour");
}
#if NATIVE11_SURFACE_STAGE >= 27
void ordering(Renderer& r){
    const auto front=mesh(r,.2f),back=mesh(r,.8f);
    Draw draws[2]{draw(front,0,1,0,.5f),draw(back,1,0,0)};draws[0].flags|=alpha_blend;
    auto f=frame(draws,2);ok(r.render(f,false),"capture ordering control");
    auto before=read(r);check_near(before.at(16,16),255,3,"capture order last opaque red");
    f.ordering=DrawOrdering::opaque_then_alpha;ok(r.render(f,false),"opaque then alpha");
    auto after=read(r);check_near(after.at(16,16),128,4,"alpha over opaque red");
    check_near(after.at(16,16,1),128,4,"alpha over opaque green");
    Draw transparent[2]{draw(front,1,0,0,.5f),draw(back,0,0,1,.5f)};
    transparent[0].flags|=alpha_blend;transparent[1].flags|=alpha_blend;
    f.draws=transparent;ok(r.render(f,false),"transparent depth sort");after=read(r);
    check_near(after.at(16,16),128,5,"near alpha red last");check_near(after.at(16,16,0),64,5,"far alpha blue first");
    FrameView bad=f;bad.ordering=static_cast<DrawOrdering>(99);unchanged(r,after,bad,"invalid ordering");
    bad=f;bad.world_to_clip[15]=0;unchanged(r,after,bad,"zero projected W");
    bad=f;bad.world_to_clip[15]=-1;unchanged(r,after,bad,"negative projected W");
    transparent[0].layer=1;transparent[1].layer=0;ok(r.render(f,false),"layered ordering");
    after=read(r);check_near(after.at(16,16),128,5,"layer ordering front red");
    transparent[0].layer=transparent[1].layer=0;transparent[1].mesh=front;
    ok(r.render(f,false),"stable equal-depth alpha order");after=read(r);
    check_near(after.at(16,16,0),128,5,"equal-depth last blue retained");
    check_near(after.at(16,16),64,5,"equal-depth first red retained");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 28
void surfaces(Renderer& r){
    const auto front=mesh(r,.2f),back=mesh(r,.8f),alpha=texture(r,{255,255,255,128},1,1);
    Draw d=draw(front,1,1,1);d.texture=alpha;d.flags|=cutout;
    auto f=frame(&d,1);d.alpha_cutoff=.75f;ok(r.render(f,false),"high cutout cutoff");
    auto image=read(r);check_near(image.at(16,16),0,2,"cutout rejected");
    d.alpha_cutoff=.25f;ok(r.render(f,false),"low cutout cutoff");image=read(r);
    need(image.at(16,16)>120,"cutout admitted");
    d.alpha_cutoff=std::numeric_limits<float>::quiet_NaN();unchanged(r,image,f,"nonfinite cutoff");
    d.alpha_cutoff=.5f;
    Draw pair[2]{draw(back,1,1,1),draw(front,1,1,1)};pair[1].shadow_only=true;
    Light light{};light.direction[0]=1;light.direction[1]=0;light.direction[2]=1;
    f=frame(pair,2);f.ambient[0]=f.ambient[1]=f.ambient[2]=.1f;
    f.lights=&light;f.light_count=1;f.shadows.light_mask=1;f.shadows.resolution=256;
    ok(r.render(f,false),"caster baseline");auto baseline=read(r);
    pair[1].casts_shadow=false;ok(r.render(f,false),"caster opt out");auto no_cast=read(r);
    need(no_cast.at(24,16)>baseline.at(24,16)+30,"cast shadow opt out removes footprint");
    pair[1].casts_shadow=true;pair[0].receives_shadow=false;
    ok(r.render(f,false),"receiver opt out");auto no_receive=read(r);
    need(no_receive.at(24,16)>baseline.at(24,16)+30,"receive opt out bypasses occlusion");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 29
void composition(Renderer& r){
    const auto front=mesh(r,.2f),back=mesh(r,.8f),small=mesh(r,.1f,.3f);
    Draw pair[2]{draw(front,1,0,0),draw(back,0,1,0)};auto f=frame(pair,2);
    ok(r.render(f,false),"automatic depth");check_near(read(r).at(16,16),255,3,"automatic writes depth");
    pair[0].depth_mode=DepthMode::read_only;ok(r.render(f,false),"read-only depth");
    check_near(read(r).at(16,16,1),255,3,"read-only permits later far surface");
    pair[0].depth_mode=DepthMode::automatic;pair[1].depth_mode=DepthMode::disabled;
    ok(r.render(f,false),"disabled depth");check_near(read(r).at(16,16,1),255,3,"disabled overlays near surface");
    Draw red=draw(front,1,0,0);f=frame(&red,1);ok(r.render(f,false),"load target base");
    Draw green=draw(small,0,1,0);f.draws=&green;f.clear_colour_target=false;f.clear_depth_target=false;
    ok(r.render(f,false),"load target overlay");auto loaded=read(r);
    check_near(loaded.at(16,16,1),255,3,"loaded target new centre");
    check_near(loaded.at(2,2),255,3,"loaded target old outer red");
    green.depth_mode=static_cast<DepthMode>(99);unchanged(r,loaded,f,"invalid depth mode");
    Renderer fresh;ok(fresh.initialize_offscreen(32,32),"fresh composition target");
    Draw fresh_draw=draw(mesh(fresh,.5f),1,0,0);auto fresh_frame=frame(&fresh_draw,1);
    fresh_frame.clear_colour_target=fresh_frame.clear_depth_target=false;
    need(fresh.render(fresh_frame,false)==E_INVALIDARG,"uninitialized target cannot be loaded");
    fresh_frame.clear_colour_target=fresh_frame.clear_depth_target=true;
    ok(fresh.render(fresh_frame,false),"initialize target before load");
    ok(fresh.resize(16,16),"composition resize invalidates load contents");
    fresh_frame.clear_colour_target=fresh_frame.clear_depth_target=false;
    need(fresh.render(fresh_frame,false)==E_INVALIDARG,"recreated target cannot load old contents");
    fresh_frame.clear_colour_target=fresh_frame.clear_depth_target=true;
    ok(fresh.render(fresh_frame,false),"recreated target recovers on clear");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 30
void sampling(Renderer& r){
    const auto plane=mesh(r,.5f,1,1.25f,.5f);
    const auto tex=texture(r,{0,0,255,255,0,255,0,255},2,1);
    Draw d=draw(plane,1,1,1);d.texture=tex;auto f=frame(&d,1);
    d.sampler=Sampling::point_clamp;ok(r.render(f,false),"point clamp");auto clamped=read(r);
    check_near(clamped.at(16,16,1),255,3,"clamped green texel");
    d.sampler=Sampling::point_wrap;ok(r.render(f,false),"point wrap");auto wrapped=read(r);
    check_near(wrapped.at(16,16),255,3,"wrapped red texel");
    d.mesh=mesh(r,.5f,1,1.5f,.5f);d.sampler=Sampling::linear_wrap;
    ok(r.render(f,false),"linear wrap interpolation");auto interpolated=read(r);
    check_near(interpolated.at(16,16),128,5,"linear wrap red interpolation");
    check_near(interpolated.at(16,16,1),128,5,"linear wrap green interpolation");
    d.sampler=Sampling::linear_clamp;ok(r.render(f,false),"linear clamp edge");auto linear_clamped=read(r);
    check_near(linear_clamped.at(16,16,1),255,3,"linear clamp preserves green edge");
    d.sampler=static_cast<Sampling>(99);unchanged(r,linear_clamped,f,"invalid sampler");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 31
void uv_transform(Renderer& r){
    const auto plane=mesh(r,.5f,1,.25f,.5f);
    const auto tex=texture(r,{0,0,255,255,0,255,0,255},2,1);
    Draw d=draw(plane,1,1,1);d.texture=tex;d.sampler=Sampling::point_clamp;
    auto f=frame(&d,1);ok(r.render(f,false),"UV identity");auto identity=read(r);
    check_near(identity.at(16,16),255,3,"identity red texel");
    d.uv_transform[2]=.5f;ok(r.render(f,false),"UV translation");auto shifted=read(r);
    check_near(shifted.at(16,16,1),255,3,"translated green texel");
    d.uv_transform[2]=std::numeric_limits<float>::quiet_NaN();unchanged(r,shifted,f,"invalid UV transform");
    d.mesh=mesh(r,.5f,1,std::numeric_limits<float>::max(),.5f);
    d.uv_transform[2]=0;d.uv_transform[0]=2;
    unchanged(r,shifted,f,"UV arithmetic overflow rejected before rendering");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 32
void fog(Renderer& r){
    const auto plane=mesh(r,.5f);Draw d=draw(plane,1,1,1);auto f=frame(&d,1);
    ok(r.render(f,false),"fog default off");check_near(read(r).at(16,16),255,3,"default no fog");
    f.fog.enabled=true;f.fog.start=0;f.fog.end=1;
    ok(r.render(f,false),"radial linear fog");auto fogged=read(r);
    need(fogged.at(16,16)>100&&fogged.at(16,16)<155,"half-distance fog");
    d.fog_enabled=false;ok(r.render(f,false),"per-draw fog opt out");
    check_near(read(r).at(16,16),255,3,"per-draw fog bypass");
    d.fog_enabled=true;f.fog.end=0;auto prior=read(r);unchanged(r,prior,f,"invalid fog interval");
}
#endif
#if NATIVE11_SURFACE_STAGE >= 33
void shadow_filter(Renderer& r){
    const auto receiver=mesh(r,.8f,.95f),blocker=mesh(r,.3f,.2f);
    Draw draws[2]{draw(receiver,1,1,1),draw(blocker,1,1,1)};
    Light light{};light.direction[0]=.5f;light.direction[1]=0;light.direction[2]=1;
    auto f=frame(draws,2);f.ambient[0]=f.ambient[1]=f.ambient[2]=.1f;
    f.lights=&light;f.light_count=1;f.shadows.light_mask=1;f.shadows.resolution=128;
    ok(r.render(f,false),"default bilinear shadow");auto bilinear=read(r);
    f.shadows.filter=ShadowFilter::hard;ok(r.render(f,false),"hard shadow");auto hard=read(r);
    f.shadows.filter=ShadowFilter::pcf3;ok(r.render(f,false),"PCF3 shadow");auto soft=read(r);
    need(hard.at(24,16)<hard.at(6,16),"hard filter retains footprint");
    need(soft.at(24,16)<soft.at(6,16),"PCF3 retains footprint");
    need(bilinear.bytes!=hard.bytes||soft.bytes!=hard.bytes,"filter selection changes edge pixels");
    f.shadows.filter=static_cast<ShadowFilter>(99);unchanged(r,soft,f,"invalid shadow filter");
}
#endif
void run(){Renderer r;ok(r.initialize_offscreen(32,32),"offscreen renderer");
#if NATIVE11_SURFACE_STAGE >= 27
    ordering(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 28
    surfaces(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 29
    composition(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 30
    sampling(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 31
    uv_transform(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 32
    fog(r);
#endif
#if NATIVE11_SURFACE_STAGE >= 33
    shadow_filter(r);
#endif
    FrameView boundary{};boundary.shadows.depth_bias=.1f;
    ok(r.render(boundary,false),"maximum binary32 shadow bias accepted");
    const auto boundary_image=read(r);
    boundary.shadows.depth_bias=std::nextafter(.1f,std::numeric_limits<float>::infinity());
    unchanged(r,boundary_image,boundary,"shadow bias above allowed binary32 endpoint refused");
    std::puts("PASS native11 private surface stages");
}
}
int main(){run();}
