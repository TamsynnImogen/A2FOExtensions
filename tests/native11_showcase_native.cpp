// Synthetic API7 composition showcase: no engine, HWND, or input automation.
// argv[1] is the renderer DLL; argv[2] receives a top-down 32-bit TGA.
#include "../renderer/native11/primitive_session.hpp"
#include "../renderer/native11/quad_geometry.hpp"

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool value,const char* what){if(!value){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
int channel(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned x,unsigned y,unsigned c){
    return pixels[(std::size_t(y)*width+x)*4+c];
}
Vertex vertex(float x,float y,float z,float nx,float ny,float nz,float u,float v){
    return {{x,y,z},{nx,ny,nz},{u,v}};
}
PrimitiveMesh mesh(std::uint64_t id,const QuadGeometry& quad){
    need(quad.vertices.size()==4&&quad.indices.size()==6,"CPU quad geometry");
    return {id,quad.vertices,quad.indices};
}
PrimitiveMesh floor_mesh(){
    PrimitiveMesh result;result.id=1;
    result.vertices={vertex(-1,1,.8f,0,0,-1,0,0),
                     vertex(1,1,.8f,0,0,-1,4,0),
                     vertex(1,-1,.8f,0,0,-1,4,4),
                     vertex(-1,-1,.8f,0,0,-1,0,4)};
    result.indices={0,1,2,0,2,3};return result;
}
PrimitiveMesh cube_mesh(){
    PrimitiveMesh result;result.id=2;
    const std::array<std::array<float,3>,6> normals{{{{0,0,-1}},{{0,0,1}},
        {{-1,0,0}},{{1,0,0}},{{0,-1,0}},{{0,1,0}}}};
    constexpr float half=.235f;
    for(std::size_t face=0;face<normals.size();++face){
        const auto& n=normals[face];
        const float right[3]{n[2]!=0?(n[2]<0?1.f:-1.f):
                             (n[1]!=0?(n[1]>0?1.f:-1.f):0.f),
                             0.f,
                             n[0]!=0?(n[0]>0?-1.f:1.f):0.f};
        // up = right x outward normal gives CW triangles with outward normals.
        const float up[3]{right[1]*n[2]-right[2]*n[1],
                          right[2]*n[0]-right[0]*n[2],
                          right[0]*n[1]-right[1]*n[0]};
        const unsigned col=static_cast<unsigned>(face%3),row=static_cast<unsigned>(face/3);
        const float u0=(col*32.f+1)/96.f,u1=((col+1)*32.f-1)/96.f;
        const float v0=(row*32.f+1)/64.f,v1=((row+1)*32.f-1)/64.f;
        const float uv[4][2]{{u0,v0},{u1,v0},{u1,v1},{u0,v1}};
        for(unsigned corner=0;corner<4;++corner){
            const float sx=(corner==0||corner==3)?-1.f:1.f;
            const float sy=(corner==0||corner==1)?1.f:-1.f;
            result.vertices.push_back(vertex(half*(n[0]+sx*right[0]+sy*up[0]),
                half*(n[1]+sx*right[1]+sy*up[1]),
                half*(n[2]+sx*right[2]+sy*up[2]),n[0],n[1],n[2],
                uv[corner][0],uv[corner][1]));
        }
        const std::uint32_t base=static_cast<std::uint32_t>(face*4);
        for(auto index:std::array<std::uint32_t,6>{{0,1,2,0,2,3}})
            result.indices.push_back(base+index);
    }
    need(result.vertices.size()==24&&result.indices.size()==36,"closed cube topology");
    for(std::size_t face=0;face<6;++face){
        const auto& a=result.vertices[face*4];
        const auto& b=result.vertices[face*4+1];
        const auto& c=result.vertices[face*4+2];
        const float ab[3]{b.position[0]-a.position[0],b.position[1]-a.position[1],
                          b.position[2]-a.position[2]};
        const float ac[3]{c.position[0]-a.position[0],c.position[1]-a.position[1],
                          c.position[2]-a.position[2]};
        const float cross[3]{ab[1]*ac[2]-ab[2]*ac[1],
                             ab[2]*ac[0]-ab[0]*ac[2],
                             ab[0]*ac[1]-ab[1]*ac[0]};
        need(cross[0]*a.normal[0]+cross[1]*a.normal[1]+cross[2]*a.normal[2]>0,
             "closed cube has CW outward faces");
    }
    return result;
}
PrimitiveTexture checker(){
    PrimitiveTexture result;result.id=11;result.width=result.height=64;
    result.bgra.resize(64*64*4);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
        const bool alternate=((x/8+y/8)&1)!=0;
        const unsigned i=(y*64+x)*4;
        result.bgra[i+0]=alternate?55:73;
        result.bgra[i+1]=alternate?68:93;
        result.bgra[i+2]=alternate?60:80;
        result.bgra[i+3]=255;
    }
    return result;
}
PrimitiveTexture cube_atlas(){
    PrimitiveTexture result;result.id=12;result.width=96;result.height=64;
    result.bgra.resize(96*64*4);
    const std::array<std::array<std::uint8_t,3>,6> colours{{
        {{231,150,52}},{{39,195,245}},{{44,114,214}},
        {{80,206,191}},{{88,98,225}},{{245,177,75}}}}; // RGB
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<96;++x){
        const auto& c=colours[(y/32)*3+x/32];
        const bool stripe=(x%32==5||y%32==5||x%32==26||y%32==26);
        const unsigned i=(y*96+x)*4;
        result.bgra[i+0]=stripe?20:c[2];
        result.bgra[i+1]=stripe?28:c[1];
        result.bgra[i+2]=stripe?35:c[0];
        result.bgra[i+3]=255;
    }
    return result;
}
PrimitiveTexture glow_texture(){
    PrimitiveTexture result;result.id=13;result.width=64;result.height=32;
    result.bgra.resize(64*32*4);
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x){
        const float dx=(float(x)-31.5f)/27.f,dy=(float(y)-15.5f)/10.f;
        const float radius=dx*dx+dy*dy;
        const float brightness=std::max(0.f,1.f-radius);
        const unsigned i=(y*64+x)*4;
        result.bgra[i+0]=static_cast<std::uint8_t>(155*brightness);
        result.bgra[i+1]=static_cast<std::uint8_t>(230*brightness);
        result.bgra[i+2]=static_cast<std::uint8_t>(75*brightness);
        result.bgra[i+3]=255;
    }
    return result;
}
PrimitiveTexture hud_atlas(){
    PrimitiveTexture result;result.id=14;result.width=result.height=64;
    result.bgra.resize(64*64*4);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){
        const unsigned i=(y*64+x)*4;
        std::uint8_t red=12,green=30,blue=46,alpha=185;
        if(x>=32&&y<32){red=40;green=227;blue=237;alpha=255;}
        if(x<32&&y>=32){red=242;green=173;blue=49;alpha=255;}
        if(x>=32&&y>=32){red=25;green=42;blue=58;alpha=225;}
        result.bgra[i+0]=blue;result.bgra[i+1]=green;
        result.bgra[i+2]=red;result.bgra[i+3]=alpha;
    }
    return result;
}
PrimitiveMesh screen_mesh(std::uint64_t id,float left,float top,float width,float height,
                          UvRect uv,std::string& error){
    ScreenSprite sprite;sprite.left=left;sprite.top=top;sprite.width=width;
    sprite.height=height;sprite.depth=.05f;sprite.uv=uv;
    QuadGeometry geometry;
    need(build_screen_sprite(sprite,256,192,geometry,error),"build HUD screen quad");
    return mesh(id,geometry);
}
std::shared_ptr<PrimitiveFrame> resources(std::string& error){
    auto result=std::make_shared<PrimitiveFrame>();result->source_epoch=1;
    result->meshes.push_back(floor_mesh());
    result->meshes.push_back(cube_mesh());
    WorldBillboard billboard;
    billboard.center[0]=-.45f;billboard.center[1]=.36f;billboard.center[2]=.28f;
    billboard.width=.52f;billboard.height=.18f;
    const float angle=.3f;
    billboard.camera_right[0]=std::cos(angle);billboard.camera_right[1]=std::sin(angle);
    billboard.camera_up[0]=-std::sin(angle);billboard.camera_up[1]=std::cos(angle);
    QuadGeometry quad;need(build_world_billboard(billboard,quad,error),"build glowing billboard");
    result->meshes.push_back(mesh(3,quad));
    result->meshes.push_back(screen_mesh(4,8,8,110,30,{0,0,.5f,.5f},error));
    result->meshes.push_back(screen_mesh(5,15,19,76,6,{.5f,0,1,.5f},error));
    result->meshes.push_back(screen_mesh(6,15,29,44,3,{0,.5f,.5f,1},error));
    result->meshes.push_back(screen_mesh(7,202,8,46,20,{.5f,.5f,1,1},error));
    result->textures={checker(),cube_atlas(),glow_texture(),hud_atlas()};
    return result;
}
PrimitiveDraw placed(std::uint64_t mesh_id,std::uint64_t texture_id){
    PrimitiveDraw result;result.mesh=mesh_id;result.texture=texture_id;return result;
}
std::shared_ptr<const PrimitiveFrame> world(const PrimitiveFrame& manifest,
                                             std::uint64_t generation,bool shadows,
                                             float cube_shift){
    auto result=std::make_shared<PrimitiveFrame>(manifest);result->generation=generation;
    result->settings.ambient[0]=.15f;result->settings.ambient[1]=.17f;
    result->settings.ambient[2]=.21f;
    result->settings.clear_colour[0]=.025f;result->settings.clear_colour[1]=.045f;
    result->settings.clear_colour[2]=.085f;
    result->settings.shadows.light_mask=shadows?1u:0u;
    result->settings.shadows.resolution=256;
    result->settings.shadows.filter=ShadowFilter::pcf3;
    result->settings.fog.enabled=true;
    result->settings.fog.colour[0]=.035f;result->settings.fog.colour[1]=.065f;
    result->settings.fog.colour[2]=.11f;
    result->settings.fog.start=.3f;result->settings.fog.end=3.5f;
    result->settings.fog.camera_position[2]=-1;
    Light sun{};sun.direction[0]=.55f;sun.direction[1]=.28f;sun.direction[2]=1;
    sun.colour[0]=1;sun.colour[1]=.72f;sun.colour[2]=.47f;sun.intensity=.9f;
    Light fill{};fill.type=LightType::point;fill.position[0]=-.6f;
    fill.position[1]=-.35f;fill.position[2]=-.25f;
    fill.colour[0]=.22f;fill.colour[1]=.72f;fill.colour[2]=1;
    fill.intensity=1.2f;fill.falloff_start=.35f;fill.falloff_range=2.5f;
    result->lights={sun,fill};
    auto floor=placed(1,11);floor.surface.flags=two_sided;
    floor.surface.sampler=Sampling::point_wrap;
    auto cube=placed(2,12);cube.surface.flags=two_sided;
    // Tilt the closed volume in all three axes so the acceptance image shows
    // visible side faces as well as a source-dependent cast footprint.
    const float z=.45f,y=.45f,x=.30f;
    const float cz=std::cos(z),sz=std::sin(z),cy=std::cos(y),sy=std::sin(y),cx=std::cos(x),sx=std::sin(x);
    const float rotation[9]{cz*cy,cz*sy*sx-sz*cx,cz*sy*cx+sz*sx,
                           sz*cy,sz*sy*sx+cz*cx,sz*sy*cx-cz*sx,
                           -sy,cy*sx,cy*cx};
    for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)
        cube.surface.local_to_world[row*4+col]=rotation[row*3+col];
    cube.surface.local_to_world[3]=cube_shift;
    cube.surface.local_to_world[7]=-.08f;
    cube.surface.local_to_world[11]=.40f;
    auto glow=placed(3,13);glow.surface.flags=two_sided|additive;
    glow.surface.emissive[0]=glow.surface.emissive[1]=glow.surface.emissive[2]=1;
    glow.surface.casts_shadow=false;glow.surface.receives_shadow=false;
    glow.surface.fog_enabled=false;
    result->draws={floor,cube,glow};return result;
}
std::shared_ptr<const PrimitiveFrame> hud(const PrimitiveFrame& manifest,
                                           std::uint64_t generation){
    auto result=std::make_shared<PrimitiveFrame>(manifest);result->generation=generation;
    result->settings.clear_colour_target=false;
    result->settings.clear_depth_target=false;
    result->settings.ambient[0]=result->settings.ambient[1]=result->settings.ambient[2]=0;
    for(const auto id:{4ull,5ull,6ull,7ull}){
        auto draw=placed(id,14);draw.surface.flags=two_sided|alpha_blend;
        draw.surface.sampler=Sampling::point_clamp;
        draw.surface.depth_mode=DepthMode::disabled;
        draw.surface.casts_shadow=false;draw.surface.receives_shadow=false;
        draw.surface.fog_enabled=false;
        draw.surface.emissive[0]=draw.surface.emissive[1]=draw.surface.emissive[2]=1;
        result->draws.push_back(draw);
    }
    return result;
}
void read(PrimitiveRenderSession& session,std::vector<std::uint8_t>& pixels,
          std::uint32_t& width,std::uint32_t& height){
    std::string error;
    need(session.readback(pixels,width,height,error)&&pixels.size()==std::size_t(width)*height*4,
         "showcase readback");
}
void write_tga(const char* path,const std::vector<std::uint8_t>& pixels){
    const std::uint8_t header[18]{0,0,2,0,0,0,0,0,0,0,0,0,0,1,192,0,32,0x28};
    FILE* file=std::fopen(path,"wb");need(file!=nullptr,"open output TGA");
    const bool written=std::fwrite(header,1,sizeof(header),file)==sizeof(header)&&
                       std::fwrite(pixels.data(),1,pixels.size(),file)==pixels.size();
    const int closed=std::fclose(file);
    need(written&&closed==0,"write top-down BGRA TGA");
}
}

int main(int argc,char** argv){
    need(argc==3,"renderer DLL and output TGA arguments");
    HMODULE dll=LoadLibraryA(argv[1]);need(dll!=nullptr,"load renderer DLL");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");
    need(symbol!=nullptr,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API7 api{};need(get(7,sizeof(api),&api)>=0,"query API7");
    PrimitiveRenderSession session;std::string error;
    need(session.initialize(api,256,192,error),"initialize 256x192 offscreen session");
    auto manifest=resources(error);
    auto shadowed=world(*manifest,1,true,0);
    need(session.render(shadowed,error),"shadowed world pass");
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;
    read(session,pixels,width,height);
    need(width==256&&height==192&&pixels.size()==256*192*4,"world image dimensions");
    const auto shadowed_image=pixels;
    A2FO_N11_Usage resident{};need(session.usage(resident,error),"world usage");
    auto unshadowed=world(*manifest,2,false,0);
    need(session.render(unshadowed,error),"shadow-disabled world pass");
    read(session,pixels,width,height);
    need(pixels!=shadowed_image,"shadow disable changes world pixels");
    auto posed=world(*manifest,3,true,.12f);
    need(session.render(posed,error),"pose-only world pass");
    A2FO_N11_Usage hot{};need(session.usage(hot,error)&&
        hot.meshes==resident.meshes&&hot.textures==resident.textures&&
        hot.mesh_bytes==resident.mesh_bytes&&hot.texture_bytes==resident.texture_bytes,
        "pose-only packet keeps resident uploads");
    read(session,pixels,width,height);
    const auto world_image=pixels;

    auto overlay=hud(*manifest,4);
    need(session.render(overlay,error),"retained-target HUD pass");
    read(session,pixels,width,height);
    need(channel(pixels,256,20,21,1)>120&&
         channel(pixels,256,20,21,1)>channel(pixels,256,20,21,2),
         "HUD cyan bar pixel");
    const std::size_t outside=(std::size_t(160)*256+200)*4;
    need(std::equal(pixels.begin()+outside,pixels.begin()+outside+4,
                    world_image.begin()+outside),"outside HUD retains exact world pixel");
    const auto composed=pixels;
    need(!session.render(overlay,error),"stale HUD generation refused");
    auto bad=std::make_shared<PrimitiveFrame>(*overlay);
    bad->generation=5;bad->draws[0].mesh=9999;
    need(!session.render(bad,error),"missing HUD mesh refused");
    read(session,pixels,width,height);
    need(pixels==composed,"failed packets retain composed image");
    write_tga(argv[2],composed);

    need(session.resize(128,96,error),"resize target");
    pixels={0x5a};width=77;height=88;
    need(!session.readback(pixels,width,height,error)&&pixels==std::vector<std::uint8_t>{0x5a}&&
         width==77&&height==88,"resize blocks stale readback");
    need(!session.render(bad,error),"bad generation remains retryable after resize");
    auto recovered=world(*manifest,5,true,0);
    need(session.render(recovered,error),"corrected same generation recovers");
    read(session,pixels,width,height);
    need(width==128&&height==96&&pixels.size()==128*96*4,"resized fresh image");
    std::weak_ptr<const PrimitiveFrame> held=recovered;
    recovered.reset();
    need(!held.expired(),"session retains accepted packet");
    need(session.close(error),"explicit session close");
    need(held.expired(),"close releases accepted packet");
    need(FreeLibrary(dll)!=0,"unload renderer after close");
    std::puts("PASS synthetic native11 showcase world, shadow, retained HUD, TGA and resize");
}
