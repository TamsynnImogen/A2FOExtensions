#include "native11_template_fleet.hpp"
#include "native11_template_fleet_profile.hpp"
#include "native11_capture.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_template_fleet {
namespace {
bool read(std::uintptr_t address,void* output,std::size_t size) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),output,size,&copied)&&copied==size;
}
bool executable(std::uintptr_t address,std::size_t size,HMODULE allocation=nullptr) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    MEMORY_BASIC_INFORMATION p{};
    if(VirtualQuery(reinterpret_cast<void*>(address),&p,sizeof(p))!=sizeof(p)||p.State!=MEM_COMMIT||
       (allocation&&(p.Type!=MEM_IMAGE||p.AllocationBase!=allocation))||
       (p.Protect!=PAGE_EXECUTE_READ&&p.Protect!=PAGE_EXECUTE_READWRITE&&p.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(p.BaseAddress);
    return p.RegionSize<=UINTPTR_MAX-base&&address-base<=p.RegionSize&&size<=p.RegionSize-(address-base);
}
bool wrapper(HMODULE fleet,const profile::Region& region) noexcept {
    const auto base=reinterpret_cast<std::uintptr_t>(fleet);
    std::array<std::uint8_t,128> bytes{};
    if(!fleet||region.rva>UINTPTR_MAX-base||region.size>bytes.size()||
       !executable(base+region.rva,region.size,fleet)||!read(base+region.rva,bytes.data(),region.size))return false;
    const auto delta=static_cast<std::uint32_t>(base)-profile::fleet_preferred_base;
    for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];if(offset+4u>region.size)return false;
        std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
        std::memcpy(bytes.data()+offset,&value,4);
    }
    return !std::memcmp(bytes.data(),region.bytes,region.size);
}
}
bool owned(HMODULE image,HMODULE fleet) noexcept {
    if(!image||!fleet||!wrapper(fleet,profile::outer_ctor)||!wrapper(fleet,profile::inner_ctor))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(image),fb=reinterpret_cast<std::uintptr_t>(fleet);
    if(profile::root_patch_rva>UINTPTR_MAX-base||profile::root_ctor_rva>UINTPTR_MAX-base||fb>UINTPTR_MAX-0x246a1b)return false;
    std::array<std::uint8_t,20> block{};std::uint32_t relative=0;
    const auto site=base+profile::root_patch_rva;
    if(!read(site,block.data(),block.size())||
       std::memcmp(block.data(),profile::root_patch_prefix.data(),profile::root_patch_prefix.size())||block[15]!=0xe8)return false;
    std::memcpy(&relative,block.data()+16,4);
    if(static_cast<std::uint32_t>(site+20+relative)!=fb+profile::outer_ctor.rva)return false;
    std::array<std::uint8_t,6> entry{};std::uint32_t target=0;
    if(!read(base+profile::root_ctor_rva,entry.data(),entry.size())||entry[0]!=0x68||entry[5]!=0xc3)return false;
    std::memcpy(&target,entry.data()+1,4);
    if(target!=fb+profile::inner_ctor.rva)return false;
    // The outer wrapper resolves the public native constructor through this
    // exact table cell; its inner wrapper then calls the saved native stub.
    std::uint32_t table=0;
    if(!read(fb+0x212870,&table,4)||table!=fb+0x210b38||!read(table,&target,4)||
       target!=base+profile::root_ctor_rva)return false;
    std::uint32_t original=0;
    if(!read(fb+0x246a18,&original,4)||!executable(original,16))return false;
    std::array<std::uint8_t,16> stub{};
    if(!read(original,stub.data(),stub.size())||stub[10]!=0x68||stub[15]!=0xc3)return false;
    auto prefix=profile::root_ctor_prolog;
    std::memcpy(&target,prefix.data()+6,4);
    target+=static_cast<std::uint32_t>(base)-profile::root_preferred_base;
    std::memcpy(prefix.data()+6,&target,4);
    if(std::memcmp(stub.data(),prefix.data(),prefix.size()))return false;
    std::memcpy(&target,stub.data()+11,4);
    return target==base+profile::root_ctor_rva+10;
}
bool qualify(HMODULE image,HMODULE fleet) noexcept {
    if(!native11_capture::exact_engine_image(image)||
       !native11_capture::exact_image_file(fleet,profile::fleet_sha256.data())||!owned(image,fleet))return false;
    HMODULE retained=nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(fleet),&retained)&&retained==fleet;
}
bool normalize(HMODULE image,HMODULE fleet,std::uint32_t rva,std::uint8_t* bytes,std::size_t size) noexcept {
    if(!bytes||!size||size-1>UINT32_MAX-rva)return false;
    constexpr auto site=profile::root_patch_rva;constexpr auto length=profile::root_original_bytes.size();
    if(std::uint64_t(rva)+size<=site||rva>=site+length)return true;
    if(rva>site||std::uint64_t(rva)+size<site+length||!owned(image,fleet))return false;
    std::memcpy(bytes+(site-rva),profile::root_original_bytes.data(),length);return true;
}
}
