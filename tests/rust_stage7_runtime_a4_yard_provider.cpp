#include <cstdint>
#include <cstring>
namespace {int mode=0;}
extern "C" __declspec(dllexport) void __cdecl FixtureQueueMode(int next) {mode=next;}
extern "C" __declspec(dllexport) std::uint32_t __cdecl A2FO_ProducerLogicalQueueSize(void* object) {
    if(mode)return UINT32_MAX;
    std::uint32_t handle=0;
    std::memcpy(&handle,static_cast<std::uint8_t*>(object)+0x28,sizeof(handle));
    return handle==101?4:handle==102?1:3;
}
