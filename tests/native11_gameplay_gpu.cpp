// Real API8 owned world plus Rust-decoded sprite HUD on one hidden presentation target.
#include "../renderer/native11/gameplay_session.hpp"
#include "../renderer/native11/sprite_frame_stream.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool value,const char* why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Source {std::shared_ptr<const ModelAsset> model;std::shared_ptr<const TextureAsset> world_texture;};
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
    auto value=static_cast<Source*>(raw)->model;
    return value&&value->info.database==db&&value->info.serial==serial?value:nullptr;
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r){
    std::vector<std::uint8_t> out(21);out[2]=2;out[12]=out[14]=1;
    out[16]=24;out[17]=0x20;out[18]=b;out[19]=g;out[20]=r;return out;
}
Source source(){
    Source out;auto model=std::make_shared<ModelAsset>();model->info.database=7;
    model->info.serial=11;std::strcpy(model->info.name,"world.sod");
    model->bytes=native11_fixture::model("world.tga");model->info.byte_count=model->bytes.size();
    out.model=model;
    auto texture=std::make_shared<TextureAsset>();texture->serial=31;texture->texture=301;
    texture->logical_name="world.tga";
    texture->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(255,0,0));
    out.world_texture=texture;return out;
}
std::shared_ptr<const BoundMaterialCapture> capture(Source& owner,std::uint64_t generation,float x){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;frame->complete_casters=true;
    for(unsigned i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.entity=201;object.instance=101;
    object.database=7;object.model_serial=11;object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;object.world[3]=x;
    std::strcpy(object.model_name,"world.sod");
    auto observation=std::make_shared<MaterialObservation>();observation->instance=101;
    observation->database=7;observation->model_serial=11;observation->model=owner.model;
    MaterialBinding part;part.authored_texture="world.tga";part.texture=owner.world_texture;
    part.node_state_valid=true;observation->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{101,observation}};
    std::shared_ptr<const BoundMaterialCapture> out;std::string error;
    if(!build_bound_material_capture(*frame,10,resolve,&owner,observations,out,error)){
        std::fprintf(stderr,"FAIL world capture: %s\n",error.c_str());std::exit(1);
    }
    need(out->frame().requires_part_states&&out->frame().complete_casters,"sealed complete world pose");
    return out;
}
std::shared_ptr<const TextureAsset> hud_texture(){
    auto out=std::make_shared<TextureAsset>();out->serial=41;
    out->bytes=std::make_shared<const std::vector<std::uint8_t>>(tga(0,0,255));return out;
}
std::shared_ptr<const PrimitiveFrame> hud(SpriteFrameStream& stream,
    const std::shared_ptr<const TextureAsset>& owner,std::uint64_t generation,float left){
    CapturedSpriteBatch input;input.view=1;input.width=input.height=32;
    input.generation=generation;input.source_ticket=1;
    CapturedScreenSprite sprite;sprite.texture=owner;sprite.sampling=Sampling::point_clamp;
    sprite.geometry.left=left;sprite.geometry.top=12;
    sprite.geometry.width=sprite.geometry.height=8;sprite.geometry.depth=.1f;
    input.sprites.push_back(sprite);
    std::shared_ptr<const PrimitiveFrame> out;std::string error;
    if(!stream.update(input,19+generation,out,error)){
        std::fprintf(stderr,"FAIL sprite stream: %s\n",error.c_str());std::exit(1);
    }
    need(out->source_epoch==20&&out->textures.size()==1&&
         !out->settings.clear_colour_target&&!out->settings.clear_depth_target,
         "hot Rust-decoded HUD LOAD packet");return out;
}
std::shared_ptr<const GameplayFrame> packet(Source& owner,SpriteFrameStream& stream,
    const std::shared_ptr<const TextureAsset>& hud_owner,std::uint64_t generation,
    float world_x,float hud_x){
    auto out=std::make_shared<GameplayFrame>();out->identity={1,generation,10,5};
    out->width=out->height=32;out->world_domains=static_cast<std::uint32_t>(FrameDomain::objects);
    // This private fixture has exactly one object producer and one HUD producer.
    out->absent_domains=all_frame_domains&~(static_cast<std::uint32_t>(FrameDomain::objects)|
        static_cast<std::uint32_t>(FrameDomain::hud));out->complete_geometry=true;
    auto world=std::make_shared<OwnedWorldFrame>();world->materials=capture(owner,generation,world_x);
    world->light_camera=1;world->light_generation=generation;world->complete_casters=true;
    world->lighting.ambient[0]=world->lighting.ambient[1]=world->lighting.ambient[2]=1;
    out->world=world;out->overlays.push_back({FrameDomain::hud,hud(stream,hud_owner,generation,hud_x)});
    return out;
}
struct Current {bool live=true;std::uint64_t ticket=5;};
bool current(void* raw,const FrameIdentity& id) noexcept {
    const auto& source=*static_cast<Current*>(raw);
    return source.live&&id.view==1&&id.source_ticket==source.ticket;
}
decltype(A2FO_N11_API8::present) real_present=nullptr;
unsigned presents=0;
std::int32_t A2FO_N11_CALL counted_present(A2FO_N11_Session session,const A2FO_N11_Present* request){
    ++presents;return real_present(session,request);
}
void presented(PresentationResult result,const std::string& error,const char* why){
    if(result!=PresentationResult::presented){
        std::fprintf(stderr,"FAIL %s: result=%d %s\n",why,int(result),error.c_str());std::exit(1);
    }
}
std::vector<std::uint8_t> image(GameplayRenderSession& session){
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;std::string error;
    if(!session.readback(pixels,width,height,error)){
        std::fprintf(stderr,"FAIL gameplay readback: %s\n",error.c_str());std::exit(1);
    }
    need(width==32&&height==32&&pixels.size()==32*32*4,"gameplay readback dimensions");
    return pixels;
}
void pixel(const std::vector<std::uint8_t>& pixels,unsigned x,unsigned y,
           int r,int g,int b,const char* why){
    const auto* p=pixels.data()+(y*32+x)*4;
    if(std::abs(int(p[2])-r)>4||std::abs(int(p[1])-g)>4||std::abs(int(p[0])-b)>4){
        std::fprintf(stderr,"FAIL %s: RGB %u,%u,%u expected %d,%d,%d\n",
            why,p[2],p[1],p[0],r,g,b);std::exit(1);
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
    A2FO_N11_API8 api{};need(get(8,sizeof(api),&api)>=0,"query API8");
    real_present=api.present;api.present=counted_present;
    HWND window=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Native11 gameplay fixture",
        WS_POPUP,0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    need(window&&!IsWindowVisible(window),"hidden no-activate presentation target");
    GameplayRenderSession session;std::string error;
    need(session.initialize(api,32,32,window,error),"initialize API8 owned gameplay session");
    Source owner=source();SpriteFrameStream stream;auto texture=hud_texture();Current source_current;
    auto first=packet(owner,stream,texture,1,0,12);
    presented(session.render(first,current,&source_current,error),error,"world plus HUD presentation");
    need(presents==1&&session.current()==first&&session.accepted_generation()==1,
         "first complete frame presented and retained");
    auto pixels=image(session);pixel(pixels,16,16,255,0,0,"HUD overlays world at center");
    pixel(pixels,10,20,0,0,255,"world survives outside HUD");
    auto second=packet(owner,stream,texture,2,.5f,20);
    need(second->world->materials->source_epoch()==10&&
         first->world->materials->same_resources(*second->world->materials)&&
         first->overlays[0].frame->source_epoch==second->overlays[0].frame->source_epoch,
         "world and HUD resource epochs stay hot after motion");
    auto incomplete=std::make_shared<GameplayFrame>(*second);incomplete->absent_domains=0;
    need(session.render(incomplete,current,&source_current,error)==PresentationResult::fallback&&
         presents==1&&!session.current(),"incomplete domain inventory never presents or publishes");
    presented(session.render(second,current,&source_current,error),error,"moved world and HUD presentation");
    need(presents==2&&session.accepted_generation()==2,"second generation presented");
    pixels=image(session);pixel(pixels,24,16,255,0,0,"moved HUD overlays moved world");
    pixel(pixels,20,20,0,0,255,"moved world remains outside HUD");
    pixel(pixels,10,20,1,2,5,"old world footprint cleared");
    source_current.live=false;need(!session.current()&&session.accepted_generation()==0,
        "source revocation clears publication");
    const auto prior=pixels;std::uint32_t width=77,height=88;
    need(!session.readback(pixels,width,height,error)&&pixels==prior&&width==77&&height==88,
        "revoked source blocks readback without caller mutation");
    need(session.close(error),"close gameplay session");
    need(DestroyWindow(window)!=0,"destroy hidden fixture window");
    no_legacy();need(FreeLibrary(dll)!=0,"unload renderer helper");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path argument");run(argv[1]);
    std::puts("PASS real API8 owned world plus HUD GPU presentation and revocation");}
