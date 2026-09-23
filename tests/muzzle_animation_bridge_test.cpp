#include "../modules/A2FOMuzzleFlashes/module.cpp"
#include <stdexcept>
#include <iostream>
#define CHECK(x) do {if(!(x)) throw std::runtime_error(#x);}while(0)
namespace {
unsigned notifications=0;void* received=nullptr;
std::uintptr_t built=0;
void __cdecl fired_fake(void* weapon){++notifications;received=weapon;}
std::uintptr_t __fastcall build_fake(void*,void*,std::uintptr_t,std::uintptr_t,std::uintptr_t){return built;}
}
int main(){
    // No muzzle flash policy is required for the animation notification.
    std::array<unsigned char,64> weapon{};
    g_animation_fired=fired_fake;g_build_hook.gateway=reinterpret_cast<void*>(build_fake);
    built=123;
    CHECK(ordnance_build_hook(nullptr,nullptr,0,0,reinterpret_cast<std::uintptr_t>(weapon.data()))==123);
    CHECK(notifications==1 && received==weapon.data());
    built=0;
    CHECK(ordnance_build_hook(nullptr,nullptr,0,0,reinterpret_cast<std::uintptr_t>(weapon.data()))==0);
    CHECK(notifications==1);
    std::cout<<"Muzzle/animation bridge: successful build notified without flash policy; failed build silent; native result preserved\n";
}
