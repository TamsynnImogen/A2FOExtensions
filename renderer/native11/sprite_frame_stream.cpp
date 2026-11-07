#include "sprite_frame_stream.hpp"
#include "assets.h"
#include "texture_bgra.hpp"
#include <cmath>
#include <cstddef>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::uint64_t mib=1024ull*1024;
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
struct Parsed {
    std::vector<std::uint8_t> pixels;
    std::uint32_t width=0,height=0;
    bool colour_key=false,called=false;
    std::string error;
};
int receive(void* opaque,const A2FO_N11_Texture* input) noexcept {
    auto& state=*static_cast<Parsed*>(opaque);
    if(state.called||!input||!input->payload||!input->mips||!input->mip_count||input->mip_count>15||input->reserved){
        fail(state.error,"invalid parsed texture");return 0;
    }
    state.called=true;
    const auto format=static_cast<TextureFormat>(input->format);
    if(format==TextureFormat::bgra8_srgb||format==TextureFormat::rgba8_srgb||
       format==TextureFormat::bc1_srgb||format==TextureFormat::bc2_srgb||format==TextureFormat::bc3_srgb||
       format==TextureFormat::bc7_srgb){fail(state.error,"sRGB primitive texture unsupported");return 0;}
    TextureMip mips[15]{};
    for(std::uint32_t i=0;i<input->mip_count;++i){
        const auto& src=input->mips[i];
        mips[i]={src.width,src.height,src.row_pitch,src.slice_pitch,src.offset,src.byte_count};
    }
    TextureView view{format,static_cast<TextureAlpha>(input->alpha_mode),input->width,input->height,
        mips,input->mip_count,input->payload,input->payload_bytes};
    if(!texture_base_to_bgra(view,state.colour_key,state.pixels,state.error))return 0;
    state.width=input->width;state.height=input->height;return 1;
}
bool decode(const TextureAsset& asset,PrimitiveTexture& output,std::string& error) noexcept {
    Parsed state;state.colour_key=asset.colour_key;
    if(a2fo_native11_parse_texture(asset.bytes->data(),
        static_cast<std::uint32_t>(asset.bytes->size()),receive,&state)!=1||!state.called){
        if(!state.error.empty())error.swap(state.error);
        else fail(error,"texture parse failed");
        return false;
    }
    output.id=asset.serial;output.width=state.width;output.height=state.height;
    output.bgra.swap(state.pixels);return true;
}
PrimitiveMesh unit_quad(){
    PrimitiveMesh result;result.id=1;
    result.vertices={{{0,0,0},{0,0,-1},{0,0}},{{1,0,0},{0,0,-1},{1,0}},
                     {{1,1,0},{0,0,-1},{1,1}},{{0,1,0},{0,0,-1},{0,1}}};
    result.indices={0,1,2,0,2,3};return result;
}
}

bool SpriteFrameStream::update(const CapturedSpriteBatch& input,std::uint64_t fresh_epoch,
    std::shared_ptr<const PrimitiveFrame>& out,std::string& error) noexcept {
    if(active_)return fail(error,"sprite stream reentrant update");
    if(!input.view||!input.width||!input.height||input.width>4096||input.height>4096||
       !input.generation||input.generation<=last_generation_||!fresh_epoch||
       fresh_epoch<=last_fresh_epoch_||!input.source_ticket||input.sprites.size()>8192)
        return fail(error,"invalid sprite batch identity or limits");
    active_=true;
    struct Guard {bool& active;~Guard(){active=false;}} guard{active_};
    try {
        auto candidate=std::make_shared<PrimitiveFrame>();
        candidate->generation=input.generation;
        candidate->meshes.push_back(unit_quad());
        candidate->settings.ambient[0]=candidate->settings.ambient[1]=candidate->settings.ambient[2]=1;
        candidate->settings.clear_colour_target=false;
        candidate->settings.clear_depth_target=false;
        candidate->settings.viewport[0]=candidate->settings.viewport[1]=0;
        candidate->settings.viewport[2]=static_cast<float>(input.width);
        candidate->settings.viewport[3]=static_cast<float>(input.height);
        std::map<std::uint64_t,Cached> next;
        std::uint64_t encoded=candidate->meshes[0].vertices.size()*sizeof(Vertex)+
            candidate->meshes[0].indices.size()*sizeof(std::uint32_t);
        std::uint64_t decoded=encoded;
        candidate->draws.reserve(input.sprites.size());
        for(const auto& sprite:input.sprites){
            if(!sprite.texture||!sprite.texture->serial||sprite.texture->flags||
               !sprite.texture->bytes||sprite.texture->bytes->empty()||
               sprite.texture->bytes->size()>64*mib||
               static_cast<std::uint32_t>(sprite.sampling)>3)
                return fail(error,"invalid sprite texture or sampling");
            for(float value:sprite.colour)if(!std::isfinite(value)||value<0||value>1)
                return fail(error,"invalid sprite colour");
            QuadGeometry geometry;
            if(!build_screen_sprite(sprite.geometry,input.width,input.height,geometry,error))return false;
            const auto id=sprite.texture->serial;
            auto found=next.find(id);
            if(found==next.end()){
                if(next.size()>=256)return fail(error,"too many sprite textures");
                encoded+=sprite.texture->bytes->size();
                if(encoded>128*mib)return fail(error,"sprite encoded budget exceeded");
                Cached record;record.owner=sprite.texture;record.bytes=sprite.texture->bytes;
                const auto previous=cache_.find(id);
                if(previous!=cache_.end()&&previous->second.owner==record.owner&&
                   previous->second.bytes==record.bytes){record.decoded=previous->second.decoded;}
                else if(!decode(*record.owner,record.decoded,error))return false;
                decoded+=record.decoded.bgra.size();
                if(decoded>128*mib)return fail(error,"sprite decoded budget exceeded");
                found=next.emplace(id,std::move(record)).first;
                candidate->textures.push_back(found->second.decoded);
            }else if(found->second.owner!=sprite.texture||found->second.bytes!=sprite.texture->bytes){
                return fail(error,"sprite texture ID has conflicting owner");
            }
            if(geometry.vertices.empty())continue;
            const auto& a=geometry.vertices[0],&b=geometry.vertices[1],&d=geometry.vertices[3];
            // A positive pixel span can collapse when converted to float NDC.
            // It covers no representable area and must not publish a singular
            // model transform, which the renderer correctly refuses.
            if(a.position[0]==b.position[0]||a.position[1]==d.position[1])continue;
            PrimitiveDraw draw;draw.mesh=1;draw.texture=id;
            auto& surface=draw.surface;
            surface.local_to_world[0]=b.position[0]-a.position[0];
            surface.local_to_world[3]=a.position[0];
            surface.local_to_world[5]=d.position[1]-a.position[1];
            surface.local_to_world[7]=a.position[1];
            surface.local_to_world[11]=a.position[2];
            surface.uv_transform[0]=b.uv[0]-a.uv[0];surface.uv_transform[2]=a.uv[0];
            surface.uv_transform[4]=d.uv[1]-a.uv[1];surface.uv_transform[5]=a.uv[1];
            for(unsigned i=0;i<4;++i)surface.colour[i]=sprite.colour[i];
            surface.flags=two_sided|(sprite.additive?additive:alpha_blend);
            surface.sampler=sprite.sampling;surface.depth_mode=DepthMode::disabled;
            surface.casts_shadow=false;surface.receives_shadow=false;surface.fog_enabled=false;
            candidate->draws.push_back(draw);
        }
        bool same=current_&&input.source_ticket==last_ticket_&&next.size()==cache_.size();
        if(same){
            auto a=next.begin(),b=cache_.begin();
            for(;a!=next.end();++a,++b)if(a->first!=b->first||a->second.owner!=b->second.owner||
                a->second.bytes!=b->second.bytes){same=false;break;}
        }
        candidate->source_epoch=same?last_source_epoch_:fresh_epoch;
        std::shared_ptr<const PrimitiveFrame> published=std::move(candidate);
        cache_.swap(next);current_=published;
        last_generation_=input.generation;last_fresh_epoch_=fresh_epoch;
        last_source_epoch_=published->source_epoch;last_ticket_=input.source_ticket;
        out=std::move(published);error.clear();return true;
    }catch(...){return fail(error,"sprite frame allocation failed");}
}
void SpriteFrameStream::clear() noexcept {
    if(active_)return;
    cache_.clear();current_.reset();last_ticket_=0;
}
} // namespace a2fo::native11
