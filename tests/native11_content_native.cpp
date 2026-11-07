#include "../renderer/native11/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool ok,const char* message) {if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
std::vector<std::uint8_t> sod(const char* texture,bool two_parts=false) {
    std::vector<std::uint8_t> b;
    auto word=[&](std::uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);};
    auto number=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);dword(bits);};
    auto text=[&](const char* s){word(static_cast<std::uint16_t>(std::strlen(s)));while(*s)b.push_back(*s++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);
    word(1);text("mat");for(float v:{.1f,.2f,.3f,.25f,.5f,.75f,0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(1);
    word(1);word(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,4.f,6.f,5.f})number(v);
    text("additive");dword(4);dword(1);text(texture);dword(0);text("");word(0);
    word(3);word(3);word(two_parts?2:1);
    for(float v:{0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f})number(v);
    for(float v:{0.f,0.f,1.f,0.f,0.f,1.f})number(v);
    for(int part=0;part<(two_parts?2:1);++part) {
        word(1);text("mat");for(unsigned i=0;i<3;++i){word(i);word(i);}
    }
    b.push_back(0);word(0);word(0);word(0);
    return b;
}
std::vector<std::uint8_t> tga() {
    std::vector<std::uint8_t> b(24);b[2]=2;b[12]=2;b[14]=1;b[16]=24;b[17]=0x20;
    const std::uint8_t pixels[]{3,2,1,6,5,4};std::memcpy(b.data()+18,pixels,6);return b;
}
std::vector<std::uint8_t> dds() {
    std::vector<std::uint8_t> b(144);
    auto word=[&](std::size_t at,std::uint32_t value){for(unsigned i=0;i<4;++i)b[at+i]=(value>>(i*8))&255;};
    word(0,0x20534444);word(4,124);word(8,0x21007);word(12,4);word(16,4);
    word(28,2);word(76,32);word(80,4);word(84,0x31545844);
    for(unsigned i=0;i<16;++i)b[128+i]=static_cast<std::uint8_t>(i+1);
    return b;
}
struct Resolver;
struct Reader {
    std::vector<std::uint8_t> model,texture;
    std::string expected_model="opaque-model-id",expected_texture="hull.tga";
    unsigned models=0,textures=0;
    bool fail_model=false,fail_texture=false;
    std::vector<std::string> texture_ids;
    Resolver* resolver=nullptr;
};
bool read(void* context,AssetKind kind,const std::string& identifier,
          std::vector<std::uint8_t>& bytes,std::string& error) {
    auto& r=*static_cast<Reader*>(context);
    if(kind==AssetKind::model){++r.models;need(identifier==r.expected_model,"opaque model ID unchanged");
        if(r.fail_model){error="model reader failure";return false;}bytes=r.model;return true;}
    ++r.textures;need(identifier==r.expected_texture||
        std::find(r.texture_ids.begin(),r.texture_ids.end(),identifier)!=r.texture_ids.end(),"opaque/resolved texture ID unchanged");
    if(r.fail_texture){error="texture reader failure";return false;}bytes=r.texture;return true;
}
struct Resolver {
    std::string expected_model="opaque-model-id";
    std::vector<std::uint32_t> parts;
    std::vector<std::string> authored;
    std::string first="resolved-a",second="resolved-b";
    int fail_part=-1,throw_part=-1,malformed_part=-1,malformed_kind=0;
};
bool resolve(void* context,const std::string& model,std::uint32_t part,
             const std::string& authored,std::string& resolved,std::string& error) {
    auto& reader=*static_cast<Reader*>(context);
    need(reader.resolver!=nullptr,"resolver fixture context is attached");
    auto& r=*reader.resolver;
    need(model==r.expected_model,"resolver receives scoped model ID");
    r.parts.push_back(part);r.authored.push_back(authored);
    if(static_cast<int>(part)==r.fail_part){error="later part resolver failure";return false;}
    if(static_cast<int>(part)==r.throw_part)throw std::runtime_error("fixture resolver exception");
    if(static_cast<int>(part)==r.malformed_part) {
        if(r.malformed_kind==1)resolved="";
        else if(r.malformed_kind==2)resolved.assign(1025,'x');
        else if(r.malformed_kind==3)resolved=std::string("bad\0name",8);
        else resolved="bad\nname";
        return true;
    }
    resolved=part==0?r.first:r.second;return true;
}
void check_owned(const ModelContent& c) {
    need(c.parts.size()==2&&c.textures.size()==1,"two parts and deduplicated texture");
    need(c.parts[0].texture_index==0&&c.parts[1].texture_index==0,"shared texture index");
    need(c.parts[0].vertices.size()==3&&c.parts[0].indices.size()==3,"owned geometry counts");
    need(c.parts[0].indices[0]==0&&c.parts[0].indices[1]==2&&c.parts[0].indices[2]==1,"authored front normalized");
    need(c.parts[0].vertices[0].normal[2]==-1.f,"outward normal owned");
    need(c.parts[0].local_to_model[3]==4.f&&c.parts[0].local_to_model[7]==6.f&&c.parts[0].local_to_model[11]==5.f,"transform owned");
    need(c.parts[0].colour[0]==0.f&&c.parts[0].emissive[0]==.25f&&c.parts[0].flags==9,"material owned");
    const auto& t=c.textures[0];
    const std::uint8_t pixels[]{3,2,1,255,6,5,4,255};
    need(t.identifier=="hull.tga"&&t.width==2&&t.height==1&&t.mips.size()==1&&t.bytes.size()==8,"texture identifier and layout owned");
    need(t.mips[0].row_pitch==8&&t.mips[0].byte_count==8&&
         std::memcmp(t.bytes.data(),pixels,8)==0,"decoded bytes owned");
}
}
int main() {
    Reader r{sod("hull.tga",true),tga(),"opaque-model-id","hull.tga",0,0,false,false,{},nullptr};
    ModelContent output;std::string error;
    need(load_model_content({&r,read},r.expected_model,output,error),"valid model and TGA");
    need(r.models==1&&r.textures==1,"repeated texture read once");
    r.model.clear();r.texture.clear();check_owned(output);
    const auto preserved=output.parts.size();
    r.model=sod("hull.tga");r.fail_texture=true;
    need(!load_model_content({&r,read},r.expected_model,output,error)&&error=="texture reader failure"&&output.parts.size()==preserved,"reader failure transactional");
    r.fail_texture=false;r.texture={1,2,3};
    need(!load_model_content({&r,read},r.expected_model,output,error)&&output.parts.size()==preserved,"malformed texture transactional");
    r.model={1,2,3};
    need(!load_model_content({&r,read},r.expected_model,output,error)&&output.parts.size()==preserved,"unsupported model transactional");
    r.model=sod("");r.textures=0;
    need(load_model_content({&r,read},r.expected_model,output,error)&&output.parts.size()==1&&
         output.parts[0].texture_index==no_texture&&output.textures.empty()&&r.textures==0,"no diffuse maps to white");
    r.model=sod("hull.tga");r.fail_model=true;
    need(!load_model_content({&r,read},r.expected_model,output,error)&&output.parts.size()==1,"model reader failure transactional");
    need(!load_model_content({&r,read},"",output,error)&&output.parts.size()==1,"empty model ID rejected");
    r.fail_model=false;r.model=sod("hull.tga");r.texture=dds();
    need(load_model_content({&r,read},r.expected_model,output,error),"authored DDS mip chain");
    need(output.textures.size()==1&&output.textures[0].format==TextureFormat::bc1&&
         output.textures[0].mips.size()==2&&output.textures[0].mips[0].offset==0&&
         output.textures[0].mips[1].offset==8&&output.textures[0].mips[1].width==2&&
         output.textures[0].bytes.size()==16&&output.textures[0].bytes[0]==1&&
         output.textures[0].bytes[15]==16,"authored mip metadata and blocks preserved");
    r.model=sod("hull.tga",true);r.texture=tga();r.textures=0;r.texture_ids={"resolved-a","resolved-b"};
    Resolver resolver;
    r.resolver=&resolver;
    need(load_model_content({&r,read,resolve},r.expected_model,output,error),"per-part resolver succeeds");
    need(resolver.parts==std::vector<std::uint32_t>{0,1}&&
         resolver.authored==std::vector<std::string>{"hull.tga","hull.tga"},"resolver gets each emitted part and authored ID");
    need(output.textures.size()==2&&output.textures[0].identifier=="resolved-a"&&
         output.textures[1].identifier=="resolved-b"&&output.parts[0].texture_index==0&&
         output.parts[1].texture_index==1&&r.textures==2,"same authored texture resolves per part");
    resolver=Resolver{};resolver.first=resolver.second="shared-resolved";r.texture_ids={"shared-resolved"};r.textures=0;
    need(load_model_content({&r,read,resolve},r.expected_model,output,error)&&
         output.textures.size()==1&&output.parts[0].texture_index==0&&output.parts[1].texture_index==0&&
         r.textures==1,"deduplicate by resolved identifier");
    const auto before_resolver_failure=output.parts.size();
    resolver=Resolver{};resolver.fail_part=1;r.textures=0;
    need(!load_model_content({&r,read,resolve},r.expected_model,output,error)&&
         error=="later part resolver failure"&&output.parts.size()==before_resolver_failure&&r.textures==0,
         "later resolver failure is transactional without fallback");
    resolver=Resolver{};resolver.throw_part=1;r.textures=0;
    need(!load_model_content({&r,read,resolve},r.expected_model,output,error)&&
         error=="texture resolver threw"&&output.parts.size()==before_resolver_failure&&r.textures==0,
         "resolver exception is caught transactionally");
    for(int kind=0;kind<=3;++kind) {
        resolver=Resolver{};resolver.malformed_part=1;resolver.malformed_kind=kind;r.textures=0;
        need(!load_model_content({&r,read,resolve},r.expected_model,output,error)&&
             error=="invalid resolved texture identifier"&&output.parts.size()==before_resolver_failure&&r.textures==0,
             "malformed resolved ID is transactional");
    }
    resolver=Resolver{};r.model=sod("");
    need(load_model_content({&r,read,resolve},r.expected_model,output,error)&&
         output.parts[0].texture_index==no_texture&&resolver.parts.empty(),"untextured part skips resolver");
    std::puts("PASS native11 content ownership dedup transaction no-diffuse authored-mips CPU fixture");
}
