#include "model_material_baseline_cache.hpp"
#include "model_lod_template.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
namespace a2fo::native11 {
namespace {
constexpr std::size_t hard_entries=64,hard_bytes=128u*1024u*1024u;
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
bool add(std::size_t& total,std::size_t count,std::size_t unit,std::size_t limit=hard_bytes) noexcept {
    if(!unit||total>limit||count>(limit-total)/unit)return false;
    total+=count*unit;return true;
}
bool printable(const std::string& value,std::size_t max) noexcept {
    if(value.empty()||value.size()>max)return false;
    for(unsigned char c:value)if(c<32||c>126)return false;
    return true;
}
bool model_valid(const std::shared_ptr<const ModelAsset>& model) noexcept {
    if(!model||!model->info.database||!model->info.serial||model->bytes.empty()||
       model->bytes.size()>64u*1024u*1024u||model->info.byte_count!=model->bytes.size())return false;
    const auto* end=static_cast<const char*>(std::memchr(model->info.name,0,sizeof(model->info.name)));
    if(!end||end==model->info.name)return false;
    for(const char* p=model->info.name;p<end;++p)
        if(static_cast<unsigned char>(*p)<32||static_cast<unsigned char>(*p)>126)return false;
    return true;
}
bool texture_valid(const std::shared_ptr<const TextureAsset>& texture) noexcept {
    return texture&&texture->texture&&texture->serial&&
        printable(texture->logical_name,1024)&&texture->bytes&&
        !texture->bytes->empty()&&texture->bytes->size()<=64u*1024u*1024u;
}
bool metadata(const ModelMaterialBaseline& snapshot,std::size_t& output) {
    if(!model_valid(snapshot.model)||!snapshot.authored||!snapshot.nodes||!snapshot.data)return false;
    const auto count=snapshot.nodes->nodes.size();
    if(!count||count>4096||snapshot.authored->nodes.size()!=count||!snapshot.nodes->root_address)return false;
    const auto role=[count](std::uint32_t index){return index==UINT32_MAX||index<count;};
    if(!role(snapshot.nodes->hardpoints)||!role(snapshot.nodes->damage)||!role(snapshot.nodes->lights))return false;
    std::size_t roots=0,mesh_count=0;std::uint32_t root=UINT32_MAX;
    std::set<std::uint32_t> addresses;std::set<std::string> names;
    std::size_t total=0;
    if(!add(total,1,sizeof(ModelMaterialBaselineData))||
       !add(total,snapshot.data->meshes.size(),sizeof(ModelMeshMaterialBaseline))||
       !add(total,1,sizeof(AuthoredPoseData))||
       !add(total,snapshot.authored->nodes.size(),sizeof(AuthoredPoseNode))||
       !add(total,snapshot.authored->channels.size(),sizeof(AuthoredPoseChannel))||
       !add(total,1,sizeof(ModelNodeTemplateData))||
       !add(total,count,sizeof(ModelNodeTemplateNode)))return false;
    for(std::size_t k=0;k<count;++k){const auto& node=snapshot.nodes->nodes[k];
        const auto& authored=snapshot.authored->nodes[k];
        if(!node.address||!node.vtable||!node.type_function||!node.name_pointer||
           !addresses.insert(node.address).second||!printable(node.name,259)||
           !names.insert(node.name).second||
           node.name!=authored.name||node.parent!=authored.parent||!role(node.parent)||
           !role(node.first_child)||!role(node.next_sibling)||
           node.parent_address!=(node.parent==UINT32_MAX?0:snapshot.nodes->nodes[node.parent].address)||
           node.child_address!=(node.first_child==UINT32_MAX?0:snapshot.nodes->nodes[node.first_child].address)||
           node.sibling_address!=(node.next_sibling==UINT32_MAX?0:snapshot.nodes->nodes[node.next_sibling].address)||
           !add(total,node.name.size(),1)||!add(total,authored.name.size(),1))return false;
        if(node.parent==UINT32_MAX){++roots;root=static_cast<std::uint32_t>(k);
            if(node.address!=snapshot.nodes->root_address)return false;}
        if(authored.kind==1)++mesh_count;
    }
    if(roots!=1||mesh_count!=snapshot.data->meshes.size())return false;
    if(snapshot.nodes->nodes[root].next_sibling!=UINT32_MAX)return false;
    std::vector<std::uint8_t> seen(count);std::vector<std::uint32_t> pending{root};
    std::size_t visited=0;
    while(!pending.empty()){const auto index=pending.back();pending.pop_back();
        if(index>=count||seen[index]++)return false;
        ++visited;const auto& node=snapshot.nodes->nodes[index];
        if(node.next_sibling!=UINT32_MAX){
            if(snapshot.nodes->nodes[node.next_sibling].parent!=node.parent)return false;
            pending.push_back(node.next_sibling);
        }
        if(node.first_child!=UINT32_MAX){
            if(snapshot.nodes->nodes[node.first_child].parent!=index)return false;
            pending.push_back(node.first_child);
        }
    }
    if(visited!=count)return false;
    for(const auto& channel:snapshot.authored->channels)
        if(!add(total,channel.keys.size(),sizeof(float)))return false;
    if(snapshot.nodes->lods){
        if(!add(total,1,sizeof(ModelLodTemplateData))||
           !add(total,snapshot.nodes->lods->controls.size(),sizeof(ModelLodControl)))return false;
        for(const auto& control:snapshot.nodes->lods->controls)
            if(!add(total,control.levels.size(),sizeof(ModelLodLevel)))return false;
    }
    std::set<std::uint32_t> mesh_indices,texture_addresses;
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> owners;
    const auto check_texture=[&](const std::shared_ptr<const TextureAsset>& asset){
        if(!asset)return true;
        if(!texture_valid(asset))return false;
        const auto existing=owners.emplace(asset->texture,asset);
        return existing.second||existing.first->second==asset;
    };
    for(const auto& mesh:snapshot.data->meshes){
        if(mesh.node_index>=count||!mesh_indices.insert(mesh.node_index).second||
           snapshot.authored->nodes[mesh.node_index].kind!=1||
           mesh.node_address!=snapshot.nodes->nodes[mesh.node_index].address||
           mesh.slots.size()>8||(mesh.slots.size()&&!mesh.slot_vector)||
           !add(total,mesh.slots.size(),sizeof(std::shared_ptr<const TextureAsset>)))return false;
        for(const auto& asset:mesh.slots)if(!check_texture(asset))return false;
        if(!check_texture(mesh.borg_alternate))return false;
    }
    for(const auto& [pointer,asset]:owners){
        if(!texture_addresses.insert(pointer).second||
           !add(total,1,sizeof(TextureAsset))||
           !add(total,asset->logical_name.size()+asset->resolved_path.size()+
                      asset->requested_path.size(),1))return false;
    }
    output=total;return true;
}
template<class Entries>bool total_bytes(const Entries& entries,std::size_t limit,std::size_t& output){
    std::size_t total=0;std::set<const std::vector<std::uint8_t>*> held_bytes;
    for(const auto& entry:entries){
        if(!add(total,entry.metadata,1,limit))return false;
        for(const auto& mesh:entry.data->meshes){
            const auto count=[&](const std::shared_ptr<const TextureAsset>& asset){
                return !asset||!held_bytes.insert(asset->bytes.get()).second||
                    add(total,asset->bytes->size(),1,limit);
            };
            for(const auto& asset:mesh.slots)if(!count(asset))return false;
            if(!count(mesh.borg_alternate))return false;
        }
    }output=total;return true;
}
}
ModelMaterialBaselineCache::ModelMaterialBaselineCache(std::size_t max_entries,
    std::size_t max_bytes) noexcept :
    max_entries_(std::min(max_entries,hard_entries)),max_bytes_(std::min(max_bytes,hard_bytes)) {}
void ModelMaterialBaselineCache::prune() const noexcept {
    for(auto it=entries_.begin();it!=entries_.end();)
        if(it->model.expired())it=entries_.erase(it);else ++it;
    try {std::size_t total=0;
        bytes_=total_bytes(entries_,max_bytes_,total)?total:max_bytes_;
    }catch(...){bytes_=max_bytes_;}
}
bool ModelMaterialBaselineCache::publish(std::shared_ptr<const ModelMaterialBaseline> snapshot,
    std::string& error) noexcept {
    prune();
    try {
        std::size_t cost=0;
        if(!snapshot||!metadata(*snapshot,cost))return fail(error,"invalid model material baseline or metadata");
        for(const auto& entry:entries_){
            if(entry.serial!=snapshot->model->info.serial)continue;
            const auto owner=entry.model.lock();
            if(owner!=snapshot->model)return fail(error,"model serial collides with another live owner");
            if(entry.authored!=snapshot->authored||entry.nodes!=snapshot->nodes||entry.data!=snapshot->data)
                return fail(error,"live material baseline tuple cannot be replaced");
            error.clear();return true;
        }
        if(entries_.size()>=max_entries_)return fail(error,"material baseline cache entry capacity exceeded");
        Entry candidate;candidate.serial=snapshot->model->info.serial;
        candidate.model=snapshot->model;candidate.authored=snapshot->authored;
        candidate.nodes=snapshot->nodes;candidate.data=snapshot->data;candidate.metadata=cost;
        entries_.push_back(std::move(candidate));
        std::size_t total=0;
        try {if(!total_bytes(entries_,max_bytes_,total)){
                entries_.pop_back();return fail(error,"material baseline cache byte capacity exceeded");
            }}catch(...){entries_.pop_back();throw;}
        bytes_=total;error.clear();return true;
    }catch(...){return fail(error,"material baseline cache allocation failure");}
}
bool ModelMaterialBaselineCache::find(std::shared_ptr<const ModelAsset> model,
    std::shared_ptr<const ModelMaterialBaseline>& output,std::string& error) const noexcept {
    prune();
    try {
        if(!model_valid(model))return fail(error,"invalid model material lookup owner");
        for(const auto& entry:entries_){
            if(entry.serial!=model->info.serial)continue;
            if(entry.model.lock()!=model)return fail(error,"model serial collides with another live owner");
            auto result=std::make_shared<ModelMaterialBaseline>();
            result->model=std::move(model);result->authored=entry.authored;
            result->nodes=entry.nodes;result->data=entry.data;
            output=std::move(result);error.clear();return true;
        }
        return fail(error,"model material baseline cache miss");
    }catch(...){return fail(error,"material baseline lookup allocation failure");}
}
void ModelMaterialBaselineCache::clear() noexcept {entries_.clear();bytes_=0;}
std::size_t ModelMaterialBaselineCache::size() const noexcept {prune();return entries_.size();}
std::size_t ModelMaterialBaselineCache::bytes() const noexcept {prune();return bytes_;}
} // namespace a2fo::native11
