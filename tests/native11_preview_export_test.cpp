#include "../renderer/native11/preview_export.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
std::uint32_t u32(const std::vector<std::uint8_t>& bytes,std::size_t at){
    return std::uint32_t(bytes[at])|(std::uint32_t(bytes[at+1])<<8)|
        (std::uint32_t(bytes[at+2])<<16)|(std::uint32_t(bytes[at+3])<<24);
}
}
int main(){
    std::string error;
    std::vector<std::uint8_t> bmp;
    const std::vector<std::uint8_t> top_down={
        1,2,3,0, 4,5,6,255, 7,8,9,128,
        10,11,12,255, 13,14,15,0, 16,17,18,42};
    need(encode_preview_bmp(top_down,3,2,bmp,error)&&error.empty(),"BMP encode");
    need(bmp.size()==78&&bmp[0]=='B'&&bmp[1]=='M'&&u32(bmp,2)==78&&
         u32(bmp,10)==54&&u32(bmp,14)==40&&u32(bmp,18)==3&&
         u32(bmp,22)==0xfffffffeu&&bmp[26]==1&&bmp[27]==0&&
         bmp[28]==24&&bmp[29]==0&&u32(bmp,30)==0&&u32(bmp,34)==24,
         "top-down BMP24 header and aligned image size");
    const std::vector<std::uint8_t> expected={
        1,2,3,4,5,6,7,8,9,0,0,0,
        10,11,12,13,14,15,16,17,18,0,0,0};
    need(std::vector<std::uint8_t>(bmp.begin()+54,bmp.end())==expected,
         "BGR pixels, source row order, ignored alpha and zero padding");
    const auto accepted=bmp;
    auto reject_bmp=[&](const std::vector<std::uint8_t>& input,
                        std::uint32_t width,std::uint32_t height,const char* why){
        need(!encode_preview_bmp(input,width,height,bmp,error)&&bmp==accepted&&
             !error.empty(),why);
    };
    reject_bmp(top_down,0,2,"zero width transactional");
    reject_bmp(top_down,3,0,"zero height transactional");
    reject_bmp(top_down,4097,2,"dimension cap transactional");
    reject_bmp(top_down,std::numeric_limits<std::uint32_t>::max(),2,
               "huge dimension transactional");
    reject_bmp({1,2,3},1,1,"truncated BGRA transactional");
    reject_bmp({1,2,3,4,5},1,1,"extra BGRA transactional");
    std::vector<std::uint8_t> maximum(4096u*4096u*4u,0);
    need(encode_preview_bmp(maximum,4096,4096,bmp,error)&&error.empty()&&
         bmp.size()==54u+4096u*4096u*3u&&u32(bmp,22)==0xfffff000u,
         "maximum accepted dimensions remain within byte caps");
    need(encode_preview_bmp({1,2,3,4},1,1,bmp,error)&&error.empty()&&
         bmp.size()==58&&bmp[54]==1&&bmp[55]==2&&bmp[56]==3&&bmp[57]==0,
         "valid encode after rejection");

    PreviewExportStatus status{};
    status.state="frame";status.reason="quote \" slash \\ line\n byte ";
    status.reason.push_back(static_cast<char>(0x80));
    status.serial=std::numeric_limits<std::uint64_t>::max();
    status.frame=(1ull<<53)+1;status.source_epoch=42;
    status.source_ticket=7;status.objects=8;status.width=3;status.height=2;status.slot='b';
    std::string json;
    need(encode_preview_status(status,json,error)&&error.empty(),"frame JSON encode");
    need(json=="{\"state\":\"frame\",\"reason\":\"quote \\\" slash \\\\ line\\u000a byte \\u0080\","
        "\"serial\":\"18446744073709551615\",\"frame\":\"9007199254740993\","
        "\"source_epoch\":\"42\",\"source_ticket\":7,\"objects\":8,"
        "\"width\":3,\"height\":2,\"slot\":\"b\"}",
        "ASCII escaping, JS-safe counters, and frame fields");
    const auto accepted_json=json;
    auto reject_status=[&](const PreviewExportStatus& bad,const char* why){
        json=accepted_json;
        need(!encode_preview_status(bad,json,error)&&json==accepted_json&&
             !error.empty(),why);
    };
    auto bad=status;bad.state="other";reject_status(bad,"unknown state transactional");
    bad=status;bad.slot='c';reject_status(bad,"unknown frame slot transactional");
    bad=status;bad.width=0;reject_status(bad,"missing frame width transactional");
    bad=status;bad.height=4097;reject_status(bad,"oversized frame transactional");
    bad=status;bad.reason.assign(513,'x');reject_status(bad,"reason cap transactional");
    for(const char* state:{"waiting","unavailable","stopped"}){
        status={};status.state=state;
        need(encode_preview_status(status,json,error)&&error.empty()&&
             json.find("\"slot\"")==std::string::npos&&
             json.find("\"width\":0,\"height\":0")!=std::string::npos,
             "non-frame JSON has no slot or dimensions");
        bad=status;bad.slot='a';reject_status(bad,"non-frame slot rejected");
        bad=status;bad.width=1;reject_status(bad,"non-frame dimensions rejected");
    }
    std::puts("native11 preview export test passed");
}
