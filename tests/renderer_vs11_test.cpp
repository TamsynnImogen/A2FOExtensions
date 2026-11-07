#include "../renderer/shaders/vs11.hpp"
#include "fixtures/renderer_vs11_capture.inc"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace a2fo::renderer::shaders;
namespace {
void check(bool yes,const char* reason) {
    if(!yes){std::fprintf(stderr,"FAIL: %s\n",reason);std::exit(1);}
}
bool translate(const std::vector<std::uint32_t>& words,Vs11Translation& out,std::string& error) {
    return translate_vs11(words.data(),words.size(),out,error);
}
void reject(std::vector<std::uint32_t> words,const char* message) {
    Vs11Translation out;out.hlsl="stale";out.input_mask=0xffff;out.output_masks.fill(15);
    std::string error;
    check(!translate(words,out,error),message);
    check(out.hlsl.empty()&&out.input_mask==0&&out.output_masks[0]==0&&!error.empty(),"refusal clears stale translation and explains failure");
}
}
int main() {
    const std::uint32_t* captures[]={program_1,program_2,program_4};
    const std::size_t lengths[]={std::size(program_1),std::size(program_2),std::size(program_4)};
    const std::size_t instructions[]={13,21,39};
    for(unsigned p=0;p<3;++p) {
        Vs11Translation out;std::string error;
        check(translate_vs11(captures[p],lengths[p],out,error),"native captured program translates");
        check(out.instruction_count==instructions[p]&&out.input_mask==0x3d&&out.output_masks[0]==15&&out.output_masks[1]==15,"native program register/dependency metadata");
        for(std::size_t prefix=0;prefix<lengths[p];++prefix)
            check(!translate_vs11(captures[p],prefix,out,error),"every incomplete native program refused");
    }
    const std::vector<std::uint32_t> simple={0xfffe0101,1,0xc00f0000,0x90e40000,0xffff};
    Vs11Translation out;std::string error;
    check(translate(simple,out,error)&&out.instruction_count==1&&out.input_mask==1,"position-only valid program");
    check(out.hlsl.find("v15 : TEXCOORD15")==std::string::npos,"unused IA inputs omitted from shader signature");
    auto commented=simple;commented.insert(commented.begin()+1,{0x0002fffe,0xffffffff,0x0000ffff});
    Vs11Translation with_comment;
    check(translate(commented,with_comment,error)&&with_comment.hlsl==out.hlsl,"assembler comments do not change semantic shader cache source");
    auto words=simple;words[0]=0xffff0101;reject(words,"pixel stream refused");
    words=simple;words[0]=0xfffe0100;
    check(translate(words,with_comment,error)&&with_comment.hlsl==out.hlsl,"VS1.0 common instruction subset uses the same shader semantics");
    words=simple;words.push_back(0);reject(words,"trailing stream data refused");
    words=simple;words[1]|=0x40000000;reject(words,"coissue flags refused");
    words=simple;words[2]|=0x00100000;reject(words,"destination saturate modifier refused");
    words=simple;words[2]=0xc0000000;reject(words,"empty output mask refused");
    words=simple;words[2]=0xc0070000;reject(words,"missing output position W refused");
    words=simple;words[2]=0xc00f0001;reject(words,"unimplemented fog output refused");
    words=simple;words[3]|=0x00002000;reject(words,"relative source address refused");
    words=simple;words[3]=0x92e40000;reject(words,"non-negate source modifier refused");
    words=simple;words[3]=0x90e40010;reject(words,"input register outside v0-v15 refused");
    words=simple;words[3]=0xa0e40060;reject(words,"constant outside c0-c95 refused");
    words=simple;words[3]=0x80e4000c;reject(words,"temporary outside r0-r11 refused");
    words=simple;words[3]=0x80e40000;reject(words,"unwritten temporary read refused");
    reject({0xfffe0101,1,0x80010000,0xa0000000,1,0xc00f0000,0x80e40000,0xffff},"partially written temporary read refused");
    reject({0xfffe0101,6,0xc00f0000,0xa0000000,0xffff},"unsupported reciprocal opcode refused");
    reject({0xfffe0101,20,0xc00f0000,0x90e40000,0xa0e4005d,0xffff},"matrix c93 extends beyond constant bank");
    reject({0xfffe0101,20,0xc00f0000,0x90e40000,0xa0000002,0xffff},"matrix swizzle refused");
    reject({0xfffe0101,20,0xc00f0000,0x91e40000,0xa0e40002,0xffff},"uncaptured matrix vector modifier refused");
    reject({0xfffe0101,20,0xc0070000,0x90e40000,0xa0e40002,0xffff},"matrix partial destination refused");
    reject({0xfffe0101,1,0x800f0000,0x90e40000,20,0x800f0000,0x80e40000,0xa0e40002,1,0xc00f0000,0x80e40000,0xffff},"matrix source-destination alias refused");
    words={0xfffe0101,1,0x80010000,0xa0000000,1,0x800e0000,0x90540000,1,0xc00f0000,0x80e40000,0xffff};
    check(translate(words,out,error)&&out.output_masks[0]==15,"temporary assembled by disjoint masks is defined");
    words={0xfffe0101,1,0xc00f0000,0xa0e40000,0xffff};
    check(translate(words,out,error)&&out.input_mask==0&&out.hlsl.find("VSMain()")!=std::string::npos,"constant-only program needs no IA signature");
    words={0xfffe0101,
           1,0x800f0000,0x90e40003,
           9,0xd0010000,0x801b0000,0xa0e40006,
           1,0xc00f0000,0x90e40000,0xffff};
    check(translate(words,out,error)&&out.instruction_count==3&&out.input_mask==0x9&&out.output_masks[1]==1,
          "masked DP4 with a fully swizzled temporary translates");
    check(out.hlsl.find("dot((r0.wzyx),(c[6].xyzw))")!=std::string::npos&&
          out.hlsl.find("o.diffuse.x = value1.x;")!=std::string::npos,
          "DP4 retains all four swizzled lanes and writes only its destination mask");
    words[2]=0x80070000;
    words[6]=0x80e40000;
    reject(words,"DP4 masked destination still requires unwritten source W");
    words[6]=0x80000000;
    check(translate(words,out,error),"DP4 replicated X swizzle needs only initialized X");
    check(out.hlsl.find("dot((r0.xxxx),(c[6].xyzw))")!=std::string::npos,
          "DP4 replicated swizzle is retained in scalar result");
    Vs11Translation bounds;
    check(!translate_vs11(nullptr,0,bounds,error),"null input refused");
    check(!translate_vs11(simple.data(),4097,bounds,error),"oversize span refused before memory access");
    std::puts("PASS: bounded VS1.1 translation, captured programs, register dependencies and explicit refusals");
}
