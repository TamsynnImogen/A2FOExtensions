#include "../renderer/native11/node_state.hpp"
#include <cassert>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <map>
using namespace a2fo::native11;
namespace {
constexpr std::uint32_t inst=0x1000,db=0x2000,root=0x3000,a=0x4000,b=0x5000,c=0x6000;
constexpr std::uint32_t iv=0x7000,dv=0x7100,mv=0x7200,other=0x7300,mesh=0xabcdef;
struct Memory {
    std::map<std::uint32_t,std::uint8_t> bytes;
    template<class T> void put(std::uint32_t at,T v){for(std::size_t i=0;i<sizeof(v);++i)bytes[at+i]=reinterpret_cast<std::uint8_t*>(&v)[i];}
    void name(std::uint32_t at,const char* s){for(std::size_t i=0;i<=std::strlen(s);++i)put(at+i,s[i]);}
    static bool read(void* ctx,std::uint32_t at,void* dst,std::size_t n){auto& m=*static_cast<Memory*>(ctx);
        auto* p=static_cast<std::uint8_t*>(dst);for(std::size_t i=0;i<n;++i){auto it=m.bytes.find(at+i);if(it==m.bytes.end())return false;p[i]=it->second;}return true;}
};
void node(Memory& m,std::uint32_t ptr,std::uint32_t parent,std::uint32_t child,
          std::uint32_t sibling,std::uint32_t flags,const char* name,
          float scale=1,float tx=0,float ty=0,float tz=0,bool is_mesh=true){
    m.put(ptr,is_mesh?mv:other);m.put(ptr+8,ptr+0x300);m.name(ptr+0x300,name);
    m.put(ptr+0x18,parent);m.put(ptr+0x1c,child);m.put(ptr+0x20,sibling);m.put(ptr+0xbc,flags);
    const float raw[12]={1,0,0,0,1,0,0,0,1,tx,ty,tz};
    for(unsigned i=0;i<12;++i)m.put(ptr+0x24+i*4,raw[i]);
    m.put(ptr+0x54,scale);
}
void setup(Memory& m){m.put(inst,iv);m.put(inst+0x80,db);m.put(db,dv);m.put(db+0x3c,root);
    m.put(mv+4,mesh);m.put(other+4,0x123456u);
    node(m,root,0,a,0,0,"root",1,10,0,0,false);
    node(m,a,root,c,b,0,"a",2,0,2,0);
    node(m,b,root,0,0,0,"b",1,0,0,3);
    node(m,c,a,0,0,0,"c",1,1,0,0);
}
Object object(){Object o;o.instance=inst;o.database=db;o.instance_vtable=iv;o.database_vtable=dv;
    o.model_serial=3;o.model_identity=ModelIdentity::plain_database_name;
    std::strcpy(o.model_name,"ship.sod");return o;}
ModelMaterialSource source(){ModelMaterialSource s;auto model=std::make_shared<ModelAsset>();
    model->info.database=db;model->info.serial=3;std::strcpy(model->info.name,"ship.sod");
    model->bytes={1};model->info.byte_count=1;s.model=model;
    s.parts={{"c","",3,0},{"a","",1,0},{"c","",3,1},{"b","",2,0}};return s;}
bool capture(Memory& m,const Object& o,const ModelMaterialSource& s,std::vector<PartState>& out,std::string& err){
    return capture_node_states(o,s,{iv,dv,mesh},&m,Memory::read,out,err);
}
void fail(Memory& m,const Object& o,const ModelMaterialSource& s,std::vector<PartState>& out){
    auto before=out;std::string error;assert(!capture(m,o,s,out,error));assert(!error.empty());
    assert(out.size()==before.size());if(!before.empty())assert(out[0].local_to_model[3]==before[0].local_to_model[3]);
}
void near(float a,float b){assert(std::fabs(a-b)<1e-5f);}
}
int main(){Memory m;setup(m);auto o=object();auto s=source();std::vector<PartState> out;std::string error;
    if(!capture(m,o,s,out,error)){std::fprintf(stderr,"capture: %s\n",error.c_str());return 1;}
    assert(out.size()==4);near(out[0].local_to_model[3],12);
    near(out[0].local_to_model[7],2);near(out[1].local_to_model[0],2);
    near(out[2].local_to_model[3],12);near(out[3].local_to_model[3],10);
    near(out[3].local_to_model[11],3);
    const float rotated[9]={0,1,0,-1,0,0,0,0,1};
    for(unsigned i=0;i<9;++i)m.put(a+0x24+i*4,rotated[i]);
    assert(capture(m,o,s,out,error));near(out[0].local_to_model[3],10);
    near(out[0].local_to_model[7],4);near(out[0].local_to_model[1],-2);
    const float unrotated[9]={1,0,0,0,1,0,0,0,1};
    for(unsigned i=0;i<9;++i)m.put(a+0x24+i*4,unrotated[i]);
    m.put(a+0xbc,1u);assert(capture(m,o,s,out,error));assert(!out[1].visible&&out[0].visible&&out[2].visible);
    m.put(a+0xbc,2u);assert(capture(m,o,s,out,error));assert(out[1].visible&&!out[0].visible&&out[3].visible);
    m.put(a+0xbc,0u);m.put(a+0x54,0.f);fail(m,o,s,out);
    m.put(a+0xbc,3u);assert(capture(m,o,s,out,error));m.put(a+0x54,1.f);m.put(a+0xbc,0u);
    m.put(a+0x54,-1.f);assert(capture(m,o,s,out,error));near(out[1].local_to_model[0],-1);
    m.put(a+0x54,NAN);fail(m,o,s,out);m.put(a+0x54,2.f);
    m.put(b+0x18,a);fail(m,o,s,out);m.put(b+0x18,root);
    m.put(c+0x20,a);fail(m,o,s,out);m.put(c+0x20,0u);
    m.put(root+0x20,b);fail(m,o,s,out);m.put(root+0x20,0u);
    m.put(c+8,0xdead0000u);fail(m,o,s,out);m.put(c+8,c+0x300);
    m.put(a,mv+0x100);fail(m,o,s,out);m.put(a,mv);
    node(m,0x8000,root,0,0,0,"extra");m.put(b+0x20,0x8000u);fail(m,o,s,out);
    m.put(0x8000+0xbc,1u);assert(capture(m,o,s,out,error));m.put(b+0x20,0u);
    auto bad=s;bad.parts[1].node_name="c";fail(m,o,bad,out);
    auto wrong=o;wrong.model_serial=4;fail(m,wrong,s,out);
}
