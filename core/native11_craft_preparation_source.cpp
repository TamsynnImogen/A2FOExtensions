#include "native11_craft_preparation_source.hpp"
#include "native11_craft_preparation_profile.hpp"
#include "native11_craft_preparation_fleet_profile.hpp"
#include "native11_capture.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_craft_preparation::source {
namespace {
bool read(std::uintptr_t address,void* out,std::size_t size) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),out,size,&copied)&&copied==size;
}
bool executable(std::uintptr_t address,std::size_t size,HMODULE allocation=nullptr) noexcept {
    MEMORY_BASIC_INFORMATION page{};
    if(!address||!size||size-1>UINTPTR_MAX-address||VirtualQuery(reinterpret_cast<const void*>(address),&page,sizeof(page))!=sizeof(page)||
       page.State!=MEM_COMMIT||(allocation&&(page.Type!=MEM_IMAGE||page.AllocationBase!=allocation))||
       (page.Protect!=PAGE_EXECUTE_READ&&page.Protect!=PAGE_EXECUTE_READWRITE&&page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    return address>=base&&address-base<=page.RegionSize&&size<=page.RegionSize-(address-base);
}
bool closure(HMODULE fleet) noexcept {
    if(!fleet)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(fleet);
    const auto delta=static_cast<std::uint32_t>(base)-fleet_profile::preferred_base;
    for(const auto& region:fleet_profile::regions){
        std::array<std::uint8_t,128> bytes{};
        if(region.size>bytes.size()||region.rva>UINTPTR_MAX-base||!executable(base+region.rva,region.size,fleet)||
           !read(base+region.rva,bytes.data(),region.size))return false;
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }return true;
}
bool gateway(HMODULE image,HMODULE fleet,const std::uint8_t* entry,std::uint32_t& saved) noexcept {
    if(!fleet||!entry||!closure(fleet))return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto fb=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fleet));
    std::uint32_t callback=0;
    if(entry[0]!=0x68||entry[5]!=0xc3)return false;
    std::memcpy(&callback,entry+1,4);
    if(callback!=fb+fleet_profile::callback_rva||
       !read(std::uintptr_t(fb)+fleet_profile::saved_original_rva,&saved,4)||!saved||!executable(saved,14))return false;
    std::array<std::uint8_t,14> stub{};std::array<std::uint8_t,8> expected{};
    std::memcpy(expected.data(),profile::craft_prepare_bytes.data(),expected.size());
    const auto delta=base-profile::preferred_base;
    for(auto offset:profile::craft_prepare_fixes)if(offset+4u<=expected.size()){
        std::uint32_t value=0;std::memcpy(&value,expected.data()+offset,4);value+=delta;
        std::memcpy(expected.data()+offset,&value,4);
    }
    if(!read(saved,stub.data(),stub.size())||std::memcmp(stub.data(),expected.data(),expected.size())||
       stub[8]!=0x68||stub[13]!=0xc3||entry[6]!=expected[6]||entry[7]!=expected[7])return false;
    std::uint32_t continuation=0;std::memcpy(&continuation,stub.data()+9,4);
    return continuation==base+0xcb398;
}
}
bool current(HMODULE image,HMODULE fleet,Proof& output) noexcept {
    if(!image)return false;
    const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image));
    const auto delta=base-profile::preferred_base;
    Proof pending;pending.image=base;pending.fleet=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fleet));
    std::array<std::uint8_t,8> saved_entry{};
    for(const auto& region:profile::logical_regions){
        std::array<std::uint8_t,512> bytes{};
        if(region.size>bytes.size()||region.rva>UINT32_MAX-base||
           !read(base+region.rva,bytes.data(),region.size))return false;
        if(region.rva==0xcb390){
            if(!executable(base+region.rva,region.size,image))return false;
            std::memcpy(saved_entry.data(),bytes.data(),saved_entry.size());
            if(bytes[0]==0x68){
                if(!gateway(image,fleet,bytes.data(),pending.gateway))return false;
                pending.fleet_base_wrapper=true;
                // The gateway validates the whole relocated eight-byte prefix;
                // only the six patched entry bytes are replaced locally.
                std::array<std::uint8_t,8> original{};
                if(!read(pending.gateway,original.data(),original.size()))return false;
                std::memcpy(bytes.data(),original.data(),6);
            }
        }
        if(region.rva==0xaf100&&!executable(base+region.rva,region.size,image))return false;
        for(std::size_t k=0;k<region.relocation_count;++k){const auto offset=region.relocations[k];
            if(offset+4u>region.size)return false;
            std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
            std::memcpy(bytes.data()+offset,&value,4);
        }
        if(std::memcmp(bytes.data(),region.bytes,region.size))return false;
    }
    for(const auto& slot:profile::logical_slots){std::uint32_t actual=0;
        if(slot[0]>UINT32_MAX-base||slot[1]>UINT32_MAX-base||
           !read(base+slot[0],&actual,4)||actual!=base+slot[1])return false;
    }
    std::array<std::uint8_t,8> entry{};
    if(!read(base+0xcb390,entry.data(),entry.size())||entry!=saved_entry)return false;
    if(pending.fleet_base_wrapper){std::uint32_t after=0;
        if(!gateway(image,fleet,entry.data(),after)||after!=pending.gateway)return false;
    }
    output=pending;return true;
}
bool qualify(HMODULE image,HMODULE fleet) noexcept {
    if(!native11_capture::exact_engine_image(image)||
       (fleet&&!native11_capture::exact_image_file(fleet,fleet_profile::sha256.data())))return false;
    if(fleet){HMODULE pinned=nullptr;
        if(!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<const char*>(fleet),&pinned)||pinned!=fleet)return false;
    }
    Proof proof;return current(image,fleet,proof);
}
}
