#include "primitive_scene.hpp"

#include <wincrypt.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace a2fo::native11 {
namespace {
using Digest=std::array<unsigned char,32>;
void message(std::string& error,const char* value) noexcept {
    try { error=value; } catch (...) { error.clear(); }
}
bool finite_vertices(const PrimitiveMesh& mesh) noexcept {
    for(const auto& vertex:mesh.vertices) {
        for(float x:vertex.position)if(!std::isfinite(x))return false;
        for(float x:vertex.normal)if(!std::isfinite(x))return false;
        for(float x:vertex.uv)if(!std::isfinite(x))return false;
    }
    for(auto index:mesh.indices)if(index>=mesh.vertices.size())return false;
    return true;
}
class Sha256 {
    HCRYPTPROV provider_=0;
    HCRYPTHASH hash_=0;
public:
    Sha256() noexcept {
        if(!CryptAcquireContextA(&provider_,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)||
           !CryptCreateHash(provider_,CALG_SHA_256,0,0,&hash_)) {
            if(provider_)CryptReleaseContext(provider_,0);
            provider_=0;
        }
    }
    ~Sha256(){if(hash_)CryptDestroyHash(hash_);if(provider_)CryptReleaseContext(provider_,0);}
    bool add(const void* bytes,std::size_t size) noexcept {
        return hash_ && size<=MAXDWORD &&
               CryptHashData(hash_,static_cast<const BYTE*>(bytes),static_cast<DWORD>(size),0);
    }
    bool finish(Digest& result) noexcept {
        DWORD size=static_cast<DWORD>(result.size());
        return hash_ && CryptGetHashParam(hash_,HP_HASHVAL,result.data(),&size,0) &&
               size==result.size();
    }
};
bool signature(const PrimitiveMesh& mesh,Digest& output) noexcept {
    static_assert(sizeof(Vertex)==8*sizeof(float),"canonical packed vertex");
    Sha256 hash;
    const std::uint64_t sizes[]{mesh.vertices.size(),mesh.indices.size()};
    return hash.add(sizes,sizeof(sizes)) &&
           hash.add(mesh.vertices.data(),mesh.vertices.size()*sizeof(Vertex)) &&
           hash.add(mesh.indices.data(),mesh.indices.size()*sizeof(std::uint32_t)) &&
           hash.finish(output);
}
bool signature(const PrimitiveTexture& texture,Digest& output) noexcept {
    Sha256 hash;
    const std::uint32_t dimensions[]{texture.width,texture.height};
    return hash.add(dimensions,sizeof(dimensions)) &&
           hash.add(texture.bgra.data(),texture.bgra.size()) && hash.finish(output);
}
struct Resource {
    std::uint32_t handle=0;
    Digest digest{};
    bool operator==(const Resource& other) const noexcept {return digest==other.digest;}
};
using ResourceMap=std::map<std::uint64_t,Resource>;
bool same_manifest(const ResourceMap& a,const ResourceMap& b) noexcept {
    if(a.size()!=b.size())return false;
    auto left=a.begin(),right=b.begin();
    for(;left!=a.end();++left,++right)
        if(left->first!=right->first || !(left->second==right->second))return false;
    return true;
}
}

struct PrimitiveScene::Impl {
    Renderer& renderer;
    const DWORD thread=GetCurrentThreadId();
    bool busy=false,bound=false;
    std::uint64_t generation=0,source_epoch=0,device_epoch=0;
    ResourceMap meshes,textures;
    explicit Impl(Renderer& value):renderer(value){}
    void release(const ResourceMap& mesh_map,const ResourceMap& texture_map,
                 std::uint64_t epoch) noexcept {
        ResourceUsage usage{};
        if(GetCurrentThreadId()!=thread || FAILED(renderer.resource_usage(usage)) ||
           usage.epoch!=epoch)return;
        for(const auto& entry:mesh_map)if(entry.second.handle)renderer.release_mesh(entry.second.handle);
        for(const auto& entry:texture_map)if(entry.second.handle)renderer.release_texture(entry.second.handle);
    }
    void clear() noexcept {
        release(meshes,textures,device_epoch);
        meshes.clear();textures.clear();bound=false;device_epoch=0;
    }
};

PrimitiveScene::PrimitiveScene(Renderer& renderer):p_(std::make_unique<Impl>(renderer)){}
PrimitiveScene::~PrimitiveScene(){if(p_)p_->clear();}
void PrimitiveScene::clear() noexcept {
    if(p_ && GetCurrentThreadId()==p_->thread && !p_->busy)p_->clear();
}

HRESULT PrimitiveScene::render(const PrimitiveFrame& packet,std::string& error) noexcept {
    if(GetCurrentThreadId()!=p_->thread || p_->busy) {
        message(error,"primitive scene foreign-thread or reentrant call");return E_UNEXPECTED;
    }
    struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}} busy(p_->busy);
    try {
        if(!packet.generation || packet.generation<=p_->generation ||
           !packet.source_epoch || packet.source_epoch<p_->source_epoch ||
           (!p_->bound && packet.source_epoch<=p_->source_epoch) ||
           packet.settings.draws || packet.settings.draw_count ||
           packet.settings.lights || packet.settings.light_count ||
           packet.meshes.size()>2048 || packet.textures.size()>256 ||
           packet.draws.size()>8192 || packet.lights.size()>8) {
            message(error,"invalid primitive frame identity or limits");return E_INVALIDARG;
        }
        std::uint64_t vertex_count=0,index_count=0,payload=0;
        ResourceMap mesh_manifest,texture_manifest;
        for(const auto& mesh:packet.meshes) {
            if(!mesh.id || mesh.vertices.size()<3 || mesh.indices.size()<3 ||
               mesh.indices.size()%3 || mesh.vertices.size()>1000000 ||
               mesh.indices.size()>3000000 || !finite_vertices(mesh)) {
                message(error,"invalid primitive mesh or payload limit");return E_INVALIDARG;
            }
            vertex_count+=mesh.vertices.size();index_count+=mesh.indices.size();
            payload+=std::uint64_t(mesh.vertices.size())*sizeof(Vertex)+
                     std::uint64_t(mesh.indices.size())*sizeof(std::uint32_t);
            if(vertex_count>1000000 || index_count>3000000 || payload>128ull*1024*1024) {
                message(error,"primitive mesh payload limit exceeded");return E_INVALIDARG;
            }
            Digest digest{};
            if(!signature(mesh,digest)){message(error,"primitive mesh SHA-256 failed");return E_FAIL;}
            if(!mesh_manifest.emplace(mesh.id,Resource{0,digest}).second) {
                message(error,"duplicate primitive mesh ID");return E_INVALIDARG;
            }
        }
        for(const auto& texture:packet.textures) {
            const std::uint64_t bytes=std::uint64_t(texture.width)*texture.height*4;
            payload+=texture.bgra.size();
            if(!texture.id || !texture.width || !texture.height ||
               texture.width>16384 || texture.height>16384 ||
               bytes!=texture.bgra.size() || payload>128ull*1024*1024) {
                message(error,"invalid primitive BGRA or payload limit");return E_INVALIDARG;
            }
            Digest digest{};
            if(!signature(texture,digest)){message(error,"primitive texture SHA-256 failed");return E_FAIL;}
            if(!texture_manifest.emplace(texture.id,Resource{0,digest}).second) {
                message(error,"duplicate primitive texture ID");return E_INVALIDARG;
            }
        }
        for(const auto& draw:packet.draws) {
            if(!draw.mesh || !mesh_manifest.count(draw.mesh) ||
               (draw.texture && !texture_manifest.count(draw.texture)) ||
               draw.surface.mesh || draw.surface.texture) {
                message(error,"primitive draw has missing resource or foreign handle");return E_INVALIDARG;
            }
        }
        if(p_->bound && packet.source_epoch==p_->source_epoch &&
           (!same_manifest(mesh_manifest,p_->meshes) ||
            !same_manifest(texture_manifest,p_->textures))) {
            message(error,"primitive content changed inside source epoch");return E_INVALIDARG;
        }
        const HRESULT available=p_->renderer.render_status();
        if(FAILED(available)){message(error,"native renderer unavailable");return available;}
        ResourceUsage usage{};
        const HRESULT usable=p_->renderer.resource_usage(usage);
        if(FAILED(usable)){message(error,"native renderer resource usage unavailable");return usable;}
        const bool hot=p_->bound && packet.source_epoch==p_->source_epoch &&
                       usage.epoch==p_->device_epoch;
        struct Pending {
            Impl& self;
            ResourceMap& meshes;
            ResourceMap& textures;
            std::uint64_t epoch;
            bool committed=false;
            ~Pending(){if(!committed)self.release(meshes,textures,epoch);}
        } pending{*p_,mesh_manifest,texture_manifest,usage.epoch};
        if(hot) {
            for(auto& entry:mesh_manifest)entry.second.handle=p_->meshes.at(entry.first).handle;
            for(auto& entry:texture_manifest)entry.second.handle=p_->textures.at(entry.first).handle;
            pending.committed=true; // hot handles belong to the old cache
        } else {
            for(const auto& mesh:packet.meshes) {
                std::uint32_t handle=0;
                const HRESULT hr=p_->renderer.upload_mesh({mesh.vertices.data(),
                    static_cast<std::uint32_t>(mesh.vertices.size()),mesh.indices.data(),
                    static_cast<std::uint32_t>(mesh.indices.size())},handle);
                if(FAILED(hr)){message(error,"primitive mesh upload failed");return hr;}
                mesh_manifest.at(mesh.id).handle=handle;
            }
            for(const auto& texture:packet.textures) {
                std::uint32_t handle=0;
                const HRESULT hr=p_->renderer.upload_texture({texture.bgra.data(),
                    texture.width,texture.height,texture.width*4,texture.bgra.size()},handle);
                if(FAILED(hr)){message(error,"primitive texture upload failed");return hr;}
                texture_manifest.at(texture.id).handle=handle;
            }
        }
        std::vector<Draw> draws;
        draws.reserve(packet.draws.size());
        for(const auto& input:packet.draws) {
            Draw draw=input.surface;
            draw.mesh=mesh_manifest.at(input.mesh).handle;
            draw.texture=input.texture?texture_manifest.at(input.texture).handle:0;
            draws.push_back(draw);
        }
        FrameView frame=packet.settings;
        frame.lights=packet.lights.data();
        frame.light_count=static_cast<std::uint32_t>(packet.lights.size());
        frame.draws=draws.data();
        frame.draw_count=static_cast<std::uint32_t>(draws.size());
        const HRESULT hr=p_->renderer.render(frame,false);
        if(FAILED(hr)){message(error,"primitive frame render failed");return hr;}
        if(!hot) {
            p_->release(p_->meshes,p_->textures,p_->device_epoch);
            p_->meshes.swap(mesh_manifest);
            p_->textures.swap(texture_manifest);
            p_->device_epoch=usage.epoch;
            pending.committed=true;
        }
        p_->source_epoch=packet.source_epoch;
        p_->generation=packet.generation;
        p_->bound=true;
        error.clear();return S_OK;
    } catch(const std::bad_alloc&) {
        message(error,"primitive scene allocation failed");return E_OUTOFMEMORY;
    } catch(...) {
        message(error,"primitive scene failed");return E_FAIL;
    }
}

} // namespace a2fo::native11
