#include "owned_part_states.hpp"
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_nodes=4096,max_parts=8192,max_metadata=1024u*1024u;
constexpr std::size_t max_single_bytes=64u*1024u*1024u,max_total_bytes=128u*1024u*1024u;
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
bool printable(const std::string& name,bool nonempty,std::size_t max) noexcept {
    if((nonempty&&name.empty())||name.size()>max)return false;
    for(unsigned char c:name)if(c<0x20||c>0x7e)return false;
    return true;
}
bool copied_name(const char* data,std::size_t capacity,std::string& name) {
    const auto* end=static_cast<const char*>(std::memchr(data,0,capacity));
    if(!end||end==data)return false;
    name.assign(data,end);
    return printable(name,true,capacity-1);
}
}
bool bind_owned_part_states(const Object& object,const ModelPoseSnapshot& snapshot,
    const ModelMaterialSource& source,const OwnedAnimationPose& pose,
    const std::vector<std::uint8_t>& visible,
    const MaterialObservation& selected,
    std::shared_ptr<const MaterialObservation>& output,std::string& error) noexcept {
    try {
        std::string object_name,asset_name;
        if(!snapshot.model||!snapshot.pose||source.model!=snapshot.model||
           selected.model!=snapshot.model||!object.instance||!object.database||
           object.model_identity!=ModelIdentity::plain_database_name||
           !object.model_serial||snapshot.model->info.database!=object.database||
           snapshot.model->info.serial!=object.model_serial||
           !copied_name(object.model_name,sizeof(object.model_name),object_name)||
           !copied_name(snapshot.model->info.name,sizeof(snapshot.model->info.name),asset_name)||
           object_name!=asset_name||
           pose.instance!=object.instance||pose.database!=object.database||
           pose.model_serial!=object.model_serial||
           selected.instance!=object.instance||selected.database!=object.database||
           selected.model_serial!=object.model_serial||
           snapshot.model->bytes.empty()||
           snapshot.model->bytes.size()>max_single_bytes||
           snapshot.model->bytes.size()!=snapshot.model->info.byte_count||
           snapshot.pose->nodes.size()>max_nodes||
           pose.local.size()!=snapshot.pose->nodes.size()||
           pose.scales.size()!=snapshot.pose->nodes.size()||
           pose.local_to_model.size()!=snapshot.pose->nodes.size()||
           visible.size()!=snapshot.pose->nodes.size()||
           source.parts.empty()||source.parts.size()>max_parts||
           selected.parts.size()!=source.parts.size())
            return fail(error,"invalid owned part identity or count");
        for(std::size_t i=0;i<visible.size();++i){
            if(visible[i]>1)return fail(error,"invalid per-node visibility");
            PartState state;state.visible=false;
            std::memcpy(state.local_to_model,pose.local_to_model[i].data(),
                        sizeof(state.local_to_model));
            if(!valid_part_state(state))return fail(error,"invalid owned affine pose");
        }
        std::size_t metadata=0,total_bytes=snapshot.model->bytes.size();
        std::map<std::uint64_t,std::shared_ptr<const TextureAsset>> textures;
        std::pair<std::uint32_t,std::uint32_t> previous{};
        bool have_previous=false;
        for(std::size_t i=0;i<source.parts.size();++i){
            const auto& part=source.parts[i];const auto& binding=selected.parts[i];
            if(part.node_index>=snapshot.pose->nodes.size()||
               snapshot.pose->nodes[part.node_index].kind!=1||
               part.group_index>=snapshot.pose->nodes[part.node_index].group_count||
               part.node_name!=snapshot.pose->nodes[part.node_index].name||
               !printable(part.node_name,true,255)||
               !printable(part.authored_texture,false,1024)||
               !printable(binding.authored_texture,false,1024)||
               part.authored_texture!=binding.authored_texture||
               part.node_name.size()+part.authored_texture.size()>max_metadata-metadata)
                return fail(error,"invalid authored part metadata or material match");
            const auto key=std::make_pair(part.node_index,part.group_index);
            if(have_previous&&!(previous<key))
                return fail(error,"duplicate or unordered authored groups");
            previous=key;have_previous=true;
            metadata+=part.node_name.size()+part.authored_texture.size();
            PartState state;state.visible=visible[part.node_index]!=0;
            std::memcpy(state.local_to_model,pose.local_to_model[part.node_index].data(),
                        sizeof(state.local_to_model));
            if(!valid_part_state(state))
                return fail(error,"invalid visible mesh part transform");
            if(static_cast<bool>(binding.texture)!=!binding.authored_texture.empty())
                return fail(error,"selected texture presence mismatch");
            if(binding.texture){
                const auto& texture=binding.texture;
                if(!texture->serial||!texture->texture||texture->flags||texture->colour_key||
                   !printable(texture->logical_name,true,1024)||!texture->bytes||
                   texture->bytes->empty()||texture->bytes->size()>max_single_bytes)
                    return fail(error,"invalid selected texture owner");
                auto [found,inserted]=textures.emplace(texture->serial,texture);
                if(!inserted&&found->second!=texture)
                    return fail(error,"selected texture serial collision");
                if(inserted){
                    if(textures.size()>256||texture->bytes->size()>max_total_bytes-total_bytes)
                        return fail(error,"selected texture budget exceeded");
                    total_bytes+=texture->bytes->size();
                }
            }
        }
        auto fresh=std::make_shared<MaterialObservation>(selected);
        for(std::size_t i=0;i<source.parts.size();++i){
            auto& binding=fresh->parts[i];
            const auto node=source.parts[i].node_index;
            binding.node_state_valid=true;
            binding.node_state.visible=visible[node]!=0;
            std::memcpy(binding.node_state.local_to_model,
                        pose.local_to_model[node].data(),
                        sizeof(binding.node_state.local_to_model));
        }
        output=std::move(fresh);error.clear();return true;
    }catch(...){return fail(error,"owned part binding allocation failure");}
}
} // namespace a2fo::native11
