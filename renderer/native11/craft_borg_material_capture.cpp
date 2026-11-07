#include "craft_borg_material_capture.hpp"
#include "model_material_baseline_cache.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
struct Reader {
    void* context;Read read;
    bool at(std::uint32_t base,std::uint32_t offset,void* out,std::size_t size) const {
        return base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
            read(context,base+offset,out,size);
    }
    template<class T>bool field(std::uint32_t base,std::uint32_t offset,T& out)const{
        return at(base,offset,&out,sizeof(out));
    }
    bool name(std::uint32_t pointer,std::string& out,std::size_t maximum)const{
        if(!pointer)return false;
        out.clear();
        for(std::uint32_t k=0;k<=maximum;++k){std::uint8_t ch=0;
            if(!field(pointer,k,ch))return false;
            if(!ch)return !out.empty();
            if(ch<32||ch>126||k==maximum)return false;
            out.push_back(static_cast<char>(ch));
        }return false;
    }
};
bool same_owner(const std::shared_ptr<const LightClassRecord>& klass,
    const std::shared_ptr<const ModelMaterialBaseline>& baseline,
    void* model_context,PinBorgModel pin_model,void* class_context,PinBorgClass pin_class,
    void* texture_context,ResolveTextureAsset resolve){
    if(pin_class(class_context,klass->class_id)!=klass||
       pin_model(model_context,baseline->model->info.database,baseline->model->info.serial)!=baseline->model)
        return false;
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> owners;
    const auto check=[&](const std::shared_ptr<const TextureAsset>& asset){
        if(!asset)return true;
        const auto prior=owners.emplace(asset->texture,asset);
        return (prior.second||prior.first->second==asset)&&resolve(texture_context,asset->texture)==asset;
    };
    for(const auto& mesh:baseline->data->meshes){
        for(const auto& asset:mesh.slots)if(!check(asset))return false;
        if(!check(mesh.borg_alternate))return false;
    }return true;
}
struct Pass {
    std::uint32_t db_name_pointer=0,class_vtable=0,class_database=0,table_pointer=0;
    std::int32_t signed_count=0;
    std::vector<std::array<std::uint32_t,2>> raw_rows;
};
bool pass(const ModelMaterialBaseline& baseline,const LightClassRecord& klass,
    MaterialBaselineReadProfile profile,Reader reader,
    const std::vector<CraftBorgMaterialRow>& expected,Pass& out,std::string& error){
    const auto db=baseline.model->info.database;
    std::uint32_t vtable=0,root=0;std::string name;
    if(!reader.field(db,0,vtable)||vtable!=profile.database_vtable||
       !reader.field(db,0x38,out.db_name_pointer)||
       !reader.name(out.db_name_pointer,name,259)||name!=baseline.model->info.name||
       !reader.field(db,0x3c,root)||root!=baseline.nodes->root_address)
        return fail(error,"Borg table database source header changed");
    const auto roles=std::array<std::uint32_t,3>{{baseline.nodes->hardpoints,
        baseline.nodes->damage,baseline.nodes->lights}};
    for(std::uint32_t k=0;k<3;++k){std::uint32_t actual=0;
        const auto target=roles[k]==UINT32_MAX?0:baseline.nodes->nodes[roles[k]].address;
        if(!reader.field(db,0x2c+4*k,actual)||actual!=target)
            return fail(error,"Borg table database role changed");
    }
    for(std::size_t k=0;k<baseline.nodes->nodes.size();++k){
        const auto& node=baseline.nodes->nodes[k];const auto& authored=baseline.authored->nodes[k];
        std::uint32_t owner=0,type=0,name_pointer=0,parent=0,child=0,sibling=0;
        std::string live_name;
        if(!reader.field(node.address,0,vtable)||vtable!=node.vtable||
           !reader.field(vtable,4,type)||type!=node.type_function||
           !reader.field(node.address,4,owner)||owner!=db||
           !reader.field(node.address,8,name_pointer)||name_pointer!=node.name_pointer||
           !reader.name(name_pointer,live_name,259)||live_name!=node.name||
           !reader.field(node.address,0x18,parent)||parent!=node.parent_address||
           !reader.field(node.address,0x1c,child)||child!=node.child_address||
           !reader.field(node.address,0x20,sibling)||sibling!=node.sibling_address||
           ((authored.kind==1)!=(type==profile.mesh_type_function)))
            return fail(error,"Borg table node identity or topology changed");
        if(authored.kind==1){
            const auto mesh=std::find_if(baseline.data->meshes.begin(),baseline.data->meshes.end(),
                [k](const ModelMeshMaterialBaseline& value){return value.node_index==k;});
            if(mesh==baseline.data->meshes.end())return fail(error,"Borg table mesh missing from baseline");
            const auto alternate=mesh->borg_alternate?mesh->borg_alternate->texture:0;
            std::uint32_t live_alternate=0;
            if(!reader.field(node.address,0x13c,live_alternate)||live_alternate!=alternate)
                return fail(error,"Borg alternate differs from completed load");
        }
    }
    if(!reader.field(static_cast<std::uint32_t>(klass.class_id),0,out.class_vtable)||!out.class_vtable||
       !reader.field(static_cast<std::uint32_t>(klass.class_id),0x1d8,out.class_database)||
       out.class_database!=db||
       !reader.field(static_cast<std::uint32_t>(klass.class_id),0x424,out.signed_count)||
       out.signed_count<0||out.signed_count>4096||
       !reader.field(static_cast<std::uint32_t>(klass.class_id),0x428,out.table_pointer)||
       bool(out.table_pointer)!=(out.signed_count!=0)||
       static_cast<std::size_t>(out.signed_count)!=expected.size())
        return fail(error,"Borg class header or row count unavailable");
    out.raw_rows.reserve(static_cast<std::size_t>(out.signed_count));
    for(std::int32_t k=0;k<out.signed_count;++k){std::array<std::uint32_t,2> raw{};
        if(!reader.at(out.table_pointer,static_cast<std::uint32_t>(k)*8,raw.data(),8)||
           raw[0]!=baseline.nodes->nodes[expected[static_cast<std::size_t>(k)].node_index].address||
           raw[1]!=expected[static_cast<std::size_t>(k)].original_texture)
            return fail(error,"Borg class row unavailable or reordered");
        out.raw_rows.push_back(raw);
    }
    return true;
}
bool same(const Pass& a,const Pass& b) noexcept {
    return a.db_name_pointer==b.db_name_pointer&&a.class_vtable==b.class_vtable&&
        a.class_database==b.class_database&&a.table_pointer==b.table_pointer&&
        a.signed_count==b.signed_count&&a.raw_rows==b.raw_rows;
}
}
bool capture_craft_borg_material_route(const CapturedCraftPreparation& held,
    CraftBorgMaterialRouteProfile profile,void* read_context,Read read,
    void* class_context,PinBorgClass pin_class,CraftBorgMaterialRoute& output,
    std::string& error) noexcept {
    try {
        const auto& member=held.member;
        if(!read||!pin_class||!profile.instance_vtable||!profile.render_function||
           !member.entity||!member.instance||!member.object_class||!member.class_version||
           !held.model||held.database!=held.model->info.database||!held.database||
           !held.object_class||held.object_class->class_id!=member.object_class||
           held.object_class->version!=member.class_version)
            return fail(error,"invalid Craft material route target/profile");
        Reader reader{read_context,read};
        const auto pass_route=[&](CraftBorgMaterialRoute& route){
            std::uint32_t instance=0,klass=0,database=0,target=0;
            return pin_class(class_context,member.object_class)==held.object_class&&
                reader.field(member.entity,4,instance)&&instance==member.instance&&
                reader.field(member.entity,0x40,klass)&&klass==member.object_class&&
                reader.field(instance,0,route.instance_vtable)&&route.instance_vtable==profile.instance_vtable&&
                reader.field(route.instance_vtable,0x14,target)&&target==profile.render_function&&
                reader.field(instance,0x98,klass)&&klass==member.object_class&&
                reader.field(instance,0x80,database)&&database==held.database&&
                reader.field(klass,0,route.class_vtable)&&route.class_vtable&&
                reader.field(klass,0x1dc,route.primary_database)&&
                pin_class(class_context,member.object_class)==held.object_class;
        };
        CraftBorgMaterialRoute first,second;
        if(!pass_route(first)||!pass_route(second))
            return fail(error,"Craft material route identity, owner or dispatch unavailable");
        first.selector_reached=held.database==first.primary_database;
        second.selector_reached=held.database==second.primary_database;
        if(!same_craft_borg_material_route(first,second))
            return fail(error,"Craft material route changed during capture");
        output=first;error.clear();return true;
    }catch(...){return fail(error,"Craft material route read or allocation failure");}
}
bool capture_craft_borg_table(std::shared_ptr<const LightClassRecord> klass,
    std::shared_ptr<const ModelMaterialBaseline> baseline,MaterialBaselineReadProfile profile,
    void* read_context,Read read,void* model_context,PinBorgModel pin_model,
    void* class_context,PinBorgClass pin_class,void* texture_context,ResolveTextureAsset resolve,
    CapturedCraftBorgTable& output,std::string& error) noexcept {
    try {
        if(!klass||!klass->class_id||klass->class_id>UINT32_MAX-0x42b||!klass->version||
           !baseline||!baseline->model||!baseline->data||!profile.database_vtable||
           !profile.mesh_type_function||!read||!pin_model||!pin_class||!resolve)
            return fail(error,"invalid Borg table source/profile");
        ModelMaterialBaselineCache checked(1);
        if(!checked.publish(baseline,error))return false;
        if(!same_owner(klass,baseline,model_context,pin_model,class_context,pin_class,
            texture_context,resolve))return fail(error,"Borg table source owner unavailable");
        std::map<std::uint32_t,const ModelMeshMaterialBaseline*> meshes;
        for(const auto& mesh:baseline->data->meshes)meshes.emplace(mesh.node_index,&mesh);
        std::map<std::uint32_t,std::uint32_t> indices;
        for(std::size_t k=0;k<baseline->nodes->nodes.size();++k)
            indices.emplace(baseline->nodes->nodes[k].address,static_cast<std::uint32_t>(k));
        std::vector<CraftBorgMaterialRow> expected;
        const auto root=indices.at(baseline->nodes->root_address);
        std::vector<std::uint32_t> pending{root};
        while(!pending.empty()){const auto k=pending.back();pending.pop_back();
            const auto& node=baseline->nodes->nodes[k];
            const auto mesh=meshes.find(k);
            if(mesh!=meshes.end()&&mesh->second->borg_alternate){
                const auto& value=*mesh->second;
                if(value.slots.size()<4||value.slots.size()>8)
                    return fail(error,"eligible Borg mesh slot count outside native domain");
                expected.push_back({k,value.slots[0]?value.slots[0]->texture:0,
                    value.borg_alternate->texture,static_cast<std::uint32_t>(value.slots.size())});
            }
            if(node.first_child!=UINT32_MAX)pending.push_back(node.first_child);
            if(node.next_sibling!=UINT32_MAX)pending.push_back(node.next_sibling);
        }
        Reader reader{read_context,read};Pass first,second;
        if(!pass(*baseline,*klass,profile,reader,expected,first,error))return false;
        if(!same_owner(klass,baseline,model_context,pin_model,class_context,pin_class,
            texture_context,resolve))return fail(error,"Borg table source owner changed during capture");
        if(!pass(*baseline,*klass,profile,reader,expected,second,error))return false;
        if(!same(first,second))return fail(error,"Borg table changed during capture");
        if(!same_owner(klass,baseline,model_context,pin_model,class_context,pin_class,
            texture_context,resolve))return fail(error,"Borg table source owner changed during capture");
        CapturedCraftBorgTable result;result.object_class=std::move(klass);
        result.baseline=std::move(baseline);result.class_vtable=first.class_vtable;
        result.table_pointer=first.table_pointer;result.rows=std::move(expected);
        output=std::move(result);error.clear();return true;
    }catch(...){return fail(error,"Borg table read or allocation failure");}
}
} // namespace a2fo::native11
