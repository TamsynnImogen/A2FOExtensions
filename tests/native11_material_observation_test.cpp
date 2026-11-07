#include "../renderer/native11/material_observation.hpp"
#include <cassert>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t inst=0x1000, db=0x2000, node=0x3000, node2=0x4000;
constexpr std::uint32_t tex1=0x5000,tex2=0x6000,ivee=0x7000,dvee=0x7100,mvee=0x7200,vec=0x8000;
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    template<class T> void put(std::uint32_t at,T value){for(std::size_t i=0;i<sizeof(T);++i)bytes[at+i]=reinterpret_cast<std::uint8_t*>(&value)[i];}
    void name(std::uint32_t at,const char* s){for(std::size_t i=0;i<=std::strlen(s);++i)put(at+i,s[i]);}
    static bool read(void* ctx,std::uint32_t at,void* out,std::size_t n){auto& m=*static_cast<Memory*>(ctx);
        auto* p=static_cast<std::uint8_t*>(out);for(std::size_t i=0;i<n;++i){auto x=m.bytes.find(at+i);if(x==m.bytes.end())return false;p[i]=x->second;}return true;}
};
struct Parser {
    bool duplicate=false, bad_name=false, bad_texture=false, same_name_other_index=false, white=false;
};
Parser parser;
int parse(const std::uint8_t*,std::uint32_t,A2FO_N11_DetailedPartSink sink,void* ctx){
    A2FO_N11_Vertex vert{};std::uint32_t index=0;
    const char* name=parser.bad_name?"\n":"mesh";
    const char* texture=parser.bad_texture?"\n":(parser.white?"":"authored.dds");
    A2FO_N11_Part part{};part.vertices=&vert;part.vertex_count=1;part.indices=&index;part.index_count=1;
    part.texture_name=reinterpret_cast<const std::uint8_t*>(texture);part.texture_name_length=std::strlen(texture);
    A2FO_N11_PartSource origin{reinterpret_cast<const std::uint8_t*>(name),
        static_cast<std::uint32_t>(std::strlen(name)),1,0};
    if(!sink(ctx,&part,&origin))return 0;
    origin.group_index=parser.duplicate?0:1;
    if(parser.same_name_other_index)origin.node_index=2;
    return sink(ctx,&part,&origin);
}
std::shared_ptr<const ModelAsset> model(){auto m=std::make_shared<ModelAsset>();m->info.database=db;m->info.serial=3;
    std::strcpy(m->info.name,"ship.sod");m->bytes={1,2};m->info.byte_count=2;return m;}
std::shared_ptr<const TextureAsset> texture(std::uint32_t ptr,const char* name,std::uint8_t byte){
    auto t=std::make_shared<TextureAsset>();t->texture=ptr;t->serial=byte;t->logical_name=name;
    t->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{byte});return t;
}
struct Resolver {std::map<std::uint32_t,std::shared_ptr<const TextureAsset>> values;bool switch_on_second=false;int calls=0;
    std::shared_ptr<const TextureAsset> replacement;
    static std::shared_ptr<const TextureAsset> get(void* ctx,std::uint32_t ptr){auto& r=*static_cast<Resolver*>(ctx);
        ++r.calls;if(r.switch_on_second&&r.calls>=2)return r.replacement;return r.values[ptr];}
};
void setup(Memory& m,std::uint32_t slot=tex1){
    m.put(inst,ivee);m.put(inst+0x80,db);m.put(db,dvee);m.put(db+0x3c,node);
    m.put(node,mvee);m.put(node+8,0x9000u);m.name(0x9000,"mesh");m.put(node+0x1c,0u);m.put(node+0x20,0u);
    m.put(mvee+4,0xabcdefu);m.put(node+0xbc,0x15u);m.put(node+0x114,vec);
    m.put(node+0x11c,1u);m.put(vec,slot);
    m.put(tex1+8,0xa000u);m.name(0xa000,"first.dds");m.put(tex1+0x18,0u);m.put(tex1+0x2c,std::uint8_t{0});
    m.put(tex2+8,0xb000u);m.name(0xb000,"second.dds");m.put(tex2+0x18,0u);m.put(tex2+0x2c,std::uint8_t{0});
}
Object object(){Object o;o.instance=inst;o.database=db;o.instance_vtable=ivee;o.database_vtable=dvee;
    o.model_identity=ModelIdentity::plain_database_name;o.model_serial=3;std::strcpy(o.model_name,"ship.sod");return o;}
bool capture(const Object& o,const ModelMaterialSource& src,Memory& m,Resolver& r,
             std::shared_ptr<const MaterialObservation>& out,std::string& err){
    return capture_material_observation(o,src,{ivee,dvee,0xabcdef},&m,Memory::read,&r,Resolver::get,out,err);
}
bool capture_posed(const Object& o,const ModelMaterialSource& src,Memory& m,Resolver& r,
                   const std::vector<PartState>& states,
                   std::shared_ptr<const MaterialObservation>& out,std::string& err) {
    return capture_material_observation(o,src,{ivee,dvee,0xabcdef},&m,Memory::read,
                                        &r,Resolver::get,out,err,&states);
}
void fail(const Object& o,const ModelMaterialSource& src,Memory& m,Resolver& r,
          std::shared_ptr<const MaterialObservation>& out){auto prior=out;std::string err;
    assert(!capture(o,src,m,r,out,err));assert(!err.empty());assert(out==prior);}
}
int main(){
    auto asset=model();std::shared_ptr<const ModelMaterialSource> source;std::string err;
    assert(inspect_model_materials(asset,parse,source,err));assert(source->parts.size()==2);
    assert(source->parts[0].node_name=="mesh"&&source->parts[1].group_index==1);
    Memory m;setup(m);Resolver r;r.values[tex1]=texture(tex1,"first.dds",1);
    r.values[tex2]=texture(tex2,"second.dds",2);auto o=object();
    std::shared_ptr<const MaterialObservation> first,second;
    assert(capture(o,*source,m,r,first,err));assert(first->parts.size()==2&&
       first->parts[0].texture==first->parts[1].texture&&first->parts[0].node_flags==0x15&&
       first->parts[0].authored_texture=="authored.dds");
    constexpr std::uint32_t inst2=0x1100; m.put(inst2,ivee);m.put(inst2+0x80,db);
    auto o2=o;o2.instance=inst2;
    m.put(vec,tex2);assert(capture(o2,*source,m,r,second,err));
    assert(first->parts[0].texture->bytes->at(0)==1&&second->parts[0].texture->bytes->at(0)==2);
    m.put(inst+0x80,0u);fail(o,*source,m,r,second);m.put(inst+0x80,db);
    m.put(mvee+4,0u);fail(o,*source,m,r,second);m.put(mvee+4,0xabcdefu);
    m.put(node+0x11c,9u);fail(o,*source,m,r,second);m.put(node+0x11c,1u);
    m.put(node+0x1c,node);fail(o,*source,m,r,second);m.put(node+0x1c,0u);
    m.put(node+0x20,node2);m.put(node2,mvee);m.put(node2+8,0xc000u);m.name(0xc000,"mesh");
    m.put(node2+0x1c,0u);m.put(node2+0x20,0u);fail(o,*source,m,r,second);m.put(node+0x20,0u);
    m.put(node+8,0xd000u);fail(o,*source,m,r,second);m.put(node+8,0x9000u);
    m.put(tex2+0x18,1u);fail(o,*source,m,r,second);m.put(tex2+0x18,0u);
    auto alpha=std::make_shared<TextureAsset>(*r.values[tex2]);
    alpha->flags=texture_has_alpha;
    r.values[tex2]=alpha;m.put(tex2+0x18,texture_has_alpha);
    assert(capture(o,*source,m,r,second,err)&&second->parts[0].texture==alpha&&
           second->parts[1].texture==alpha&&second->parts[0].texture->flags==texture_has_alpha);
    auto retained_alpha=second;
    m.put(tex2+0x18,0u);fail(o,*source,m,r,second);
    assert(second==retained_alpha);
    m.put(tex2+0x18,4u);
    auto unknown=std::make_shared<TextureAsset>(*alpha);unknown->flags=4;
    r.values[tex2]=unknown;fail(o,*source,m,r,second);
    assert(second==retained_alpha);
    r.values[tex2]=texture(tex2,"second.dds",2);m.put(tex2+0x18,0u);
    m.put(tex2+0x2c,std::uint8_t{1});fail(o,*source,m,r,second);m.put(tex2+0x2c,std::uint8_t{0});
    m.name(0xb000,"wrong.dds");fail(o,*source,m,r,second);m.name(0xb000,"second.dds");
    r.values.erase(tex2);fail(o,*source,m,r,second);r.values[tex2]=texture(tex2,"second.dds",2);
    r.calls=0;r.switch_on_second=true;r.replacement=texture(tex2,"second.dds",3);fail(o,*source,m,r,second);
    r.switch_on_second=false;r.calls=0;
    auto wrong=o;wrong.model_serial=4;fail(wrong,*source,m,r,second);
    auto altered=*source;altered.parts[1].group_index=0;fail(o,altered,m,r,second);
    altered=*source;altered.parts[1].node_index=2;fail(o,altered,m,r,second);
    altered=*source;altered.parts[0].node_name="\n";fail(o,altered,m,r,second);
    parser.duplicate=true;std::shared_ptr<const ModelMaterialSource> kept=source;
    assert(!inspect_model_materials(asset,parse,source,err)&&source==kept);parser={};
    parser.bad_name=true;assert(!inspect_model_materials(asset,parse,source,err));parser={};
    parser.bad_texture=true;assert(!inspect_model_materials(asset,parse,source,err));parser={};
    parser.same_name_other_index=true;assert(!inspect_model_materials(asset,parse,source,err));parser={};
    parser.white=true;std::shared_ptr<const ModelMaterialSource> white;
    assert(inspect_model_materials(asset,parse,white,err));m.put(vec,tex1);
    assert(capture(o,*white,m,r,second,err)&&second->parts[0].texture==r.values[tex1]&&
           second->parts[0].node_index==1&&second->parts[0].group_index==0);
    m.put(vec,0u);assert(capture(o,*white,m,r,second,err));assert(!second->parts[0].texture);
    parser.white=false;std::shared_ptr<const ModelMaterialSource> textured;
    assert(inspect_model_materials(asset,parse,textured,err));
    assert(capture(o,*textured,m,r,second,err)&&!second->parts[0].texture&&
           second->parts[0].authored_texture=="authored.dds");
    m.put(node+0x11c,0u);assert(capture(o,*white,m,r,second,err));assert(!second->parts[0].texture);
    auto bad=std::make_shared<ModelAsset>(*asset);bad->info.byte_count=3;
    assert(!inspect_model_materials(bad,parse,source,err)&&source==kept);

    // An authoritative hidden pose still validates the live mesh/tree/slot,
    // but it does not need a texture owner that the engine will not draw.
    m.put(node+0x11c,1u);m.put(vec,tex2);r.values.erase(tex2);
    std::vector<PartState> states(textured->parts.size());
    for(auto& state:states)state.visible=false;
    states[0].local_to_model[3]=5;
    r.calls=0;std::shared_ptr<const MaterialObservation> posed;
    assert(capture_posed(o,*textured,m,r,states,posed,err));
    assert(r.calls==0&&posed->parts.size()==2);
    for(const auto& part:posed->parts)
        assert(part.node_state_valid&&!part.node_state.visible&&!part.texture&&
               part.authored_texture=="authored.dds");
    states[0].local_to_model[3]=99;
    assert(posed->parts[0].node_state.local_to_model[3]==5);
    auto prior=posed;
    states[1].visible=true;
    assert(!capture_posed(o,*textured,m,r,states,posed,err)&&!err.empty()&&posed==prior);
    assert(r.calls==1);
    states[1].visible=false;
    states.pop_back();
    assert(!capture_posed(o,*textured,m,r,states,posed,err)&&!err.empty()&&posed==prior);
    states.resize(textured->parts.size());states[0].local_to_model[0]=
        std::numeric_limits<float>::infinity();
    assert(!capture_posed(o,*textured,m,r,states,posed,err)&&!err.empty()&&posed==prior);
    states[0].local_to_model[0]=1;
    // Hidden state does not relax the authoritative mesh/slot structure.
    m.put(node+0x11c,9u);
    assert(!capture_posed(o,*textured,m,r,states,posed,err)&&!err.empty()&&posed==prior);
}
