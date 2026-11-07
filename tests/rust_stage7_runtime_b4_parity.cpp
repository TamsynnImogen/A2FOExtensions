// Headless native point-defense hook fixture. MODULE_SOURCE selects frozen/current.
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include MODULE_SOURCE
extern "C" volatile std::uintptr_t mock_native_result;
namespace {
std::vector<std::string> trace;
std::uint8_t* armada_image = nullptr;
std::uint8_t* fleet_image = nullptr;
unsigned install_count = 0;
unsigned fail_install = 0;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void A2FO_CALL logger(const char*, const char* message) { trace.emplace_back(std::string("log:")+message); }
void* A2FO_CALL armada_module() { trace.emplace_back("armada"); return armada_image; }
void* A2FO_CALL fleetops_module() { trace.emplace_back("fleet"); return fleet_image; }
bool A2FO_CALL install(void*, void*, std::size_t, const std::uint8_t*, A2FO_InlineHook* hook) {
    ++install_count; trace.emplace_back("hook:"+std::to_string(install_count));
    if (install_count==fail_install) return false;
    hook->gateway=reinterpret_cast<void*>(0x1234); return true;
}
bool A2FO_CALL patch(void*, void*, const std::uint8_t*, std::size_t) {
    trace.emplace_back("patch"); return true;
}
template<std::size_t N> void seed(std::uint8_t* image, std::uintptr_t rva, const std::uint8_t (&bytes)[N]) {
    std::memcpy(image+rva,bytes,N);
}
void run(const char* mode) {
    armada_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    fleet_image=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    check(armada_image && fleet_image,"images");
    seed(armada_image,kPointDefenseBuildClassRva,kExpectedPointDefenseBuildClass);
    seed(armada_image,kPointDefenseSimulateRva,kExpectedPointDefenseSimulate);
    seed(armada_image,kPointDefenseTimerGateRva,kExpectedPointDefenseTimerGate);
    seed(armada_image,kWeaponResetShotTimerRva,kExpectedWeaponResetShotTimer);
    seed(armada_image,kWeaponLoadRva,kExpectedWeaponLoad);
    seed(armada_image,kWeaponSaveRva,kExpectedWeaponSave);
    seed(armada_image,kWeaponDestructorRva,kExpectedWeaponDestructor);
    seed(armada_image,kParameterDbGetFloatRva,kExpectedParameterDbGetFloat);
    seed(armada_image,kParameterDbGetStringRva,kExpectedParameterDbGetString);
    seed(armada_image,kFileWriterOutFloatRva,kExpectedFileWriterOutFloat);
    seed(armada_image,kFileWriterOutIntegerRva,kExpectedFileWriterOutInteger);
    seed(armada_image,kFileReaderInFloatRva,kExpectedFileReaderInFloat);
    seed(armada_image,kFileReaderInIntegerRva,kExpectedFileReaderInInteger);
    seed(fleet_image,kOrdnanceDefenseBuildClassRva,kExpectedOrdnanceDefenseBuildClass);
    seed(fleet_image,kOrdnanceDefenseSimulateRva,kExpectedOrdnanceDefenseSimulate);
    seed(fleet_image,kOrdnanceDefenseDelaySiteRva,kExpectedOrdnanceDefenseDelaySite);
    if (std::strcmp(mode,"signature")==0) armada_image[kWeaponSaveRva]=0;
    if (std::strcmp(mode,"partial")==0) fail_install=3;
    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);api.api_version=A2FO_MODULE_API_VERSION;
    api.log=&logger;api.armada_module=&armada_module;api.fleetops_module=&fleetops_module;
    api.install_inline_hook=&install;api.patch_jump=&patch;
    check(A2FO_ModuleInit(&api),"init");
    const bool success=std::strcmp(mode,"success")==0;
    check(g_runtime_ready==success,"runtime gate");
    check(install_count==(success?8u:std::strcmp(mode,"partial")==0?3u:0u),
          "checked install order");
    trace.emplace_back(std::string("ready:")+(g_runtime_ready?"1":"0"));
    std::array<std::uint8_t,0x300> weapon{};
    float timer=1.25f;
    std::memcpy(weapon.data()+kShotTimerOnWeaponOffset,&timer,sizeof(timer));
    const auto first_gate=a2fo_point_defense_advance_pdl_timer(weapon.data(),0.5f);
    check(first_gate==(success?0u:1u),"timer gate");
    trace.emplace_back("gate:"+std::to_string(first_gate));
    std::memcpy(&timer,weapon.data()+kShotTimerOnWeaponOffset,sizeof(timer));
    trace.emplace_back("timer:"+std::to_string(timer));
    std::uint16_t original_cw=0;
    __asm__ volatile("fnstcw %0" : "=m"(original_cw));
    for (std::uint16_t precision: {std::uint16_t(0x0000),
                                  std::uint16_t(0x0200),
                                  std::uint16_t(0x0300)}) {
        const std::uint16_t selected=static_cast<std::uint16_t>(
            (original_cw & ~std::uint16_t(0x0300)) | precision);
        __asm__ volatile("fldcw %0" : : "m"(selected));
        float sample=0.99999994f;
        std::memcpy(weapon.data()+kShotTimerOnWeaponOffset,&sample,sizeof(sample));
        const auto gate=a2fo_point_defense_advance_pdl_timer(weapon.data(),0.33333334f);
        std::memcpy(&sample,weapon.data()+kShotTimerOnWeaponOffset,sizeof(sample));
        std::uint32_t bits=0;std::memcpy(&bits,&sample,sizeof(bits));
        trace.emplace_back("precision:"+std::to_string(precision)+":"+
            std::to_string(gate)+":"+std::to_string(bits));
    }
    __asm__ volatile("fldcw %0" : : "m"(original_cw));
    if (g_runtime_ready) {
        std::array<std::uint8_t,0x300> weapon_class{};
        void* class_pointer=weapon_class.data();
        std::memcpy(weapon.data()+kWeaponClassOnWeaponOffset,&class_pointer,sizeof(class_pointer));
        mock_native_result=reinterpret_cast<std::uintptr_t>(class_pointer);
        check(point_defense_build_class_hook(class_pointer,nullptr,
              reinterpret_cast<void*>(1))==reinterpret_cast<std::uintptr_t>(class_pointer),
              "class gateway");
        check(g_class_policies.count(class_pointer)==1,"class policy registered");
        mock_native_result=0x55;
        const float first=select_delay(weapon.data());
        const float second=select_delay(weapon.data());
        check(first==0.25f && second==0.5f,"firing cycle order");
        trace.emplace_back("delays:"+std::to_string(first)+","+std::to_string(second));
        trace.emplace_back("simulate:"+std::to_string(point_defense_simulate_hook(weapon.data(),nullptr,0.1f)));
        trace.emplace_back("reset:"+std::to_string(weapon_reset_shot_timer_hook(weapon.data(),nullptr)));
        trace.emplace_back("destroy:"+std::to_string(weapon_destructor_hook(weapon.data(),nullptr)));
        g_class_policies.erase(class_pointer);
    }
    for (const auto& item:trace) std::puts(item.c_str());
    VirtualFree(armada_image,0,MEM_RELEASE);VirtualFree(fleet_image,0,MEM_RELEASE);
}
}
extern "C" {
volatile std::uintptr_t mock_native_result=0x55;
__attribute__((noinline)) std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_0(void*,void*) {return mock_native_result;}
__attribute__((noinline)) std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_1(void*,void*,std::uintptr_t) {return mock_native_result;}
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_3(void*,void*,std::uintptr_t key,
                                                            std::uintptr_t output,std::uintptr_t) {
    const char* name=reinterpret_cast<const char*>(key);
    float value=0.0f;
    if (std::strcmp(name,"shotDelay0")==0) value=0.25f;
    else if (std::strcmp(name,"shotDelay1")==0) value=0.5f;
    else if (std::strcmp(name,"shotCycleResetTime")==0) value=2.0f;
    else return 0;
    std::memcpy(reinterpret_cast<void*>(output),&value,sizeof(value));
    return 1;
}
std::uintptr_t __cdecl a2fo_point_defense_call_thiscall_4(void*,void*,std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t) {return 0;}
void a2fo_point_defense_odw_delay_bridge() {}
void a2fo_point_defense_pdl_timer_gate_bridge() {}
}
int main(int argc,char** argv) {try {run(argc>1?argv[1]:"success");return 0;} catch(const std::exception& e){std::fprintf(stderr,"point: %s\n",e.what());return 1;}}
