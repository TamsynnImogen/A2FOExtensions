#include "native11_borg_material_source.hpp"
#include "native11_borg_material_profile.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_borg_material::source {
bool matches(HMODULE image) noexcept {
    if(!image)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    if(base>UINT32_MAX)return false;
    const auto delta=static_cast<std::uint32_t>(base)-profile::preferred_base;
    for(const auto& region:profile::regions){
        std::array<std::uint8_t,512> bytes{};MEMORY_BASIC_INFORMATION page{};
        if(!region.size||region.size>bytes.size()||region.rva>UINT32_MAX-base)return false;
        const auto address=base+region.rva;
        if(region.size-1>UINT32_MAX-address||
           VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))!=sizeof(page)||
           page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=image||
           (page.Protect!=PAGE_EXECUTE_READ&&page.Protect!=PAGE_EXECUTE_READWRITE&&page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
        const auto page_base=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
        if(address<page_base||address-page_base>page.RegionSize||region.size>page.RegionSize-(address-page_base))return false;
        SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),bytes.data(),region.size,&copied)||copied!=region.size)return false;
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    return true;
}
}
