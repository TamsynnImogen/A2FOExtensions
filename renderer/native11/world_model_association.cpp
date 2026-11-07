#include "world_model_association.hpp"
#include <cmath>
#include <cstring>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_members=4096;
bool fail(std::string& error,const char* text) noexcept {
    try{error=text;}catch(...){}return false;
}
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,void* out,std::size_t size){
    return read&&base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
        read(context,base+offset,out,size);
}
template<class T>bool field(void* context,Read read,std::uint32_t base,std::uint32_t offset,T& out){
    return at(context,read,base,offset,&out,sizeof(out));
}
struct Entry {
    std::uint32_t node=0,next=0,prev=0,name_pointer=0;
    WorldModelAssociation value;
};
struct Pass {
    std::uint32_t container=0,sentinel=0,head=0,tail=0;
    std::vector<Entry> entries;
};
void associate(WorldModelReadProfile profile,void* context,Read read,
    void* model_context,PinWorldModel pin,Entry& item){
    auto& out=item.value;const auto instance=out.member.instance;
    if(!instance){out.status=WorldModelStatus::no_instance;return;}
    if(!field(context,read,instance,0,out.instance_vtable))return;
    if(out.instance_vtable!=profile.craft_instance_vtable){out.status=WorldModelStatus::unsupported_instance;return;}
    if(!field(context,read,instance,0x80,out.database))return;
    if(!out.database){out.status=WorldModelStatus::no_database;return;}
    if(!field(context,read,out.database,0,out.database_vtable))return;
    if(out.database_vtable!=profile.plain_database_vtable){out.status=WorldModelStatus::unsupported_database;return;}
    if(!field(context,read,out.database,0x38,item.name_pointer)||!item.name_pointer)return;
    bool terminated=false;
    for(std::uint32_t i=0;i<sizeof(out.model_name);++i){char c=0;
        if(!field(context,read,item.name_pointer,i,c))return;
        if(!c){terminated=i!=0;break;}
        if(static_cast<unsigned char>(c)<0x20||static_cast<unsigned char>(c)>0x7e||i==sizeof(out.model_name)-1)return;
        out.model_name[i]=c;
    }
    if(!terminated)return;
    if(!at(context,read,instance,0x44,out.native_placement.data(),sizeof(out.native_placement))||
       !field(context,read,instance,0x74,out.scale_active)||
       (out.scale_active&&!at(context,read,instance,0x28,out.runtime_scale.data(),sizeof(out.runtime_scale))))return;
    const auto& m=out.native_placement;
    for(float v:m)if(!std::isfinite(v)){out.status=WorldModelStatus::invalid_placement;return;}
    const double determinant=double(m[0])*(double(m[4])*m[8]-double(m[5])*m[7])-
        double(m[1])*(double(m[3])*m[8]-double(m[5])*m[6])+
        double(m[2])*(double(m[3])*m[7]-double(m[4])*m[6]);
    if(!std::isfinite(determinant)||determinant==0){out.status=WorldModelStatus::invalid_placement;return;}
    for(float v:out.runtime_scale)if(!std::isfinite(v)||v==0){out.status=WorldModelStatus::invalid_placement;return;}
    auto model=pin(model_context,out.database,out.model_name);
    if(!model||model->info.database!=out.database||!model->info.serial||model->bytes.empty()||
       model->info.byte_count!=model->bytes.size()||std::strncmp(model->info.name,out.model_name,sizeof(out.model_name))){
        out.status=WorldModelStatus::model_unavailable;return;
    }
    out.model=std::move(model);out.status=WorldModelStatus::bound_current_database;
}
bool pass(WorldModelReadProfile profile,const std::vector<WorldMember>& expected,
    void* context,Read read,void* model_context,PinWorldModel pin,Pass& out){
    if(!field(context,read,profile.object_list_pointer,0,out.container)||!out.container||
       !field(context,read,out.container,4,out.sentinel)||!out.sentinel||
       !field(context,read,out.sentinel,0,out.head)||!out.head||
       !field(context,read,out.sentinel,4,out.tail)||!out.tail)return false;
    std::set<std::uint32_t> nodes,entities,instances;
    std::uint32_t current=out.head,previous=out.sentinel;
    while(current!=out.sentinel){
        if(!current||out.entries.size()>=expected.size()||!nodes.insert(current).second)return false;
        Entry entry;entry.node=current;entry.value.member=expected[out.entries.size()];
        std::uint32_t entity=0,instance=0,klass=0;
        if(!field(context,read,current,0,entry.next)||!entry.next||
           !field(context,read,current,4,entry.prev)||entry.prev!=previous||
           !field(context,read,current,8,entity)||entity!=entry.value.member.entity||
           !entities.insert(entity).second||!field(context,read,entity,4,instance)||
           instance!=entry.value.member.instance||!field(context,read,entity,0x40,klass)||
           klass!=entry.value.member.object_class||(instance&&!instances.insert(instance).second))return false;
        associate(profile,context,read,model_context,pin,entry);
        out.entries.push_back(std::move(entry));previous=current;current=out.entries.back().next;
    }
    return previous==out.tail&&out.entries.size()==expected.size();
}
bool same(const Pass& a,const Pass& b){
    if(a.container!=b.container||a.sentinel!=b.sentinel||a.head!=b.head||a.tail!=b.tail||a.entries.size()!=b.entries.size())return false;
    for(std::size_t i=0;i<a.entries.size();++i){
        const auto& x=a.entries[i];const auto& y=b.entries[i];const auto& p=x.value;const auto& q=y.value;
        if(x.node!=y.node||x.next!=y.next||x.prev!=y.prev||x.name_pointer!=y.name_pointer||
           p.status!=q.status||p.instance_vtable!=q.instance_vtable||p.database!=q.database||p.database_vtable!=q.database_vtable||
           std::memcmp(p.model_name,q.model_name,sizeof(p.model_name))||p.scale_active!=q.scale_active||
           std::memcmp(p.native_placement.data(),q.native_placement.data(),sizeof(p.native_placement))||
           std::memcmp(p.runtime_scale.data(),q.runtime_scale.data(),sizeof(p.runtime_scale))||p.model!=q.model)return false;
    }
    return true;
}
}
bool same_world_model_associations(const WorldModelAssociations& a,const WorldModelAssociations& b) noexcept {
    if(a.members.size()!=b.members.size())return false;
    for(std::size_t i=0;i<a.members.size();++i){const auto& x=a.members[i];const auto& y=b.members[i];
        if(x.member.entity!=y.member.entity||x.member.instance!=y.member.instance||
           x.member.object_class!=y.member.object_class||x.member.class_version!=y.member.class_version||
           x.status!=y.status||x.instance_vtable!=y.instance_vtable||x.database!=y.database||
           x.database_vtable!=y.database_vtable||x.scale_active!=y.scale_active||x.model!=y.model||
           std::memcmp(x.model_name,y.model_name,sizeof(x.model_name))||
           std::memcmp(x.native_placement.data(),y.native_placement.data(),sizeof(x.native_placement))||
           std::memcmp(x.runtime_scale.data(),y.runtime_scale.data(),sizeof(x.runtime_scale)))return false;
    }return true;
}
bool capture_world_model_associations(WorldModelReadProfile profile,const std::vector<WorldMember>& expected,
    void* context,Read read,void* model_context,PinWorldModel pin,WorldModelAssociations& output,std::string& error) noexcept {
    try {
        if(!profile.object_list_pointer||!profile.craft_instance_vtable||!profile.plain_database_vtable||
           profile.craft_instance_vtable==profile.plain_database_vtable||!read||!pin||expected.size()>max_members)
            return fail(error,"missing world model association profile or capacity");
        for(const auto& m:expected)if(!m.entity||!m.object_class||!m.class_version)
            return fail(error,"invalid expected world member identity");
        Pass first,second;
        if(!pass(profile,expected,context,read,model_context,pin,first)||!pass(profile,expected,context,read,model_context,pin,second))
            return fail(error,"world model roster changed or unreadable");
        if(!same(first,second))return fail(error,"world model association changed during capture");
        WorldModelAssociations result;result.members.reserve(first.entries.size());
        for(auto& entry:first.entries)result.members.push_back(std::move(entry.value));
        output=std::move(result);error.clear();return true;
    }catch(...){return fail(error,"world model association allocation or callback failure");}
}
}
