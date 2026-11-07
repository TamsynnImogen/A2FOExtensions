// Headless native tests. Synthetic committed images stand in for engine code;
// installed PE images are mapped for byte checks only, never executed.
#include "../modules/A2FOMultiYardBuild/module.cpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>

extern "C" {
void test_call_stub(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
void test_capture(), test_capture_one(), test_capture_many();
void test_capture_check(), test_capture_accept(), test_capture_cancel_empty();
void test_capture_route_loop(), test_queue_gateway();
void test_capture_menu_open(), test_capture_menu_exit();
void test_native_qsize();
extern std::uint32_t test_before_sp, test_after_sp, test_after_eax, test_after_ebx;
extern std::uint32_t test_after_esi, test_after_edi, test_after_ecx, test_path;
extern std::uint32_t test_native_ecx, test_cont_edx, test_menu_entry_eax;
}

namespace {
struct Craft { std::array<std::uint8_t,0x300> bytes{}; };
std::array<Craft,4> craft{};
std::array<void*,4> objects{};
std::array<std::uint32_t,4> handles{{101,102,103,104}};
void* yard_class=reinterpret_cast<void*>(0x12340000);
void* other_class=reinterpret_cast<void*>(0x12350000);
int team=2;
unsigned patches=0, fail_patch=0, commands=0, registrations=0;
std::uint32_t command_kind=0;
void* command_target=nullptr;
void* command_class=nullptr;
char config_root[MAX_PATH]{};
constexpr std::uint8_t kMenuRejectBytes[]{0x8b,0x45,0xfc,0x83,0xb8,0x24,0x01,0,0,0x04,
                                          0x0f,0x85,0x35,0x01,0,0};
constexpr std::uint8_t kMenuResetBytes[]{0x8b,0x45,0xfc,0x33,0xd2,0x89,0x90,0x24,0x01,0,0,
                                         0xe9,0xb2,0x01,0,0};

template<typename T> void put(void* base,std::ptrdiff_t offset,T value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset,&value,sizeof(value));
}
template<typename T> T get_signed(const void* base,std::ptrdiff_t offset) {
    T value{};
    std::memcpy(&value,static_cast<const std::uint8_t*>(base)+offset,sizeof(value));
    return value;
}
void* __cdecl fake_entity(std::uint32_t handle) {
    for (std::size_t i=0;i<handles.size();++i) if (handle==handles[i]) return objects[i];
    return nullptr;
}
int __cdecl fake_team() { return team; }
std::uint32_t __attribute__((fastcall)) fake_qsize(void* self,void*) {
    return read<std::uint32_t>(self,0x200);
}
extern "C" void test_queue_handler(void* self,std::uintptr_t kind,std::uintptr_t klass) {
    ++commands; command_target=self; command_kind=kind;
    command_class=reinterpret_cast<void*>(klass);
    auto count=read<std::uint32_t>(self,0x200);
    put(self,0x200,count+1);
}
void A2FO_CALL fake_log(const char*,const char*) {}
void* A2FO_CALL fake_armada() { return g_armada; }
void* A2FO_CALL fake_fo() { return g_fleet_ops; }
std::uint32_t A2FO_CALL fake_root_count() { return 1; }
const char* A2FO_CALL fake_root(std::uint32_t i) { return i==0 ? config_root : nullptr; }
bool A2FO_CALL fake_patch(void* target,void*,const std::uint8_t* expected,std::size_t n) {
    assert(readable(target,n) && std::memcmp(target,expected,n)==0);
    ++patches;
    return patches!=fail_patch;
}
bool A2FO_CALL fake_register(const char*,const char* const* fields,std::uint32_t n,
                             A2FO_GameObjectClassLoadedHandler handler,void*) {
    assert(n==1 && std::strcmp(fields[0],"classLabel")==0 && handler==class_loaded);
    ++registrations;
    return true;
}
void trampoline(void* location,void* target) {
    const std::uint8_t jump[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(location,jump,sizeof(jump));
    put(location,1,reinterpret_cast<std::uint32_t>(target));
}
void preserving_trampoline(void* location,void* target) {
    // Continuation capture must not overwrite the EAX being asserted.
    const auto next=reinterpret_cast<std::uint32_t>(location)+5;
    *static_cast<std::uint8_t*>(location)=0xe9;
    put(location,1,reinterpret_cast<std::uint32_t>(target)-next);
}
void seed_signatures() {
    for(const auto& s:sites)
        std::memcpy(at(s.fleet?g_fleet_ops:g_armada,s.rva),s.bytes,s.size);
    const std::uint8_t get[]{0x55,0x8b,0xec,0x8b,0x4d,0x08};
    const std::uint8_t qs[]{0x8b,0x89,0x70,0x02,0,0,0x33,0xc0,0x85,0xc9,0x74,0x08,0x8b,0x49,0x08,0x40,0x85,0xc9,0x75,0xf8,0xc3};
    std::memcpy(at(g_armada,0xcfff0),get,sizeof(get));
    std::memcpy(at(g_armada,0xb7b70),qs,sizeof(qs));
    const std::uint8_t team_code[]{0x8b,0x0d,0,0,0,0,0xe9,0x45,0x78,0x08,0};
    std::memcpy(at(g_armada,0xd0060),team_code,sizeof(team_code));
    put(at(g_armada,0xd0060),2,at(g_armada,0x36b8d4));
    // Native rejection checks Special (4), otherwise clears current menu and
    // exits. The real gate bytes are one of the pinned patch signatures.
    std::memcpy(at(g_fleet_ops,0x1e6fee),kMenuRejectBytes,sizeof(kMenuRejectBytes));
    std::memcpy(at(g_fleet_ops,0x1e7133),kMenuResetBytes,sizeof(kMenuResetBytes));
}
void setup_craft() {
    for(std::size_t i=0;i<craft.size();++i) {
        objects[i]=craft[i].bytes.data();
        put(objects[i],0x14,std::uint8_t{4});
        put(objects[i],0x28,handles[i]);
        put(objects[i],0x40,yard_class);
        put(objects[i],0xec,team);
        put(objects[i],0x200,std::uint32_t{0});
    }
    g_yard_classes.insert(yard_class);
    g_active=true;
    multi_yard_native_qsize=reinterpret_cast<std::uintptr_t>(&fake_qsize);
}
void stub_call(void (*stub)(),void* frame,std::uint32_t ebx=0x2345,
               std::uint32_t esi=0x3456,std::uint32_t edi=0x4567,
               std::uint32_t ecx=0x5678,bool preserve_esi=true) {
    test_call_stub(reinterpret_cast<void*>(stub),frame,ebx,esi,edi,ecx);
    assert(test_before_sp==test_after_sp && test_after_ebx==ebx && test_after_edi==edi);
    if (preserve_esi) assert(test_after_esi==esi);
}
} // namespace

int main(int argc,char** argv) {
    assert(argc==3);
    HMODULE installed_armada=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    HMODULE installed_fo=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    assert(installed_armada && installed_fo);
    g_armada=installed_armada; g_fleet_ops=installed_fo;
    // LoadLibraryEx on an EXE maps a reference image without applying the
    // executable's absolute operands. Verify the pinned raw operand before
    // normalizing this one private mapped-copy word, as a relocated live
    // image would contain it. No game instruction is executed or file edited.
    const auto team_global=at(g_armada,0x36b8d4);
    if (read<void*>(at(g_armada,0xd0060),2)!=team_global) {
        assert(read<std::uint32_t>(at(g_armada,0xd0060),2)==0x0076b8d4);
        DWORD old=0,ignored=0;
        assert(VirtualProtect(at(g_armada,0xd0062),sizeof(void*),PAGE_EXECUTE_READWRITE,&old));
        put(at(g_armada,0xd0060),2,team_global);
        assert(VirtualProtect(at(g_armada,0xd0062),sizeof(void*),old,&ignored));
    }
    if (!signatures_match()) {
        for(const auto& s:sites) {
            const void* address=at(s.fleet?g_fleet_ops:g_armada,s.rva);
            if (!readable(address,s.size) || std::memcmp(address,s.bytes,s.size))
                std::fprintf(stderr,"Patch signature mismatch: %s RVA %lx\n",
                    s.fleet?"FleetOpsHook":"ArmadaL",static_cast<unsigned long>(s.rva));
        }
        std::fprintf(stderr,"Mapped ArmadaL %p; GetUserTeam operand %p expected %p\n",
            static_cast<void*>(g_armada),read<void*>(at(g_armada,0xd0060),2),at(g_armada,0x36b8d4));
    }
    assert(signatures_match());
    assert(std::memcmp(at(g_fleet_ops,0x1e6fee),kMenuRejectBytes,sizeof(kMenuRejectBytes))==0);
    assert(std::memcmp(at(g_fleet_ops,0x1e7133),kMenuResetBytes,sizeof(kMenuResetBytes))==0);
    FreeLibrary(installed_armada); FreeLibrary(installed_fo);

    g_armada=static_cast<HMODULE>(VirtualAlloc(nullptr,0x400000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    g_fleet_ops=static_cast<HMODULE>(VirtualAlloc(nullptr,0x200000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    assert(g_armada && g_fleet_ops);
    seed_signatures(); assert(signatures_match());
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api); api.api_version=A2FO_MODULE_API_VERSION;
    api.log=fake_log; api.armada_module=fake_armada; api.fleetops_module=fake_fo;
    api.patch_jump=fake_patch; api.patch_call=fake_patch;
    api.register_game_object_class_loaded_handler=fake_register;
    api.extension_root_count=fake_root_count; api.extension_root=fake_root;
    const DWORD root_size=GetTempPathA(MAX_PATH,config_root);
    assert(root_size>0 && root_size<MAX_PATH-48);
    std::snprintf(config_root+root_size,MAX_PATH-root_size,"multi-yard-test-%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()));
    assert(CreateDirectoryA(config_root,nullptr) || GetLastError()==ERROR_ALREADY_EXISTS);
    const std::string ini=std::string(config_root)+"\\info.ini";
    DeleteFileA(ini.c_str());
    patches=0; assert(A2FO_ModuleInit(&api) && patches==0 && !g_active);
    assert(WritePrivateProfileStringA("mod","MultiYardBuild","0",ini.c_str()));
    patches=0; assert(A2FO_ModuleInit(&api) && patches==0 && !g_active);
    assert(WritePrivateProfileStringA("mod","MultiYardBuild","1",ini.c_str()));
    for(const auto& s:sites) {
        auto* p=static_cast<std::uint8_t*>(at(s.fleet?g_fleet_ops:g_armada,s.rva));
        *p^=1; patches=0;
        assert(!A2FO_ModuleInit(&api) && patches==0 && !g_active);
        *p^=1;
    }
    assert(signatures_match());
    patches=0; fail_patch=3; assert(A2FO_ModuleInit(&api));
    assert(patches==3 && !g_active);
    fail_patch=0; patches=0; assert(A2FO_ModuleInit(&api));
    assert(patches==7 && g_active && registrations==2);
    setup_craft();
    trampoline(at(g_armada,0xcfff0),reinterpret_cast<void*>(&fake_entity));
    trampoline(at(g_armada,0xd0060),reinterpret_cast<void*>(&fake_team));
    trampoline(at(g_armada,0xd4280),reinterpret_cast<void*>(&test_queue_gateway));

    const A2FO_OdfFieldView field{{"classLabel",10},{"shipyard",8}};
    A2FO_GameObjectClassLoadedEvent event{sizeof(event),yard_class,{},&field,1};
    class_loaded(&event,nullptr);
    void* menu_yards[3]{objects[0],objects[1],objects[2]};
    int menu_one[3]{reinterpret_cast<int>(menu_yards),0,1};
    int menu_two[3]{reinterpret_cast<int>(menu_yards),0,2};
    int menu_three[3]{reinterpret_cast<int>(menu_yards),0,3};
    std::array<std::uint8_t,0x128> popup{};
    put(popup.data(),0x124,std::uint32_t{2});
    assert(multi_yard_menu_permit(menu_two,popup.data())==1);
    assert(multi_yard_menu_permit(menu_three,popup.data())==1);
    assert(multi_yard_menu_permit(menu_one,popup.data())==1);
    put(objects[1],0x40,other_class);
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    put(objects[1],0x40,yard_class);
    put(objects[1],0xec,team+1);
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    put(objects[1],0xec,team);
    put(objects[1],0x27,std::uint8_t{1});
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    put(objects[1],0x27,std::uint8_t{0});
    put(popup.data(),0x124,std::uint32_t{3});
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    put(popup.data(),0x124,std::uint32_t{4});
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    assert(multi_yard_menu_permit(menu_one,popup.data())==1);
    g_active=false;
    put(popup.data(),0x124,std::uint32_t{2});
    assert(multi_yard_menu_permit(menu_one,popup.data())==1);
    assert(multi_yard_menu_permit(menu_two,popup.data())==0);
    g_active=true;

    // Execute the pinned stock gate and rejection/reset bytes in the synthetic
    // image, then the real bridge, with only later palette/exit targets captured.
    preserving_trampoline(at(g_fleet_ops,0x1e6ffe),reinterpret_cast<void*>(&test_capture_menu_open));
    preserving_trampoline(at(g_fleet_ops,0x1e72f5),reinterpret_cast<void*>(&test_capture_menu_exit));
    std::array<std::uint8_t,0x100> menu_frame{};
    void* menu_ebp=menu_frame.data()+0x50;
    put(menu_ebp,0x8,menu_two);
    put(menu_ebp,-0x4,popup.data());
    test_path=0;
    stub_call(reinterpret_cast<void(*)()>(at(g_fleet_ops,0x1e6fe5)),menu_ebp);
    assert(test_path==8 && get_signed<std::uint32_t>(popup.data(),0x124)==0);
    put(popup.data(),0x124,std::uint32_t{2});
    test_path=0; test_menu_entry_eax=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==7 && get_signed<std::uint32_t>(popup.data(),0x124)==2);
    assert(test_menu_entry_eax==reinterpret_cast<std::uint32_t>(menu_two));
    assert(test_after_ecx==0x5678 && test_after_eax==reinterpret_cast<std::uint32_t>(menu_two));
    put(menu_ebp,0x8,menu_three);
    test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==7 && get_signed<std::uint32_t>(popup.data(),0x124)==2);
    put(objects[1],0x40,other_class);
    test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==8 && get_signed<std::uint32_t>(popup.data(),0x124)==0);
    put(objects[1],0x40,yard_class);
    put(menu_ebp,0x8,menu_two);
    put(popup.data(),0x124,std::uint32_t{3});
    test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==8 && get_signed<std::uint32_t>(popup.data(),0x124)==0);
    put(popup.data(),0x124,std::uint32_t{4});
    test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==7 && get_signed<std::uint32_t>(popup.data(),0x124)==4);
    put(menu_ebp,0x8,menu_one);
    g_active=false; test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==7 && test_menu_entry_eax==reinterpret_cast<std::uint32_t>(menu_one));
    put(menu_ebp,0x8,menu_two);
    put(popup.data(),0x124,std::uint32_t{2});
    test_path=0;
    stub_call(multi_yard_menu_stub,menu_ebp);
    assert(test_path==8 && get_signed<std::uint32_t>(popup.data(),0x124)==0);
    g_active=true;
    void* selected[3]{objects[0],objects[1],objects[2]};
    int arr[3]{reinterpret_cast<int>(selected),0,3};
    assert(multi_yard_build_enable(arr)==1);
    std::array<unsigned,3> distribution{};
    for(int i=0;i<5;++i) {
        commands=0; multi_yard_build_route(selected,3,yard_class);
        assert(commands==1 && command_kind==0x19 && command_class==yard_class);
        const auto index=static_cast<std::size_t>(std::find(selected,selected+3,command_target)-selected);
        assert(index<3); ++distribution[index];
    }
    assert((distribution==std::array<unsigned,3>{{2,2,1}}));
    for(int i=0;i<3;++i) put(objects[i],0x200,std::uint32_t{0});
    g_last_built=handles[1]; commands=0;
    multi_yard_build_route(selected,3,yard_class);
    assert(commands==1 && command_target==objects[2]);
    put(objects[0],0x200,std::uint32_t{3});
    put(objects[1],0x200,std::uint32_t{1});
    put(objects[2],0x200,std::uint32_t{2});
    g_last_built=handles[2]; commands=0;
    multi_yard_build_route(selected,3,yard_class);
    assert(commands==1 && command_target==objects[1]);

    std::array<std::uint8_t,0x200> ci{}; put(ci.data(),0x1a0,std::uint32_t{0x13});
    assert(multi_yard_longest_queue(arr)==3);
    multi_yard_cancel_filter(ci.data(),arr);
    assert(arr[2]==1 && selected[0]==objects[0]);
    selected[0]=objects[0]; arr[2]=3;
    put(ci.data(),0x1a0,std::uint32_t{0x12});
    multi_yard_cancel_filter(ci.data(),arr);
    assert(arr[2]==3 && selected[0]==objects[0]);

    std::array<std::uint8_t,0x450> selection{};
    put(selection.data(),0xb8,std::uint32_t{2});
    put(selection.data(),0x3d0,handles[0]); put(selection.data(),0x3d4,handles[1]);
    assert(multi_yard_selection_permit(selection.data(),objects[2])==1);
    put(objects[1],0x40,other_class);
    assert(multi_yard_selection_permit(selection.data(),objects[2])==0);
    put(objects[1],0x40,yard_class);

    void* mixed[3]{objects[0],objects[1],objects[2]};
    int mixed_arr[3]{reinterpret_cast<int>(mixed),0,3};
    put(objects[1],0x40,other_class);
    assert(!multi_yard_build_enable(mixed_arr));
    commands=0; multi_yard_build_route(mixed,3,yard_class); assert(commands==3);
    put(objects[1],0x40,yard_class);
    put(objects[1],0xec,team+1); assert(!multi_yard_build_enable(mixed_arr));
    put(objects[1],0xec,team);
    put(objects[1],0x27,std::uint8_t{1}); assert(!multi_yard_build_enable(mixed_arr));
    put(objects[1],0x27,std::uint8_t{0});
    mixed[1]=objects[0]; assert(!multi_yard_build_enable(mixed_arr));
    mixed[1]=objects[1];
    assert(multi_yard_build_enable(mixed_arr));

    multi_yard_enable_continue=reinterpret_cast<std::uintptr_t>(&test_capture);
    multi_yard_route_continue=reinterpret_cast<std::uintptr_t>(&test_capture);
    multi_yard_route_loop=reinterpret_cast<std::uintptr_t>(&test_capture_route_loop);
    multi_yard_cancel_continue=reinterpret_cast<std::uintptr_t>(&test_capture);
    multi_yard_cancel_empty=reinterpret_cast<std::uintptr_t>(&test_capture_cancel_empty);
    multi_yard_select_one=reinterpret_cast<std::uintptr_t>(&test_capture_one);
    multi_yard_select_many=reinterpret_cast<std::uintptr_t>(&test_capture_many);
    multi_yard_select_check=reinterpret_cast<std::uintptr_t>(&test_capture_check);
    multi_yard_select_accept=reinterpret_cast<std::uintptr_t>(&test_capture_accept);
    std::array<std::uint8_t,0x100> frame{};
    void* ebp=frame.data()+0x50;
    put(ebp,0x8,arr);
    stub_call(multi_yard_enable_stub,ebp);
    assert(test_after_eax==1 && test_after_ecx==0x5678);
    put(ebp,0x8,mixed_arr);
    put(objects[1],0x40,other_class);
    stub_call(multi_yard_enable_stub,ebp);
    assert(test_after_eax==0 && test_after_ecx==0x5678);
    put(objects[1],0x40,yard_class);
    std::array<std::uint8_t,0x100> edi_object{};
    put(ebp,-0x20,selected);
    put(ebp,-0x18,int{3});
    put(edi_object.data(),0xc,yard_class);
    commands=0;
    stub_call(multi_yard_route_stub,ebp,0x2345,0x3456,
              reinterpret_cast<std::uint32_t>(edi_object.data()));
    assert(commands==1 && command_kind==0x19);
    put(ebp,-0x18,int{31}); test_path=0; commands=0;
    stub_call(multi_yard_route_stub,ebp,0x2345,0x3456,
              reinterpret_cast<std::uint32_t>(edi_object.data()),0x5678,false);
    assert(test_path==6 && commands==0 && test_after_eax==31 && test_after_esi==0);
    put(ebp,-0x18,int{3});
    put(ebp,-0x28,reinterpret_cast<int>(selected));
    put(ebp,-0x20,int{3});
    put(ci.data(),0x1a0,std::uint32_t{0x13}); test_path=0;
    stub_call(multi_yard_cancel_stub,ebp,0x2345,reinterpret_cast<std::uint32_t>(ci.data()));
    assert(test_path==0 && get_signed<int>(ebp,-0x20)==1);
    put(ebp,-0x20,int{0}); test_path=0;
    stub_call(multi_yard_cancel_stub,ebp,0x2345,reinterpret_cast<std::uint32_t>(ci.data()));
    assert(test_path==5);
    multi_yard_native_qsize=reinterpret_cast<std::uintptr_t>(&test_native_qsize);
    test_native_ecx=0;
    stub_call(multi_yard_qsize_stub,ebp,0x2345,0x3456,0x4567,0x7788);
    assert(test_after_eax==77 && test_native_ecx==0x7788);
    put(ebp,-0x20,int{3});
    multi_yard_native_qsize=reinterpret_cast<std::uintptr_t>(&fake_qsize);
    // Earlier stock-fallback routing deliberately queued on every object.
    // Give this longest-queue ABI case its own explicit workload fixture.
    put(objects[0],0x200,std::uint32_t{3});
    put(objects[1],0x200,std::uint32_t{1});
    put(objects[2],0x200,std::uint32_t{2});
    stub_call(multi_yard_qsize_stub,ebp,0x2345,0x3456,0x4567,0x7788);
    assert(test_after_eax==3 && test_after_ecx==0x7788);
    test_path=0; put(ebp,0x8,selection.data()); put(ebp,0xc,objects[2]);
    stub_call(multi_yard_select_first_stub,ebp); assert(test_path==2);
    test_path=0; stub_call(multi_yard_select_compat_stub,ebp); assert(test_path==4);
    put(objects[1],0x40,other_class); test_path=0;
    stub_call(multi_yard_select_first_stub,ebp); assert(test_path==2);
    put(ebp,0x1c,std::uint8_t{0}); test_path=0;
    stub_call(multi_yard_select_compat_stub,ebp); assert(test_path==4);
    put(ebp,0x1c,std::uint8_t{1}); test_path=0;
    stub_call(multi_yard_select_compat_stub,ebp); assert(test_path==3);
    put(objects[1],0x40,yard_class);
    put(selection.data(),0xb8,std::uint32_t{1});
    test_path=0; stub_call(multi_yard_select_first_stub,ebp); assert(test_path==2);
    put(objects[0],0x40,other_class);
    test_path=0; stub_call(multi_yard_select_first_stub,ebp); assert(test_path==1);
    A2FO_ModuleShutdown(); assert(!g_active);
    assert(DeleteFileA(ini.c_str()));
    assert(RemoveDirectoryA(config_root));
    VirtualFree(g_fleet_ops,0,MEM_RELEASE); VirtualFree(g_armada,0,MEM_RELEASE);
    std::puts("multi-yard native checks passed");
}
