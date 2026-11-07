#include "craft_visibility_inputs.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <utility>

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {
    try{error=why;}catch(...){error.clear();}return false;
}
bool name(const char* text,std::size_t capacity,std::string& result){
    const auto* end=static_cast<const char*>(std::memchr(text,0,capacity));
    if(!end||end==text)return false;
    result.assign(text,end);
    for(unsigned char c:result)if(c<32||c>126)return false;
    return true;
}
struct Reader {
    void* context;Read read;
    bool bytes(std::uint32_t base,std::uint32_t offset,void* out,std::size_t size) const {
        return read&&base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
            read(context,base+offset,out,size);
    }
    template<class T>bool get(std::uint32_t base,std::uint32_t offset,T& out)const{
        return bytes(base,offset,&out,sizeof(out));
    }
    bool string(std::uint32_t pointer,const std::string& expected)const{
        if(!pointer||expected.empty()||expected.size()>259)return false;
        for(std::uint32_t i=0;i<=expected.size();++i){char c=0;
            if(!get(pointer,i,c)||c!=(i==expected.size()?0:expected[i]))return false;
        }
        return true;
    }
};
struct Pass {
    std::shared_ptr<const LightClassRecord> owner;
    std::uint32_t engine=0,context=0,quality=0,primary=0;
    std::array<std::uint32_t,3> database_roles{};
    std::array<std::uint32_t,7> class_roles{}; // damage parent, five systems, assimilation
    std::array<std::uint8_t,5> systems{};
    std::uint8_t hidden=0,assimilated=0;
    float hull=1;
    std::vector<std::uint32_t> name_pointers;
};
constexpr std::array<std::uint32_t,5> class_system_offsets{{0x40c,0x418,0x410,0x41c,0x414}};
constexpr std::array<std::uint32_t,5> instance_system_offsets{{0x9c,0x9d,0x9e,0xa0,0x9f}};
bool capture_pass(const detail::CraftVisibilitySubject& object,const WorldMember& member,
    const std::shared_ptr<const LightClassRecord>& owner,const ModelNodeTemplate& baseline,
    const std::map<std::uint32_t,std::uint32_t>& indices,CraftVisibilityReadProfile profile,
    Reader reader,void* class_context,PinLightClass pin,const PreparedCraftState* prepared,Pass& out,std::string& error){
    out.owner=pin(class_context,member.object_class);
    if(out.owner!=owner)return fail(error,"Craft class owner changed before read");
    std::uint32_t instance=0,object_class=0,vtable=0,database=0,name_pointer=0,root=0;
    if(!reader.get(object.entity,4,instance)||instance!=object.instance||
       !reader.get(object.entity,0x40,object_class)||object_class!=member.object_class||
       !reader.get(instance,0,vtable)||vtable!=profile.instance_vtable||
       !reader.get(instance,0x80,database)||database!=object.database||
       !reader.get(instance,0x98,object_class)||object_class!=member.object_class||
       !reader.get(database,0,vtable)||vtable!=profile.database_vtable||
       !reader.get(database,0x38,name_pointer)||!reader.string(name_pointer,baseline.model->info.name)||
       !reader.get(database,0x3c,root)||root!=baseline.data->root_address)
        return fail(error,"Craft object, database or class identity changed");
    out.name_pointers.push_back(name_pointer);
    if(!reader.get(profile.engine_pointer,0,out.engine)||!out.engine||
       !reader.get(out.engine,0x104,out.context)||out.context!=profile.render_context||
       !reader.get(out.context,0x40,out.quality)||
       !reader.get(instance,0x90,out.hidden)||out.hidden>1||
       !reader.get(object_class,0x1dc,out.primary)||!out.primary)
        return fail(error,"Craft context, class database or hidden state unavailable");
    for(std::size_t i=0;i<3;++i){
        if(!reader.get(database,0x2c+static_cast<std::uint32_t>(i)*4,out.database_roles[i]))
            return fail(error,"Craft database roles unavailable");
        const std::array<std::uint32_t,3> roles{{baseline.data->hardpoints,baseline.data->damage,baseline.data->lights}};
        const auto expected=roles[i]==UINT32_MAX?0:baseline.data->nodes[roles[i]].address;
        if(out.database_roles[i]!=expected)return fail(error,"Craft database role changed since load");
    }
    // Validate static topology and ownership, excluding mutable flags, matrices
    // and scales. Damage recursion relies on the complete current tree order.
    for(const auto& node:baseline.data->nodes){
        std::uint32_t node_owner=0,parent=0,child=0,sibling=0,type=0;
        if(!reader.get(node.address,0,vtable)||vtable!=node.vtable||
           !reader.get(vtable,4,type)||type!=node.type_function||
           !reader.get(node.address,4,node_owner)||node_owner!=database||
           !reader.get(node.address,8,name_pointer)||!reader.string(name_pointer,node.name)||
           !reader.get(node.address,0x18,parent)||parent!=node.parent_address||
           !reader.get(node.address,0x1c,child)||child!=node.child_address||
           !reader.get(node.address,0x20,sibling)||sibling!=node.sibling_address)
            return fail(error,"Craft node topology or owner differs from load baseline");
        out.name_pointers.push_back(name_pointer);
    }
    if(out.primary==database){
        if(!reader.get(object_class,0x408,out.class_roles[0])||
           !reader.get(object_class,0x420,out.class_roles[6]))
            return fail(error,"Craft damage or assimilation role unavailable");
        if(prepared){out.assimilated=prepared->borg_variant?1:0;out.hull=prepared->hull_fraction;}
        else if(!reader.get(instance,0xa1,out.assimilated)||out.assimilated>1||
                !reader.get(instance,0xa4,out.hull))
            return fail(error,"Craft stored damage or assimilation input unavailable");
        if(!std::isfinite(out.hull)||out.hull<0)return fail(error,"invalid Craft hull fraction");
        for(std::size_t i=0;i<5;++i){
            if(!reader.get(object_class,class_system_offsets[i],out.class_roles[i+1]))
                return fail(error,"Craft system visibility role unavailable");
            if(prepared)out.systems[i]=prepared->systems_hidden[i]?1:0;
            else if(!reader.get(instance,instance_system_offsets[i],out.systems[i])||out.systems[i]>1)
                return fail(error,"Craft stored system visibility input unavailable");
        }
        for(auto role:out.class_roles)if(role&&!indices.count(role))
            return fail(error,"Craft class role is outside the captured model tree");
    }
    if(pin(class_context,member.object_class)!=owner)
        return fail(error,"Craft class owner changed after read");
    return true;
}
bool same(const Pass& a,const Pass& b)noexcept{
    return a.owner==b.owner&&a.engine==b.engine&&a.context==b.context&&a.quality==b.quality&&
        a.primary==b.primary&&a.database_roles==b.database_roles&&a.class_roles==b.class_roles&&
        a.systems==b.systems&&a.hidden==b.hidden&&a.assimilated==b.assimilated&&
        std::memcmp(&a.hull,&b.hull,sizeof(float))==0&&a.name_pointers==b.name_pointers;
}
}
bool detail::capture_craft_visibility_inputs(const CraftVisibilitySubject& object,const WorldMember& member,
    std::shared_ptr<const LightClassRecord> owner,std::shared_ptr<const ModelNodeTemplate> baseline,
    CraftVisibilityReadProfile profile,const PreparedCraftState* prepared,void* context,Read read,
    void* class_context,PinLightClass pin,CapturedCraftVisibility& output,std::string& error)noexcept{
    try{
        std::string object_name,model_name;
        if(!read||!pin||!profile.instance_vtable||!profile.database_vtable||
           !profile.engine_pointer||!profile.render_context||!owner||!baseline||
           !baseline->model||!baseline->authored||!baseline->data||
           !object.entity||!object.instance||!object.database||!object.model_serial||
           object.instance_vtable!=profile.instance_vtable||object.database_vtable!=profile.database_vtable||
           object.model_identity!=ModelIdentity::plain_database_name||
           member.entity!=object.entity||member.instance!=object.instance||!member.object_class||
           owner->class_id!=member.object_class||!member.class_version||owner->version!=member.class_version||
           baseline->model->info.database!=object.database||baseline->model->info.serial!=object.model_serial||
           baseline->model->bytes.empty()||baseline->model->bytes.size()>64u*1024u*1024u||
           baseline->model->bytes.size()!=baseline->model->info.byte_count||
           !name(object.model_name,sizeof(object.model_name),object_name)||
           !name(baseline->model->info.name,sizeof(baseline->model->info.name),model_name)||object_name!=model_name||
           baseline->data->nodes.empty()||baseline->data->nodes.size()>4096||
           baseline->authored->nodes.size()!=baseline->data->nodes.size())
            return fail(error,"invalid Craft source identity, context or model baseline");
        const auto& data=*baseline->data;
        for(auto role:{data.hardpoints,data.damage,data.lights})
            if(role!=UINT32_MAX&&role>=data.nodes.size())return fail(error,"invalid Craft baseline role");
        std::map<std::uint32_t,std::uint32_t> indices;
        for(std::size_t i=0;i<data.nodes.size();++i){const auto& node=data.nodes[i];
            if(!node.address||!indices.emplace(node.address,static_cast<std::uint32_t>(i)).second||
               node.parent!=baseline->authored->nodes[i].parent||node.name!=baseline->authored->nodes[i].name||
               (node.parent!=UINT32_MAX&&node.parent>=data.nodes.size())||
               (node.first_child!=UINT32_MAX&&node.first_child>=data.nodes.size())||
               (node.next_sibling!=UINT32_MAX&&node.next_sibling>=data.nodes.size()))
                return fail(error,"invalid Craft baseline node mapping");
        }
        if(!indices.count(data.root_address))return fail(error,"missing Craft baseline root");
        Pass first,second;Reader reader{context,read};
        if(!capture_pass(object,member,owner,*baseline,indices,profile,reader,class_context,pin,prepared,first,error)||
           !capture_pass(object,member,owner,*baseline,indices,profile,reader,class_context,pin,prepared,second,error))return false;
        if(!same(first,second))return fail(error,"Craft visibility input changed during capture");
        CapturedCraftVisibility candidate;
        candidate.baseline=std::move(baseline);candidate.object_class=std::move(owner);
        candidate.entity=object.entity;candidate.instance=object.instance;
        candidate.engine=first.engine;candidate.render_context=first.context;candidate.quality=first.quality;
        auto& input=candidate.input;input.root=indices.at(data.root_address);
        for(const auto& node:data.nodes)input.nodes.push_back({node.parent,node.first_child,node.next_sibling,node.flags});
        input.primary_database=first.primary==object.database;
        input.database_damage=data.damage;input.base_group=data.lights;
        input.instance_hidden=first.hidden!=0;input.context_enabled=first.quality!=0;
        input.hull_fraction=first.hull;
        if(input.primary_database){
            if(first.class_roles[0])input.damage_child=data.nodes[indices.at(first.class_roles[0])].first_child;
            for(std::size_t i=0;i<5;++i){
                if(first.class_roles[i+1])input.system_groups[i]=indices.at(first.class_roles[i+1]);
                input.system_hidden[i]=first.systems[i]!=0;
            }
            if(first.class_roles[6])input.assimilation_group=indices.at(first.class_roles[6]);
            input.assimilated=first.assimilated!=0;
        }
        output=std::move(candidate);error.clear();return true;
    }catch(...){return fail(error,"Craft visibility capture allocation or callback failure");}
}
bool capture_craft_visibility(const Object& object,const WorldMember& member,
    std::shared_ptr<const LightClassRecord> owner,std::shared_ptr<const ModelNodeTemplate> baseline,
    CraftVisibilityReadProfile profile,void* context,Read read,
    void* class_context,PinLightClass pin,CapturedCraftVisibility& output,std::string& error)noexcept{
    detail::CraftVisibilitySubject subject;
    subject.entity=object.entity;subject.instance=object.instance;subject.database=object.database;
    subject.instance_vtable=object.instance_vtable;subject.database_vtable=object.database_vtable;
    subject.model_serial=object.model_serial;subject.model_identity=object.model_identity;
    static_assert(sizeof(subject.model_name)==sizeof(object.model_name),"model name identity capacity");
    std::memcpy(subject.model_name,object.model_name,sizeof(subject.model_name));
    return detail::capture_craft_visibility_inputs(subject,member,std::move(owner),std::move(baseline),
        profile,nullptr,context,read,class_context,pin,output,error);
}
} // namespace a2fo::native11
