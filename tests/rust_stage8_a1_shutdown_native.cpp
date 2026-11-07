// Actual whole A1 source. Only GUI-owned background destructors are mocked.
#include A1_SOURCE
#include <cstdlib>
#include <exception>
#include <stdexcept>
static bool throw_log{};
extern "C" void a2fo_a1_fixture_background(unsigned id){
    std::printf("background:%u\n",id);
}
static void A2FO_CALL fixture_log(const char*,const char* message){
    if(throw_log){std::puts("log-enter");throw std::runtime_error("native log failure");}
    std::printf("log:%s\n",message);
}
static A2FO_ModuleApi fixture_api(){A2FO_ModuleApi api{};api.log=&fixture_log;return api;}
static void run_case(const char* label,unsigned mode){
    std::printf("case:%s\n",label);
    auto* slots=static_cast<void**>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!slots)std::abort();
    const void* expected[5]={reinterpret_cast<void*>(&resource_panel_render_vtable_hook),
        reinterpret_cast<void*>(&control_button_press_vtable_hook),
        reinterpret_cast<void*>(&producer_push_build_queue_item_hook),
        reinterpret_cast<void*>(&starbase_start_construction_effect_hook),
        reinterpret_cast<void*>(&starbase_finish_build_hook)};
    void* originals[5]={reinterpret_cast<void*>(0x1010),reinterpret_cast<void*>(0x2020),
        reinterpret_cast<void*>(0x3030),reinterpret_cast<void*>(0x4040),reinterpret_cast<void*>(0x5050)};
    for(unsigned i=0;i<5;++i)slots[i]=const_cast<void*>(expected[i]);
    if(mode==1)slots[1]=reinterpret_cast<void*>(0x9999); // alien hook must remain untouched
    if(mode==2)slots[2]=reinterpret_cast<void*>(0x8888);
    g_resource_panel_render_vtable_slot=&slots[0];g_resource_panel_render_original=originals[0];
    g_resource_panel_render_vtable_hook_installed=true;
    g_control_button_press_vtable_slot=&slots[1];g_control_button_press_original=originals[1];
    g_control_button_press_vtable_hook_installed=mode!=3;
    g_producer_push_target_slot=&slots[2];g_producer_push_target_original=originals[2];
    g_producer_push_target_hook_installed=true;
    g_starbase_start_effect_vtable_slot=&slots[3];g_starbase_start_effect_original=originals[3];
    g_starbase_start_effect_vtable_hook_installed=true;
    g_starbase_finish_build_vtable_slot=&slots[4];g_starbase_finish_build_original=originals[4];
    g_starbase_finish_build_vtable_hook_installed=true;
    g_constructor_menu_capability_count=17;
    g_legacy_resource_panel_runtime_ready=true;
    g_legacy_cursor_override_source="fixture-cursor";
    g_project_id_get_odf_name=reinterpret_cast<void*>(0x6060);
    g_race_menu_callback_ready=true;
    InterlockedExchange(&g_legacy_resource_maximum_post_config_pending,1);
    auto api=fixture_api();g_api=&api;g_armada=reinterpret_cast<void*>(0x7070);
    g_fleet_ops=reinterpret_cast<void*>(0x8080);
    A2FO_ModuleShutdown();
    std::printf("slots:%u:%u:%u:%u:%u\n",slots[0]==originals[0],
        slots[1]==originals[1],slots[2]==originals[2],
        slots[3]==originals[3],slots[4]==originals[4]);
    std::printf("state:%u:%u:%u:%u:%u:%u:%u:%u\n",
        g_resource_panel_render_vtable_slot==nullptr,
        g_control_button_press_vtable_slot==nullptr,
        g_producer_push_target_slot==nullptr,
        g_constructor_menu_capability_count==0,
        !g_legacy_resource_panel_runtime_ready,
        g_legacy_cursor_override_source.empty(),
        g_project_id_get_odf_name==nullptr,
        g_api==nullptr&&g_armada==nullptr&&g_fleet_ops==nullptr);
    std::printf("pending:%ld:%u\n",
        InterlockedCompareExchange(&g_legacy_resource_maximum_post_config_pending,0,0),
        g_race_menu_callback_ready);
    VirtualFree(slots,0,MEM_RELEASE);
}
int main(int argc,char**argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc==2&&std::strcmp(argv[1],"throw-log")==0){
        std::set_terminate(+[](){std::puts("terminate:log");std::fflush(stdout);ExitProcess(77);});
        auto api=fixture_api();g_api=&api;throw_log=true;
        A2FO_ModuleShutdown();return 78;
    }
    run_case("installed",0);
    run_case("alien-control",1);
    run_case("alien-producer",2);
    run_case("uninstalled-control",3);
    std::puts("A1 shutdown native fixture: PASS");
}
