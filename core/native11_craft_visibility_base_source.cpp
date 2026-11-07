#include "native11_craft_visibility_base_source.hpp"
#include "native11_craft_visibility_base_profile.hpp"
#include "native11_capture.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_craft_visibility_base::source {
namespace {
bool read(std::uintptr_t address,void* out,std::size_t size) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),out,size,&copied)&&copied==size;
}
bool code(HMODULE image,std::uintptr_t address,std::size_t size) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if(!address||!size||size-1>UINTPTR_MAX-address||VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))!=sizeof(page)||
       page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=image||
       (page.Protect!=PAGE_EXECUTE_READ&&page.Protect!=PAGE_EXECUTE_READWRITE&&page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    return address>=base&&address-base<=page.RegionSize&&size<=page.RegionSize-(address-base);
}
}
bool current(HMODULE image) noexcept {
    if(!image)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    const auto delta=static_cast<std::uint32_t>(base)-profile::preferred_base;
    for(const auto& region:profile::regions){
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||region.rva>UINTPTR_MAX-base||
           (region.code&&!code(image,base+region.rva,region.size))||!read(base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:profile::slots){std::uint32_t value=0;
        if(slot[0]>UINTPTR_MAX-base||slot[1]>UINTPTR_MAX-base||
           !read(base+slot[0],&value,4)||value!=base+slot[1])return false;
    }return true;
}
bool qualify(HMODULE image) noexcept {return native11_capture::exact_engine_image(image)&&current(image);}
}
