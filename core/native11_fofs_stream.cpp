#include "native11_fofs_stream.hpp"
#include "native11_fofs_stream_profile.hpp"
#include "native11_capture.hpp"
#include <array>
#include <cstring>
namespace a2fo::native11_fofs_stream {
namespace {
bool read(std::uintptr_t address,void* out,std::size_t size) noexcept {
    if(!address||!size||size-1>UINTPTR_MAX-address)return false;
    SIZE_T copied=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),out,size,&copied)&&copied==size;
}
bool callback_matches(HMODULE module) noexcept {
    if(!module)return false;
    const auto base=reinterpret_cast<std::uintptr_t>(module);
    const auto& region=profile::fopen_callback;
    std::array<std::uint8_t,512> bytes{};
    if(region.rva>UINTPTR_MAX-base||region.size>bytes.size()||
       !read(base+region.rva,bytes.data(),region.size))return false;
    MEMORY_BASIC_INFORMATION page{};
    if(VirtualQuery(reinterpret_cast<void*>(base+region.rva),&page,sizeof(page))!=sizeof(page)||
       page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=module||
       (page.Protect!=PAGE_EXECUTE_READ&&page.Protect!=PAGE_EXECUTE_READWRITE&&
        page.Protect!=PAGE_EXECUTE_WRITECOPY))return false;
    const auto page_base=reinterpret_cast<std::uintptr_t>(page.BaseAddress);
    if(page.RegionSize>UINTPTR_MAX-page_base||region.size>UINTPTR_MAX-(base+region.rva)||
       base+region.rva+region.size>page_base+page.RegionSize)return false;
    const auto delta=static_cast<std::uint32_t>(base)-profile::fleet_preferred_base;
    for(std::size_t i=0;i<region.relocation_count;++i){
        const auto offset=region.relocations[i];if(offset+4u>region.size)return false;
        std::uint32_t value=0;std::memcpy(&value,bytes.data()+offset,4);value-=delta;
        std::memcpy(bytes.data()+offset,&value,4);
    }
    return !std::memcmp(bytes.data(),region.bytes,region.size);
}
}
bool owned(HMODULE image,HMODULE fleet_ops) noexcept {
    if(!image||!fleet_ops||!callback_matches(fleet_ops))return false;
    const auto base=reinterpret_cast<std::uintptr_t>(image);
    if(profile::open_call_rva>UINTPTR_MAX-base)return false;
    const auto site=base+profile::open_call_rva;
    std::array<std::uint8_t,6> call{};std::uint32_t relative=0;
    if(!read(site,call.data(),call.size())||call[0]!=0xe8||call[5]!=0x90)return false;
    std::memcpy(&relative,call.data()+1,4);
    return static_cast<std::uint32_t>(site+5+relative)==
        reinterpret_cast<std::uintptr_t>(fleet_ops)+profile::fopen_callback.rva;
}
bool qualify(HMODULE image,HMODULE fleet_ops) noexcept {
    if(!native11_capture::exact_engine_image(image)||
       !native11_capture::exact_image_file(fleet_ops,profile::fleet_sha256.data())||
       !owned(image,fleet_ops))return false;
    HMODULE retained=nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<const char*>(fleet_ops),&retained)&&retained==fleet_ops;
}
bool normalize(HMODULE image,HMODULE fleet_ops,std::uint32_t rva,
    std::uint8_t* bytes,std::size_t size) noexcept {
    if(!bytes||!size||size-1>UINT32_MAX-rva)return false;
    constexpr auto site=profile::open_call_rva;
    constexpr auto length=profile::open_call_bytes.size();
    if(std::uint64_t(rva)+size<=site||rva>=site+length)return true;
    if(rva>site||std::uint64_t(rva)+size<site+length||!owned(image,fleet_ops))return false;
    std::memcpy(bytes+site-rva,profile::open_call_bytes.data(),length);return true;
}
}
