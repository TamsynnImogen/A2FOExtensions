#include "node_state.hpp"
#include <cmath>
#include <cfloat>
#include <cstring>
#include <map>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_model=64u*1024u*1024u, max_parts=8192, max_metadata=1024u*1024u;
constexpr std::size_t max_nodes=4096, max_depth=256;
bool printable(const std::string& s,bool nonempty,std::size_t max) {
    if((nonempty&&s.empty())||s.size()>max)return false;
    for(unsigned char c:s)if(c<32||c>126)return false;
    return true;
}
bool copied_name(const char* p,std::size_t n,std::string& out) {
    const auto* end=static_cast<const char*>(std::memchr(p,0,n));
    if(!end||end==p)return false;
    out.assign(p,end);return printable(out,true,n-1);
}
bool valid_model(const std::shared_ptr<const ModelAsset>& asset) {
    std::string name;
    return asset&&asset->info.database&&asset->info.serial&&
        copied_name(asset->info.name,sizeof(asset->info.name),name)&&
        !asset->bytes.empty()&&asset->bytes.size()<=max_model&&asset->bytes.size()==asset->info.byte_count;
}
struct Reader {
    void* ctx;Read read;
    bool at(std::uint32_t base,std::uint32_t offset,void* dst,std::size_t n) const noexcept {
        return read&&base&&n&&offset<=UINT32_MAX-base&&n-1<=UINT32_MAX-base-offset&&
            read(ctx,base+offset,dst,n);
    }
    bool word(std::uint32_t base,std::uint32_t offset,std::uint32_t& dst) const noexcept {
        return at(base,offset,&dst,sizeof(dst));
    }
    bool name(std::uint32_t node,std::string& out) const {
        std::uint32_t p=0;if(!word(node,8,p)||!p)return false;
        out.clear();
        for(std::uint32_t i=0;i<=255;++i){char c=0;
            if(!at(p,i,&c,1))return false;
            if(!c)return printable(out,true,255);
            if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126)return false;
            out.push_back(c);
        }
        return false;
    }
};
void identity(float (&m)[16]) {for(unsigned i=0;i<16;++i)m[i]=(i%5==0)?1.f:0.f;}
bool composed(const float (&parent)[16],const float (&raw)[12],float scale,float (&out)[16]) {
    float local[16]{};
    // Engine Matrix34 stores three row-vector basis rows followed by translation.
    // Transpose its basis for native11's column-vector matrices.
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)local[r*4+c]=raw[c*3+r]*scale;
    for(unsigned r=0;r<3;++r)local[r*4+3]=raw[9+r];
    local[15]=1.f;
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){
        double v=0;for(unsigned k=0;k<4;++k)v+=double(parent[r*4+k])*local[k*4+c];
        if(!std::isfinite(v)||v>FLT_MAX||v< -FLT_MAX)return false;
        out[r*4+c]=static_cast<float>(v);
    }
    return true;
}
struct Pending {
    std::uint32_t node, parent;
    std::size_t depth;
    bool inherited_pruned;
    float parent_matrix[16];
};
}
bool capture_node_states(const Object& object,const ModelMaterialSource& source,
    MaterialReadProfile profile,void* read_context,Read read,
    std::vector<PartState>& out,std::string& error) noexcept {
    try {
        std::string object_name,asset_name;
        if(!read||!profile.instance_vtable||!profile.database_vtable||!profile.mesh_type_function||
           !object.instance||!object.database||!object.model_serial||
           object.instance_vtable!=profile.instance_vtable||
           object.database_vtable!=profile.database_vtable||
           object.model_identity!=ModelIdentity::plain_database_name||
           !valid_model(source.model)||source.model->info.database!=object.database||
           source.model->info.serial!=object.model_serial||
           !copied_name(object.model_name,sizeof(object.model_name),object_name)||
           !copied_name(source.model->info.name,sizeof(source.model->info.name),asset_name)||
           object_name!=asset_name||source.parts.empty()||source.parts.size()>max_parts) {
            error="invalid node state target";return false;
        }
        std::map<std::string,std::uint32_t> expected;
        std::set<std::pair<std::uint32_t,std::uint32_t>> groups;
        std::size_t metadata=0;
        for(const auto& part:source.parts){
            if(!printable(part.node_name,true,255)||!printable(part.authored_texture,false,1024)||
               part.node_name.size()+part.authored_texture.size()>max_metadata-metadata||
               !groups.insert({part.node_index,part.group_index}).second||
               (expected.count(part.node_name)&&expected.at(part.node_name)!=part.node_index)) {
                error="invalid node source metadata";return false;
            }
            expected.emplace(part.node_name,part.node_index);
            metadata+=part.node_name.size()+part.authored_texture.size();
        }
        Reader r{read_context,read};std::uint32_t vtable=0,database=0,root=0,root_parent=0,root_sibling=0;
        if(!r.word(object.instance,0,vtable)||vtable!=profile.instance_vtable||
           !r.word(object.instance,0x80,database)||database!=object.database||
           !r.word(database,0,vtable)||vtable!=profile.database_vtable||
           !r.word(database,0x3c,root)||!root||
           !r.word(root,0x18,root_parent)||root_parent||
           !r.word(root,0x20,root_sibling)||root_sibling){error="node owner or root mismatch";return false;}
        std::set<std::uint32_t> visited;
        std::map<std::string,PartState> found;
        Pending first{root,0,1,false,{}};identity(first.parent_matrix);
        std::vector<Pending> pending{first};
        while(!pending.empty()){
            const auto current=pending.back();pending.pop_back();
            if(!current.node||current.depth>max_depth||visited.size()>=max_nodes||
               !visited.insert(current.node).second){error="node tree cycle, alias, or limit";return false;}
            std::string name;
            std::uint32_t node_parent=0,child=0,sibling=0,flags=0,node_vtable=0,type=0;
            float raw[12]{},scale=0;
            if(!r.name(current.node,name)||!r.word(current.node,0x18,node_parent)||
               node_parent!=current.parent||!r.word(current.node,0x1c,child)||
               !r.word(current.node,0x20,sibling)||!r.word(current.node,0xbc,flags)||
               !r.word(current.node,0,node_vtable)||!node_vtable||
               !r.word(node_vtable,4,type)||
               !r.at(current.node,0x24,raw,sizeof(raw))||
               !r.at(current.node,0x54,&scale,sizeof(scale))){
                error="unreadable or inconsistent node tree";return false;
            }
            for(float x:raw)if(!std::isfinite(x)){error="nonfinite node transform";return false;}
            if(!std::isfinite(scale)){error="nonfinite node scale";return false;}
            PartState state;
            state.visible=!current.inherited_pruned&&!(flags&1u);
            if(!composed(current.parent_matrix,raw,scale,state.local_to_model)||
               !valid_part_state(state)){error="invalid node transform";return false;}
            const bool wanted=expected.count(name)!=0;
            if(type==profile.mesh_type_function&&state.visible&&!wanted){
                error="visible unrepresented mesh";return false;
            }
            if(wanted){
                if(type!=profile.mesh_type_function||!found.emplace(name,state).second){
                    error="missing, duplicate, or nonmesh source node";return false;
                }
            }
            if(sibling){Pending next{sibling,current.parent,current.depth,current.inherited_pruned,{}};
                std::memcpy(next.parent_matrix,current.parent_matrix,sizeof(next.parent_matrix));
                pending.push_back(next);
            }
            if(child){Pending next{child,current.node,current.depth+1,
                current.inherited_pruned||bool(flags&2u),{}};
                std::memcpy(next.parent_matrix,state.local_to_model,sizeof(next.parent_matrix));
                pending.push_back(next);
            }
        }
        if(found.size()!=expected.size()){error="missing source node";return false;}
        std::vector<PartState> result;result.reserve(source.parts.size());
        for(const auto& part:source.parts)result.push_back(found.at(part.node_name));
        out=std::move(result);error.clear();return true;
    }catch(...){try{error="node state exception";}catch(...){}return false;}
}
}
