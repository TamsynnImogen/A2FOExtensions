// Whole frozen/current A1 source with synthetic class/vtable memory.
#include <windows.h>
#include <cstddef>
void a2fo_fixture_class_odf_name(void*,char*,std::size_t) noexcept;
#include A1_SOURCE
#include <stdexcept>

static std::uint8_t* fake_armada{};
static void* upgrade_class{};
static void* label_upgrade_class{};
static void* regular_class{};
static void* producer_class{};
static const char* producer_name="fbase";
static void** live_items{};
static bool original_visible=true;
static unsigned original_mutation=0;
static bool throw_label=false;
static bool throw_original=false;
static void dummy_executable(){}

static bool A2FO_CALL fixture_classlabel(void* object,char* output,std::uint32_t size){
    std::printf("classlabel:%u\n",object==label_upgrade_class?1u:object==regular_class?2u:3u);
    if(throw_label)throw std::runtime_error("classlabel");
    const char* label=object==label_upgrade_class?"upgrade":"ship";
    std::snprintf(output,size,"%s",label);return true;
}
void a2fo_fixture_class_odf_name(void* object,char* output,std::size_t size) noexcept {
    const char* name=object==producer_class?producer_name:"unknown";
    std::printf("odf-name:%s\n",name);
    if(output&&size)std::snprintf(output,size,"%s",name);
}
static bool __attribute__((regparm(3))) fixture_original(
    void* producer,std::uintptr_t team,std::uintptr_t slot){
    std::printf("original:%u:%lu:%lu\n",producer==producer_class,
        static_cast<unsigned long>(team),static_cast<unsigned long>(slot));
    if(throw_original)throw std::runtime_error("original");
    if(original_mutation==1)live_items[slot]=upgrade_class;
    if(original_mutation==2)g_officer_upgrade_races[upgrade_class][0]='\0';
    if(original_mutation==3){
        // The original samples the item-table pointer and gateway before this
        // call, but samples the selected row after it returns.
        void* absent=nullptr;
        std::memcpy(static_cast<std::uint8_t*>(producer)+kProducerClassBuildItemsOffset,
                    &absent,sizeof(absent));
        g_producer_build_button_visible_hook.gateway=nullptr;
        live_items[slot]=upgrade_class;
    }
    if(original_mutation==4){
        StarbaseClassPolicy policy{};
        std::snprintf(policy.race.data(),policy.race.size(),"borg");
        g_starbase_class_policies[producer]=policy;
    }
    return original_visible;
}

static std::array<std::uint8_t,0x500> upgrade_storage{},label_storage{},regular_storage{},producer_storage{};
static std::array<void*,kProducerClassBuildItemCapacity> items{};
static void set_vtable(void* object,void* vtable){std::memcpy(object,&vtable,sizeof(vtable));}
static std::array<char,64> race(const char* text){std::array<char,64> result{};
    std::snprintf(result.data(),result.size(),"%s",text);return result;}
static void setup(){
    fake_armada=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,
        MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!fake_armada)std::abort();
    g_armada=fake_armada;
    auto* upgrade_vtable=at<void>(g_armada,kOfficerUpgradeClassVtableRva);
    auto* normal_vtable=at<void>(g_armada,kOfficerUpgradeClassVtableRva+0x100);
    void* executable=reinterpret_cast<void*>(&dummy_executable);
    std::memcpy(upgrade_vtable,&executable,sizeof(executable));
    std::memcpy(normal_vtable,&executable,sizeof(executable));
    upgrade_class=upgrade_storage.data();label_upgrade_class=label_storage.data();
    regular_class=regular_storage.data();producer_class=producer_storage.data();
    set_vtable(upgrade_class,upgrade_vtable);set_vtable(label_upgrade_class,normal_vtable);
    set_vtable(regular_class,normal_vtable);set_vtable(producer_class,normal_vtable);
    live_items=items.data();void* table=live_items;
    std::memcpy(producer_storage.data()+kProducerClassBuildItemsOffset,&table,sizeof(table));
    InitializeCriticalSection(&g_officer_state_lock);g_officer_state_lock_ready=true;
    g_producer_build_button_visible_hook.gateway=reinterpret_cast<void*>(&fixture_original);
}
static void teardown(){g_armada=nullptr;VirtualFree(fake_armada,0,MEM_RELEASE);
    g_officer_state_lock_ready=false;DeleteCriticalSection(&g_officer_state_lock);}

static void class_cases(){
    std::printf("class:vtable:%u\n",is_officer_upgrade_class(upgrade_class));
    std::printf("class:label:%u\n",is_officer_upgrade_class(label_upgrade_class));
    std::printf("class:regular:%u\n",is_officer_upgrade_class(regular_class));
    std::printf("class:null:%u\n",is_officer_upgrade_class(nullptr));
    void* saved=g_armada;g_armada=nullptr;
    std::printf("class:no-armada:%u\n",is_officer_upgrade_class(upgrade_class));g_armada=saved;
    for(const char* name:{static_cast<const char*>(nullptr),"","starbase","ConstructionRig","construction","other"})
        std::printf("starbase-label:%s:%u\n",name?name:"<null>",is_starbase_label(name));
}
static void scan_cases(){
    items.fill(nullptr);
    std::printf("build-officer:empty:%u\n",producer_class_has_officer_upgrade_build_item(producer_class));
    items.back()=label_upgrade_class;
    std::printf("build-officer:last:%u\n",producer_class_has_officer_upgrade_build_item(producer_class));
    items.back()=regular_class;
    std::printf("build-officer:regular:%u\n",producer_class_has_officer_upgrade_build_item(producer_class));
    std::printf("build-any:regular:%u\n",producer_class_has_build_items(producer_class));
    items.fill(nullptr);
    std::printf("build-any:empty:%u\n",producer_class_has_build_items(producer_class));
    std::printf("build-officer:null:%u\n",producer_class_has_officer_upgrade_build_item(nullptr));
    std::printf("build-any:null:%u\n",producer_class_has_build_items(nullptr));
}
static void race_cases(){
    for(const char* name:{"fbasehq","bconst","klingoff","romoff","zclon","DOMINION","unknown","bbase_fbase","dominion_romoff"}){
        std::array<char,64> result{};result.fill('Q');
        const bool found=infer_starbase_race_from_object_name(name,&result);
        std::printf("race-infer:%s:%u:%s\n",name,found,result.data());
    }
    auto result=race("old");
    std::printf("race-infer:null:%u\n",infer_starbase_race_from_object_name(nullptr,&result));
}
static void match_cases(){
    g_officer_upgrade_races[upgrade_class]=race("federation");
    std::printf("race-match:yes:%u\n",officer_upgrade_matches_race(race("FEDERATION"),upgrade_class));
    std::printf("race-match:no:%u\n",officer_upgrade_matches_race(race("borg"),upgrade_class));
    std::printf("race-match:empty:%u\n",officer_upgrade_matches_race(race(""),upgrade_class));
    StarbaseClassPolicy policy{};policy.race=race("federation");
    g_starbase_class_policies[producer_class]=policy;
    std::printf("registered-match:yes:%u\n",officer_upgrade_matches_registered_starbase(producer_class,upgrade_class));
    g_starbase_class_policies[producer_class].race=race("borg");
    std::printf("registered-match:no:%u\n",officer_upgrade_matches_registered_starbase(producer_class,upgrade_class));
    g_starbase_class_policies.erase(producer_class);
    std::printf("registered-match:missing:%u\n",officer_upgrade_matches_registered_starbase(producer_class,upgrade_class));
    g_officer_upgrade_races[regular_class]=race("");
    std::printf("registered-match:empty-target:%u\n",officer_upgrade_matches_registered_starbase(producer_class,regular_class));
}
static void button_cases(){
    items.fill(nullptr);items[0]=regular_class;
    original_visible=true;
    std::printf("button:regular:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    items[0]=upgrade_class;g_officer_upgrade_races[upgrade_class]=race("federation");
    StarbaseClassPolicy policy{};policy.race=race("federation");g_starbase_class_policies[producer_class]=policy;
    std::printf("button:registered:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    policy.race=race("borg");g_starbase_class_policies[producer_class]=policy;
    std::printf("button:wrong-race:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    g_starbase_class_policies.erase(producer_class);producer_name="fbase";
    std::printf("button:fallback:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    producer_name="unknown";
    std::printf("button:unknown:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    producer_name="fbase";items[0]=regular_class;original_mutation=1;
    std::printf("button:mutated-slot:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    original_mutation=2;items[0]=upgrade_class;
    std::printf("button:mutated-map:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    original_mutation=0;g_officer_upgrade_races[upgrade_class]=race("federation");
    void* original_gateway=g_producer_build_button_visible_hook.gateway;
    original_mutation=3;items[0]=regular_class;
    std::printf("button:mutated-table-gateway:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    void* restored_items=items.data();
    std::memcpy(producer_storage.data()+kProducerClassBuildItemsOffset,
                &restored_items,sizeof(restored_items));
    g_producer_build_button_visible_hook.gateway=original_gateway;
    original_mutation=4;items[0]=upgrade_class;
    std::printf("button:mutated-registration:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    original_mutation=0;g_starbase_class_policies.erase(producer_class);
    original_visible=false;
    std::printf("button:original-off:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    original_visible=true;
    std::printf("button:outside:%u\n",producer_build_button_visible_hook(producer_class,1,kProducerClassBuildItemCapacity));
    void* old=g_producer_build_button_visible_hook.gateway;
    g_producer_build_button_visible_hook.gateway=nullptr;
    std::printf("button:no-gateway:%u\n",producer_build_button_visible_hook(producer_class,1,0));
    g_producer_build_button_visible_hook.gateway=old;
    std::printf("button:null:%u\n",producer_build_button_visible_hook(nullptr,1,0));
}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    std::set_terminate([]{
        std::puts("A1 officer expected native noexcept termination");
        std::fflush(nullptr);std::_Exit(77);
    });
    A2FO_ModuleApi api{};api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.get_original_classlabel=&fixture_classlabel;g_api=&api;
    setup();
    if(argc==2&&std::strcmp(argv[1],"throw-label")==0){
        throw_label=true;is_officer_upgrade_class(label_upgrade_class);return 3;
    }
    if(argc==2&&std::strcmp(argv[1],"throw-original")==0){
        throw_original=true;producer_build_button_visible_hook(producer_class,1,0);return 3;
    }
    class_cases();scan_cases();race_cases();match_cases();button_cases();teardown();
    std::puts("A1 officer native fixture: PASS");
}
