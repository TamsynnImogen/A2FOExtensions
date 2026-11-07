#include "material_observation.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_model = 64u*1024u*1024u;
constexpr std::size_t max_parts = 8192, max_metadata = 1024u*1024u;
constexpr std::uint32_t max_nodes = 4096;
bool printable(const std::string& s,bool nonempty,std::size_t max) {
    if((nonempty&&s.empty())||s.size()>max)return false;
    for(unsigned char c:s)if(c<32||c>126)return false;
    return true;
}
bool copied_name(const char* data,std::size_t size,std::string& out) {
    const auto* end=static_cast<const char*>(std::memchr(data,0,size));
    if(!end||end==data)return false;
    out.assign(data,end);return printable(out,true,size-1);
}
bool valid_model(const std::shared_ptr<const ModelAsset>& asset) {
    std::string name;
    return asset&&asset->info.database&&asset->info.serial&&
        copied_name(asset->info.name,sizeof(asset->info.name),name)&&
        !asset->bytes.empty()&&asset->bytes.size()<=max_model&&asset->bytes.size()==asset->info.byte_count;
}
struct ParseContext {
    ModelMaterialSource source;
    std::set<std::pair<std::uint32_t,std::uint32_t>> pairs;
    std::map<std::string,std::uint32_t> names;
    std::size_t metadata=0;
    bool failed=false;
};
int sink(void* raw,const A2FO_N11_Part* part,const A2FO_N11_PartSource* origin) noexcept {
    auto* ctx=static_cast<ParseContext*>(raw);
    if(!ctx||ctx->failed||!part||!origin)return 0;
    try {
        if(ctx->source.parts.size()>=max_parts||!part->vertices||!part->indices||
           !part->vertex_count||part->vertex_count>1048576||!part->index_count||
           part->index_count>1048576||!origin->node_name||
           !origin->node_name_length||origin->node_name_length>255||
           part->texture_name_length>1024||
           (part->texture_name_length&&!part->texture_name)) {ctx->failed=true;return 0;}
        MaterialPartSource item;
        item.node_name.assign(reinterpret_cast<const char*>(origin->node_name),origin->node_name_length);
        if(part->texture_name_length)item.authored_texture.assign(
            reinterpret_cast<const char*>(part->texture_name),part->texture_name_length);
        if(!printable(item.node_name,true,255)||!printable(item.authored_texture,false,1024)||
           item.node_name.size()+item.authored_texture.size()>max_metadata-ctx->metadata) {
            ctx->failed=true;return 0;
        }
        item.node_index=origin->node_index;item.group_index=origin->group_index;
        item.source_triangles=part->index_count%3==0?part->index_count/3:0;
        item.source_flags=part->flags;
        if(!ctx->pairs.insert({item.node_index,item.group_index}).second||
           (ctx->names.count(item.node_name)&&ctx->names.at(item.node_name)!=item.node_index)) {
            ctx->failed=true;return 0;
        }
        ctx->names.emplace(item.node_name,item.node_index);
        ctx->metadata+=item.node_name.size()+item.authored_texture.size();
        ctx->source.parts.push_back(std::move(item));return 1;
    }catch(...){ctx->failed=true;return 0;}
}
struct Reader {
    void* context;Read read;
    bool at(std::uint32_t base,std::uint32_t offset,void* out,std::size_t n) const noexcept {
        return read&&base&&n&&offset<=UINT32_MAX-base&&n-1<=UINT32_MAX-base-offset&&
            read(context,base+offset,out,n);
    }
    bool word(std::uint32_t base,std::uint32_t offset,std::uint32_t& out) const noexcept {
        return at(base,offset,&out,sizeof(out));
    }
    bool name(std::uint32_t object,std::string& out,std::size_t maximum) const {
        std::uint32_t pointer=0;if(!word(object,8,pointer)||!pointer)return false;
        out.clear();
        for(std::size_t i=0;i<=maximum;++i){char c=0;
            if(!at(pointer,static_cast<std::uint32_t>(i),&c,1))return false;
            if(!c)return printable(out,true,maximum);
            if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)return false;
            out.push_back(c);
        }
        return false;
    }
};
}
bool inspect_model_materials(const std::shared_ptr<const ModelAsset>& asset,InspectSod parser,
    std::shared_ptr<const ModelMaterialSource>& out,std::string& error) noexcept {
    try {
        if(!valid_model(asset)||!parser){error="invalid model or parser";return false;}
        ParseContext ctx;ctx.source.model=asset;
        if(parser(asset->bytes.data(),static_cast<std::uint32_t>(asset->bytes.size()),sink,&ctx)!=1||
           ctx.failed||ctx.source.parts.empty()){error="invalid detailed SOD parts";return false;}
        out=std::make_shared<ModelMaterialSource>(std::move(ctx.source));error.clear();return true;
    }catch(...){try{error="material inspection exception";}catch(...){}return false;}
}
bool capture_material_observation(const Object& object,const ModelMaterialSource& source,
    MaterialReadProfile profile,void* read_context,Read read,void* texture_context,
    ResolveTextureAsset resolve,std::shared_ptr<const MaterialObservation>& out,std::string& error,
    const std::vector<PartState>* posed_states) noexcept {
    try {
        std::string object_name,asset_name;
        if(!read||!resolve||!profile.instance_vtable||!profile.database_vtable||
           !profile.mesh_type_function||!object.instance||!object.database||!object.model_serial||
           object.instance_vtable!=profile.instance_vtable||
           object.database_vtable!=profile.database_vtable||
           object.model_identity!=ModelIdentity::plain_database_name||
           !valid_model(source.model)||source.model->info.database!=object.database||
           source.model->info.serial!=object.model_serial||
           !copied_name(object.model_name,sizeof(object.model_name),object_name)||
           !copied_name(source.model->info.name,sizeof(source.model->info.name),asset_name)||
           object_name!=asset_name||source.parts.empty()||source.parts.size()>max_parts) {
            error="invalid material observation target";return false;
        }
        if(posed_states){
            if(posed_states->size()!=source.parts.size()){
                error="material node state count mismatch";return false;
            }
            for(const auto& state:*posed_states)if(!valid_part_state(state)){
                error="invalid material node state";return false;
            }
        }
        std::map<std::string,std::uint32_t> expected;
        std::set<std::pair<std::uint32_t,std::uint32_t>> groups;
        std::size_t metadata=0;
        for(const auto& part:source.parts){
            if(!printable(part.node_name,true,255)||!printable(part.authored_texture,false,1024)||
               part.node_name.size()+part.authored_texture.size()>max_metadata-metadata||
               !groups.insert({part.node_index,part.group_index}).second||
               (expected.count(part.node_name)&&expected.at(part.node_name)!=part.node_index)) {
                error="invalid material source metadata";return false;
            }
            expected.emplace(part.node_name,part.node_index);
            metadata+=part.node_name.size()+part.authored_texture.size();
        }
        Reader r{read_context,read};std::uint32_t vtable=0,database=0,root=0;
        if(!r.word(object.instance,0,vtable)||vtable!=profile.instance_vtable||
           !r.word(object.instance,0x80,database)||database!=object.database||
           !r.word(database,0,vtable)||vtable!=profile.database_vtable||
           !r.word(database,0x3c,root)||!root){error="material owner or root mismatch";return false;}
        std::map<std::string,std::uint32_t> found;
        std::set<std::uint32_t> visited;
        std::vector<std::uint32_t> pending{root};
        while(!pending.empty()){
            const auto node=pending.back();pending.pop_back();
            if(!node||visited.size()>=max_nodes||!visited.insert(node).second){error="material tree cycle or alias";return false;}
            std::string name;std::uint32_t link_1c=0,link_20=0;
            if(!r.name(node,name,255)||!r.word(node,0x20,link_20)||!r.word(node,0x1c,link_1c)){
                error="unreadable material tree";return false;
            }
            if(expected.count(name)&&!found.emplace(name,node).second){error="ambiguous material node";return false;}
            if(link_1c)pending.push_back(link_1c);
            if(link_20)pending.push_back(link_20);
        }
        if(found.size()!=expected.size()){error="missing material node";return false;}
        auto observation=std::make_shared<MaterialObservation>();
        observation->instance=object.instance;observation->database=database;
        observation->model_serial=object.model_serial;observation->model=source.model;
        observation->parts.reserve(source.parts.size());
        std::map<std::uint32_t,std::pair<std::uint32_t,std::uint32_t>> nodes;
        std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> textures;
        for(const auto& part:source.parts){
            const auto node=found.at(part.node_name);
            auto cache=nodes.find(node);
            if(cache==nodes.end()){
                std::uint32_t node_vtable=0,type=0,flags=0,data=0,count=0,slot=0;
                if(!r.word(node,0,node_vtable)||!node_vtable||
                   !r.word(node_vtable,4,type)||type!=profile.mesh_type_function||
                   !r.word(node,0xbc,flags)||!r.word(node,0x114,data)||
                   !r.word(node,0x11c,count)||count>8||
                   (count&&(!data||!r.word(data,0,slot)))){
                    error="invalid material mesh or slot vector";return false;
                }
                cache=nodes.emplace(node,std::make_pair(flags,slot)).first;
            }
            MaterialBinding binding;binding.authored_texture=part.authored_texture;
            binding.node_index=part.node_index;binding.group_index=part.group_index;
            binding.native_node=node;
            binding.source_triangles=part.source_triangles;binding.source_flags=part.source_flags;
            binding.node_flags=cache->second.first;
            if(posed_states){binding.node_state_valid=true;
                binding.node_state=posed_states->at(observation->parts.size());}
            const auto ptr=cache->second.second;
            if(ptr&&(!binding.node_state_valid||binding.node_state.visible)){
                auto texture=textures.find(ptr);
                if(texture==textures.end()){
                    auto asset=resolve(texture_context,ptr);std::string runtime_name;
                    std::uint32_t flags=0;std::uint8_t key=0;
                    const bool live_name=r.name(ptr,runtime_name,1024);
                    const bool live_flags=r.word(ptr,0x18,flags);
                    const bool live_key=r.at(ptr,0x2c,&key,1);
                    if(!asset||asset->texture!=ptr||!asset->serial||
                       (asset->flags&~texture_has_alpha)||asset->colour_key||
                       !asset->bytes||asset->bytes->empty()||asset->bytes->size()>max_model||
                       !printable(asset->logical_name,true,1024)||
                       !live_name||runtime_name!=asset->logical_name||
                       !live_flags||flags!=asset->flags||!live_key||bool(key)!=asset->colour_key){
                        char detail[384]{};
                        std::snprintf(detail,sizeof(detail),"invalid live texture version: texture=%08x owner=%u serial=%llu flags=%08x/%08x key=%u/%u bytes=%u name_match=%u name=%.128s",
                            ptr,asset?1u:0u,static_cast<unsigned long long>(asset?asset->serial:0),
                            asset?asset->flags:0,flags,asset&&asset->colour_key?1u:0u,unsigned(key),
                            asset&&asset->bytes?static_cast<unsigned>(asset->bytes->size()):0u,
                            asset&&live_name&&runtime_name==asset->logical_name?1u:0u,runtime_name.c_str());
                        error=detail;return false;
                    }
                    texture=textures.emplace(ptr,std::move(asset)).first;
                }
                binding.texture=texture->second;
            }
            observation->parts.push_back(std::move(binding));
        }
        for(const auto& [ptr,asset]:textures)
            if(resolve(texture_context,ptr)!=asset){error="texture replaced during observation";return false;}
        out=std::move(observation);error.clear();return true;
    }catch(...){try{error="material observation exception";}catch(...){}return false;}
}
} // namespace a2fo::native11
