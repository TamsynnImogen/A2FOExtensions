#include "model_node_template_cache.hpp"
#include "model_lod_template.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace a2fo::native11 {
namespace {
constexpr std::size_t hard_entries=64,hard_bytes=64u*1024u*1024u;
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
bool add(std::size_t& total,std::size_t count,std::size_t unit,
         std::size_t limit=hard_bytes) noexcept {
    if(!unit||total>limit||count>(limit-total)/unit)return false;
    total+=count*unit;return true;
}
bool copied_name(const char* name,std::size_t capacity) noexcept {
    const auto* end=static_cast<const char*>(std::memchr(name,0,capacity));
    if(!end||end==name)return false;
    for(const char* p=name;p<end;++p){const auto c=static_cast<unsigned char>(*p);
        if(c<32||c>126)return false;}
    return true;
}
bool role(std::uint32_t id,std::size_t count) noexcept {
    return id==UINT32_MAX||id<count;
}
bool valid_lods(const ModelNodeTemplate& snapshot) noexcept {
    if(!snapshot.data->lods)return true;
    const auto& controls=snapshot.data->lods->controls;
    const auto count=snapshot.data->nodes.size();
    if(controls.size()>count)return false;
    std::uint8_t seen_controls[4096]{},seen_levels[4096]{};
    std::size_t total_levels=0;
    for(const auto& control:controls){
        if(control.node_index>=count||seen_controls[control.node_index]++||
           !std::isfinite(control.radius)||control.radius<0||
           control.levels.size()>4096-total_levels)return false;
        for(float value:control.centre)if(!std::isfinite(value))return false;
        total_levels+=control.levels.size();
        for(const auto& level:control.levels){
            if(level.node_index>=count||seen_levels[level.node_index]++||
               snapshot.data->nodes[level.node_index].parent!=control.node_index)
                return false;
        }
    }
    return true;
}
bool metadata_bytes(const ModelNodeTemplate& snapshot,std::size_t& output) noexcept {
    std::size_t total=0;
    if(!add(total,1,sizeof(ModelNodeTemplateData))||
       !add(total,snapshot.data->nodes.size(),sizeof(ModelNodeTemplateNode))||
       !add(total,1,sizeof(AuthoredPoseData))||
       !add(total,snapshot.authored->nodes.size(),sizeof(AuthoredPoseNode))||
       !add(total,snapshot.authored->channels.size(),sizeof(AuthoredPoseChannel)))return false;
    for(const auto& node:snapshot.data->nodes)
        if(!add(total,node.name.size(),1))return false;
    for(const auto& node:snapshot.authored->nodes)
        if(!add(total,node.name.size(),1))return false;
    for(const auto& channel:snapshot.authored->channels)
        if(!add(total,channel.keys.size(),sizeof(float)))return false;
    if(snapshot.data->lods){
        if(!add(total,1,sizeof(ModelLodTemplateData))||
           !add(total,snapshot.data->lods->controls.size(),sizeof(ModelLodControl)))return false;
        for(const auto& control:snapshot.data->lods->controls)
            if(!add(total,control.levels.size(),sizeof(ModelLodLevel)))return false;
    }
    output=total;return true;
}
bool basic_valid(const ModelNodeTemplate& snapshot,std::size_t& bytes) noexcept {
    if(!snapshot.model||!snapshot.authored||!snapshot.data)return false;
    const auto& model=*snapshot.model;
    const auto count=snapshot.data->nodes.size();
    if(!model.info.database||!model.info.serial||
       !copied_name(model.info.name,sizeof(model.info.name))||
       !model.info.byte_count||model.info.byte_count!=model.bytes.size()||
       model.bytes.size()>hard_bytes||!count||count>4096||
       snapshot.authored->nodes.size()!=count||!snapshot.data->root_address||
       !role(snapshot.data->hardpoints,count)||!role(snapshot.data->damage,count)||
       !role(snapshot.data->lights,count))return false;
    bool root_mapped=false;
    for(std::size_t i=0;i<count;++i)
        if(snapshot.data->nodes[i].address==snapshot.data->root_address&&
           snapshot.data->nodes[i].parent==UINT32_MAX&&
           snapshot.authored->nodes[i].parent==UINT32_MAX)root_mapped=true;
    return root_mapped&&valid_lods(snapshot)&&metadata_bytes(snapshot,bytes);
}
}
ModelNodeTemplateCache::ModelNodeTemplateCache(std::size_t max_entries,
    std::size_t max_bytes) noexcept :
    max_entries_(std::min(max_entries,hard_entries)),
    max_bytes_(std::min(max_bytes,hard_bytes)) {}

void ModelNodeTemplateCache::prune() const noexcept {
    for(auto it=entries_.begin();it!=entries_.end();){
        if(it->model.expired()){bytes_-=it->bytes;it=entries_.erase(it);}
        else ++it;
    }
}
bool ModelNodeTemplateCache::publish(std::shared_ptr<const ModelNodeTemplate> snapshot,
    std::string& error) noexcept {
    prune();
    try {
        std::size_t cost=0;
        if(!snapshot||!basic_valid(*snapshot,cost))
            return fail(error,"invalid model node template snapshot or metadata budget");
        for(const auto& entry:entries_){
            if(entry.serial!=snapshot->model->info.serial)continue;
            const auto owner=entry.model.lock();
            if(owner!=snapshot->model)
                return fail(error,"model serial collides with another live owner");
            if(entry.authored!=snapshot->authored||entry.data!=snapshot->data)
                return fail(error,"model template owner tuple cannot be replaced");
            error.clear();return true;
        }
        if(entries_.size()>=max_entries_||cost>max_bytes_-bytes_)
            return fail(error,"model node template cache capacity exceeded");
        Entry pending;pending.serial=snapshot->model->info.serial;
        pending.model=snapshot->model;pending.authored=snapshot->authored;
        pending.data=snapshot->data;pending.bytes=cost;
        entries_.push_back(std::move(pending));bytes_+=cost;
        error.clear();return true;
    }catch(...){return fail(error,"model node template cache allocation failed");}
}
bool ModelNodeTemplateCache::find(std::shared_ptr<const ModelAsset> model,
    std::shared_ptr<const ModelNodeTemplate>& out,std::string& error) const noexcept {
    prune();
    try {
        if(!model||!model->info.serial)return fail(error,"invalid model lookup owner");
        for(const auto& entry:entries_){
            if(entry.serial!=model->info.serial)continue;
            const auto owner=entry.model.lock();
            if(owner!=model)return fail(error,"model serial collides with another live owner");
            auto result=std::make_shared<const ModelNodeTemplate>(
                ModelNodeTemplate{std::move(model),entry.authored,entry.data});
            out=std::move(result);error.clear();return true;
        }
        return fail(error,"model node template cache miss");
    }catch(...){return fail(error,"model node template lookup allocation failed");}
}
void ModelNodeTemplateCache::clear() noexcept {prune();entries_.clear();bytes_=0;}
std::size_t ModelNodeTemplateCache::size() const noexcept {prune();return entries_.size();}
std::size_t ModelNodeTemplateCache::bytes() const noexcept {prune();return bytes_;}
} // namespace a2fo::native11
