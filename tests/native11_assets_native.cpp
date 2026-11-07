#include "../renderer/native11/assets.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <vector>
static_assert(sizeof(A2FO_N11_Vertex)==32);
static_assert(sizeof(A2FO_N11_Part)==120);
static_assert(offsetof(A2FO_N11_Part,local_to_model)==16);
static_assert(offsetof(A2FO_N11_Part,texture_name)==80);
static_assert(offsetof(A2FO_N11_Part,flags)==116);
namespace {
void need(bool condition,const char* message){if(!condition){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
struct Counts {unsigned parts=0;std::uint64_t triangles=0;};
int mesh(void* user,const A2FO_N11_Part* part){
    if(!part||!part->vertices||!part->indices||part->vertex_count<3||!part->index_count||part->index_count%3)return 0;
    for(unsigned i=0;i<16;++i)if(!std::isfinite(part->local_to_model[i]))return 0;
    if(part->local_to_model[12]!=0||part->local_to_model[13]!=0||part->local_to_model[14]!=0||part->local_to_model[15]!=1)return 0;
    for(unsigned i=0;i<part->index_count;++i)if(part->indices[i]>=part->vertex_count)return 0;
    auto& counts=*static_cast<Counts*>(user);++counts.parts;counts.triangles+=part->index_count/3;return 1;
}
int image(void* user,const std::uint8_t* bgra,std::uint32_t w,std::uint32_t h,std::uint32_t pitch,std::uint64_t length){
    const std::uint8_t expected[]{3,2,1,255,6,5,4,255};
    if(!bgra||w!=2||h!=1||pitch!=8||length!=8||std::memcmp(bgra,expected,8))return 0;
    *static_cast<bool*>(user)=true;return 1;
}
}
int main(int argc,char** argv){
    std::uint8_t tga[24]{};tga[2]=2;tga[12]=2;tga[14]=1;tga[16]=24;tga[17]=0x20;
    const std::uint8_t pixels[]{3,2,1,6,5,4};std::memcpy(tga+18,pixels,6);bool seen=false;
    need(a2fo_native11_decode_image(tga,sizeof(tga),image,&seen)==1&&seen,"i686 image callback ABI and BGRA values");
    for(int i=1;i<argc;++i){
        std::ifstream file(argv[i],std::ios::binary|std::ios::ate);need(bool(file),"open SOD fixture");
        const auto length=file.tellg();need(length>0&&length<64*1024*1024,"SOD fixture size");
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());need(bool(file),"read SOD fixture");
        Counts counts;need(a2fo_native11_parse_sod(bytes.data(),static_cast<std::uint32_t>(bytes.size()),mesh,&counts)==1,"i686 real SOD callback ABI");
        need(counts.parts>0&&counts.triangles>0,"nonempty SOD output");
        std::printf("PASS i686 native11 asset: parts=%u triangles=%llu path=%s\n",counts.parts,static_cast<unsigned long long>(counts.triangles),argv[i]);
        Counts invalid;need(a2fo_native11_parse_sod(bytes.data(),13,mesh,&invalid)==0&&invalid.parts==0,"truncated input no callbacks");
    }
    std::puts("PASS i686 native11 CPU ABI, BGRA decoder, callback lifetime and malformed admission");
}
