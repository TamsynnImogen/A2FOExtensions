#include "sprite_observation.hpp"
#include "native_arithmetic.hpp"
#include "sprite_colour_policy.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace a2fo::native11 {
namespace {
enum Field : std::size_t {device_vtable,draw_target,device_material,sprite_vtable,
    red,green,blue,sprite_material,texture,alpha_test,src_blend,dst_blend,fill_mode,
    alpha,address_u,address_v,mag_filter,min_filter,mip_filter,
    left,top,depth,width,height,u,v,du,dv,count};
using Words=std::array<std::uint32_t,count>;
bool word(void* context,Read read,std::uint32_t base,std::uint32_t offset,
    std::uint32_t& output) noexcept {
    constexpr auto maximum=std::numeric_limits<std::uint32_t>::max();
    if(!base||offset>maximum-3||base>maximum-offset-3)return false;
    return read(context,base+offset,&output,sizeof(output));
}
bool snapshot(const SpriteDispatch& request,void* context,Read read,Words& out) noexcept {
    if(!word(context,read,request.device,0,out[device_vtable])||
       !word(context,read,request.dispatch_table,0xc4,out[draw_target])||
       !word(context,read,request.device,0x44,out[device_material])||
       !word(context,read,request.sprite,0,out[sprite_vtable])||
       !word(context,read,request.sprite,0x24,out[red])||
       !word(context,read,request.sprite,0x28,out[green])||
       !word(context,read,request.sprite,0x2c,out[blue])||
       !word(context,read,request.sprite,0x54,out[sprite_material])||
       !word(context,read,request.sprite,0x58,out[texture]))return false;
    const auto material=out[device_material];
    if(!material||!word(context,read,material,0x18,out[alpha_test])||
       !word(context,read,material,0x1c,out[src_blend])||
       !word(context,read,material,0x20,out[dst_blend])||
       !word(context,read,material,0x28,out[fill_mode])||
       !word(context,read,material,0x24,out[alpha])||
       !word(context,read,request.device,0x264,out[address_u])||
       !word(context,read,request.device,0x268,out[address_v])||
       !word(context,read,request.device,0x270,out[mag_filter])||
       !word(context,read,request.device,0x274,out[min_filter])||
       !word(context,read,request.device,0x278,out[mip_filter])||
       !word(context,read,request.position,0,out[left])||
       !word(context,read,request.position,4,out[top])||
       !word(context,read,request.position,8,out[depth])||
       !word(context,read,request.size,0,out[width])||
       !word(context,read,request.size,4,out[height]))return false;
    for(std::size_t i=0;i<4;++i){
        std::memcpy(&out[u+i],&request.uv[i],sizeof(float));
    }
    return true;
}
float as_float(std::uint32_t raw) noexcept {float out=0;std::memcpy(&out,&raw,4);return out;}
bool bounded(float value) noexcept {return std::isfinite(value)&&std::fabs(value)<=1000000.f;}
bool valid_owner(const std::shared_ptr<const TextureAsset>& owner,std::uint32_t id) noexcept {
    return owner&&owner->texture==id&&owner->bytes&&!owner->bytes->empty();
}
bool fail(std::string& error,const char* why) noexcept {
    try{error=why;}catch(...){error.clear();}return false;
}
}
bool capture_sprite_observation(const SpriteDispatch& request,SpriteReadProfile profile,
    void* read_context,Read read,void* texture_context,ResolveTextureAsset resolve,
    std::shared_ptr<const SpriteObservation>& output,std::string& error) noexcept {
    try {
        if(!read||!request.device||!request.dispatch_table||!request.sprite||
           !request.position||!request.size||!profile.device_vtable||
           !profile.sprite_vtable||!profile.draw_target||
           !request.native_x87_control||
           !valid_native_x87_encoding(request.native_x87_control))
            return fail(error,"invalid sprite observation request");
        Words first{},second{};
        if(!snapshot(request,read_context,read,first))
            return fail(error,"unreadable sprite observation");
        if(first[device_vtable]!=request.dispatch_table||
           first[device_vtable]!=profile.device_vtable||first[draw_target]!=profile.draw_target||
           first[sprite_vtable]!=profile.sprite_vtable||
           !first[device_material]||first[device_material]!=first[sprite_material])
            return fail(error,"sprite dispatch or material owner mismatch");
        const auto texture_id=first[texture];
        if(texture_id&&!resolve)return fail(error,"sprite texture resolver unavailable");
        auto owner=texture_id?resolve(texture_context,texture_id):nullptr;
        if(texture_id&&!valid_owner(owner,texture_id))
            return fail(error,"sprite texture owner unavailable");
        const auto owner_serial=owner?owner->serial:0;
        const auto owner_bytes=owner?owner->bytes:nullptr;
        const auto owner_size=owner_bytes?owner_bytes->size():0;
        if(!snapshot(request,read_context,read,second)||first!=second)
            return fail(error,"sprite observation changed during read");
        auto again=texture_id?resolve(texture_context,texture_id):nullptr;
        if(again!=owner||(texture_id&&(!valid_owner(again,texture_id)||
           again->serial!=owner_serial||again->bytes!=owner_bytes||
           again->bytes->size()!=owner_size)))
            return fail(error,"sprite texture owner changed during read");
        const float rgb[3]{as_float(first[red]),as_float(first[green]),as_float(first[blue])};
        const float opacity=as_float(first[alpha]);
        const float x=as_float(first[left]),y=as_float(first[top]),z=as_float(first[depth]);
        const float w=as_float(first[width]),h=as_float(first[height]);
        const float uv[4]{as_float(first[u]),as_float(first[v]),as_float(first[du]),as_float(first[dv])};
        if(!bounded(x)||!bounded(y)||!std::isfinite(z)||z<0||z>1||
           !std::isfinite(w)||w<0||w>1000000.f||!std::isfinite(h)||h<0||h>1000000.f||
           !bounded(uv[0])||!bounded(uv[1])||!bounded(uv[2])||!bounded(uv[3]))
            return fail(error,"sprite geometry outside supported bounds");
        auto pending=std::make_shared<SpriteObservation>();
        if(!native_sprite_colour(rgb,opacity,request.native_x87_control,pending->packed_argb))
            return fail(error,"invalid sprite colour or arithmetic");
        float u1=0,v1=0;
        {NativeArithmeticScope arithmetic(request.native_x87_control);
            if(!arithmetic.valid()||!arithmetic.known())return fail(error,"unknown sprite arithmetic");
            asm volatile("flds %1; fadds %2; fstps %0":"=m"(u1):"m"(uv[0]),"m"(uv[2]):"st","memory");
            asm volatile("flds %1; fadds %2; fstps %0":"=m"(v1):"m"(uv[1]),"m"(uv[3]):"st","memory");
        }
        if(!bounded(u1)||!bounded(v1))return fail(error,"sprite UV endpoint outside supported bounds");
        pending->dispatch=request;
        pending->geometry.left=x;pending->geometry.top=y;pending->geometry.depth=z;
        pending->geometry.width=w;pending->geometry.height=h;
        pending->geometry.uv={uv[0],uv[1],u1,v1};
        pending->material=first[device_material];pending->alpha_test=first[alpha_test];
        pending->src_blend=first[src_blend];pending->dst_blend=first[dst_blend];
        pending->fill_mode=first[fill_mode];pending->address_u=first[address_u];
        pending->address_v=first[address_v];pending->mag_filter=first[mag_filter];
        pending->min_filter=first[min_filter];pending->mip_filter=first[mip_filter];
        pending->texture=texture_id;pending->texture_owner=std::move(owner);
        output=std::move(pending);error.clear();return true;
    }catch(...){return fail(error,"sprite observation exception");}
}
} // namespace a2fo::native11
