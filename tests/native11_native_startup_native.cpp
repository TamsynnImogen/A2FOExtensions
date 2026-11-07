#include "../renderer/native11/native_startup.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
using namespace a2fo::native11;
namespace {
void need(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
HWND hidden(unsigned w,unsigned h){auto v=CreateWindowExW(0,L"STATIC",L"Native11 independent startup fixture",WS_POPUP,0,0,w,h,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(v&&!IsWindowVisible(v),"hidden fixture");return v;}
struct Source {bool valid=true;std::uint64_t ticket=1;NativeStartup* startup=nullptr;bool refused=false;};
bool current(void* context,const FrameIdentity& id) noexcept {
    auto& s=*static_cast<Source*>(context);
    if(s.startup){std::string error;s.refused=!s.startup->shutdown(error);}
    return s.valid&&s.ticket==id.source_ticket;
}
std::shared_ptr<const FrameBundle> bundle(std::uint64_t generation,unsigned w,unsigned h){
    auto world=std::make_shared<PrimitiveFrame>();world->generation=generation;world->source_epoch=1;
    world->settings.viewport[2]=static_cast<float>(w);world->settings.viewport[3]=static_cast<float>(h);
    world->settings.ambient[0]=world->settings.ambient[1]=world->settings.ambient[2]=1;
    PrimitiveMesh mesh;mesh.id=1;
    mesh.vertices={{{-1,1,.8f},{0,0,-1},{0,0}},{{1,1,.8f},{0,0,-1},{1,0}},
                   {{1,-1,.8f},{0,0,-1},{1,1}},{{-1,-1,.8f},{0,0,-1},{0,1}}};
    mesh.indices={0,1,2,0,2,3};world->meshes.push_back(mesh);
    PrimitiveDraw draw;draw.mesh=1;draw.surface.colour[0]=.2f;draw.surface.colour[1]=.4f;draw.surface.colour[2]=.6f;
    world->draws.push_back(draw);
    auto hud=std::make_shared<PrimitiveFrame>(*world);hud->settings.clear_colour_target=false;
    hud->draws[0].surface.colour[0]=.9f;hud->draws[0].surface.colour[1]=.7f;hud->draws[0].surface.colour[2]=.1f;
    hud->draws[0].surface.local_to_world[0]=hud->draws[0].surface.local_to_world[5]=.3f;
    hud->draws[0].surface.local_to_world[3]=.45f;hud->draws[0].surface.local_to_world[7]=.45f;
    hud->draws[0].surface.depth_mode=DepthMode::disabled;
    auto value=std::make_shared<FrameBundle>();value->identity={1,generation,1,1};value->width=w;value->height=h;
    value->complete_geometry=true;value->absent_domains=all_frame_domains&~(1u|32u);
    value->passes={{FrameDomain::background,world},{FrameDomain::hud,hud}};return value;
}
void rendered(PresentationResult result,const std::string& error){
    if(result==PresentationResult::fallback)std::fprintf(stderr,"startup fallback: %s\n",error.c_str());
    need(result==PresentationResult::presented||result==PresentationResult::occluded,"real complete-frame presentation");
}
}
int main(int argc,char** argv){
    need(argc==2,"private helper argument");const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,nullptr,0);
    need(count>1,"helper UTF8 path");std::wstring path(static_cast<size_t>(count),L'\0');
    need(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,argv[1],-1,path.data(),count)==count,"convert helper path");path.pop_back();
    auto window=hidden(64,64);NativeStartup startup;std::string error;Source source;
    need(!startup.initialize(L"A2FONative11Renderer.dll",64,64,window,error),"relative startup rejected");
    need(!startup.initialize(path+L".missing",64,64,window,error),"missing helper rejected");
    need(startup.initialize(path,64,64,window,error),error.c_str());
    bool foreign_refused=false;
    std::thread other([&]{std::string e;foreign_refused=!startup.shutdown(e);});other.join();
    need(foreign_refused,"foreign-thread shutdown refused before wrapper mutation");
    source.startup=&startup;
    auto incomplete=std::make_shared<FrameBundle>(*bundle(1,64,64));incomplete->absent_domains=0;
    need(startup.render(incomplete,current,&source,error)==PresentationResult::fallback&&!startup.current(),"missing domains block takeover");
    rendered(startup.render(bundle(1,64,64),current,&source,error),error);
    need(source.refused,"reentrant shutdown refused before wrapper mutation");source.startup=nullptr;
    if(startup.current()){source.valid=false;need(!startup.current(),"publication revoked immediately");source.valid=true;}
    source.valid=false;need(startup.render(bundle(2,64,64),current,&source,error)==PresentationResult::fallback,"revoked source blocks presentation");source.valid=true;
    need(startup.resize(0,0,nullptr,error),"suspend presentation");
    need(startup.render(bundle(2,64,64),current,&source,error)==PresentationResult::fallback,"suspension blocks presentation");
    need(DestroyWindow(window)!=0,"destroy detached fixture");window=hidden(80,48);
    need(startup.resize(80,48,window,error),error.c_str());
    rendered(startup.render(bundle(3,80,48),current,&source,error),error);
    need(DestroyWindow(window)!=0,"lose target");
    need(startup.render(bundle(4,80,48),current,&source,error)==PresentationResult::fallback,"lost window blocks takeover");
    need(startup.shutdown(error),error.c_str());need(!startup.current(),"shutdown clears publication");
    window=hidden(64,64);need(startup.initialize(path,64,64,window,error),error.c_str());
    rendered(startup.render(bundle(1,64,64),current,&source,error),error);
    need(startup.shutdown(error)&&DestroyWindow(window),"explicit close/unload and hidden fixture cleanup");
    std::puts("PASS independent native startup, complete-frame presentation, revocation and restart");
}
