#include "content.hpp"
#include "assets.h"
#include "model_sources.h"
#include <cstring>
#include <exception>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>

namespace a2fo::native11 {
namespace {
constexpr std::size_t mib=1024*1024;
constexpr std::size_t model_input_limit=64*mib;
constexpr std::size_t texture_input_limit=256*mib;
constexpr std::size_t geometry_limit=128*mib;
constexpr std::size_t texture_payload_limit=128*mib;
constexpr std::size_t total_limit=256*mib;
static_assert(sizeof(Vertex)==sizeof(A2FO_N11_Vertex));

struct Import {
    ModelContent* content;
    std::vector<std::string>* names;
    std::unordered_map<std::string,std::size_t>* texture_indices;
    AssetReader reader;
    const std::string* model_identifier;
    std::size_t geometry_bytes=0;
    const char* failure=nullptr;
    std::string resolver_error;
};
struct GeometryTransform {
    ModelGeometryPolicy policy;
    std::string error;
};
int transform_positions(void* context,float* xyz,std::uint32_t count) noexcept {
    auto& state=*static_cast<GeometryTransform*>(context);
    return apply_model_geometry_policy(state.policy,xyz,count,state.error)?1:0;
}
int part_sink(void* context,const A2FO_N11_Part* source) noexcept {
    auto& state=*static_cast<Import*>(context);
    try {
        if(!source||!source->vertices||!source->indices||!source->vertex_count||
           !source->index_count||source->vertex_count>1048576||
           source->index_count>1048576||source->texture_name_length>1024||
           (source->texture_name_length&&!source->texture_name)) {
            state.failure="invalid model part";return 0;
        }
        if(source->texture_name_length&&std::memchr(source->texture_name,0,source->texture_name_length)) {
            state.failure="invalid texture identifier";return 0;
        }
        if(state.content->parts.size()>=8192) {state.failure="part count exceeds budget";return 0;}
        const std::size_t vertex_bytes=std::size_t(source->vertex_count)*sizeof(Vertex);
        const std::size_t index_bytes=std::size_t(source->index_count)*sizeof(std::uint32_t);
        if(vertex_bytes>geometry_limit||index_bytes>geometry_limit-vertex_bytes||
           state.geometry_bytes>geometry_limit-vertex_bytes-index_bytes) {
            state.failure="geometry exceeds budget";return 0;
        }
        for(std::uint32_t i=0;i<source->index_count;++i)
            if(source->indices[i]>=source->vertex_count) {state.failure="invalid model index";return 0;}
        ContentPart part;
        part.vertices.resize(source->vertex_count);
        std::memcpy(part.vertices.data(),source->vertices,vertex_bytes);
        part.indices.assign(source->indices,source->indices+source->index_count);
        std::memcpy(part.local_to_model,source->local_to_model,sizeof(part.local_to_model));
        std::memcpy(part.colour,source->colour,sizeof(part.colour));
        std::memcpy(part.emissive,source->emissive,sizeof(part.emissive));
        part.flags=source->flags;
        part.seed.part_index=static_cast<std::uint32_t>(state.content->parts.size());
        if(source->texture_name_length)
            part.seed.authored_texture.assign(reinterpret_cast<const char*>(source->texture_name),
                source->texture_name_length);
        std::memcpy(part.seed.colour,source->colour,sizeof(part.seed.colour));
        std::memcpy(part.seed.emissive,source->emissive,sizeof(part.seed.emissive));
        part.seed.flags=source->flags;
        if(source->texture_name_length&&!state.reader.resolve_live_material) {
            const std::string authored(reinterpret_cast<const char*>(source->texture_name),source->texture_name_length);
            std::string name=authored;
            if(state.reader.resolve_texture) {
                auto printable=[](const std::string& value) {
                    if(value.empty()||value.size()>1024)return false;
                    for(unsigned char c:value)if(c<0x20||c>0x7e)return false;
                    return true;
                };
                if(!printable(authored)) {state.failure="invalid authored texture identifier";return 0;}
                bool resolved=false;
                state.resolver_error.clear();
                try {
                    resolved=state.reader.resolve_texture(state.reader.context,*state.model_identifier,
                        static_cast<std::uint32_t>(state.content->parts.size()),authored,name,state.resolver_error);
                } catch(...) {state.failure="texture resolver threw";return 0;}
                if(!resolved) {
                    if(state.resolver_error.empty())state.failure="texture resolver failed";
                    return 0;
                }
                if(!printable(name)) {state.failure="invalid resolved texture identifier";return 0;}
            }
            auto found=state.texture_indices->find(name);
            if(found==state.texture_indices->end()) {
                part.texture_index=state.names->size();
                state.names->push_back(name);
                state.texture_indices->emplace(name,part.texture_index);
            } else part.texture_index=found->second;
        }
        state.content->parts.push_back(std::move(part));
        state.geometry_bytes+=vertex_bytes+index_bytes;
        return 1;
    } catch(...) {state.failure="model callback allocation failed";return 0;}
}
struct DetailedImport {
    const std::vector<ContentPart>* parts;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> sources;
    std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
    std::size_t next=0;
    const char* failure=nullptr;
};
int detailed_sink(void* raw,const A2FO_N11_Part* part,const A2FO_N11_PartSource* source) noexcept {
    auto& state=*static_cast<DetailedImport*>(raw);
    try {
        if(!part||!source||state.next>=state.parts->size()||
           (source->node_name_length&&!source->node_name)||source->node_name_length>259||
           (part->texture_name_length&&!part->texture_name)){
            state.failure="invalid detailed part source";return 0;
        }
        const auto& original=(*state.parts)[state.next];
        if(part->vertex_count!=original.vertices.size()||
           part->index_count!=original.indices.size()||
           part->texture_name_length!=original.seed.authored_texture.size()||
           (part->texture_name_length&&std::memcmp(part->texture_name,
               original.seed.authored_texture.data(),part->texture_name_length))||
           std::memcmp(part->colour,original.seed.colour,sizeof(original.seed.colour))||
           std::memcmp(part->emissive,original.seed.emissive,sizeof(original.seed.emissive))||
           part->flags!=original.seed.flags||
           !state.seen.insert({source->node_index,source->group_index}).second){
            state.failure="detailed part order or authored material mismatch";return 0;
        }
        for(std::uint32_t k=0;k<source->node_name_length;++k)
            if(source->node_name[k]<32||source->node_name[k]>126){
                state.failure="invalid detailed node name";return 0;
            }
        state.sources.push_back({source->node_index,source->group_index});
        ++state.next;return 1;
    }catch(...){state.failure="detailed part allocation failed";return 0;}
}
struct ImageImport {
    ContentTexture* content;
    std::size_t remaining;
    const char* failure=nullptr;
    bool called=false;
};
int texture_sink(void* context,const A2FO_N11_Texture* source) noexcept {
    auto& state=*static_cast<ImageImport*>(context);
    try {
        if(state.called||!source||!source->payload||!source->mips||!source->mip_count||
           source->reserved!=0||
           source->mip_count>15||source->payload_bytes>state.remaining) {
            state.failure="invalid texture payload or budget";return 0;
        }
        state.called=true;
        TextureMip mips[15];
        for(std::uint32_t i=0;i<source->mip_count;++i) {
            const auto& m=source->mips[i];
            mips[i]={m.width,m.height,m.row_pitch,m.slice_pitch,m.offset,m.byte_count};
        }
        TextureView view{static_cast<TextureFormat>(source->format),
            static_cast<TextureAlpha>(source->alpha_mode),source->width,source->height,
            mips,source->mip_count,
            source->payload,source->payload_bytes};
        TexturePlan plan{};
        if(!plan_texture(view,plan)) {state.failure="invalid texture layout";return 0;}
        auto& result=*state.content;
        result.format=view.format;result.alpha=view.alpha;
        result.width=view.width;result.height=view.height;
        result.mips.reserve(source->mip_count);
        for(std::uint32_t i=0;i<source->mip_count;++i) {
            const auto& m=source->mips[i];
            result.mips.push_back({m.width,m.height,m.row_pitch,m.slice_pitch,m.offset,m.byte_count});
        }
        result.bytes.assign(source->payload,source->payload+source->payload_bytes);
        return 1;
    } catch(...) {state.failure="texture callback allocation failed";return 0;}
}
}
bool load_model_content(AssetReader reader,const std::string& model_identifier,
                        ModelContent& output,std::string& error,bool all_model_parts) noexcept {
    try {
        if(!reader.read||model_identifier.empty()||model_identifier.size()>260||
           model_identifier.find('\0')!=std::string::npos) {
            error="invalid model identifier or reader";return false;
        }
        GeometryTransform geometry;
        if(reader.resolve_geometry_policy){
            if(!all_model_parts){error="native geometry requires complete all-part scope";return false;}
            if(!reader.resolve_geometry_policy(reader.context,model_identifier,geometry.policy,error)||
               !valid_model_geometry_policy(geometry.policy)){
                if(error.empty())error="invalid or unknown native model geometry policy";
                return false;
            }
        }
        std::vector<std::uint8_t> model_bytes;
        if(!reader.read(reader.context,AssetKind::model,model_identifier,model_bytes,error)) {
            if(error.empty())error="model read failed";
            return false;
        }
        if(model_bytes.empty()||model_bytes.size()>model_input_limit) {
            error="model input exceeds budget or is empty";return false;
        }
        ModelContent pending;
        std::vector<std::string> names;
        std::unordered_map<std::string,std::size_t> indices;
        Import import{&pending,&names,&indices,reader,&model_identifier,0,nullptr,{}};
        const auto parsed=reader.resolve_geometry_policy?
            a2fo_native11_parse_sod_native_geometry(model_bytes.data(),static_cast<std::uint32_t>(model_bytes.size()),
                transform_positions,&geometry,part_sink,&import):
            (all_model_parts?a2fo_native11_parse_sod_all:a2fo_native11_parse_sod)(
                model_bytes.data(),static_cast<std::uint32_t>(model_bytes.size()),part_sink,&import);
        if(parsed!=1) {
            error=!import.resolver_error.empty()?std::move(import.resolver_error):
                (!geometry.error.empty()?std::move(geometry.error):
                (import.failure?import.failure:"invalid or unsupported model"));return false;
        }
        if(pending.parts.empty()) {error="model has no static geometry";return false;}
        if(reader.resolve_live_material){
            if(!all_model_parts){error="live material requires complete all-part scope";return false;}
            DetailedImport detailed{&pending.parts,{},{},0,nullptr};
            if(a2fo_native11_inspect_sod_all(model_bytes.data(),
                static_cast<std::uint32_t>(model_bytes.size()),detailed_sink,&detailed)!=1||
                detailed.next!=pending.parts.size()){
                error=detailed.failure?detailed.failure:"detailed source traversal mismatch";return false;
            }
            for(std::size_t k=0;k<pending.parts.size();++k){
                auto& part=pending.parts[k];
                part.seed.node_index=detailed.sources[k].first;
                part.seed.group_index=detailed.sources[k].second;
                LiveMaterial selected;
                error.clear();
                if(!reader.resolve_live_material(reader.context,model_identifier,part.seed,
                    selected,error)||!valid_live_material(selected)){
                    if(error.empty())error="live material resolution failed";
                    return false;
                }
                part.live_material=selected;
                std::memcpy(part.colour,selected.colour,sizeof(part.colour));
                std::memcpy(part.emissive,selected.emissive,sizeof(part.emissive));
                part.flags=selected.flags;
                if(!selected.texture_identifier.empty()){
                    auto found=indices.find(selected.texture_identifier);
                    if(found==indices.end()){
                        part.texture_index=names.size();
                        names.push_back(selected.texture_identifier);
                        indices.emplace(selected.texture_identifier,part.texture_index);
                    }else part.texture_index=found->second;
                }
            }
        }
        std::size_t texture_bytes_used=0;
        for(const auto& name:names) {
            std::vector<std::uint8_t> encoded;
            error.clear();
            if(!reader.read(reader.context,AssetKind::texture,name,encoded,error)) {
                if(error.empty())error="texture read failed";
                return false;
            }
            if(encoded.empty()||encoded.size()>texture_input_limit) {
                error="texture input exceeds budget or is empty";return false;
            }
            ContentTexture texture;
            texture.identifier=name;
            ImageImport image{&texture,texture_payload_limit-texture_bytes_used};
            if(a2fo_native11_parse_texture(encoded.data(),static_cast<std::uint32_t>(encoded.size()),texture_sink,&image)!=1||!image.called) {
                error=image.failure?image.failure:"invalid or unsupported texture";return false;
            }
            texture_bytes_used+=texture.bytes.size();
            if(import.geometry_bytes>total_limit-texture_bytes_used) {
                error="combined content exceeds budget";return false;
            }
            pending.textures.push_back(std::move(texture));
        }
        output=std::move(pending);
        error.clear();return true;
    } catch(const std::bad_alloc&) {error="oom";return false;}
      catch(const std::exception&) {error="failed";return false;}
      catch(...) {error="failed";return false;}
}
bool load_content_texture(AssetReader reader,const std::string& identifier,
    ContentTexture& output,std::string& error) noexcept {
    try {
        if(!reader.read||identifier.empty()||identifier.size()>1024||
           identifier.find('\0')!=std::string::npos){error="invalid texture identifier or reader";return false;}
        for(unsigned char ch:identifier)if(ch<32||ch>126){error="invalid texture identifier";return false;}
        std::vector<std::uint8_t> encoded;
        if(!reader.read(reader.context,AssetKind::texture,identifier,encoded,error)){
            if(error.empty())error="texture read failed";
            return false;
        }
        if(encoded.empty()||encoded.size()>texture_input_limit){error="texture input exceeds budget or is empty";return false;}
        ContentTexture pending;pending.identifier=identifier;
        ImageImport image{&pending,texture_payload_limit};
        if(a2fo_native11_parse_texture(encoded.data(),static_cast<std::uint32_t>(encoded.size()),
            texture_sink,&image)!=1||!image.called){
            error=image.failure?image.failure:"invalid or unsupported texture";return false;
        }
        output=std::move(pending);error.clear();return true;
    }catch(...){error="texture decode allocation failure";return false;}
}
} // namespace a2fo::native11
