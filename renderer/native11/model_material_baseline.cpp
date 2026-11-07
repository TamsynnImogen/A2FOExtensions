#include "model_material_baseline.hpp"
#include <cstring>
#include <map>
#include <set>
#include <utility>
namespace a2fo::native11 {
namespace {
constexpr std::size_t max_model=64u*1024u*1024u,max_texture=64u*1024u*1024u;
bool fail(std::string& error,const char* why) noexcept {try{error=why;}catch(...){}return false;}
bool printable(const std::string& value,std::size_t maximum) noexcept {
    if(value.empty()||value.size()>maximum)return false;
    for(unsigned char c:value)if(c<32||c>126)return false;
    return true;
}
bool copied_name(const char* text,std::size_t capacity,std::string& out){
    const auto* end=static_cast<const char*>(std::memchr(text,0,capacity));
    if(!end||end==text)return false;
    out.assign(text,end);return printable(out,capacity-1);
}
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
            if(!ch)return printable(out,maximum);
            if(ch<32||ch>126||k==maximum)return false;
            out.push_back(static_cast<char>(ch));
        }return false;
    }
};
bool template_valid(const ModelNodeTemplate& source,std::string& name){
    if(!source.model||!source.authored||!source.data||
       !source.model->info.database||!source.model->info.serial||
       !copied_name(source.model->info.name,sizeof(source.model->info.name),name)||
       source.model->bytes.empty()||source.model->bytes.size()>max_model||
       source.model->info.byte_count!=source.model->bytes.size())return false;
    const auto& nodes=source.data->nodes;const auto count=nodes.size();
    if(!count||count>4096||source.authored->nodes.size()!=count||!source.data->root_address)return false;
    const auto role=[count](std::uint32_t index){return index==UINT32_MAX||index<count;};
    if(!role(source.data->hardpoints)||!role(source.data->damage)||!role(source.data->lights))return false;
    std::set<std::uint32_t> addresses;std::set<std::string> names;
    std::size_t roots=0;std::uint32_t root=UINT32_MAX;
    for(std::size_t k=0;k<count;++k){const auto& node=nodes[k];const auto& authored=source.authored->nodes[k];
        if(!node.address||!node.vtable||!node.type_function||!node.name_pointer||
           !addresses.insert(node.address).second||!printable(node.name,259)||
           !names.insert(node.name).second||node.name!=authored.name||
           node.parent!=authored.parent||!role(node.parent)||!role(node.first_child)||
           !role(node.next_sibling)||node.parent==k)return false;
        if(node.parent==UINT32_MAX){++roots;root=static_cast<std::uint32_t>(k);}
    }
    if(roots!=1||nodes[root].address!=source.data->root_address||
       nodes[root].next_sibling!=UINT32_MAX)return false;
    for(std::size_t k=0;k<count;++k){const auto& node=nodes[k];
        if(node.parent_address!=(node.parent==UINT32_MAX?0:nodes[node.parent].address)||
           node.child_address!=(node.first_child==UINT32_MAX?0:nodes[node.first_child].address)||
           node.sibling_address!=(node.next_sibling==UINT32_MAX?0:nodes[node.next_sibling].address)||
           (node.next_sibling!=UINT32_MAX&&nodes[node.next_sibling].parent!=node.parent)||
           (node.first_child!=UINT32_MAX&&nodes[node.first_child].parent!=k))return false;
    }
    std::vector<std::uint8_t> seen(count);std::vector<std::uint32_t> pending{root};
    std::size_t visited=0;
    while(!pending.empty()){
        const auto k=pending.back();pending.pop_back();
        if(k>=count||seen[k]++)return false;
        ++visited;const auto& node=nodes[k];
        if(node.next_sibling!=UINT32_MAX)pending.push_back(node.next_sibling);
        if(node.first_child!=UINT32_MAX)pending.push_back(node.first_child);
    }
    return visited==count;
}
struct TextureMeta {std::uint32_t name_pointer=0,flags=0;std::uint8_t key=0;};
bool same(const TextureMeta& a,const TextureMeta& b) noexcept {
    return a.name_pointer==b.name_pointer&&a.flags==b.flags&&a.key==b.key;
}
struct Pass {
    std::uint32_t db_name_pointer=0;
    std::vector<ModelMeshMaterialBaseline> meshes;
    std::map<std::uint32_t,TextureMeta> textures;
};
bool texture(std::uint32_t pointer,Reader reader,void* context,ResolveTextureAsset resolve,
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>>& owners,Pass& pass,
    std::shared_ptr<const TextureAsset>& out,std::string& error){
    if(!pointer){out.reset();return true;}
    auto asset=resolve(context,pointer);TextureMeta meta;
    std::string name;
    if(!asset||asset->texture!=pointer||!asset->serial||
       !asset->bytes||asset->bytes->empty()||asset->bytes->size()>max_texture||
       !printable(asset->logical_name,1024)||
       !reader.field(pointer,8,meta.name_pointer)||
       !reader.name(meta.name_pointer,name,1024)||name!=asset->logical_name||
       !reader.field(pointer,0x18,meta.flags)||meta.flags!=asset->flags||
       !reader.field(pointer,0x2c,meta.key)||bool(meta.key)!=asset->colour_key)
        return fail(error,"invalid immutable or runtime texture source");
    const auto owner=owners.emplace(pointer,asset);
    if(!owner.second&&owner.first->second!=asset)return fail(error,"texture owner changed across mesh slots");
    const auto found=pass.textures.emplace(pointer,meta);
    if(!found.second&&!same(found.first->second,meta))return fail(error,"texture metadata changed across mesh slots");
    out=std::move(asset);return true;
}
bool pass(const ModelNodeTemplate& source,MaterialBaselineReadProfile profile,
    const std::string& model_name,Reader reader,void* texture_context,ResolveTextureAsset resolve,
    std::map<std::uint32_t,std::shared_ptr<const TextureAsset>>& owners,Pass& result,std::string& error){
    const auto db=source.model->info.database;std::uint32_t vtable=0,root=0;
    std::string name;
    if(!reader.field(db,0,vtable)||vtable!=profile.database_vtable||
       !reader.field(db,0x38,result.db_name_pointer)||
       !reader.name(result.db_name_pointer,name,259)||name!=model_name||
       !reader.field(db,0x3c,root)||root!=source.data->root_address)
        return fail(error,"database source header changed");
    const auto roles=std::array<std::uint32_t,3>{{source.data->hardpoints,source.data->damage,source.data->lights}};
    for(std::size_t k=0;k<roles.size();++k){std::uint32_t actual=0;
        const auto expected=roles[k]==UINT32_MAX?0:source.data->nodes[roles[k]].address;
        if(!reader.field(db,0x2c+static_cast<std::uint32_t>(4*k),actual)||actual!=expected)
            return fail(error,"database role pointer changed");
    }
    for(std::size_t k=0;k<source.data->nodes.size();++k){
        const auto& node=source.data->nodes[k];const auto& authored=source.authored->nodes[k];
        std::uint32_t owner=0,type=0,name_pointer=0,parent=0,child=0,sibling=0;
        std::string node_name;
        if(!reader.field(node.address,0,vtable)||vtable!=node.vtable||
           !reader.field(vtable,4,type)||type!=node.type_function||
           !reader.field(node.address,4,owner)||owner!=db||
           !reader.field(node.address,8,name_pointer)||name_pointer!=node.name_pointer||
           !reader.name(name_pointer,node_name,259)||node_name!=node.name||
           !reader.field(node.address,0x18,parent)||parent!=node.parent_address||
           !reader.field(node.address,0x1c,child)||child!=node.child_address||
           !reader.field(node.address,0x20,sibling)||sibling!=node.sibling_address)
            return fail(error,"node identity or topology changed since load");
        if((authored.kind==1)!=(type==profile.mesh_type_function))
            return fail(error,"authored and native mesh type mismatch");
        if(authored.kind!=1)continue;
        ModelMeshMaterialBaseline mesh;mesh.node_index=static_cast<std::uint32_t>(k);
        mesh.node_address=node.address;
        std::uint32_t count=0,alternate=0;
        if(!reader.field(node.address,0x114,mesh.slot_vector)||
           !reader.field(node.address,0x11c,count)||count>8||
           (count&&!mesh.slot_vector)||
           !reader.field(node.address,0x13c,alternate))
            return fail(error,"mesh slot vector or alternate unavailable");
        mesh.slots.reserve(count);
        for(std::uint32_t s=0;s<count;++s){std::uint32_t pointer=0;
            if(!reader.field(mesh.slot_vector,4*s,pointer))return fail(error,"mesh slot pointer unavailable");
            std::shared_ptr<const TextureAsset> asset;
            if(!texture(pointer,reader,texture_context,resolve,owners,result,asset,error))return false;
            mesh.slots.push_back(std::move(asset));
        }
        if(!texture(alternate,reader,texture_context,resolve,owners,result,mesh.borg_alternate,error))return false;
        result.meshes.push_back(std::move(mesh));
    }
    return true;
}
bool same(const Pass& a,const Pass& b) noexcept {
    if(a.db_name_pointer!=b.db_name_pointer||a.meshes.size()!=b.meshes.size()||
       a.textures.size()!=b.textures.size())return false;
    for(std::size_t k=0;k<a.meshes.size();++k){const auto& x=a.meshes[k];const auto& y=b.meshes[k];
        if(x.node_index!=y.node_index||x.node_address!=y.node_address||x.slot_vector!=y.slot_vector||
           x.slots!=y.slots||x.borg_alternate!=y.borg_alternate)return false;
    }
    for(const auto& [pointer,meta]:a.textures){const auto found=b.textures.find(pointer);
        if(found==b.textures.end()||!same(meta,found->second))return false;
    }return true;
}
}
bool capture_model_material_baseline(const ModelNodeTemplate& source,
    MaterialBaselineReadProfile profile,void* read_context,Read read,
    void* texture_context,ResolveTextureAsset resolve,
    std::shared_ptr<const ModelMaterialBaseline>& output,std::string& error) noexcept {
    try {
        std::string name;
        if(!read||!resolve||!profile.database_vtable||!profile.mesh_type_function||
           !template_valid(source,name))return fail(error,"invalid completed-load model or template");
        Reader reader{read_context,read};
        std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> owners;
        Pass first,second;
        if(!pass(source,profile,name,reader,texture_context,resolve,owners,first,error)||
           !pass(source,profile,name,reader,texture_context,resolve,owners,second,error))return false;
        if(!same(first,second))return fail(error,"mesh material source changed across passes");
        for(const auto& [pointer,asset]:owners)
            if(resolve(texture_context,pointer)!=asset)return fail(error,"texture owner changed after capture");
        auto data=std::make_shared<ModelMaterialBaselineData>();data->meshes=std::move(first.meshes);
        auto result=std::make_shared<ModelMaterialBaseline>();
        result->model=source.model;result->authored=source.authored;result->nodes=source.data;
        result->data=std::move(data);output=std::move(result);error.clear();return true;
    }catch(...){return fail(error,"model material baseline read or allocation failure");}
}
} // namespace a2fo::native11
