#include "../renderer/native11/frame_bundle.hpp"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* label) {
    if(!yes){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}
}
std::shared_ptr<PrimitiveFrame> packet() {
    auto p=std::make_shared<PrimitiveFrame>();
    p->generation=7;p->source_epoch=3;
    p->settings.viewport[2]=64;p->settings.viewport[3]=32;
    PrimitiveMesh m{};m.id=11;m.vertices.resize(3);m.indices={0,1,2};
    p->meshes.push_back(m);
    PrimitiveTexture t{};t.id=12;t.width=t.height=1;t.bgra={1,2,3,4};
    p->textures.push_back(t);
    PrimitiveDraw d{};d.mesh=11;d.texture=12;p->draws.push_back(d);
    return p;
}
FrameBundle sample() {
    FrameBundle b{};b.identity={5,7,3,9};b.width=64;b.height=32;
    b.complete_geometry=true;b.absent_domains=all_frame_domains&~3u;
    b.passes.push_back({FrameDomain::background,packet()});
    auto second=packet();second->settings.clear_colour_target=false;
    b.passes.push_back({FrameDomain::terrain,second});return b;
}
void check(bool expected,FrameBundle b,const char* label) {
    std::string error="old";
    const bool got=validate_frame_bundle(b,error);
    need(got==expected,label);
    need(expected?error.empty():!error.empty(),"error output state");
}
}
int main() {
    check(true,sample(),"complete ordered bundle");
    auto b=sample();b.absent_domains&=~4u;check(false,b,"missing domain");
    b=sample();b.absent_domains|=2;check(false,b,"present and absent conflict");
    b=sample();b.absent_domains|=128;check(false,b,"unknown absent domain");
    b=sample();b.passes[1].domain=FrameDomain::background;check(false,b,"missing terrain after repeat");
    b=sample();b.passes[0].domain=FrameDomain::terrain;
    b.passes[1].domain=FrameDomain::background;check(false,b,"unordered passes");
    b=sample();b.passes[1].frame.reset();check(false,b,"null pass");
    b=sample();b.identity.source_ticket=0;check(false,b,"zero source ticket");
    b=sample();auto p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->source_epoch=4;b.passes[1].frame=p;check(false,b,"mixed source epoch");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->generation=8;b.passes[1].frame=p;check(false,b,"mixed generation");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->textures[0].bgra[0]=7;b.passes[1].frame=p;check(false,b,"manifest bytes");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->meshes[0].id=13;p->draws[0].mesh=13;b.passes[1].frame=p;
    check(false,b,"manifest IDs");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    auto extra=p->meshes[0];extra.id=14;
    auto first=std::make_shared<PrimitiveFrame>(*b.passes[0].frame);
    first->meshes.push_back(extra);p->meshes.insert(p->meshes.begin(),extra);
    b.passes[0].frame=first;b.passes[1].frame=p;
    check(false,b,"manifest order");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->settings.clear_colour_target=true;b.passes[1].frame=p;check(false,b,"later colour clear");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[0].frame);
    p->settings.clear_depth_target=false;b.passes[0].frame=p;check(false,b,"first depth load");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[0].frame);
    p->settings.viewport[2]=0;b.passes[0].frame=p;check(false,b,"implicit viewport");
    b=sample();b.width=4097;check(false,b,"target bound");
    b=sample();b.complete_geometry=false;check(false,b,"host completion declaration");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->settings.draw_count=1;b.passes[1].frame=p;check(false,b,"borrowed view count");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->draws.resize(8192);b.passes[1].frame=p;check(false,b,"aggregate draw bound");
    b=sample();p=std::make_shared<PrimitiveFrame>(*b.passes[1].frame);
    p->textures[0].bgra.resize(128ull*1024*1024);
    b.passes[1].frame=p;check(false,b,"payload and shape bound before comparison");
    b=sample();b.passes.resize(17,b.passes[1]);check(false,b,"pass bound");
    b=sample();auto large=packet();large->textures[0].width=3072;
    large->textures[0].height=4096;
    large->textures[0].bgra.resize(3072ull*4096*4);
    auto later=std::make_shared<PrimitiveFrame>(*large);
    later->settings.clear_colour_target=false;
    b.passes[0].frame=large;b.passes[1].frame=later;
    b.passes.push_back({FrameDomain::terrain,later});
    check(false,b,"aggregate payload bound");
    b=sample();auto repeat=packet();repeat->settings.clear_colour_target=false;
    b.passes.insert(b.passes.begin()+1,{FrameDomain::background,repeat});
    check(true,b,"repeated ordered domain");
    std::puts("PASS native11 complete-frame CPU admission");
}
