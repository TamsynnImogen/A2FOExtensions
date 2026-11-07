#ifndef A2FO_STAGE8_MISSION_SOURCE
#error A2FO_STAGE8_MISSION_SOURCE is required
#endif
#include A2FO_STAGE8_MISSION_SOURCE
#include <iostream>
#include <fstream>
#include <stdexcept>

extern "C" HWND A2FO_CALL a2fo_mission_selector_call_thiscall_0(void*,void*) {
    return nullptr;
}

static void write_file(const std::string& path,const std::string& contents) {
    std::ofstream file(path,std::ios::binary);
    file << contents;
}

static void print_map(const Metadata& map,const char* tag) {
    std::vector<std::string> entries;
    for(const auto& item:map)entries.push_back(item.first+"="+item.second);
    std::sort(entries.begin(),entries.end());
    for(const auto& entry:entries)std::cout<<tag<<":"<<entry<<"\n";
}

static void print_catalog(const std::vector<CampaignEntry>& campaigns,const char* tag) {
    std::cout<<tag<<":count="<<campaigns.size()<<"\n";
    for(const auto& campaign:campaigns) {
        std::cout<<tag<<":campaign="<<campaign.index<<","<<campaign.unlocked
            <<","<<campaign.title<<","<<campaign.menu_view<<","<<campaign.missions.size()<<"\n";
        for(const auto& mission:campaign.missions)
            std::cout<<tag<<":mission="<<mission.index<<","<<mission.filename
                <<","<<mission.title<<","<<mission.unlocked<<","<<mission.launch_campaign
                <<","<<mission.launch_mission<<","<<mission.replace_launch_filename
                <<","<<mission.thumbnail_path<<"\n";
    }
}

static bool throw_armada=false;
static bool throw_root=false;
static void* A2FO_CALL fake_armada() {
    if(throw_armada)throw std::runtime_error("armada module failure");
    return g_armada;
}
static void A2FO_CALL fake_log(const char*,const char* message) {
    std::cout<<"log:"<<message<<"\n";
}
static std::uint32_t A2FO_CALL fake_root_count() {return 3;}
static const char* A2FO_CALL fake_root(std::uint32_t index) {
    if(throw_root&&index==1)throw std::runtime_error("root failure");
    return index==0?"mission_fixture":index==1?"":"second_root";
}
static bool A2FO_CALL fake_hook(void*,void*,std::size_t,
    const std::uint8_t*,A2FO_InlineHook*) {return false;}
static int __cdecl original_single() {return 42;}
static int __cdecl original_mission(HWND) {return 9;}

int main() {
    std::cout<<"text:"<<trim("  '\\tAlpha Beta\\t'  ")<<","<<lower("ABC_def")
        <<","<<join_path("root", "file")<<","<<file_stem("dir\\test.bzn")
        <<","<<humanize_mission_name("bzn\\a2_fed003.bzn")<<"\n";
    for(const std::string name:{"mod_testMission12.bzn","ABC12def-Thing.bzn",
        "a2_tutorial0001.bzn","a2_kling09.bzn","a2_borg000.bzn"})
        std::cout<<"humanize:"<<name<<":"<<humanize_mission_name(name)<<"\n";
    std::cout<<"escape:"<<decode_escapes("a\\nb\\tc\\x")<<"\n";
    Metadata index_cases{{"campaign12.title","x"},{"campaign0003.mission5","x"},
        {"campaign127.title","x"},{"campaign128.title","x"},{"campaignx","x"}};
    for(int index:configured_campaign_indices(index_cases))
        std::cout<<"index:"<<index<<"\n";
    CreateDirectoryA("mission_fixture",nullptr);
    CreateDirectoryA("mission_fixture\\misc",nullptr);
    std::string metadata_source=
        "[selector]\r\ncatalogMode=ini\r\n[campaign0]\r\ntitle=Custom Zero\r\n"
        "mission0.file=first.bzn\r\nmission0.title=First Mission\r\n"
        "[campaign4]\r\ntitle=Fourth\r\nmission12.file=custom.bzn\r\n"
        "mission12.nativecampaign=2\r\nmission12.nativemission=3\r\n";
    metadata_source+="mission13.file="+std::string(kMaximumMissionName,'x')+"\r\n";
    write_file("mission_fixture\\mission_selector.ini",metadata_source);
    write_file("mission_fixture\\misc\\mission_selector.ini",
        "[campaign0]\r\nmission0.description=Line\\nTwo\r\n"
        "[campaign4]\r\nmission12.unlocked=no\r\n");
    write_file("mission_fixture\\label.map",
        "mission_select\n{\n+ first.bzn {Native First}\n"
        "+ title {ignored}\n}\nsingle_player {\n"
        "+ tutorials {Tutorial Display}\n}\n");
    write_file("mission_fixture\\misc\\label.map",
        "mission_select {\n+ custom.bzn {Custom Label}\n}\n"
        "single_player\n{\n+ federation_campaign {Fed Display}\n}\n");
    g_roots={"mission_fixture"};
    print_map(load_metadata(),"metadata");
    print_map(load_mission_labels(),"mission_label");
    print_map(load_campaign_labels(),"campaign_label");
    write_file("mission_fixture\\oversize.ini",
        std::string(static_cast<std::size_t>(kMaximumMetadataSize)+1,'x'));
    Metadata oversized;
    merge_metadata_file("mission_fixture\\oversize.ini",oversized);
    std::cout<<"sizegate:"<<oversized.size()<<"\n";
    constexpr std::size_t image_size=0x3b0000;
    auto* image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,image_size,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
    if(!image){std::cerr<<"VirtualAlloc failed\n";return 2;}
    g_armada=reinterpret_cast<HMODULE>(image);
    const std::uint8_t yes[]={0xb8,1,0,0,0,0xc3};
    std::memcpy(image+kCampaignAvailableRva,yes,sizeof(yes));
    std::memcpy(image+kSetupMissionRva,yes,sizeof(yes));
    *at<std::int8_t>(g_armada,kTutorialMissionCountRva)=10;
    auto* limits=at<std::int8_t>(g_armada,kCampaignMissionLimitRva);
    for(std::size_t i=0;i<kCampaignCount;++i)limits[i]=9;
    auto** filenames=at<const char*>(g_armada,kMissionFilenameTableRva);
    for(std::size_t i=0;i<kMissionTableSize;++i)filenames[i]=nullptr;
    filenames[0]="native00.bzn";
    filenames[10]="native10.bzn";
    auto catalog=build_catalog();
    print_catalog(catalog,"ini");
    SelectorContext selected;
    selected.campaigns=catalog;
    if(!selected.campaigns.empty()&&!selected.campaigns[0].missions.empty()) {
        std::cout<<"menu:"<<selected_menu_view(selected)<<"\n";
        start_selected_mission(selected);
        std::cout<<"launch:"<<*at<std::int32_t>(g_armada,kCurrentCampaignRva)
            <<","<<static_cast<int>(*at<std::int8_t>(g_armada,kSelectedMissionRva))
            <<","<<g_accept_selected_mission<<","<<filenames[0]<<"\n";
        restore_launch_filename_override();
        std::cout<<"restored:"<<filenames[0]<<"\n";
        const std::uint8_t no[]={0x31,0xc0,0xc3};
        std::memcpy(image+kSetupMissionRva,no,sizeof(no));
        start_selected_mission(selected);
        std::cout<<"launch_rejected:"<<g_accept_selected_mission<<","<<filenames[0]
            <<","<<g_launch_filename_row<<"\n";
        std::memcpy(image+kSetupMissionRva,yes,sizeof(yes));
        std::cout<<"retain_bad:"<<retain_launch_filename_override("x.bzn",
            kMissionTableSize)<<","<<g_launch_filename_row<<"\n";
    }
    g_roots.clear();
    print_catalog(build_catalog(),"native");
    A2FO_ModuleApi api{};
    api.struct_size=A2FO_MODULE_API_V4_BASE_SIZE;
    api.api_version=A2FO_MODULE_API_VERSION;
    api.log=fake_log;
    api.armada_module=fake_armada;
    api.install_inline_hook=fake_hook;
    api.extension_root_count=fake_root_count;
    api.extension_root=fake_root;
    std::cout<<"init_invalid:"<<A2FO_ModuleInit(nullptr)<<"\n";
    std::cout<<"init_mismatch:"<<A2FO_ModuleInit(&api)<<","<<(g_api==nullptr)<<"\n";
    throw_armada=true;
    try{A2FO_ModuleInit(&api);}catch(const std::exception& e){
        std::cout<<"init_throw:"<<e.what()<<"\n";
    }
    throw_armada=false;
    g_api=&api;
    cache_roots();
    std::cout<<"roots:";
    for(const auto& root:g_roots)std::cout<<root<<",";
    std::cout<<"\n";
    throw_root=true;
    try{cache_roots();}catch(const std::exception& e){
        std::cout<<"roots_throw:"<<e.what()<<","<<g_roots.size()<<"\n";
    }
    throw_root=false;
    g_api=nullptr;
    g_do_single_hook.gateway=reinterpret_cast<void*>(&original_single);
    g_do_mission_select_hook.gateway=reinterpret_cast<void*>(&original_mission);
    for(std::size_t index=0;index<kMissionTableSize;++index)filenames[index]=nullptr;
    g_roots.clear();
    std::cout<<"single_fallback:"<<do_single_hook()<<"\n";
    InterlockedExchange(&g_accept_selected_mission,1);
    std::cout<<"accept:"<<do_mission_select_hook(nullptr)<<"\n";
    InterlockedExchange(&g_accept_selected_mission,0);
    std::cout<<"mission_fallback:"<<do_mission_select_hook(nullptr)<<"\n";
    A2FO_ModuleShutdown();
    VirtualFree(image,0,MEM_RELEASE);
}
