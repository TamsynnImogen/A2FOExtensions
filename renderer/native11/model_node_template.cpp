#include "model_node_template.hpp"
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_nodes=4096,max_depth=256,max_model_bytes=64u*1024u*1024u;
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
bool printable(const std::string& value,std::size_t maximum) noexcept {
    if(value.empty()||value.size()>maximum)return false;
    for(unsigned char c:value)if(c<0x20||c>0x7e)return false;
    return true;
}
bool copied_name(const char* data,std::size_t count,std::string& value) {
    const auto* end=static_cast<const char*>(std::memchr(data,0,count));
    if(!end||end==data)return false;
    value.assign(data,end);
    return printable(value,count-1);
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
bool read_name(void* context,Read read,std::uint32_t pointer,std::string& value) {
    if(!pointer)return false;
    for(std::uint32_t i=0;i<=259;++i){
        std::uint8_t c=0;
        if(!field(context,read,pointer,i,c))return false;
        if(c==0)return printable(value,259);
        if(c<0x20||c>0x7e||i==259)return false;
        value.push_back(static_cast<char>(c));
    }
    return false;
}
struct Pending {std::uint32_t address=0,parent=0;std::size_t depth=0;};
struct Pass {
    std::uint32_t db_vtable=0,root=0;
    std::uint32_t database_name_pointer=0;
    std::string database_name;
    std::uint32_t role_addresses[3]{};
    std::uint32_t role_indices[3]{UINT32_MAX,UINT32_MAX,UINT32_MAX};
    std::vector<ModelNodeTemplateNode> nodes;
};
bool valid_type(std::uint32_t kind,std::uint32_t function,
                const NodeTemplateReadProfile& profile) noexcept {
    switch(kind){
    case 0:return function==profile.generic_type||function==profile.lod_type;
    case 1:return function==profile.mesh_type;
    case 2:return function==profile.light_type;
    case 3:return function==profile.sprite_type;
    case 12:return function==profile.emitter_type;
    default:return false;
    }
}
bool read_pass(const ModelPoseSnapshot& model,const std::string& expected_name,
    const NodeTemplateReadProfile& profile,
    void* context,Read read,Pass& pass,std::string& error) {
    const auto db=model.model->info.database;
    if(!field(context,read,db,0,pass.db_vtable)||pass.db_vtable!=profile.db_vtable)
        return fail(error,"unreadable or wrong database vtable");
    if(!field(context,read,db,0x38,pass.database_name_pointer)||
       !read_name(context,read,pass.database_name_pointer,pass.database_name))
        return fail(error,"unreadable database name pointer or bytes");
    if(pass.database_name!=expected_name)
        return fail(error,"database name differs from model asset");
    if(!field(context,read,db,0x3c,pass.root)||!pass.root||
       !field(context,read,db,0x2c,pass.role_addresses[0])||
       !field(context,read,db,0x30,pass.role_addresses[1])||
       !field(context,read,db,0x34,pass.role_addresses[2]))
        return fail(error,"unreadable database root or role pointers");
    std::map<std::string,std::size_t> names;
    std::size_t authored_roots=0;
    for(std::size_t i=0;i<model.pose->nodes.size();++i){
        const auto& node=model.pose->nodes[i];
        if(!printable(node.name,259)||!names.emplace(node.name,i).second||
           (node.parent!=UINT32_MAX&&node.parent>=model.pose->nodes.size()))
            return fail(error,("invalid authored node metadata at index "+std::to_string(i)).c_str());
        authored_roots+=node.parent==UINT32_MAX;
    }
    if(authored_roots!=1)return fail(error,"authored pose requires exactly one root");
    std::set<std::uint32_t> visited;
    std::vector<bool> mapped(model.pose->nodes.size());
    std::vector<Pending> pending{{pass.root,0,1}};
    pass.nodes.resize(model.pose->nodes.size());
    while(!pending.empty()){
        const auto current=pending.back();pending.pop_back();
        if(!current.address||current.depth>max_depth||visited.size()>=max_nodes||
           !visited.insert(current.address).second)
            return fail(error,"native node tree cycle, alias or depth limit");
        ModelNodeTemplateNode node;node.address=current.address;
        std::uint32_t owner=0;
        if(!field(context,read,node.address,0,node.vtable)||!node.vtable||
           !field(context,read,node.vtable,4,node.type_function))
            return fail(error,"unreadable native node vtable or type function");
        if(!field(context,read,node.address,4,owner)||owner!=db)
            return fail(error,"native node database owner mismatch");
        if(
           !field(context,read,node.address,8,node.name_pointer)||
           !read_name(context,read,node.name_pointer,node.name))
            return fail(error,"unreadable native node name pointer or bytes");
        if(!field(context,read,node.address,0x18,node.parent_address))
            return fail(error,("unreadable native parent link for "+node.name).c_str());
        if(node.parent_address!=current.parent)
            return fail(error,("native node parent mismatch for "+node.name).c_str());
        if(!field(context,read,node.address,0x1c,node.child_address)||
           !field(context,read,node.address,0x20,node.sibling_address))
            return fail(error,("unreadable native child or sibling link for "+node.name).c_str());
        if(
           !at(context,read,node.address,0x24,node.local.data(),sizeof(node.local))||
           !field(context,read,node.address,0x54,node.scale)||
           !field(context,read,node.address,0xbc,node.flags))
            return fail(error,("unreadable native node transform or flags for "+node.name).c_str());
        if(current.parent==0&&node.sibling_address)
            return fail(error,"native root has a sibling");
        if(!std::isfinite(node.scale))
            return fail(error,("nonfinite native node scale for "+node.name).c_str());
        for(float value:node.local)if(!std::isfinite(value))
            return fail(error,("nonfinite native node matrix for "+node.name).c_str());
        const auto found=names.find(node.name);
        if(found==names.end())
            return fail(error,("unmatched native node name "+node.name).c_str());
        if(mapped[found->second])
            return fail(error,("duplicate native node name "+node.name).c_str());
        if(!valid_type(model.pose->nodes[found->second].kind,node.type_function,profile))
            return fail(error,("unsupported type function "+std::to_string(node.type_function)+
                " for authored node "+node.name+" at index "+std::to_string(found->second)).c_str());
        mapped[found->second]=true;
        pass.nodes[found->second]=std::move(node);
        const auto& stored=pass.nodes[found->second];
        if(stored.sibling_address)pending.push_back({stored.sibling_address,current.parent,current.depth});
        if(stored.child_address)pending.push_back({stored.child_address,stored.address,current.depth+1});
    }
    if(visited.size()!=model.pose->nodes.size())
        return fail(error,"native node tree has missing or extra authored nodes");
    std::map<std::uint32_t,std::uint32_t> addresses;
    for(std::size_t i=0;i<pass.nodes.size();++i)
        if(!addresses.emplace(pass.nodes[i].address,static_cast<std::uint32_t>(i)).second)
            return fail(error,"native node address alias");
    const auto resolve=[&addresses](std::uint32_t address,std::uint32_t& index){
        if(!address){index=UINT32_MAX;return true;}
        const auto found=addresses.find(address);
        if(found==addresses.end())return false;
        index=found->second;return true;
    };
    for(std::size_t i=0;i<pass.nodes.size();++i){
        if(!mapped[i])
            return fail(error,("missing authored node at index "+std::to_string(i)).c_str());
        const auto parent=model.pose->nodes[i].parent;
        if(pass.nodes[i].parent_address!=
           (parent==UINT32_MAX?0:pass.nodes[parent].address))
            return fail(error,("authored parent mismatch at node index "+std::to_string(i)).c_str());
        auto& node=pass.nodes[i];
        if(!resolve(node.parent_address,node.parent)||
           !resolve(node.child_address,node.first_child)||
           !resolve(node.sibling_address,node.next_sibling)||node.parent!=parent)
            return fail(error,("unmapped native link at node index "+std::to_string(i)).c_str());
    }
    for(std::size_t r=0;r<3;++r){
        if(!pass.role_addresses[r])continue;
        if(!resolve(pass.role_addresses[r],pass.role_indices[r]))
            return fail(error,("unmapped database role pointer at index "+std::to_string(r)).c_str());
    }
    return true;
}
bool same_node(const ModelNodeTemplateNode& a,const ModelNodeTemplateNode& b) noexcept {
    return a.address==b.address&&a.vtable==b.vtable&&a.type_function==b.type_function&&
        a.name_pointer==b.name_pointer&&a.parent_address==b.parent_address&&
        a.child_address==b.child_address&&a.sibling_address==b.sibling_address&&
        a.parent==b.parent&&a.first_child==b.first_child&&
        a.next_sibling==b.next_sibling&&
        a.flags==b.flags&&std::memcmp(&a.scale,&b.scale,sizeof(float))==0&&
        std::memcmp(a.local.data(),b.local.data(),sizeof(a.local))==0&&a.name==b.name;
}
bool same_pass(const Pass& a,const Pass& b) noexcept {
    if(a.db_vtable!=b.db_vtable||a.database_name_pointer!=b.database_name_pointer||
       a.database_name!=b.database_name||a.root!=b.root||
       a.nodes.size()!=b.nodes.size())return false;
    for(std::size_t r=0;r<3;++r)
        if(a.role_addresses[r]!=b.role_addresses[r]||a.role_indices[r]!=b.role_indices[r])return false;
    for(std::size_t i=0;i<a.nodes.size();++i)if(!same_node(a.nodes[i],b.nodes[i]))return false;
    return true;
}
} // namespace

bool capture_model_node_template(const ModelPoseSnapshot& model,NodeTemplateReadProfile profile,
    void* context,Read read,std::shared_ptr<const ModelNodeTemplate>& output,
    std::string& error) noexcept {
    error.clear();
    try {
        std::string name;
        if(!model.model||!model.pose||!read||!profile.db_vtable||
           !profile.generic_type||!profile.lod_type||!profile.mesh_type||
           !profile.light_type||!profile.sprite_type||!profile.emitter_type||
           !model.model->info.database||!model.model->info.serial||
           !copied_name(model.model->info.name,sizeof(model.model->info.name),name)||
           model.model->bytes.empty()||model.model->bytes.size()>max_model_bytes||
           model.model->bytes.size()!=model.model->info.byte_count||
           model.pose->nodes.empty()||model.pose->nodes.size()>max_nodes)
            return fail(error,"invalid model template identity or profile");
        Pass first,second;
        if(!read_pass(model,name,profile,context,read,first,error)||
           !read_pass(model,name,profile,context,read,second,error))
            return error.empty()?fail(error,"invalid or unreadable model node template"):false;
        if(!same_pass(first,second))return fail(error,"model node template changed during capture");
        auto data=std::make_shared<ModelNodeTemplateData>();
        data->root_address=first.root;
        data->hardpoints=first.role_indices[0];
        data->damage=first.role_indices[1];
        data->lights=first.role_indices[2];
        data->nodes=std::move(first.nodes);
        auto result=std::make_shared<ModelNodeTemplate>();
        result->model=model.model;result->authored=model.pose;result->data=std::move(data);
        output=std::move(result);error.clear();return true;
    }catch(...){return fail(error,"model node template allocation or read failure");}
}
} // namespace a2fo::native11
