// Console-only fixture. Engine is mapped as data, its entrypoint never runs.
#include "../modules/A2FORefitMenus/mission_backdrop.cpp"
#include "../rust/native_hooks_ffi.h"
#include "support/refit_fo_chat_fixture.hpp"
#include <cstdio>
#include <stdexcept>
namespace a2fo::refit_menu::menu_scene { void end_departure() {} }
namespace {
unsigned calls{},patches{};
void* seen{};void* replacement{};bool refuse{};
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void __fastcall original(void* chat,void*){++calls;seen=chat;}
bool A2FO_CALL patch(void* target,void* to,const std::uint8_t* bytes,std::size_t size){
    ++patches;if(refuse)return false;
    replacement=to;return a2fo_rs_windows_patch_call(target,to,bytes,size);
}
void A2FO_CALL diagnostic(const char*,const char* text){std::puts(text);}
DWORD WINAPI foreign_thread(void*){
    return a2fo::refit_menu::mission_backdrop::frame()==nullptr?0u:1u;
}
void put(void* target,const void* data,std::size_t size){
    DWORD old{},unused{};require(VirtualProtect(target,size,PAGE_EXECUTE_READWRITE,&old),"fixture memory protection");
    std::memcpy(target,data,size);FlushInstructionCache(GetCurrentProcess(),target,size);
    VirtualProtect(target,size,old,&unused);
}
}
int main(int argc,char** argv){
    using namespace a2fo::refit_menu::mission_backdrop;
    if(argc!=3&&argc!=4){std::fprintf(stderr,"usage: fixture ARMADAL [FO] MODE\n");return 2;}
    try{
        HMODULE image=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        require(image!=nullptr,"data-only engine mapping");
        const char* mode=argv[argc-1];
        const bool fo_mode=!std::strncmp(mode,"fo-",3);
        HMODULE fo{};void* fo_gateway{};
        if(fo_mode){
            require(argc==4,"FO replay needs explicit FO data mapping");
            fo=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
            require(fo!=nullptr,"data-only Fleet Ops mapping");
            fo_gateway=refit_fo_chat_fixture::install(image,fo,reinterpret_cast<void*>(&original));
            if(!std::strcmp(mode,"fo-target"))refit_fo_chat_fixture::transfer(reinterpret_cast<BYTE*>(image)+chat_render,reinterpret_cast<BYTE*>(fo)+0x1cf0b5);
            if(!std::strcmp(mode,"fo-gateway")){const BYTE bad=0x90;put(fo_gateway,&bad,1);}
            if(!std::strcmp(mode,"fo-callback")){const BYTE bad=0x90;put(reinterpret_cast<BYTE*>(fo)+0x1cf0b4,&bad,1);}
            if(!std::strcmp(mode,"fo-noexec")){
                DWORD old{};require(VirtualProtect(reinterpret_cast<BYTE*>(fo)+0x1cf0b4,16,PAGE_READONLY,&old),"non-executable callback fixture");
            }
        }
        A2FO_ModuleApi sdk{};sdk.struct_size=sizeof(sdk);sdk.api_version=A2FO_MODULE_API_VERSION;
        sdk.patch_call=&patch;sdk.log=&diagnostic;
        refuse=!std::strcmp(mode,"refuse");const bool changed=!std::strcmp(mode,"changed");
        if(changed){const BYTE altered=0x90;put(reinterpret_cast<BYTE*>(image)+0x198a0c,&altered,1);}
        const bool bad_fo=fo_mode&&std::strcmp(mode,"fo-success");
        const bool ok=initialize(&sdk,image,fo);
        require(ok==(!refuse&&!changed&&!bad_fo),"checked observer admission");
        require(patches==(changed||bad_fo?0u:1u),"changed code never patched");
        if(ok){
            // Replace only the verified original renderer with a fixture stub.
            // Calling the observer must never run actual game code or startup.
            BYTE jump[5]{0xe9};const auto delta=static_cast<std::int32_t>(
                reinterpret_cast<std::uintptr_t>(&original)-reinterpret_cast<std::uintptr_t>(at(chat_render))-5);
            std::memcpy(jump+1,&delta,4);if(!fo_mode)put(at(chat_render),jump,sizeof(jump));
            const std::uintptr_t chat=0x123450;
            put(at(chat_global),&chat,sizeof(chat));
            require(!active(),"creating/loading a mission is not a rendered mission");
            using Callback=void(__fastcall*)(void*,void*);
            reinterpret_cast<Callback>(replacement)(reinterpret_cast<void*>(chat),nullptr);
            require(calls==1&&seen==reinterpret_cast<void*>(chat)&&active(),"native chat thiscall forwarded once");
            const auto shot=frame();
            require(shot&&shot->width==1&&shot->pixels[0]==0xff080c12u,"missing device uses neutral backdrop, never stale fleet");
            captured={2,1,{0xff123456u,0xff789abcu}};
            HANDLE thread=CreateThread(nullptr,0,&foreign_thread,nullptr,0,nullptr);
            require(thread&&WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,"foreign-thread fixture");
            DWORD code{};require(GetExitCodeThread(thread,&code)&&!code,"foreign thread cannot access the game device");
            CloseHandle(thread);
            for(int i=0;i<100;++i)require(frame()->pixels[0]==0xff123456u,"paused and nested menus retain mission snapshot without timeout");
            reinterpret_cast<Callback>(replacement)(reinterpret_cast<void*>(chat),nullptr);
            require(calls==2&&frame()->width==1,"new gameplay frame invalidates previous pause snapshot");
            if(fo_mode){
                refit_fo_chat_fixture::counter(fo,1);
                reinterpret_cast<Callback>(replacement)(reinterpret_cast<void*>(chat),nullptr);
                require(calls==2,"Cleanshot's native HUD suppression is preserved");
                refit_fo_chat_fixture::counter(fo,0);
            }
            const std::uintptr_t absent=0;put(at(chat_global),&absent,sizeof(absent));
            require(!active()&&!frame()&&captured.pixels.empty(),"mission deletion restores front-end phase and drops snapshot");
            shutdown();reinterpret_cast<Callback>(replacement)(reinterpret_cast<void*>(chat),nullptr);
            require(calls==3&&!active(),"resident observer forwards after shutdown");
        }
        std::puts("PASS: mission observer admission, ECX/stack forwarding, paused/nested menu cache, resume/end/shutdown fallback");return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
