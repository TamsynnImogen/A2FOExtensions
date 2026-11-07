// Frozen/current CraftIdentity object-editor workflow; headless and no window.
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../modules/A2FOCraftIdentity/object_editor.cpp"

namespace {
std::array<std::uint8_t,0x100> craft{};
unsigned shield_get_calls{},sets{},defaults{},original_calls{};
bool get_ok=true,set_ok=true;
void require(bool ok,const char* label) {
    if(!ok){std::fprintf(stderr,"object editor native failure: %s\n",label);std::exit(1);}
}
bool A2FO_CALL get_shields(void*,A2FO_DirectionalShieldState* state) {
    ++shield_get_calls;
    if(!get_ok)return false;
    for(int i=0;i<4;++i){state->current[i]=1.0f+i;state->maximum[i]=5.0f+i;}
    return true;
}
bool A2FO_CALL set_shields(void*,const A2FO_DirectionalShieldState*) {
    ++sets;return set_ok;
}
bool A2FO_CALL enabled(void*) {return true;}
void A2FO_CALL read_defaults(void*,char* captain,char* registry,std::size_t) noexcept {
    ++defaults;
    std::strcpy(captain,"ODF captain");
    std::strcpy(registry,"ODF registry");
}
INT_PTR CALLBACK original_dialog(HWND,UINT,WPARAM,LPARAM) {
    ++original_calls;return 77;
}
void setup_craft() {
    const std::uint32_t flags=8,handle=7;
    void* klass=craft.data()+0x60;
    std::memcpy(craft.data()+0x14,&flags,sizeof(flags));
    std::memcpy(craft.data()+0x28,&handle,sizeof(handle));
    std::memcpy(craft.data()+0x40,&klass,sizeof(klass));
}
}

namespace a2fo::object_editor::fleetops {
bool initialize(const A2FO_ModuleApi*,HMODULE) noexcept {return false;}
}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(void*,void*) {
    return 0x1234;
}
extern "C" void a2fo_editor_save_prefix_bridge() {}

int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    require(argc==2,"one mode");
    const char* mode=argv[1];
    using namespace a2fo::object_editor;
    if(std::strcmp(mode,"inactive")==0) {
        SavedState state{};bool directional=true;
        require(!read_values(reinterpret_cast<void*>(1),&state,&directional),
                "off read gate");
        require(!apply_values(reinterpret_cast<void*>(1),state,true),
                "off apply gate");
        require(!take_loaded_shields(reinterpret_cast<void*>(1),
                reinterpret_cast<A2FO_DirectionalShieldState*>(1)),
                "off loaded output gate");
        craft_event_handler(nullptr,nullptr);
        require(!initialize(nullptr,nullptr,nullptr),"missing API");
        g_dialog_hook.gateway=reinterpret_cast<void*>(&original_dialog);
        require(dialog_proc(nullptr,WM_CLOSE,0,0)==77&&original_calls==1,
                "off dialog native gateway");
    } else if(std::strcmp(mode,"entry")==0) {
        setup_craft();g_ready=true;g_read_defaults=&read_defaults;
        SavedState state{};bool directional=true;
        require(read_values(craft.data(),&state,&directional),"initial read");
        require(!directional&&defaults==1,"default and absent shield module");
        require(std::strcmp(state.captain.data(),"ODF captain")==0,
                "default captain");
        state.flags=kCaptain;
        std::strcpy(state.captain.data(),"Custom captain");
        require(apply_values(craft.data(),state,false),"apply identity");
        require(revision(craft.data())==1,"revision published");
        SavedState copy{};directional=true;
        require(read_values(craft.data(),&copy,&directional),"cached read");
        require(std::strcmp(copy.captain.data(),"Custom captain")==0&&
                std::strcmp(copy.registry.data(),"ODF registry")==0,
                "override and default compose");
        A2FO_CraftEvent cleanup{};cleanup.struct_size=sizeof(cleanup);
        cleanup.craft=craft.data();cleanup.kind=A2FO_CRAFT_EVENT_CLEANUP;
        craft_event_handler(&cleanup,nullptr);
        require(!overrides(craft.data()),"cleanup erases entry");
    } else if(std::strcmp(mode,"shields")==0) {
        setup_craft();g_ready=true;
        g_get_shields=&get_shields;g_set_shields=&set_shields;
        g_shields_enabled=&enabled;
        SavedState state{};bool directional=false;
        require(read_values(craft.data(),&state,&directional),"shield read");
        require(directional&&(state.flags&kShields)&&shield_get_calls==1,
                "shield snapshot");
        set_ok=false;
        require(!apply_values(craft.data(),state,true)&&sets==1&&
                revision(craft.data())==0,"setter failure leaves entry unassigned");
        set_ok=true;
        require(apply_values(craft.data(),state,true)&&sets==2,
                "setter success commits entry");
        auto* entry=prepare_entry(craft.data());
        require(entry!=nullptr,"prepared entry");
        assign_entry(entry,state,true);
        A2FO_DirectionalShieldState output{};
        require(!take_loaded_shields(craft.data(),&output),
                "struct-size admission");
        output.struct_size=sizeof(output);
        require(take_loaded_shields(craft.data(),&output)&&
                output.current[3]==4.0f,"pending snapshot taken");
        require(!take_loaded_shields(craft.data(),&output),
                "pending snapshot consumed once");
    } else if(std::strcmp(mode,"load-event")==0) {
        setup_craft();g_ready=true;
        SavedState state{};state.flags=kCaptain;
        auto* entry=prepare_entry(craft.data());
        require(entry!=nullptr,"entry created");
        assign_entry(entry,state,true);
        A2FO_CraftEvent event{};event.struct_size=sizeof(event);
        event.craft=craft.data();event.kind=A2FO_CRAFT_EVENT_POST_LOAD;
        const std::uint32_t new_handle=9;
        std::memcpy(craft.data()+0x28,&new_handle,sizeof(new_handle));
        craft_event_handler(&event,nullptr);
        require(overrides(craft.data())!=nullptr,"post-load refreshes handle");
        event.kind=A2FO_CRAFT_EVENT_CLEANUP;
        craft_event_handler(&event,nullptr);
        require(overrides(craft.data())==nullptr,"cleanup after post-load");
    } else require(false,"unknown mode");
    std::printf("object editor %s: PASS\n",mode);
}
