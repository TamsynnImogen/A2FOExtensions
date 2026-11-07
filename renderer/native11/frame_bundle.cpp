#include "frame_bundle.hpp"

#include <cstring>
#include <set>

namespace a2fo::native11 {
namespace {
static_assert(sizeof(Vertex)==8*sizeof(float),"canonical packed vertex manifest");
constexpr std::uint64_t max_payload=128ull*1024*1024;
void message(std::string& error,const char* value) noexcept {
    try { error=value; } catch (...) { error.clear(); }
}
bool add_bytes(std::uint64_t& total,std::uint64_t count,std::uint64_t size) noexcept {
    if(count>max_payload/size || count*size>max_payload-total)return false;
    total+=count*size;return true;
}
bool same_manifest(const PrimitiveFrame& a,const PrimitiveFrame& b) noexcept {
    if(a.meshes.size()!=b.meshes.size() || a.textures.size()!=b.textures.size())return false;
    for(std::size_t i=0;i<a.meshes.size();++i) {
        const auto& x=a.meshes[i];const auto& y=b.meshes[i];
        if(x.id!=y.id || x.vertices.size()!=y.vertices.size() ||
           x.indices.size()!=y.indices.size() ||
           std::memcmp(x.vertices.data(),y.vertices.data(),x.vertices.size()*sizeof(Vertex)) ||
           std::memcmp(x.indices.data(),y.indices.data(),x.indices.size()*sizeof(std::uint32_t)))return false;
    }
    for(std::size_t i=0;i<a.textures.size();++i) {
        const auto& x=a.textures[i];const auto& y=b.textures[i];
        if(x.id!=y.id || x.width!=y.width || x.height!=y.height ||
           x.bgra.size()!=y.bgra.size() ||
           std::memcmp(x.bgra.data(),y.bgra.data(),x.bgra.size()))return false;
    }
    return true;
}
bool valid_packet(const PrimitiveFrame& p,std::uint64_t& total_payload,
                  std::uint64_t& total_draws) {
    if(p.draws.empty() || p.meshes.size()>2048 || p.textures.size()>256 ||
       p.draws.size()>8192 || p.lights.size()>8 ||
       p.settings.draws || p.settings.draw_count ||
       p.settings.lights || p.settings.light_count)return false;
    if(p.draws.size()>8192-total_draws)return false;
    total_draws+=p.draws.size();
    std::uint64_t vertices=0,indices=0;
    std::set<std::uint64_t> mesh_ids,texture_ids;
    for(const auto& mesh:p.meshes) {
        if(!mesh.id || mesh.vertices.size()<3 || mesh.indices.size()<3 ||
           mesh.indices.size()%3 || mesh.vertices.size()>1000000-vertices ||
           mesh.indices.size()>3000000-indices ||
           !add_bytes(total_payload,mesh.vertices.size(),sizeof(Vertex)) ||
           !add_bytes(total_payload,mesh.indices.size(),sizeof(std::uint32_t)) ||
           !mesh_ids.insert(mesh.id).second)return false;
        vertices+=mesh.vertices.size();indices+=mesh.indices.size();
        for(auto index:mesh.indices)if(index>=mesh.vertices.size())return false;
    }
    for(const auto& texture:p.textures) {
        const auto bytes=std::uint64_t(texture.width)*texture.height*4;
        if(!texture.id || !texture.width || !texture.height ||
           texture.width>16384 || texture.height>16384 ||
           texture.bgra.size()!=bytes ||
           !add_bytes(total_payload,texture.bgra.size(),1) ||
           !texture_ids.insert(texture.id).second)return false;
    }
    for(const auto& draw:p.draws)
        if(!draw.mesh || !mesh_ids.count(draw.mesh) ||
           (draw.texture && !texture_ids.count(draw.texture)) ||
           draw.surface.mesh || draw.surface.texture)return false;
    return true;
}
}

bool validate_frame_bundle(const FrameBundle& bundle,std::string& error) noexcept {
    try {
        const auto& id=bundle.identity;
        if(!id.view || !id.generation || !id.source_epoch || !id.source_ticket ||
           !bundle.width || !bundle.height || bundle.width>4096 || bundle.height>4096 ||
           !bundle.complete_geometry || (bundle.absent_domains&~all_frame_domains) ||
           bundle.passes.empty() || bundle.passes.size()>16) {
            message(error,"invalid frame identity, target, declaration or pass limit");return false;
        }
        std::uint32_t present=0,previous=0;
        std::uint64_t total_payload=0,total_draws=0;
        const PrimitiveFrame* reference=nullptr;
        for(std::size_t i=0;i<bundle.passes.size();++i) {
            const auto& pass=bundle.passes[i];
            const auto domain=static_cast<std::uint32_t>(pass.domain);
            if(!domain || (domain&~all_frame_domains) || (domain&(domain-1)) ||
               domain<previous || (domain&bundle.absent_domains) || !pass.frame) {
                message(error,"invalid, absent or unordered frame domain");return false;
            }
            previous=domain;present|=domain;
            const auto& p=*pass.frame;
            if(p.generation!=id.generation || p.source_epoch!=id.source_epoch ||
               p.settings.viewport[0]!=0 || p.settings.viewport[1]!=0 ||
               p.settings.viewport[2]!=bundle.width ||
               p.settings.viewport[3]!=bundle.height ||
               (i==0 && (!p.settings.clear_colour_target || !p.settings.clear_depth_target)) ||
               (i!=0 && p.settings.clear_colour_target)) {
                message(error,"frame source, viewport or clear/load mismatch");return false;
            }
            if(!valid_packet(p,total_payload,total_draws)) {
                message(error,"invalid primitive pass or aggregate budget");return false;
            }
            if(reference && !same_manifest(*reference,p)) {
                message(error,"frame resource manifests differ");return false;
            }
            if(!reference)reference=&p;
        }
        if((present|bundle.absent_domains)!=all_frame_domains) {
            message(error,"frame domain inventory incomplete");return false;
        }
        error.clear();return true;
    } catch(const std::bad_alloc&) {
        message(error,"frame admission allocation failed");return false;
    } catch(...) {
        message(error,"frame admission failed");return false;
    }
}

} // namespace a2fo::native11
