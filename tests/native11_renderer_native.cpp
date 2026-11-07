#include "../renderer/native11/renderer.hpp"
#include "../renderer/native11/assets.h"
#include "../renderer/native11/engine_camera.hpp"
#include <cstring>
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(HRESULT hr,const char* why){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s 0x%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
struct Window {HWND h=nullptr;HINSTANCE app=nullptr;~Window(){if(h)DestroyWindow(h);UnregisterClassA("Native11PrivateFixture",app);}};
struct Pixel {int b,g,r,a;};
Pixel pixel(const std::vector<std::uint8_t>& data,UINT width,UINT x,UINT y){
    const auto* p=data.data()+(static_cast<std::size_t>(y)*width+x)*4;
    return {p[0],p[1],p[2],p[3]};
}
std::vector<std::uint8_t> capture(Renderer& renderer,UINT expected){
    std::vector<std::uint8_t> bytes;UINT w=0,h=0;ok(renderer.readback(bytes,w,h),"readback");
    need(w==expected&&h==expected&&bytes.size()==static_cast<std::size_t>(w)*h*4,"readback dimensions");
    return bytes;
}
void close(int actual,int wanted,int tolerance,const char* why){
    if(std::abs(actual-wanted)>tolerance){std::fprintf(stderr,"FAIL %s actual=%d wanted=%d\n",why,actual,wanted);std::exit(1);}
}
// Source-format triangle: SOD front is CCW on screen, with inward +Z
// cross-product normal. The adapter must make it a CW, outward -Z front.
std::vector<std::uint8_t> sod_front_fixture(){
    std::vector<std::uint8_t> b;
    auto word=[&](std::uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](std::uint32_t v){for(unsigned k=0;k<4;++k)b.push_back((v>>(k*8))&255);};
    auto number=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);dword(bits);};
    auto text=[&](const char* v){word(static_cast<std::uint16_t>(std::strlen(v)));while(*v)b.push_back(*v++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);
    word(1);text("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(0); // material model, no self illumination
    word(1);word(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})number(v);
    text("default");dword(4);dword(1);text("");dword(0);text("");word(0);
    word(3);word(3);word(1);
    for(float v:{-1.f,-1.f,.2f,3.f,-1.f,.2f,-1.f,3.f,.2f})number(v);
    for(unsigned k=0;k<6;++k)number(0);
    word(1);text("mat");for(unsigned k=0;k<3;++k){word(k);word(k);}
    b.push_back(1);word(0);word(0);word(0); // cull, trailing, channels, refs
    return b;
}
struct SodUpload {Renderer* renderer;Draw draw;unsigned count=0;};
int upload_sod(void* context,const A2FO_N11_Part* part){
    auto& out=*static_cast<SodUpload*>(context);++out.count;
    need(part->flags==0,"source fixture stays single sided");
    std::vector<Vertex> vertices(part->vertex_count);
    for(unsigned i=0;i<part->vertex_count;++i){
        std::copy(part->vertices[i].position,part->vertices[i].position+3,vertices[i].position);
        std::copy(part->vertices[i].normal,part->vertices[i].normal+3,vertices[i].normal);
        std::copy(part->vertices[i].uv,part->vertices[i].uv+2,vertices[i].uv);
    }
    ok(out.renderer->upload_mesh({vertices.data(),part->vertex_count,part->indices,part->index_count},out.draw.mesh),"SOD mesh upload");
    out.draw.flags=part->flags;
    std::copy(part->local_to_model,part->local_to_model+16,out.draw.local_to_world);
    return 1;
}
void run(){
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="Native11PrivateFixture";
    need(RegisterClassExA(&cls)!=0,"register private class");
    Window window;window.app=cls.hInstance;
    window.h=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,
        "private native11",WS_POPUP,0,0,32,32,nullptr,nullptr,cls.hInstance,nullptr);
    need(window.h&&!IsWindowVisible(window.h),"hidden private HWND");
    Renderer renderer;ok(renderer.initialize(window.h,32,32),"initialize");
    Vertex near_vertices[3]={{{-1,-1,.2f},{0,0,1},{0,0}},{{3,-1,.2f},{0,0,1},{0,0}},{{-1,3,.2f},{0,0,1},{0,0}}};
    Vertex far_vertices[3]={{{-1,-1,.7f},{0,0,1},{0,0}},{{3,-1,.7f},{0,0,1},{0,0}},{{-1,3,.7f},{0,0,1},{0,0}}};
    const std::uint32_t indices[3]={0,1,2};std::uint32_t near_mesh=0,far_mesh=0;
    ok(renderer.upload_mesh({near_vertices,3,indices,3},near_mesh),"near mesh");
    ok(renderer.upload_mesh({far_vertices,3,indices,3},far_mesh),"far mesh");
    need(near_mesh==1&&far_mesh==2,"monotonic mesh handles");
    near_vertices[0].position[0]=200; // upload must retain its own immutable copy.
    const std::uint8_t blue_bgra[4]={255,0,0,255};std::uint32_t blue_texture=0;
    ok(renderer.upload_texture({blue_bgra,1,1,4,4},blue_texture),"blue texture");
    need(blue_texture==1,"texture zero reserved for white");
    Draw draw{};draw.mesh=near_mesh;
    FrameView frame{};frame.clear_colour[0]=frame.clear_colour[1]=frame.clear_colour[2]=0;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.draws=&draw;frame.draw_count=1;
    ok(renderer.render(frame,false),"white texture render");
    auto bytes=capture(renderer,32);auto center=pixel(bytes,32,16,16);
    close(center.r,255,2,"white texture red");close(center.g,255,2,"white texture green");
    draw.texture=blue_texture;ok(renderer.render(frame,false),"blue texture render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.b,255,2,"blue image upload");close(center.r,0,2,"blue image no red");
    draw.texture=0;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;
    Light light{};light.direction[0]=light.direction[1]=0;light.direction[2]=-1;
    frame.lights=&light;frame.light_count=1;
    ok(renderer.render(frame,false),"directional render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.r,255,3,"directional Lambert");
    light.intensity=.4f;ok(renderer.render(frame,false),"intensity render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,102,3,"light intensity");
    // Rotation plus nonuniform scale distinguishes inverse transpose from inverse.
    const float pose[16]{2,0,0,0,0,0,-.5f,0,0,3,0,0,0,0,0,1};
    const float undo[16]{.5f,0,0,0,0,0,1.f/3,0,0,-2,0,0,0,0,0,1};
    std::copy(pose,pose+16,draw.local_to_world);std::copy(undo,undo+16,frame.world_to_clip);
    light.direction[1]=1;light.direction[2]=0;light.intensity=1;
    ok(renderer.render(frame,false),"rotated scaled normal render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,255,3,"inverse transpose world normal");
    const float identity[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    std::copy(identity,identity+16,draw.local_to_world);std::copy(identity,identity+16,frame.world_to_clip);
    light.direction[1]=0;light.direction[2]=-1;
    light.type=LightType::point;light.position[0]=light.position[1]=0;light.position[2]=1.2f;
    light.intensity=1;light.falloff_start=0;light.falloff_range=2;
    ok(renderer.render(frame,false),"point render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,128,8,"point falloff");
    light.position[2]=12.2f;light.falloff_start=10;light.falloff_range=5;
    ok(renderer.render(frame,false),"point nonzero full strength radius");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,153,4,"start plus fade distance semantics");
    light.falloff_start=0;light.falloff_range=2;
    light.type=LightType::spot;light.direction[2]=-1;light.cone_inner_degrees=20;
    light.cone_outer_degrees=60;light.position[2]=1.2f;
    ok(renderer.render(frame,false),"spot render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    need(center.r>100,"spot centre illuminated");
    const auto side=pixel(bytes,32,30,16);need(side.r<10,"spot cone cutoff");
    frame.light_count=0;draw.emissive[0]=.6f;
    ok(renderer.render(frame,false),"emissive render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,153,3,"emissive without lights");
    draw.texture=blue_texture;draw.emissive[0]=draw.emissive[1]=draw.emissive[2]=.6f;
    ok(renderer.render(frame,false),"textured emission render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.b,153,3,"emissive keeps diffuse texture detail");close(center.r,0,2,"no solid emissive RGB wash");
    draw.texture=0;draw.emissive[0]=draw.emissive[1]=draw.emissive[2]=0;
    // The lit test plane above intentionally faces +Z and is two-sided;
    // reverse its winding for a front face viewed from the -Z camera.
    Vertex front_vertices[3];std::copy(far_vertices,far_vertices+3,front_vertices);
    for(auto& vertex:front_vertices){vertex.position[2]=.2f;vertex.normal[2]=-1;}
    const std::uint32_t front_indices[3]{0,2,1};std::uint32_t front_mesh=0;
    ok(renderer.upload_mesh({front_vertices,3,front_indices,3},front_mesh),"front facing cull fixture");
    draw.mesh=front_mesh;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;draw.flags=0;
    ok(renderer.render(frame,false),"single sided base winding");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,255,3,"base winding visible");
    draw.local_to_world[0]=-1;
    ok(renderer.render(frame,false),"mirrored single sided winding");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);close(center.r,255,3,"mirrored winding remains visible");
    draw.local_to_world[0]=1;draw.flags=two_sided;draw.mesh=near_mesh;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;draw.emissive[0]=.6f;
    ok(renderer.render(frame,false),"restore emission control");bytes=capture(renderer,32);
    // The CPU camera's normalized screen coordinates must land in the same
    // native viewport, including subrects rather than silently filling the RT.
    const float engine_screen[16]{.5f,0,0,0,0,-.5f,0,0,.5f,.5f,1.1f,1,0,0,-.11f,0};
    const float camera_rect[4]{8,4,16,20};EngineCameraView camera_view{};
    need(convert_engine_cpu_camera(engine_screen,camera_rect,camera_view),"CPU camera adapter for GPU");
    std::copy(camera_view.world_to_clip,camera_view.world_to_clip+16,frame.world_to_clip);
    std::copy(camera_view.viewport,camera_view.viewport+4,frame.viewport);
    draw.mesh=near_mesh;draw.flags=two_sided;draw.emissive[0]=draw.emissive[1]=draw.emissive[2]=0;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    ok(renderer.render(frame,false),"engine CPU camera subrect render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,14);close(center.r,255,3,"CPU camera subrect interior");
    close(pixel(bytes,32,3,3).r,0,2,"left of camera subrect stays clear");
    close(pixel(bytes,32,27,27).r,0,2,"right of camera subrect stays clear");
    FrameView invalid_view=frame;invalid_view.viewport[0]=24;invalid_view.clear_colour[0]=1;
    need(renderer.render(invalid_view,false)==E_INVALIDARG,"out of bounds viewport rejected");
    auto unchanged_view=capture(renderer,32);need(unchanged_view==bytes,"invalid viewport preserves prior frame");
    std::copy(identity,identity+16,frame.world_to_clip);std::fill(frame.viewport,frame.viewport+4,0);
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;draw.emissive[0]=.6f;
    ok(renderer.render(frame,false),"restore full target after camera subrect");bytes=capture(renderer,32);
    // Complete validation runs before the clear or any draw.
    FrameView invalid=frame;invalid.draw_count=8193;invalid.clear_colour[0]=1;
    need(renderer.render(invalid,false)==E_INVALIDARG,"invalid frame rejected");
    auto preserved=capture(renderer,32);need(preserved==bytes,"invalid frame did not clear");
    Draw bad_draw=draw;bad_draw.local_to_world[0]=0;invalid=frame;invalid.draws=&bad_draw;
    need(renderer.render(invalid,false)==E_INVALIDARG,"singular normal transform rejected");
    preserved=capture(renderer,32);need(preserved==bytes,"singular frame did not draw");
    // Caller order and depth test: farther green must not replace near red.
    Draw pair[2]={draw,draw};pair[0].emissive[0]=1;pair[0].emissive[1]=0;
    pair[1].mesh=far_mesh;pair[1].emissive[0]=0;pair[1].emissive[1]=1;
    frame.draws=pair;frame.draw_count=2;
    ok(renderer.render(frame,false),"ordered depth render");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    need(center.r>240&&center.g<10,"depth keeps nearer draw");
    // End-to-end SOD -> C ABI -> D3D11: test the asset convention rather
    // than merely constructing a triangle already in the renderer convention.
    auto sod=sod_front_fixture();SodUpload imported{&renderer,{},0};
    need(a2fo_native11_parse_sod(sod.data(),static_cast<std::uint32_t>(sod.size()),upload_sod,&imported)==1&&imported.count==1,"SOD fixture admission");
    frame.draws=&imported.draw;frame.draw_count=1;
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;
    light=Light{};light.direction[0]=light.direction[1]=0;light.direction[2]=1;
    frame.light_count=1;frame.lights=&light;
    ok(renderer.render(frame,false),"SOD exterior lighting");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.r,255,3,"SOD exterior visible and normal points toward camera light");
    imported.draw.local_to_world[0]=-1;
    ok(renderer.render(frame,false),"mirrored SOD exterior");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.r,255,3,"mirrored SOD exterior stays lit");
    // Turn the triangle over, preserving its depth and positive determinant.
    imported.draw.local_to_world[10]=-1;imported.draw.local_to_world[11]=.4f;
    frame.light_count=0;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    ok(renderer.render(frame,false),"SOD interior cull");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.r,0,2,"SOD back face culled");
    imported.draw.flags=two_sided;
    ok(renderer.render(frame,false),"SOD two sided control");
    bytes=capture(renderer,32);center=pixel(bytes,32,16,16);
    close(center.r,255,3,"SOD back face is in view when two sided");
    frame.draws=pair;frame.draw_count=2;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=0;
    const std::uint32_t old_near=near_mesh;
    ok(renderer.resize(48,48),"resize");
    need(near_mesh==old_near,"mesh handle survives resize");
    ok(renderer.render(frame,false),"render after resize");
    bytes=capture(renderer,48);center=pixel(bytes,48,24,24);
    need(center.r>240&&center.g<10,"resized depth render");
    std::uint32_t retained=999;
    need(renderer.upload_mesh({nullptr,0,nullptr,0},retained)==E_INVALIDARG&&retained==999,
         "invalid upload preserves output handle");
    need(renderer.resize(16384,16384)==E_INVALIDARG,"oversized framebuffer rejected before allocation");
    renderer.shutdown();ok(renderer.initialize(window.h,32,32),"reinitialize");
    std::uint32_t fresh=0;ok(renderer.upload_mesh({far_vertices,3,indices,3},fresh),"upload after reinitialize");
    need(fresh>far_mesh,"retired mesh handles not recycled");
    frame.draws=&draw;frame.draw_count=1;
    need(renderer.render(frame,false)==E_INVALIDARG,"old handle rejected after new upload");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
         !GetModuleHandleA("D3DX81ab.dll")&&!GetModuleHandleA("d3dx9_43.dll"),
         "legacy rendering libraries were not loaded");
    std::printf("PASS native11 texture pose lighting normal-matrix fade-distance textured-emission mirror SOD-exterior-normal-mirror-backface CPU-camera-viewport depth validation resize stale-handles\n");
    renderer.shutdown();
}
}
int main(){run();return 0;}
