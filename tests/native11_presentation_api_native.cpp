#include "../sdk/include/a2fo_native11_presentation_api.h"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" int native11_presentation_c_contract(void);
namespace {
void need(bool value,const char* text){if(!value){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
const A2FO_N11_API& base(const A2FO_N11_API8& a){return a.base.base.base.base.base.base.base;}
int32_t A2FO_N11_CALL unused(void*,uint32_t,const uint8_t*,uint32_t,uint32_t,A2FO_N11_BytesSink,void*){return 0;}
HWND hidden(unsigned w,unsigned h){auto value=CreateWindowExW(0,L"STATIC",L"Native11 private fixture",WS_POPUP,0,0,w,h,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);need(value&&!IsWindowVisible(value),"create hidden fixture only");return value;}
struct Cross {const A2FO_N11_API8* api;A2FO_N11_Session session;int32_t result=0;};
DWORD WINAPI cross(void* context){auto& c=*static_cast<Cross*>(context);A2FO_N11_Present p{sizeof(p),0,0,0};c.result=c.api->present(c.session,&p);return 0;}
}
int main(int argc,char** argv){
    need(argc==2&&native11_presentation_c_contract(),"C contract");
    auto dll=LoadLibraryA(argv[1]);need(dll!=nullptr,"load private helper");
    const auto symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");A2FO_N11_GetInterface get=nullptr;
    static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));need(get!=nullptr,"get interface export");
    A2FO_N11_API8 api{};need(get(8,sizeof(api),&api)==S_OK,"API8");
    need(base(api).version==8&&base(api).struct_size==76&&base(api).capabilities==511,"API8 header");
    auto sentinel=api;need(get(8,sizeof(api)-4,&api)==E_INVALIDARG&&!std::memcmp(&sentinel,&api,sizeof(api)),"invalid table size atomic");
    A2FO_N11_API7 old{};need(get(7,sizeof(old),&old)==S_OK&&old.base.base.base.base.base.base.capabilities==255,"API7 retained");
    A2FO_N11_Host host{sizeof(host),0,64,64,nullptr,unused,nullptr};A2FO_N11_Session session=0;
    need(base(api).create(&host,&session)==S_OK&&session,"create");
    auto window=hidden(64,64);A2FO_N11_PresentationTarget t{sizeof(t),0,window,64,64,0};
    need(api.configure_presentation(session,&t)==S_OK,"native target attach");
    A2FO_N11_Present p{sizeof(p),0,0,0};need(api.present(session,&p)==E_UNEXPECTED,"no uninitialized presentation");
    A2FO_N11_PrimitiveFrame f{};f.struct_size=sizeof(f);f.generation=1;f.source_epoch=1;
    f.world_to_clip[0]=f.world_to_clip[5]=f.world_to_clip[10]=f.world_to_clip[15]=1;
    f.viewport[2]=f.viewport[3]=64;f.clear_colour[0]=.2f;f.clear_colour[1]=.4f;f.clear_colour[2]=.6f;f.clear_colour[3]=1;
    f.shadow_resolution=128; // all draw lists are deliberately empty
    need(api.base.submit_primitives(session,&f)==S_OK,"clear frame submit");
    Cross c{&api,session};auto thread=CreateThread(nullptr,0,cross,&c,0,nullptr);need(thread!=nullptr,"private worker");
    need(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0,"private worker joins");CloseHandle(thread);
    need(c.result==E_UNEXPECTED,"foreign thread rejected without consuming owner frame");
    auto hr=api.present(session,&p);need(hr==S_OK||hr==S_FALSE,"actual native present result");
    need(api.present(session,&p)==E_UNEXPECTED,"image consumed once");
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);need(base(api).readback(session,&read)==S_FALSE,"readback sizing");
    std::vector<uint8_t> pixels(static_cast<size_t>(read.required_bytes));read.bgra=pixels.data();read.capacity=pixels.size();
    need(base(api).readback(session,&read)==S_OK&&pixels[0]==153&&pixels[1]==102&&pixels[2]==51&&pixels[3]==255,"GPU image retained after presentation");
    f.generation=2;need(api.base.submit_primitives(session,&f)==S_OK,"resubmit");
    A2FO_N11_Shadows shadows{sizeof(shadows),0,128,.001f};
    need(api.base.base.base.base.base.base.configure_shadows(session,&shadows)==S_OK,"configure shadows");
    need(api.present(session,&p)==E_UNEXPECTED,"configuration invalidates previous image");
    for(unsigned config=0;config<5;++config){
        f.generation=10+config;need(api.base.submit_primitives(session,&f)==S_OK,"fresh config invalidation frame");
        int32_t result=0;
        switch(config){
        case 0:result=api.base.base.base.base.base.base.configure_shadows(session,nullptr);break;
        case 1:result=api.base.base.base.base.base.base.configure_materials(session,nullptr);break;
        case 2:result=api.base.base.configure_mesh_sharing(session,nullptr);break;
        case 3:result=api.base.base.base.base.base.configure_texture_bindings(session,nullptr);break;
        default:result=api.base.base.base.base.configure_part_states(session,nullptr);break;
        }
        need(result==E_INVALIDARG&&api.present(session,&p)==E_UNEXPECTED,"each admitted invalid config revokes image");
    }
    f.generation=15;need(api.base.submit_primitives(session,&f)==S_OK,"submit before stale attempt");
    need(api.base.submit_primitives(session,&f)<0&&api.present(session,&p)==E_UNEXPECTED,"failed submit invalidates image");
    f.generation=16;need(api.base.submit_primitives(session,&f)==S_OK,"submit before window destruction");
    need(DestroyWindow(window)!=0&&api.present(session,&p)<0,"destroyed target rejected");
    t.window=nullptr;t.width=t.height=0;need(api.configure_presentation(session,&t)==S_OK,"detach destroyed target");
    window=hidden(80,48);need(base(api).resize(session,80,48)==S_OK,"resize offscreen");
    t.window=window;t.width=80;t.height=48;need(api.configure_presentation(session,&t)==S_OK,"reattach resized target");
    need(api.present(session,&p)<0,"resize requires new frame");
    f.generation=17;f.viewport[2]=80;f.viewport[3]=48;need(api.base.submit_primitives(session,&f)==S_OK,"resized fresh frame");
    p.sync_interval=5;need(api.present(session,&p)==E_INVALIDARG,"invalid sync rejected and image consumed");p.sync_interval=0;
    need(api.present(session,&p)==E_UNEXPECTED,"invalid present cannot reuse frame");
    f.generation=18;need(api.base.submit_primitives(session,&f)==S_OK,"retry frame");hr=api.present(session,&p);need(hr==S_OK||hr==S_FALSE,"retry presentation");
    need(base(api).destroy(session)==S_OK,"destroy releases swapchain");need(DestroyWindow(window)!=0,"destroy hidden fixture");
    need(FreeLibrary(dll)!=0,"unload after explicit destroy");std::puts("PASS API8 native hidden-window presentation and C ABI");
}
