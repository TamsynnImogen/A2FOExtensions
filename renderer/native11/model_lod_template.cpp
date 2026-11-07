#include "model_lod_template.hpp"
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_nodes=4096,max_levels=4096;
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,
        void* output,std::size_t size) {
    return read&&base&&size&&offset<=UINT32_MAX-base&&
        size-1<=UINT32_MAX-base-offset&&read(context,base+offset,output,size);
}
template<class T> bool field(void* context,Read read,std::uint32_t base,
                             std::uint32_t offset,T& output) {
    return at(context,read,base,offset,&output,sizeof(output));
}
struct ObservedLevel {
    std::uint32_t list_node=0,next=0,prev=0,payload=0,node=0,faces=0,index=UINT32_MAX;
};
struct ObservedControl {
    ModelLodControl value;
    std::uint32_t address=0,vtable=0,type_function=0,owner=0;
    std::uint32_t sentinel=0,count=0,head=0,tail=0;
    std::vector<ObservedLevel> levels;
};
struct Pass {std::vector<ObservedControl> controls;};
bool same(const Pass& a,const Pass& b) noexcept {
    if(a.controls.size()!=b.controls.size())return false;
    for(std::size_t i=0;i<a.controls.size();++i){
        const auto& x=a.controls[i];const auto& y=b.controls[i];
        if(x.value.node_index!=y.value.node_index||
           std::memcmp(x.value.centre.data(),y.value.centre.data(),sizeof(x.value.centre))||
           std::memcmp(&x.value.radius,&y.value.radius,sizeof(float))||
           x.address!=y.address||x.vtable!=y.vtable||
           x.type_function!=y.type_function||x.owner!=y.owner||
           x.sentinel!=y.sentinel||x.count!=y.count||x.head!=y.head||x.tail!=y.tail||
           x.levels.size()!=y.levels.size())return false;
        for(std::size_t j=0;j<x.levels.size();++j){
            const auto& p=x.levels[j];const auto& q=y.levels[j];
            if(p.list_node!=q.list_node||p.next!=q.next||p.prev!=q.prev||
               p.payload!=q.payload||p.node!=q.node||p.faces!=q.faces||p.index!=q.index)
                return false;
        }
    }
    return true;
}
bool read_pass(const ModelNodeTemplate& baseline,std::uint32_t lod_type,
    const std::map<std::uint32_t,std::uint32_t>& indices,
    void* context,Read read,Pass& pass) {
    const auto db=baseline.model->info.database;
    std::size_t total=0;
    for(std::size_t i=0;i<baseline.data->nodes.size();++i){
        const auto& template_node=baseline.data->nodes[i];
        if(template_node.type_function!=lod_type)continue;
        ObservedControl item;
        item.value.node_index=static_cast<std::uint32_t>(i);
        item.address=template_node.address;
        if(!field(context,read,item.address,0,item.vtable)||
           item.vtable!=template_node.vtable||
           !field(context,read,item.vtable,4,item.type_function)||
           item.type_function!=lod_type||
           !field(context,read,item.address,4,item.owner)||item.owner!=db||
           !field(context,read,item.address,0xc8,item.sentinel)||!item.sentinel||
           !field(context,read,item.address,0xcc,item.count)||
           item.count>max_levels-total||
           !at(context,read,item.address,0xd0,item.value.centre.data(),12)||
           !field(context,read,item.address,0xdc,item.value.radius)||
           !field(context,read,item.sentinel,0,item.head)||!item.head||
           !field(context,read,item.sentinel,4,item.tail)||!item.tail)return false;
        for(float value:item.value.centre)if(!std::isfinite(value))return false;
        if(!std::isfinite(item.value.radius)||item.value.radius<0)return false;
        std::set<std::uint32_t> list_nodes,payloads,level_indices;
        std::uint32_t current=item.head,previous=item.sentinel;
        while(current!=item.sentinel){
            if(!current||item.levels.size()>=item.count||
               !list_nodes.insert(current).second)return false;
            ObservedLevel level;level.list_node=current;
            if(!field(context,read,current,0,level.next)||!level.next||
               !field(context,read,current,4,level.prev)||level.prev!=previous||
               !field(context,read,current,8,level.payload)||!level.payload||
               !payloads.insert(level.payload).second||
               !field(context,read,level.payload,0,level.node)||!level.node||
               !field(context,read,level.payload,4,level.faces))return false;
            const auto found=indices.find(level.node);
            if(found==indices.end()||
               baseline.data->nodes[found->second].parent!=i||
               !level_indices.insert(found->second).second)return false;
            level.index=found->second;
            item.value.levels.push_back({level.index,level.faces});
            item.levels.push_back(level);
            previous=current;current=level.next;
        }
        if(item.levels.size()!=item.count||previous!=item.tail||
           (item.count==0&&(item.head!=item.sentinel||item.tail!=item.sentinel))||
           (item.count!=0&&(item.head==item.sentinel||item.tail==item.sentinel)))return false;
        total+=item.levels.size();
        pass.controls.push_back(std::move(item));
    }
    return true;
}
} // namespace
bool capture_model_lod_template(const ModelNodeTemplate& baseline,std::uint32_t lod_type_function,
    void* context,Read read,std::shared_ptr<const ModelLodTemplateData>& output,
    std::string& error) noexcept {
    try {
        if(!baseline.model||!baseline.authored||!baseline.data||!read||!lod_type_function||
           !baseline.model->info.database||!baseline.model->info.serial||
           baseline.data->nodes.empty()||baseline.data->nodes.size()>max_nodes||
           baseline.authored->nodes.size()!=baseline.data->nodes.size())
            return fail(error,"invalid LOD baseline or reader");
        std::map<std::uint32_t,std::uint32_t> indices;
        for(std::size_t i=0;i<baseline.data->nodes.size();++i){
            const auto& node=baseline.data->nodes[i];
            if(!node.address||!indices.emplace(node.address,static_cast<std::uint32_t>(i)).second)
                return fail(error,"invalid LOD baseline node addresses");
        }
        Pass first,second;
        if(!read_pass(baseline,lod_type_function,indices,context,read,first)||
           !read_pass(baseline,lod_type_function,indices,context,read,second))
            return fail(error,"invalid or unreadable LOD controls");
        if(!same(first,second))return fail(error,"LOD controls changed during capture");
        auto data=std::make_shared<ModelLodTemplateData>();
        data->controls.reserve(first.controls.size());
        for(auto& item:first.controls)data->controls.push_back(std::move(item.value));
        output=std::move(data);error.clear();return true;
    }catch(...){return fail(error,"LOD control allocation or read failure");}
}
} // namespace a2fo::native11
