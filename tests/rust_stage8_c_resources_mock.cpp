#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <array>
#include <stdexcept>

#ifndef A2FO_STAGE8_RESOURCES_SOURCE
#error A2FO_STAGE8_RESOURCES_SOURCE is required
#endif
#include A2FO_STAGE8_RESOURCES_SOURCE

extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_0(
    void*, void*) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_1(
    void* function, void*, std::uintptr_t value) {
    if(g_armada&&
       (function==at(g_armada,kStandardComponentSetTooltipTextRva)||
       function==at(g_armada,kStandardComponentSetVerboseTooltipTextRva))){
        const auto* text=reinterpret_cast<const char*>(value);
        std::printf("tooltip-set:%d:%s\n",
            function==at(g_armada,kStandardComponentSetVerboseTooltipTextRva),
            text?text:"<null>");
        return 0;
    }
    std::abort();
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_3(
    void*, void*, std::uintptr_t key_address, std::uintptr_t output_address,
    std::uintptr_t) {
    const auto* key=reinterpret_cast<const char*>(key_address);
    auto* output=reinterpret_cast<RawRectangle*>(output_address);
    std::printf("rect-read:%s\n",key);
    if(std::strcmp(key,"resource_0")==0){*output={0,0,100,100};return 1;}
    if(std::strcmp(key,"resource_1")==0){*output={0,0,200,100};return 1;}
    if(std::strcmp(key,"resource_6")==0){*output={50,30,120,18};return 1;}
    return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_7(
    void*, void*, std::uintptr_t value, std::uintptr_t rectangle,
    std::uintptr_t, std::uintptr_t colour, std::uintptr_t,
    std::uintptr_t, std::uintptr_t) {
    const auto* box=reinterpret_cast<const NativeRectangle*>(rectangle);
    const auto* tint=reinterpret_cast<const Colour*>(colour);
    std::printf("draw:%s:%d:%d:%d:%d:%d\n",
        reinterpret_cast<const char*>(value),box->left,box->top,
        box->right,box->bottom,static_cast<int>(tint->red));
    return 0;
}
extern "C" void a2fo_resources_deduct_hook() { std::abort(); }

namespace a2fo::resources {
bool parse_nonnegative(std::string_view text, std::int32_t* value) noexcept {
    if (!value || text.empty()) return false;
    std::int64_t parsed = 0;
    for (char byte : text) {
        if (byte < '0' || byte > '9') return false;
        parsed = parsed * 10 + byte - '0';
        if (parsed > INT32_MAX) return false;
    }
    *value = static_cast<std::int32_t>(parsed);
    return true;
}
bool can_afford(const Amounts& amounts, const Costs& costs) noexcept {
    for(std::size_t i=0;i<4;++i)if(costs[i]>0&&amounts[i]<costs[i])return false;
    return true;
}
void debit(Amounts& amounts, const Costs& costs) noexcept {
    for(std::size_t i=0;i<4;++i)amounts[i]-=costs[i];
}
void credit(Amounts& amounts, const Costs& costs) noexcept {
    for(std::size_t i=0;i<4;++i)amounts[i]+=costs[i];
}
std::int64_t saturating_add(std::int64_t value, std::int64_t delta) noexcept {
    if(delta>0&&value>INT64_MAX-delta)return INT64_MAX;
    if(delta<0&&value<INT64_MIN-delta)return INT64_MIN;
    return value+delta;
}
}
int g_fixture_hook_calls=0,g_fixture_patch_calls=0;
int g_fixture_fail_hook=-1,g_fixture_fail_patch=-1;
int g_fixture_throw_hook=-1,g_fixture_throw_patch=-1;
void* g_fixture_fleet=nullptr;
int g_fixture_register_calls=0,g_fixture_fail_register=-1;
void* A2FO_CALL fixture_armada_module(){return g_armada;}
void* A2FO_CALL fixture_fleet_module(){return g_fixture_fleet;}
bool A2FO_CALL fixture_register_class(const char*,const char* const* fields,
    std::uint32_t count,A2FO_GameObjectClassLoadedHandler,void*){
    const int call=g_fixture_register_calls++;
    std::printf("register-class:%d:%u:%s\n",call,count,fields[0]);
    return call!=g_fixture_fail_register;
}
bool A2FO_CALL fixture_register_race(const char*,const char* const* fields,
    std::uint32_t count,A2FO_RaceLoadedHandler,void*){
    const int call=g_fixture_register_calls++;
    std::printf("register-race:%d:%u:%s:%s\n",call,count,
                fields[0],fields[count-1]);
    return call!=g_fixture_fail_register;
}
bool A2FO_CALL fixture_register_producer(const char*,
    A2FO_ProducerEventHandler,void*){
    const int call=g_fixture_register_calls++;
    std::printf("register-producer:%d\n",call);
    return call!=g_fixture_fail_register;
}
bool g_fixture_squad_success=false;
bool A2FO_CALL fixture_squad_costs(void*,std::int32_t* costs,
                                   std::uint32_t count){
    std::printf("squad-costs:%u:%d\n",count,g_fixture_squad_success);
    if(!g_fixture_squad_success)return false;
    for(std::uint32_t index=0;index<count;++index)costs[index]=
        static_cast<std::int32_t>(10+index);
    return true;
}
void A2FO_CALL fixture_log(const char*,const char* message){
    std::printf("log:%s\n",message);
}
bool A2FO_CALL fixture_hook(void* target,void*,std::size_t,
    const std::uint8_t*,A2FO_InlineHook* hook){
    const int call=g_fixture_hook_calls++;
    const auto address=reinterpret_cast<std::uintptr_t>(target);
    const auto fleet=reinterpret_cast<std::uintptr_t>(g_fixture_fleet);
    const bool is_fleet=address>=fleet&&address<fleet+0x400000u;
    const auto base=reinterpret_cast<std::uintptr_t>(
        is_fleet?g_fixture_fleet:static_cast<void*>(g_armada));
    std::printf("hook:%d:%c:%lu\n",call,is_fleet?'F':'A',
                static_cast<unsigned long>(address-base));
    if(call==g_fixture_throw_hook)throw std::runtime_error("hook throw");
    if(call==g_fixture_fail_hook)return false;
    hook->target=target;
    hook->gateway=target;
    return true;
}
bool A2FO_CALL fixture_patch(void* target,void*,const std::uint8_t*,
    std::size_t){
    const int call=g_fixture_patch_calls++;
    std::printf("patch:%d:%lu\n",call,static_cast<unsigned long>(
        reinterpret_cast<std::uintptr_t>(target)-
        reinterpret_cast<std::uintptr_t>(g_armada)));
    if(call==g_fixture_throw_patch)throw std::runtime_error("patch throw");
    return call!=g_fixture_fail_patch;
}
template<std::size_t Size>
void fixture_signature(void* module,std::uintptr_t rva,
    const std::array<std::uint8_t,Size>& expected){
    std::memcpy(static_cast<std::uint8_t*>(module)+rva,
                expected.data(),expected.size());
}

int main() {
    const A2FO_OdfFieldView fields[] = {
        {{"TRITANIUMCOST", 13}, {"12", 2}},
        {{"supplyCost", 10}, {"-3", 2}},
        {{"creditsCost", 11}, {"99", 2}},
        {{"collectiveconnectionsCost", 25}, {"42", 2}},
        {{"crewRes", 7}, {"people", 6}},
        {{"name", 4}, {"Federation", 10}},
        {{nullptr, 0}, {"ignored", 7}},
    };
    std::string value;
    const bool found = event_field(fields, 7, "tritaniumCost", &value);
    std::printf("find:%d:%s\n", found, value.c_str());
    Costs costs{};
    parse_fields(fields, 7, kCostCommands, &costs);
    std::printf("costs:%d:%d:%d:%d\n", costs[0], costs[1], costs[2], costs[3]);
    ResourcePresentation presentation{};
    parse_presentation_fields(fields, 7, &presentation);
    std::printf("presentation:%s\n", presentation[0][0].c_str());
    std::size_t index = 99;
    for (std::uint32_t resource : {5u, 6u, 9u, 10u}) {
        const bool mapped = additional_resource_index(resource, &index);
        std::printf("index:%u:%d:%lu\n", resource, mapped,
                    static_cast<unsigned long>(index));
    }
    InitializeCriticalSection(&g_lock);
    g_lock_ready = true;
    g_runtime_alive = true;
    A2FO_ModuleApi api{};
    api.log=&fixture_log;
    g_api=&api;
    A2FO_GameObjectClassLoadedEvent class_event{};
    class_event.struct_size = sizeof(class_event);
    class_event.object_class = reinterpret_cast<void*>(0x1234);
    class_event.odf_fields = fields;
    class_event.odf_field_count = 7;
    class_loaded_handler(&class_event, nullptr);
    const auto stored_costs = g_class_costs.at(class_event.object_class);
    std::printf("class:%d:%d:%d:%d\n", stored_costs[0],
                stored_costs[1], stored_costs[2], stored_costs[3]);
    g_squad_costs=&fixture_squad_costs;
    auto resolved_costs=class_costs(class_event.object_class);
    std::printf("cost-fallback:%d:%d\n",resolved_costs[0],resolved_costs[3]);
    g_fixture_squad_success=true;
    resolved_costs=class_costs(class_event.object_class);
    std::printf("cost-squad:%d:%d\n",resolved_costs[0],resolved_costs[3]);
    g_fixture_squad_success=false;
    A2FO_RaceLoadedEvent race_event{};
    race_event.struct_size = sizeof(race_event);
    race_event.race = reinterpret_cast<void*>(0x2345);
    race_event.odf_fields = fields;
    race_event.odf_field_count = 7;
    race_loaded_handler(&race_event, nullptr);
    std::printf("race:%s:%u\n",
                g_race_presentations.at(race_event.race)[0][0].c_str(),
                g_presentation_generation);
    std::array<std::uint8_t,0x300> team_storage{};
    void* race_pointer=race_event.race;
    std::memcpy(team_storage.data()+kTeamRaceOffset,&race_pointer,
                sizeof(race_pointer));
    g_team_amounts[team_storage.data()]=Amounts{{7,8,9,10}};
    auto snapshot=team_snapshot(team_storage.data());
    std::printf("snapshot-hit:%lld:%lld\n",
                static_cast<long long>(snapshot[0]),
                static_cast<long long>(snapshot[3]));
    g_team_amounts.erase(team_storage.data());
    snapshot=team_snapshot(team_storage.data());
    std::printf("snapshot-miss:%lld:%lld\n",
                static_cast<long long>(snapshot[0]),
                static_cast<long long>(snapshot[3]));
    refresh_presentation_cache(team_storage.data(),race_pointer);
    std::printf("cache:%s:%s\n",g_presentation_cache.native[0][0].c_str(),
                g_presentation_cache.added[0][0].c_str());
    const auto lookup=native_resource_presentation_lookup(
        nullptr,"GUI_CP_CREW_RES",A2FO_RESOURCE_PRESENTATION_RES);
    std::printf("lookup:%s\n",reinterpret_cast<const char*>(lookup));
    refresh_panel_text_cache(team_storage.data(),Amounts{{1,22,333,4444}});
    std::printf("text:%s:%s\n",g_panel_text_cache.lines[0].c_str(),
                g_panel_text_cache.lines[3].c_str());
    g_team_amounts[team_storage.data()]=Amounts{{30,20,120,60}};
    std::printf("api-get:%lld\n",static_cast<long long>(
                A2FOResources_Get(team_storage.data(),6)));
    std::printf("api-set-negative:%d\n",A2FOResources_Set(
                team_storage.data(),6,-1));
    std::printf("api-set:%d\n",A2FOResources_Set(team_storage.data(),6,50));
    std::printf("api-add:%d\n",A2FOResources_Add(team_storage.data(),6,-70));
    std::printf("api-add-value:%lld\n",static_cast<long long>(
                A2FOResources_Get(team_storage.data(),6)));
    A2FOResources_Add(team_storage.data(),6,30);
    std::printf("api-cost:%d\n",A2FOResources_GetCost(
                class_event.object_class,6));
    std::printf("api-presentation:%s\n",A2FOResources_GetPresentationText(
                team_storage.data(),0,A2FO_RESOURCE_PRESENTATION_RES));
    std::array<std::uint8_t,0x120> producer{};
    void* team_pointer=team_storage.data();
    std::memcpy(producer.data()+kProducerTeamOffset,&team_pointer,
                sizeof(team_pointer));
    std::printf("api-can-pay:%d\n",a2fo_resources_can_pay(
                producer.data(),class_event.object_class));
    a2fo_resources_commit_payment(producer.data(),class_event.object_class);
    std::printf("api-after-pay:%lld\n",static_cast<long long>(
                A2FOResources_Get(team_storage.data(),6)));
    g_production_integration_ready=true;
    A2FO_ProducerEvent producer_event{sizeof(A2FO_ProducerEvent),
        A2FO_PRODUCER_EVENT_CANCELLED,producer.data(),class_event.object_class};
    std::printf("api-refund:%d\n",producer_event_handler(&producer_event,nullptr));
    std::printf("api-after-refund:%lld\n",static_cast<long long>(
                A2FOResources_Get(team_storage.data(),6)));
    refresh_panel_configuration(nullptr);
    std::printf("panel-empty:%d\n",g_panel_configuration.loaded);
    void* module_memory=VirtualAlloc(nullptr,0x400000,
                                     MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!module_memory)std::abort();
    g_armada=static_cast<HMODULE>(module_memory);
    refresh_panel_configuration(reinterpret_cast<void*>(0x4567));
    std::printf("panel-config:%d:%d:%d\n",
                g_panel_configuration.native_rectangle_found[0],
                g_panel_configuration.native_rectangle_found[1],
                g_panel_configuration.rectangle_found[0]);
    g_armada=nullptr;
    std::array<std::uint8_t,0x80> panel{};
    std::array<std::uint8_t,0x80> display{};
    std::array<std::uint8_t,0x90> text{};
    void* display_pointer=display.data();
    void* text_pointer=text.data();
    std::memcpy(panel.data()+kResourcePanelFirstTextOffset+sizeof(void*),
                &display_pointer,sizeof(display_pointer));
    std::memcpy(display.data()+kResourceDisplayTextOffset,
                &text_pointer,sizeof(text_pointer));
    const NativeRectangle live{10,20,110,50};
    std::memcpy(text.data()+kTextComponentLiveRectangleOffset,&live,sizeof(live));
    g_panel_configuration.native_rectangle_found[0]=true;
    g_panel_configuration.native_rectangle_found[1]=true;
    const auto found_context=find_panel_render_context(panel.data());
    std::printf("panel-context:%lu:%d:%d:%d:%d\n",
                static_cast<unsigned long>(found_context.index),
                found_context.rectangle.left,found_context.rectangle.top,
                found_context.rectangle.right,found_context.rectangle.bottom);
    g_panel_configuration.native_rectangles[1]={0,0,100,100};
    g_panel_configuration.rectangle_found[0]=true;
    g_panel_configuration.rectangles[0]={50,10,25,20};
    g_panel_configuration.rectangle_found[1]=true;
    g_panel_configuration.rectangles[1]={-50,-10,25,20};
    unsigned short saved_control=0;
    __asm__ volatile("fnstcw %0":"=m"(saved_control));
    for(unsigned short precision : {0x0000u,0x0200u,0x0300u}) {
        const unsigned short control=static_cast<unsigned short>(
            (saved_control&~0x0300u)|precision);
        __asm__ volatile("fldcw %0"::"m"(control));
        const NativeRectangle viewport{10,20,111,79};
        for(std::size_t index=0;index<4;++index){
            const auto projected=resource_panel_rectangle(index,1,viewport);
            const auto amount=panel_amount_rectangle(index,projected);
            std::printf("rect:%u:%lu:%d:%d:%d:%d:%d\n",
                static_cast<unsigned>(precision),
                static_cast<unsigned long>(index),projected.left,
                projected.top,projected.right,projected.bottom,amount.left);
        }
    }
    __asm__ volatile("fldcw %0"::"m"(saved_control));
    void* fleet_memory=VirtualAlloc(nullptr,0x400000,
                                    MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!fleet_memory)std::abort();
    g_fixture_fleet=fleet_memory;
    g_armada=static_cast<HMODULE>(module_memory);
    g_fleet_ops=static_cast<HMODULE>(fleet_memory);
    fixture_signature(module_memory,kTeamConstructorRva,kExpectedTeamConstructor);
    fixture_signature(module_memory,kTeamDestructorRva,kExpectedTeamDestructor);
    fixture_signature(fleet_memory,kDeductResourcesForBuildRva,kExpectedDeductResources);
    fixture_signature(module_memory,kResourcePanelRenderRva,kExpectedResourcePanelRender);
    fixture_signature(module_memory,kLocalTeamRva,kExpectedLocalTeam);
    fixture_signature(module_memory,kTeamForTeamRva,kExpectedTeamForTeam);
    fixture_signature(module_memory,kParameterDbGetRectangleRva,
                      kExpectedParameterDbGetRectangle);
    fixture_signature(module_memory,kDisplayInterfaceDrawTextRva,
                      kExpectedDisplayInterfaceDrawText);
    fixture_signature(module_memory,kStandardComponentSetTooltipTextRva,
                      kExpectedSetTooltipText);
    fixture_signature(module_memory,kStandardComponentSetVerboseTooltipTextRva,
                      kExpectedSetTooltipText);
    fixture_signature(module_memory,kResourceComponentTooltipRva,
                      kExpectedResourceComponentTooltip);
    fixture_signature(module_memory,kResourceComponentVerboseTooltipRva,
                      kExpectedResourceComponentTooltip);
    for(const auto& call_site:kNativeResourceResCallSites)
        fixture_signature(module_memory,call_site.rva,call_site.expected);
    fixture_signature(module_memory,kNativeOfficerIconCallSite.rva,
                      kNativeOfficerIconCallSite.expected);
    void* native_string=module_memory;
    std::memcpy(static_cast<std::uint8_t*>(module_memory)+kInsertCStringIatRva,
                &native_string,sizeof(native_string));
    api.install_inline_hook=&fixture_hook;
    api.patch_call=&fixture_patch;
    g_api=&api;
    static_cast<std::uint8_t*>(module_memory)[kTeamConstructorRva]^=1;
    std::printf("runtime-signature:%d\n",install_runtime_hooks());
    static_cast<std::uint8_t*>(module_memory)[kTeamConstructorRva]^=1;
    g_fixture_hook_calls=0;
    std::printf("runtime-success:%d\n",install_runtime_hooks());
    g_fixture_hook_calls=0;
    g_fixture_fail_hook=1;
    std::printf("runtime-partial:%d\n",install_runtime_hooks());
    g_fixture_fail_hook=-1;
    g_fixture_hook_calls=0;
    g_fixture_throw_hook=1;
    try{install_runtime_hooks();}
    catch(const std::exception& error){
        std::printf("runtime-throw:%s:%d\n",error.what(),g_fixture_hook_calls);
    }
    g_fixture_throw_hook=-1;
    g_fixture_hook_calls=0;
    std::printf("panel-hook:%d\n",install_resource_panel_hook());
    g_fixture_hook_calls=0;
    g_fixture_throw_hook=0;
    try{install_resource_panel_hook();}
    catch(const std::exception& error){
        std::printf("panel-throw:%s:%d\n",error.what(),g_fixture_hook_calls);
    }
    g_fixture_throw_hook=-1;
    g_fixture_hook_calls=0;
    g_fixture_patch_calls=0;
    std::printf("presentation-hooks:%d\n",install_native_presentation_hooks());
    g_fixture_hook_calls=0;
    g_fixture_patch_calls=0;
    g_fixture_fail_patch=5;
    std::printf("presentation-partial:%d\n",install_native_presentation_hooks());
    g_fixture_fail_patch=-1;
    g_fixture_hook_calls=0;
    g_fixture_patch_calls=0;
    g_fixture_throw_patch=5;
    try{install_native_presentation_hooks();}
    catch(const std::exception& error){
        std::printf("presentation-throw:%s:%d\n",error.what(),
                    g_fixture_patch_calls);
    }
    g_fixture_throw_patch=-1;
    std::array<std::uint8_t,0x90> ui_component{};
    void* display_interface=module_memory;
    std::memcpy(ui_component.data()+0x04,&display_interface,
                sizeof(display_interface));
    Colour invalid_colour{};
    invalid_colour.red=std::numeric_limits<float>::quiet_NaN();
    std::memcpy(ui_component.data()+0x70,&invalid_colour,
                sizeof(invalid_colour));
    std::printf("draw-result:%d\n",draw_panel_text(
        "Panel",NativeRectangle{10,20,110,40},ui_component.data()));
    std::array<std::uint8_t,0x80> tooltip_panel{};
    void* tooltip_component=ui_component.data();
    std::memcpy(tooltip_panel.data()+kResourcePanelBackgroundOffset,
                &tooltip_component,sizeof(tooltip_component));
    const std::int32_t cursor_inside_x=20,cursor_inside_y=20;
    std::memcpy(static_cast<std::uint8_t*>(module_memory)+kCursorXRva,
                &cursor_inside_x,sizeof(cursor_inside_x));
    std::memcpy(static_cast<std::uint8_t*>(module_memory)+kCursorYRva,
                &cursor_inside_y,sizeof(cursor_inside_y));
    std::array<NativeRectangle,kResourceCount> hover_rectangles{};
    hover_rectangles[1]={10,10,40,40};
    update_added_resource_tooltip(tooltip_panel.data(),team_storage.data(),
                                  hover_rectangles);
    update_added_resource_tooltip(tooltip_panel.data(),team_storage.data(),
                                  hover_rectangles);
    const std::int32_t cursor_outside=1000;
    std::memcpy(static_cast<std::uint8_t*>(module_memory)+kCursorXRva,
                &cursor_outside,sizeof(cursor_outside));
    update_added_resource_tooltip(tooltip_panel.data(),team_storage.data(),
                                  hover_rectangles);
    g_runtime_alive=false;
    g_lock_ready=false;
    DeleteCriticalSection(&g_lock);
    HMODULE feature_pack=LoadLibraryA("A2FOFeaturePack.dll");
    if(!feature_pack){
        std::fprintf(stderr,"fixture LoadLibraryA(A2FOFeaturePack.dll) failed: %lu\n",
                     static_cast<unsigned long>(GetLastError()));
        return 3;
    }
    api.struct_size=sizeof(api);
    api.api_version=A2FO_MODULE_API_VERSION;
    api.capabilities=A2FO_CAP_GAME_OBJECT_CLASS_LOADED|
        A2FO_CAP_RACE_LOADED|A2FO_CAP_PRODUCER_EVENTS;
    api.armada_module=&fixture_armada_module;
    api.fleetops_module=&fixture_fleet_module;
    api.register_game_object_class_loaded_handler=&fixture_register_class;
    api.register_race_loaded_handler=&fixture_register_race;
    api.register_producer_event_handler=&fixture_register_producer;
    g_fixture_register_calls=0;
    g_fixture_fail_register=1;
    std::printf("init-register-fail:%d\n",A2FO_ModuleInit(&api));
    g_fixture_register_calls=0;
    g_fixture_fail_register=-1;
    g_fixture_hook_calls=0;
    g_fixture_patch_calls=0;
    g_fixture_throw_patch=0;
    try{A2FO_ModuleInit(&api);}
    catch(const std::exception& error){
        std::printf("init-presentation-throw:%s:%d:%d\n",error.what(),
                    g_fixture_hook_calls,g_fixture_patch_calls);
    }
    g_fixture_throw_patch=-1;
    if(g_lock_ready){DeleteCriticalSection(&g_lock);g_lock_ready=false;}
    g_runtime_alive=false;
    g_fixture_register_calls=0;
    g_fixture_hook_calls=0;
    g_fixture_patch_calls=0;
    std::printf("init-success:%d\n",A2FO_ModuleInit(&api));
    A2FO_ModuleShutdown();
    if(g_lock_ready){DeleteCriticalSection(&g_lock);g_lock_ready=false;}
    FreeLibrary(feature_pack);
    g_api=nullptr;
    g_armada=nullptr;
    g_fleet_ops=nullptr;
    VirtualFree(fleet_memory,0,MEM_RELEASE);
    VirtualFree(module_memory,0,MEM_RELEASE);
    return 0;
}
