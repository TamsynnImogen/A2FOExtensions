#pragma once
// Replay only FO's verified six-byte ChatDisplay transfer/trampoline. Both
// images are DONT_RESOLVE_DLL_REFERENCES mappings. No engine/FO entrypoint or
// installer runs. The original engine continuation is replaced with a stub.
#include <windows.h>
#include <array>
#include <cstring>
#include <stdexcept>
namespace refit_fo_chat_fixture {
inline void put(void* target,const void* bytes,std::size_t size){
    DWORD old{},unused{};
    if(!VirtualProtect(target,size,PAGE_EXECUTE_READWRITE,&old))throw std::runtime_error("FO replay write protection");
    std::memcpy(target,bytes,size);FlushInstructionCache(GetCurrentProcess(),target,size);
    VirtualProtect(target,size,old,&unused);
}
inline void transfer(void* target,const void* destination){
    std::array<BYTE,6> bytes{{0x68,0,0,0,0,0xc3}};
    std::memcpy(bytes.data()+1,&destination,4);put(target,bytes.data(),bytes.size());
}
inline void* install(HMODULE engine,HMODULE fo,const void* original_stub){
    auto* entry=reinterpret_cast<BYTE*>(engine)+0x11d3c0;
    const std::array<BYTE,6> prologue{{0x55,0x8b,0xec,0x83,0xec,0x18}};
    if(std::memcmp(entry,prologue.data(),prologue.size()))throw std::runtime_error("FO replay original chat prologue");
    auto* gateway=static_cast<BYTE*>(VirtualAlloc(nullptr,12,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!gateway)throw std::runtime_error("FO replay executable gateway allocation");
    put(gateway,prologue.data(),6);transfer(gateway+6,entry+6);
    put(reinterpret_cast<BYTE*>(fo)+0x2469d4,&gateway,4);
    const unsigned zero=0;put(reinterpret_cast<BYTE*>(fo)+0x20fff4,&zero,4);
    // Undo the displaced native frame before entering the fixture's C++ stub.
    std::array<BYTE,8> tail{{0x8b,0xe5,0x5d,0xe9,0,0,0,0}};
    const auto relative=static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(original_stub)-
        reinterpret_cast<std::uintptr_t>(entry+6)-8);
    std::memcpy(tail.data()+4,&relative,4);put(entry+6,tail.data(),tail.size());
    transfer(entry,reinterpret_cast<BYTE*>(fo)+0x1cf0b4);
    return gateway;
}
inline void counter(HMODULE fo,unsigned value){put(reinterpret_cast<BYTE*>(fo)+0x20fff4,&value,4);}
}
