// Full native icon lifecycle against synthetic images and thiscall primitives.
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <new>
namespace std {void* icon_calloc(size_t,size_t) noexcept;void icon_free(void*) noexcept;}
#define calloc icon_calloc
#define free icon_free
#include "../modules/A2FOCraftIdentity/extended_weapon_icons.cpp"
#undef calloc
#undef free
namespace n=a2fo::craft_identity;
namespace {
std::uint8_t* armada{};std::uint8_t* fleet{};
std::array<std::uint8_t,0x400> ship{};
std::array<std::uint8_t,0x80> systems{};
std::array<std::uint8_t,8> craft{};
std::array<void*,8> vtable{};
unsigned allocations{},frees{},installs{};
int fail_alloc=-1,fail_construct=-1,fail_hook=-1;
bool rectangle=true,fail_retention=false,fail_next_new=false;
void destructor_sentinel(){}void render_sentinel(){}void simulate_sentinel(){}
template<class T>void put(void* p,std::size_t at,T v){std::memcpy(static_cast<std::uint8_t*>(p)+at,&v,sizeof v);}
template<class T>T slot_value(void* p,std::size_t at){T v{};std::memcpy(&v,static_cast<std::uint8_t*>(p)+at,sizeof v);return v;}
void log(const char*,const char* text){std::printf("log:%s\n",text);}
bool install(void* target,void*,std::size_t,const std::uint8_t*,A2FO_InlineHook* h){unsigned i=installs++;std::printf("hook:%u:%u\n",i,unsigned(static_cast<std::uint8_t*>(target)-fleet));if(int(i)==fail_hook)return false;h->gateway=fleet+0x1000+i;return true;}
void signatures(){
 std::memcpy(armada+0x1358f0,n::kExpectedParameterDbGetRectangle.data(),n::kExpectedParameterDbGetRectangle.size());
 std::memcpy(fleet+0x1ed100,n::kExpectedShipSystemIconConstructor.data(),n::kExpectedShipSystemIconConstructor.size());
 std::memcpy(fleet+0x1edfa0,n::kExpectedCleanup.data(),n::kExpectedCleanup.size());
 std::memcpy(fleet+0x1ee05c,n::kExpectedPostLoad.data(),n::kExpectedPostLoad.size());
 for(auto r:{0x1ee868u,0x1ee9bcu,0x1eeb14u,0x1eec14u})std::memcpy(fleet+r,n::kExpectedDisplayOrSimulate.data(),n::kExpectedDisplayOrSimulate.size());
 std::memcpy(fleet+0x1eed14,n::kExpectedAlwaysGameSimulate.data(),n::kExpectedAlwaysGameSimulate.size());
}
void reset(){
 n::destroy_sidecars(ship.data());allocations=frees=installs=0;fail_alloc=fail_construct=fail_hook=-1;rectangle=true;fail_retention=fail_next_new=false;
 n::g_hooks_armed=false;n::g_creation_report_count=0;n::g_cleanup_hook={};n::g_post_load_hook={};n::g_display_single_object_hook={};n::g_display_single_builder_hook={};n::g_simulate_single_object_hook={};n::g_simulate_single_builder_hook={};n::g_always_game_simulate_hook={};ship.fill(0);systems.fill(0);
 put(armada,0x36502c,systems.data());put(ship.data(),0x308,systems.data());put(ship.data(),0x1e8,craft.data());put(systems.data(),0x2c,craft.data());signatures();
}
}
void* operator new(std::size_t size){if(fail_next_new){fail_next_new=false;throw std::bad_alloc();}if(auto* p=std::malloc(size))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);}void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace std {
void* icon_calloc(size_t count,size_t size) noexcept {unsigned i=allocations++;std::printf("alloc:%u\n",i);return int(i)==fail_alloc?nullptr:std::calloc(count,size);}
void icon_free(void* p) noexcept {++frees;std::printf("free:%u\n",slot_value<std::uint32_t>(p,0x30));std::free(p);}
}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_0(void* fn,void* self){
 if(fn==reinterpret_cast<void*>(&destructor_sentinel)){std::printf("destroy:%u\n",slot_value<std::uint32_t>(self,0x30));return 0;}
 if(fn==reinterpret_cast<void*>(&render_sentinel)||fn==reinterpret_cast<void*>(&simulate_sentinel)){std::printf("virtual:%s:%u:%u\n",fn==reinterpret_cast<void*>(&render_sentinel)?"render":"simulate",slot_value<std::uint32_t>(self,0x30),slot_value<void*>(self,0x28)==craft.data());return 0;}
 std::printf("gateway0:%u\n",unsigned(static_cast<std::uint8_t*>(fn)-fleet));return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_1(void* fn,void* self,std::uintptr_t value){if(fn==reinterpret_cast<void*>(&destructor_sentinel)){std::printf("destroy:%u:%u\n",slot_value<std::uint32_t>(self,0x30),unsigned(value));return 0;}std::printf("gateway1:%u:%u\n",unsigned(static_cast<std::uint8_t*>(fn)-fleet),unsigned(value));return 0;}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_3(void*,void*,std::uintptr_t,std::uintptr_t out,std::uintptr_t){std::printf("rectangle:%u\n",unsigned(rectangle));*reinterpret_cast<n::RawRectangle*>(out)={1,2,3,4};return rectangle;}
extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_4(void*,void* self,std::uintptr_t,std::uintptr_t,std::uintptr_t index,std::uintptr_t){
 std::printf("construct:%u\n",unsigned(index));put(self,0,vtable.data());put(self,0x30,int(index-32)==fail_construct?128u:std::uint32_t(index));put(self,0x48,0.5f);put(self,0x4c,std::uint8_t(0));if(index==127&&fail_retention)fail_next_new=true;return 0;
}
int main(){
 armada=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x400000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));fleet=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,0x300000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));if(!armada||!fleet)return 2;
 vtable[2]=reinterpret_cast<void*>(&destructor_sentinel);vtable[3]=reinterpret_cast<void*>(&simulate_sentinel);vtable[4]=reinterpret_cast<void*>(&render_sentinel);
 A2FO_ModuleApi api{};api.log=log;api.install_inline_hook=install;
 for(int scenario=0;scenario<19;++scenario){reset();std::printf("case:%d\n",scenario);
  if(scenario<7)fail_hook=scenario;if(scenario==7)armada[0x1358f0]^=1;
  if(scenario==8)std::printf("init:%u\n",unsigned(n::install_extended_weapon_icons(nullptr,armada,fleet)));else std::printf("init:%u\n",unsigned(n::install_extended_weapon_icons(&api,armada,fleet)));
  if(scenario<=8)continue;
  if(scenario==9)rectangle=false;if(scenario==10)fail_alloc=0;if(scenario==11)fail_alloc=35;if(scenario==12)fail_construct=35;if(scenario==13)fail_retention=true;
  n::post_load_hook(ship.data(),nullptr);
  if(scenario==14)put(ship.data(),0x1e8,static_cast<void*>(nullptr));if(scenario==15)put(systems.data(),0x2c,static_cast<void*>(nullptr));
  n::display_single_object_hook(ship.data(),nullptr);n::display_single_builder_hook(ship.data(),nullptr);n::simulate_single_object_hook(ship.data(),nullptr,0x3f400000u);n::simulate_single_builder_hook(ship.data(),nullptr,0x3f400000u);
  n::always_game_simulate_hook(ship.data(),nullptr,0x40200000u);
  if(scenario==16)n::create_sidecars(ship.data());
  const auto found=n::g_sidecars.find(ship.data());
  if(found!=n::g_sidecars.end()){
   unsigned short cw;__asm__ volatile("fnstcw %0":"=m"(cw));
   for(unsigned pc:{0u,0x200u,0x300u}){
    unsigned short next=(cw&~0x300u)|pc;__asm__ volatile("fldcw %0"::"m"(next));
    for(std::uint32_t elapsed:{0u,0x80000000u,0x3f800000u,0xbf800000u,0x7fc12345u,0x7f812345u}){
     for(auto* icon:found->second.icons){put(icon,0x48,0.75f);put(icon,0x4c,std::uint8_t(3));}
     n::update_sidecar_flash(ship.data(),elapsed);auto* icon=found->second.icons[0];std::printf("flash:%u:%08x:%08x:%u\n",pc,elapsed,slot_value<std::uint32_t>(icon,0x48),unsigned(slot_value<std::uint8_t>(icon,0x4c)));
    }
   }
   __asm__ volatile("fldcw %0"::"m"(cw));
  }
  n::cleanup_hook(ship.data(),nullptr);std::printf("counts:%u:%u:%u\n",allocations,frees,unsigned(n::g_sidecars.size()));
 }
 reset();VirtualFree(armada,0,MEM_RELEASE);VirtualFree(fleet,0,MEM_RELEASE);
}
