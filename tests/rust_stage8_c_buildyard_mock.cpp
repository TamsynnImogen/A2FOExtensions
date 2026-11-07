// Both variants include their complete BuildYard translation unit and its
// original matching private headers. The fixture never starts a game/device.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include A2FO_STAGE8_BUILDYARD_SOURCE

namespace {
int scenario=0;
int patched_calls=0;
bool mutate_provider_ready=false;
void A2FO_CALL fixture_log(const char*,const char* text) {
    std::string line(text);
    if(line.find("tooltip provider invoked")!=std::string::npos){
        const auto native=line.find("native=");
        const auto comma=line.find(',',native);
        if(native!=std::string::npos&&comma!=std::string::npos)
            line.replace(native+7,comma-native-7,"<native>");
        const auto stream=line.find("stream=");
        const auto close=line.find(')',stream);
        if(stream!=std::string::npos&&close!=std::string::npos)
            line.replace(stream+7,close-stream-7,"<stream>");
    }
    std::printf("log %s\n",line.c_str());
    if(mutate_provider_ready&&
       line.find("tooltip provider invoked")!=std::string::npos){
        a2fo::g_tooltip_runtime_ready=false;
        std::puts("native mutate provider_ready=0");
    }
}
bool A2FO_CALL fixture_patch_call(void*,void*,const std::uint8_t* expected,
    std::size_t length){
    ++patched_calls;
    std::printf("native patch_call %d %lu %02x\n",patched_calls,
        static_cast<unsigned long>(length),expected[0]);
    return patched_calls!=2;
}
}
namespace a2fo {
std::uint32_t resolve_featurepack_odf_project_id(const char*) noexcept {
    std::puts("native resolve_odf"); return scenario==4?901:0;
}
}
extern "C" {
void a2fo_buildyard_call_tooltip_provider(void*,void*,void*) {}
std::uintptr_t a2fo_call_thiscall_0(void*,void*) {return 0;}
std::uintptr_t a2fo_call_thiscall_1(void*,void*,std::uintptr_t) {return 0;}
std::uintptr_t a2fo_call_thiscall_3(void*,void*,std::uintptr_t command,
    std::uintptr_t output,std::uintptr_t) {
    std::printf("native project_get %s\n",
        reinterpret_cast<const char*>(command));
    *reinterpret_cast<std::uint32_t*>(output)=scenario==3?777:0;
    return scenario==3?1:0;
}
std::uintptr_t a2fo_call_thiscall_4(void*,void*,std::uintptr_t command,
    std::uintptr_t output,std::uintptr_t,std::uintptr_t) {
    const char* name=reinterpret_cast<const char*>(command);
    std::printf("native string_get %s\n",name);
    if(scenario==0)return 0;
    const char* result=std::strstr(name,"ButtonTooltip")
        ? "AUTOTOOLTIP-gate" : scenario==2?"Gate":"Other";
    std::strcpy(reinterpret_cast<char*>(output),result);
    return 1;
}
std::uintptr_t a2fo_buildyard_call_delphi_reg3_stack1(void*,void*,void*,
    void* output,std::uintptr_t) {
    *static_cast<std::uint32_t*>(output)=42;
    std::puts("native required_project");
    return 8;
}
std::uintptr_t a2fo_buildyard_call_delphi_reg2(void*,std::uintptr_t,
    std::uintptr_t) {return 0;}
void a2fo_buildyard_project_id_call_bridge() {}
void a2fo_buildyard_localization_lookup_bridge() {}
void a2fo_buildyard_tree_text_bridge() {}
void __attribute__((regparm(3))) a2fo_buildyard_tree_root_bridge(
    void*,const char*,int) {}
}

int main(int argc,char**){
    using namespace a2fo;
    if(argc==77){
        (void)install_global_localization_hook();
        (void)patch_fleetops_localization_calls();
        (void)install_tooltip_render_hooks();
        (void)install_component_tooltip_setter_hooks();
    }
    std::size_t index=0;
    for(const char* command:{"module0RequiredTechnology",
            "Module7RequiredTechnology", "Module007RequiredTechnology",
            "Module1024RequiredTechnology", "Module4Requiredtechnology",
            "Module4RequiredTechnologyX", "Module4RequiredTechnology"}){
        const bool ok=parse_required_technology_command(command,&index);
        std::printf("command %s %u %lu\n",command,ok,
            static_cast<unsigned long>(index));
    }
    for(const std::string input:{" Ship.ODF ","Units/Foo","C:\\UP\\Name.odf",
            "", "a/b\\c.ODF"}){
        std::printf("normalize %s => %s / %s\n",input.c_str(),
            normalize_odf_key(input).c_str(),
            basename_without_extension(input).c_str());
        std::printf("suffix %s %u\n",input.c_str(),
            ends_with_case_insensitive(input,".ODF"));
    }
    for(const char* input:{" Hello World\nSecond", "  A  ","", "a\rB"}){
        std::printf("tooltip %s => %s\n",input,first_tooltip_line(input).c_str());
    }
    GateEvaluation evaluation{};
    std::printf("missing available '%s'\n",
        format_missing_requirements(evaluation).c_str());
    evaluation.available=false;
    evaluation.definition_missing=true;
    evaluation.gate_key="foo/BAR.odf";
    std::printf("missing definition '%s'\n",
        format_missing_requirements(evaluation).c_str());
    evaluation.definition_missing=false;
    evaluation.definition_unsupported=true;
    std::printf("missing unsupported '%s'\n",
        format_missing_requirements(evaluation).c_str());
    evaluation.definition_unsupported=false;
    evaluation.symbolic_missing="Race X";
    evaluation.native_required_missing=true;
    std::printf("missing joined '%s'\n",
        format_missing_requirements(evaluation).c_str());
    const std::uint8_t prologues[][16]={{0x55,0x8b,0xec,0x6a,0},
        {0x55,0x8b,0xec,0x83,0}, {0x55,0x8b,0xec,0x81,0},
        {0x50,0x51,0x52,0x8b,0xc0}, {0x90}};
    for(const auto& bytes:prologues){
        auto* code=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,4096,
            MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
        std::memcpy(code,bytes,16);
        std::printf("prologue %u %lu\n",code[0],
            static_cast<unsigned long>(localization_hook_length(code)));
        VirtualFree(code,0,MEM_RELEASE);
    }
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);
    api.log=fixture_log;
    api.patch_call=fixture_patch_call;
    g_api=&api;
    g_armada=static_cast<HMODULE>(VirtualAlloc(nullptr,0x200000,
        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    g_fleet_ops=static_cast<HMODULE>(VirtualAlloc(nullptr,0x200000,
        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    int parameter_db=0;
    for(scenario=0;scenario<=4;++scenario){
        g_module_technologies.clear();
        g_symbolic_bindings.clear();
        g_tooltip_bindings.clear();
        g_next_synthetic_project_id=kSyntheticProjectIdStart;
        if(scenario==2){
            ModuleTechnologyDefinition definition{};
            definition.key="gate.odf";
            g_module_technologies.emplace(definition.key,definition);
        }
        std::uint32_t required=0;
        const auto result=a2fo_buildyard_project_id_call_handler(
            &parameter_db,const_cast<char*>("Module2RequiredTechnology"),
            &required,0);
        std::printf("capture %d return %lu required %lu bindings %lu\n",
            scenario,static_cast<unsigned long>(result),
            static_cast<unsigned long>(required),
            static_cast<unsigned long>(g_symbolic_bindings.size()));
    }
    InitializeCriticalSection(&g_policy_lock);
    g_policy_lock_ready=true;
    g_symbolic_bindings.emplace(0xa2f00000u,SymbolicBinding{});
    std::uint8_t button[0x80]{};
    std::uint8_t entries[0x60*3]{};
    void* entry_pointer=entries;
    void* config=&entry_pointer;
    std::memcpy(button+0x78,&config,sizeof(config));
    std::uint32_t slot=2;
    std::memcpy(button+0x7c,&slot,4);
    std::uint32_t stored=0xa2f00000u;
    const char* key="AUTOTOOLTIP-gate";
    std::memcpy(entries+2*0x60+0x08,&stored,4);
    std::memcpy(entries+2*0x60+0x0c,&key,sizeof(key));
    std::uint32_t synthetic=0;
    std::size_t module=0;
    std::string tooltip_key;
    const bool bound=buildyard_button_tooltip_binding(button,&synthetic,
        &tooltip_key,&module);
    std::printf("button valid %u %08lx %s %lu\n",bound,
        static_cast<unsigned long>(synthetic),tooltip_key.c_str(),
        static_cast<unsigned long>(module));
    slot=kMaximumModuleIndex+1;
    std::memcpy(button+0x7c,&slot,4);
    std::printf("button range %u\n",buildyard_button_tooltip_binding(
        button,&synthetic,&tooltip_key,&module));
    slot=2;
    std::memcpy(button+0x7c,&slot,4);
    g_tooltip_bindings.emplace(key,stored);
    g_odf_display_names.emplace("gate.odf","Gate");
    auto binding=g_symbolic_bindings.find(stored);
    binding->second.gate_key="gate.odf";
    ModuleTechnologyDefinition blocked{};
    blocked.key="gate.odf";
    blocked.unsupported=true;
    g_module_technologies[blocked.key]=blocked;
    g_require_active_tree=false;
    g_requirement_team_ready=true;
    g_native_count_ready=true;
    g_tooltip_runtime_ready=true;
    auto* local_team=at<std::uint8_t>(g_armada,kLocalTeamRva);
    local_team[0]=0x31;local_team[1]=0xc0;local_team[2]=0xc3;
    DWORD old_protect=0;
    VirtualProtect(g_armada,0x200000,PAGE_EXECUTE_READWRITE,&old_protect);
    const auto generated=match_generated_autotooltip("Gate\nMore");
    std::printf("match generated %u %08lx %s %s\n",generated.matched,
        static_cast<unsigned long>(generated.synthetic),
        generated.tooltip_key.c_str(),generated.source_odf.c_str());
    const auto direct=match_component_tooltip_text("AUTOTOOLTIP-gate");
    std::printf("match direct %u %08lx %s\n",direct.matched,
        static_cast<unsigned long>(direct.synthetic),direct.source_odf.c_str());
    const auto display=match_component_tooltip_text("Gate");
    std::printf("match display %u %08lx %s\n",display.matched,
        static_cast<unsigned long>(display.synthetic),display.source_odf.c_str());
    int component=0;
    const char* composed_component=compose_component_module_tooltip(
        &component,"Gate",false);
    std::printf("component text %s\n",composed_component);
    std::uint8_t tooltip[kTooltipTextOffset+sizeof(const char*)]{};
    const char* native_text="Gate";
    std::memcpy(tooltip+kTooltipTextOffset,&native_text,sizeof(native_text));
    std::string composed_generated;
    const char* original_text=nullptr;
    const bool replaced=compose_generated_module_tooltip(tooltip,
        &composed_generated,&original_text);
    std::printf("generated compose %u %s %s\n",replaced,
        composed_generated.c_str(),original_text?original_text:"<null>");
    std::uint8_t stream[0x8c]{};
    buildyard_module_tooltip_provider_hook(button,nullptr,stream);
    g_logged_tooltip_appends.clear();
    g_tooltip_runtime_ready=true;
    mutate_provider_ready=true;
    buildyard_module_tooltip_provider_hook(button,nullptr,stream);
    mutate_provider_ready=false;
    g_policy_lock_ready=false;
    DeleteCriticalSection(&g_policy_lock);
    auto* image=reinterpret_cast<std::uint8_t*>(g_fleet_ops);
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic=IMAGE_DOS_SIGNATURE;
    dos->e_lfanew=0x100;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(image+0x100);
    nt->Signature=IMAGE_NT_SIGNATURE;
    nt->FileHeader.NumberOfSections=1;
    nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER32);
    auto* section=IMAGE_FIRST_SECTION(nt);
    section->Characteristics=IMAGE_SCN_MEM_EXECUTE;
    section->Misc.VirtualSize=20;
    section->SizeOfRawData=20;
    section->VirtualAddress=0x1000;
    auto* code=image+0x1000;
    const auto target=reinterpret_cast<std::uintptr_t>(
        at(g_armada,kLocalizationLookupRva));
    for(int offset:{0,5,10}){
        code[offset]=0xe8;
        const auto relative=static_cast<std::int32_t>(target-
            reinterpret_cast<std::uintptr_t>(code+offset)-5);
        std::memcpy(code+offset+1,&relative,4);
    }
    std::printf("patch total %lu\n",
        static_cast<unsigned long>(patch_fleetops_localization_calls()));
    VirtualFree(g_armada,0,MEM_RELEASE);
    VirtualFree(g_fleet_ops,0,MEM_RELEASE);
    return 0;
}
