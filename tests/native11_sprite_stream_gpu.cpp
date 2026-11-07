// Real Rust texture parser -> owned sprite packet -> API7 offscreen GPU client.
#include "../renderer/native11/sprite_frame_stream.hpp"
#include "../renderer/native11/primitive_session.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void put(std::vector<std::uint8_t>& out,std::size_t at,std::uint32_t value){
    for(unsigned i=0;i<4;++i)out.at(at+i)=static_cast<std::uint8_t>(value>>(8*i));
}
std::vector<std::uint8_t> atlas(bool replacement=false){
    // Uncompressed, top-origin TGA. File order is top-left, top-right, bottom-left, bottom-right.
    std::vector<std::uint8_t> out(18+2*2*3);
    out[2]=2;out[12]=out[14]=2;out[16]=24;out[17]=0x20;
    const std::uint8_t pixels[12]{0,static_cast<std::uint8_t>(replacement?255:0),255,
        255,0,255, 0,255,0, 255,0,0};
    std::memcpy(out.data()+18,pixels,sizeof(pixels));return out;
}
std::vector<std::uint8_t> bc1_red(){
    // DX10 DDS with one 4x4 BC1 UNORM block, all texels selecting RGB565 red.
    std::vector<std::uint8_t> out(156);
    std::memcpy(out.data(),"DDS ",4);put(out,4,124);put(out,8,0x1007);
    put(out,12,4);put(out,16,4);put(out,28,1);put(out,76,32);put(out,80,4);
    std::memcpy(out.data()+84,"DX10",4);put(out,108,0x1000);
    put(out,128,71);put(out,132,3);put(out,140,1);put(out,144,1);
    out[148]=0;out[149]=0xf8;return out;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t id,std::vector<std::uint8_t> bytes,bool key){
    auto result=std::make_shared<TextureAsset>();result->serial=id;result->colour_key=key;
    result->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::move(bytes));return result;
}
CapturedSpriteBatch batch(std::uint64_t generation,std::uint64_t ticket,
                          const std::shared_ptr<const TextureAsset>& owner){
    CapturedSpriteBatch result;result.view=1;result.width=result.height=32;
    result.generation=generation;result.source_ticket=ticket;
    CapturedScreenSprite sprite;sprite.texture=owner;
    sprite.geometry.width=sprite.geometry.height=32;sprite.geometry.depth=.3f;
    sprite.geometry.has_clip=true;sprite.geometry.clip={8,8,24,24};
    sprite.geometry.uv={0,0,1,1};sprite.sampling=Sampling::point_clamp;
    result.sprites.push_back(sprite);return result;
}
std::shared_ptr<const PrimitiveFrame> clear(std::uint64_t generation,std::uint64_t epoch,
                                            const PrimitiveFrame* manifest=nullptr){
    auto out=manifest?std::make_shared<PrimitiveFrame>(*manifest):std::make_shared<PrimitiveFrame>();
    out->generation=generation;out->source_epoch=epoch;out->draws.clear();
    out->settings.clear_colour_target=true;out->settings.clear_depth_target=true;
    out->settings.clear_colour[0]=16.f/255;out->settings.clear_colour[1]=32.f/255;
    out->settings.clear_colour[2]=48.f/255;out->settings.clear_colour[3]=1;
    out->settings.viewport[2]=out->settings.viewport[3]=32;return out;
}
void render(PrimitiveRenderSession& session,std::shared_ptr<const PrimitiveFrame> frame,const char* why){
    std::string error;if(!session.render(std::move(frame),error)){
        std::fprintf(stderr,"FAIL %s: %s\n",why,error.c_str());std::exit(1);
    }
}
void update(SpriteFrameStream& stream,const CapturedSpriteBatch& input,std::uint64_t epoch,
            std::shared_ptr<const PrimitiveFrame>& out,const char* why){
    std::string error;if(!stream.update(input,epoch,out,error)){
        std::fprintf(stderr,"FAIL %s: %s\n",why,error.c_str());std::exit(1);
    }
}
std::vector<std::uint8_t> image(PrimitiveRenderSession& session){
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;std::string error;
    need(session.readback(pixels,width,height,error)&&width==32&&height==32&&
         pixels.size()==32*32*4,"32x32 owned readback");return pixels;
}
A2FO_N11_Usage usage(PrimitiveRenderSession& session){
    A2FO_N11_Usage result{};std::string error;
    need(session.usage(result,error),"owned GPU resource usage");return result;
}
bool same_resources(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){
    return a.device_epoch==b.device_epoch&&a.meshes==b.meshes&&a.textures==b.textures&&
           a.mesh_bytes==b.mesh_bytes&&a.texture_bytes==b.texture_bytes;
}
void pixel(const std::vector<std::uint8_t>& image,unsigned x,unsigned y,int r,int g,int b,const char* why){
    const auto* p=image.data()+(y*32+x)*4;
    if(std::abs(int(p[2])-r)>3||std::abs(int(p[1])-g)>3||std::abs(int(p[0])-b)>3){
        std::fprintf(stderr,"FAIL %s: RGB %u,%u,%u expected %d,%d,%d\n",why,p[2],p[1],p[0],r,g,b);
        std::exit(1);
    }
}
void no_legacy(){
    HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());
    need(h!=INVALID_HANDLE_VALUE,"module snapshot");MODULEENTRY32W item{};item.dwSize=sizeof(item);
    need(Module32FirstW(h,&item)!=0,"module first");do{
        need(_wcsicmp(item.szModule,L"d3d8.dll")&&_wcsicmp(item.szModule,L"d3d9.dll")&&
             _wcsnicmp(item.szModule,L"d3dx",4),"no legacy D3D modules");
    }while(Module32NextW(h,&item));const DWORD end=GetLastError();CloseHandle(h);
    need(end==ERROR_NO_MORE_FILES,"complete module enumeration");
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"load renderer DLL");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API7 api{};need(get(7,sizeof(api),&api)>=0,"query API7");
    PrimitiveRenderSession session;std::string error;need(session.initialize(api,32,32,error),"API7 offscreen session");
    render(session,clear(1,1),"initial background clear");
    SpriteFrameStream stream;auto original=texture(7,atlas(),true);
    auto input=batch(2,1,original);std::shared_ptr<const PrimitiveFrame> packet;
    update(stream,input,2,packet,"real TGA atlas parse");
    need(packet->source_epoch==2&&packet->textures.size()==1&&packet->textures[0].width==2&&
         packet->textures[0].height==2&&packet->textures[0].bgra[7]==0,"decoded TGA and exact magenta key");
    auto held=packet;render(session,packet,"clipped HUD LOAD colour and depth");
    auto pixels=image(session);pixel(pixels,12,12,255,0,0,"top-left atlas orientation");
    pixel(pixels,20,12,16,32,48,"exact magenta key preserves background");
    pixel(pixels,12,20,0,255,0,"bottom-left atlas orientation");
    pixel(pixels,20,20,0,0,255,"bottom-right atlas orientation");
    pixel(pixels,4,4,16,32,48,"clip preserves outside background");
    const auto atlas_usage=usage(session);need(atlas_usage.meshes==1&&atlas_usage.textures==2,
        "atlas and implicit white GPU residency");
    render(session,clear(3,2,packet.get()),"hot background clear before alpha");
    input.generation=4;auto& sprite=input.sprites[0];sprite.geometry.has_clip=false;
    sprite.geometry.left=sprite.geometry.top=0;sprite.geometry.width=sprite.geometry.height=16;
    sprite.geometry.uv={0,0,.5f,.5f};sprite.colour[3]=.5f;
    update(stream,input,3,packet,"alpha sprite hot update");need(packet->source_epoch==2,"hot texture epoch");
    render(session,packet,"alpha blend HUD LOAD");pixels=image(session);
    pixel(pixels,8,8,136,16,24,"half alpha red over background");
    pixel(pixels,24,24,16,32,48,"alpha LOAD retains clear background");
    need(same_resources(atlas_usage,usage(session)),"alpha hot resources retained");
    render(session,clear(5,2,packet.get()),"hot background clear before additive");
    input.generation=6;sprite.geometry.uv={0,.5f,.5f,1};sprite.colour[3]=1;
    sprite.colour[0]=sprite.colour[1]=sprite.colour[2]=.5f;sprite.additive=true;
    update(stream,input,4,packet,"additive sprite hot update");need(packet->source_epoch==2,"additive hot epoch");
    render(session,packet,"additive HUD LOAD");pixels=image(session);
    pixel(pixels,8,8,16,160,48,"half green additive over background");
    need(same_resources(atlas_usage,usage(session)),"additive hot resources retained");
    render(session,clear(7,2,packet.get()),"background before palette reload");
    auto replacement=texture(7,atlas(true),true);input.generation=8;input.source_ticket=2;
    sprite.texture=replacement;sprite.geometry.left=16;sprite.geometry.uv={0,0,.5f,.5f};
    sprite.colour[0]=sprite.colour[1]=sprite.colour[2]=sprite.colour[3]=1;sprite.additive=false;
    update(stream,input,5,packet,"replacement palette owner");
    need(packet->source_epoch==5&&held->source_epoch==2&&held->textures[0].bgra[1]==0&&
         packet->textures[0].bgra[1]==255,"new epoch and immutable old palette");
    render(session,packet,"moved reloaded palette HUD");pixels=image(session);
    pixel(pixels,8,8,16,32,48,"old footprint cleared after move");
    pixel(pixels,24,8,255,255,0,"new palette owner at moved position");
    render(session,clear(9,5,packet.get()),"background before BC1");
    auto compressed=texture(9,bc1_red(),false);input.generation=10;input.source_ticket=3;
    sprite.texture=compressed;sprite.geometry.left=0;sprite.geometry.top=16;
    sprite.geometry.uv={0,0,1,1};
    update(stream,input,6,packet,"real BC1 DDS parse and decode");
    need(packet->source_epoch==6&&packet->textures[0].width==4&&packet->textures[0].height==4,
         "BC1 source epoch and decoded dimensions");
    render(session,packet,"BC1 HUD LOAD");pixels=image(session);
    pixel(pixels,8,24,255,0,0,"BC1 GPU red");pixel(pixels,24,8,16,32,48,"BC1 LOAD background");
    need(session.close(error),"close API7 session");no_legacy();need(FreeLibrary(dll)!=0,"unload renderer DLL");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path argument");run(argv[1]);
    std::puts("PASS real TGA/BC1 sprite stream API7 GPU LOAD, blend, key, palette and epochs");}
