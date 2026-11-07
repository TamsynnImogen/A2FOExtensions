#include "open_read_owner.hpp"
#include <limits>

namespace a2fo::native11 {
namespace {
constexpr std::uint32_t rel32_size=4;
constexpr std::uint32_t jmp_size=1;
std::uint32_t read_u32(const std::uint8_t* bytes) noexcept {
    return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|
           (std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
}
bool is_jump_to(const std::uint8_t* bytes,std::uint32_t instruction,
                std::uint32_t destination) noexcept {
    if(bytes[0]!=0xe9) return false;
    const auto displacement=read_u32(bytes+1);
    return instruction+jmp_size+rel32_size+displacement==destination;
}
}

bool owns_open_read_gateway(void* ctx,AssetMemoryRead read,
    const OpenReadOwner& owner) noexcept {
    constexpr auto max=std::numeric_limits<std::uint32_t>::max();
    if(!read||!owner.target||!owner.gateway||!owner.replacement||owner.length!=6||
       owner.target>max-5||owner.gateway>max-10) return false;

    std::uint8_t target[6]{};
    std::uint8_t gateway[11]{};
    if(!read(ctx,owner.target,target,sizeof target)||
       !read(ctx,owner.gateway,gateway,sizeof gateway)) return false;
    if(target[5]!=0x90||!is_jump_to(target,owner.target,owner.replacement)) return false;
    for(std::size_t i=0;i<open_read_entry_bytes.size();++i)
        if(gateway[i]!=open_read_entry_bytes[i]) return false;
    return is_jump_to(gateway+6,owner.gateway+6,owner.target+6);
}
} // namespace a2fo::native11
