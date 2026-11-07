// Private HWND-free GPU fixture for the CPU quad builders. No engine entry.
// Compile with -DNATIVE11_QUAD_STAGE=34 or 35; default exercises both.
#include "../renderer/native11/quad_geometry.hpp"
#include "../renderer/native11/renderer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace a2fo::native11;
#ifndef NATIVE11_QUAD_STAGE
#define NATIVE11_QUAD_STAGE 35
#endif
static_assert(NATIVE11_QUAD_STAGE==34 || NATIVE11_QUAD_STAGE==35,
              "select quad stage 34 or 35");

namespace {
void need(bool condition,const char* label) {
    if (!condition) { std::fprintf(stderr,"FAIL %s\n",label); std::exit(1); }
}
void ok(HRESULT hr,const char* label) {
    if (FAILED(hr)) { std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,
                                  static_cast<unsigned long>(hr)); std::exit(1); }
}
struct Image {
    std::vector<std::uint8_t> bgra;
    std::uint32_t width=0,height=0;
    int at(unsigned x,unsigned y,unsigned channel) const {
        return bgra[(std::size_t(y)*width+x)*4+channel];
    }
};
Image read(Renderer& renderer) {
    Image image;
    ok(renderer.readback(image.bgra,image.width,image.height),"quad readback");
    need(image.width==32 && image.height==32 && image.bgra.size()==4096,
         "quad readback dimensions");
    return image;
}
std::uint32_t upload(Renderer& renderer,const QuadGeometry& geometry) {
    need(geometry.vertices.size()==4 && geometry.indices.size()==6,
         "CPU quad geometry supplied");
    std::uint32_t handle=0;
    ok(renderer.upload_mesh({geometry.vertices.data(),4,geometry.indices.data(),6},handle),
       "quad upload");
    return handle;
}
FrameView frame(Draw* draws,std::uint32_t count) {
    FrameView result{};
    result.draws=draws;
    result.draw_count=count;
    result.clear_colour[0]=result.clear_colour[1]=result.clear_colour[2]=0;
    result.ambient[0]=result.ambient[1]=result.ambient[2]=1;
    return result;
}
Draw draw(std::uint32_t mesh) {
    Draw result{};
    result.mesh=mesh;
    result.flags=0; // checks the builder's CW front, not a two-sided fallback
    return result;
}
void rgb(const Image& image,unsigned x,unsigned y,int red,int green,int blue,
         const char* label) {
    const int tolerance=8;
    if (std::abs(image.at(x,y,2)-red)>tolerance ||
        std::abs(image.at(x,y,1)-green)>tolerance ||
        std::abs(image.at(x,y,0)-blue)>tolerance) {
        std::fprintf(stderr,"FAIL %s at (%u,%u): RGB=(%d,%d,%d) expected=(%d,%d,%d)\n",
                     label,x,y,image.at(x,y,2),image.at(x,y,1),image.at(x,y,0),
                     red,green,blue);
        std::exit(1);
    }
}
void screen_sprite(Renderer& renderer) {
    std::string error;
    ScreenSprite input;
    input.width=32; input.height=32; input.depth=.4f;
    input.has_clip=true; input.clip={8,8,24,24};
    QuadGeometry geometry;
    need(build_screen_sprite(input,32,32,geometry,error),"build clipped screen sprite");
    const auto front=upload(renderer,geometry);

    // BGRA atlas: red/green top row, blue/white bottom row.
    const std::uint8_t atlas[16]{0,0,255,255, 0,255,0,255,
                                 255,0,0,255, 255,255,255,255};
    std::uint32_t texture=0;
    ok(renderer.upload_texture({atlas,2,2,8,sizeof(atlas)},texture),"quad atlas upload");
    Draw sprite=draw(front);
    sprite.texture=texture;
    sprite.sampler=Sampling::point_clamp;
    auto view=frame(&sprite,1);
    ok(renderer.render(view,false),"clipped sprite render");
    auto image=read(renderer);
    rgb(image,10,10,255,0,0,"top-left atlas orientation and clipped UV");
    rgb(image,22,10,0,255,0,"top-right atlas and clipped UV");
    rgb(image,10,22,0,0,255,"bottom-left atlas and clipped UV");
    rgb(image,22,22,255,255,255,"bottom-right atlas and clipped UV");
    rgb(image,7,16,0,0,0,"outside left clip clear");
    rgb(image,24,16,0,0,0,"outside right clip clear");
    rgb(image,16,7,0,0,0,"outside top clip clear");
    rgb(image,16,24,0,0,0,"outside bottom clip clear");

    input.depth=.8f;
    need(build_screen_sprite(input,32,32,geometry,error),"build rear sprite");
    Draw draws[2]{sprite,draw(upload(renderer,geometry))};
    draws[1].colour[0]=draws[1].colour[1]=draws[1].colour[2]=0;
    view=frame(draws,2);
    ok(renderer.render(view,false),"sprite depth render");
    image=read(renderer);
    rgb(image,10,10,255,0,0,"near sprite survives later rear draw");
    rgb(image,22,22,255,255,255,"near sprite depth across clip");
}

#if NATIVE11_QUAD_STAGE >= 35
void world_billboard(Renderer& renderer) {
    std::string error;
    WorldBillboard input;
    input.center[2]=.5f;
    input.width=1.2f; input.height=.3f;
    QuadGeometry geometry;
    need(build_world_billboard(input,geometry,error),"build horizontal billboard");
    Draw horizontal=draw(upload(renderer,geometry));
    Light light{};
    light.direction[0]=0; light.direction[1]=0; light.direction[2]=1;
    auto view=frame(&horizontal,1);
    view.ambient[0]=view.ambient[1]=view.ambient[2]=0;
    view.lights=&light; view.light_count=1;
    ok(renderer.render(view,false),"horizontal billboard render");
    auto image=read(renderer);
    need(image.at(8,16,2)>100,"horizontal billboard left pixel lit");
    rgb(image,16,8,0,0,0,"horizontal billboard excludes upper pixel");

    input.camera_right[0]=0; input.camera_right[1]=1;
    input.camera_up[0]=-1; input.camera_up[1]=0;
    need(build_world_billboard(input,geometry,error),"build rotated billboard");
    Draw rotated=draw(upload(renderer,geometry));
    view.draws=&rotated;
    ok(renderer.render(view,false),"rotated billboard render");
    image=read(renderer);
    need(image.at(16,8,2)>100,"rotated billboard upper pixel lit");
    rgb(image,8,16,0,0,0,"rotated billboard excludes left pixel");

    light.direction[2]=-1;
    ok(renderer.render(view,false),"opposite billboard light render");
    image=read(renderer);
    rgb(image,16,8,0,0,0,"billboard normal rejects opposite light");
}
#endif
}

int main() {
    Renderer renderer;
    ok(renderer.initialize_offscreen(32,32),"quad offscreen renderer");
    screen_sprite(renderer);
#if NATIVE11_QUAD_STAGE >= 35
    world_billboard(renderer);
#endif
    renderer.shutdown();
    std::puts("PASS native11 quad GPU stage "
#if NATIVE11_QUAD_STAGE == 34
              "34"
#else
              "35"
#endif
    );
}
