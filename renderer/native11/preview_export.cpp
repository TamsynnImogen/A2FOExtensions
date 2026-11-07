#include "preview_export.hpp"
#include <cstddef>

namespace a2fo::native11 {
namespace {
constexpr std::uint64_t max_bytes=64ull*1024*1024;
void fail(std::string& error,const char* message) noexcept {
    try { error=message; } catch(...) { /* Preserve noexcept. */ }
}
void u16(std::vector<std::uint8_t>& bytes,std::size_t pos,std::uint16_t value) noexcept {
    bytes[pos]=static_cast<std::uint8_t>(value);
    bytes[pos+1]=static_cast<std::uint8_t>(value>>8);
}
void u32(std::vector<std::uint8_t>& bytes,std::size_t pos,std::uint32_t value) noexcept {
    for(unsigned i=0;i<4;++i) bytes[pos+i]=static_cast<std::uint8_t>(value>>(i*8));
}
void quoted(std::string& output,const std::string& value) {
    constexpr char hex[]="0123456789abcdef";
    output.push_back('"');
    for(unsigned char c:value){
        if(c=='"'||c=='\\') { output.push_back('\\');output.push_back(static_cast<char>(c)); }
        else if(c>=0x20&&c<=0x7e) output.push_back(static_cast<char>(c));
        else {
            output+="\\u00";
            output.push_back(hex[c>>4]);output.push_back(hex[c&15]);
        }
    }
    output.push_back('"');
}
} // namespace

bool encode_preview_bmp(const std::vector<std::uint8_t>& bgra,
    std::uint32_t width,std::uint32_t height,
    std::vector<std::uint8_t>& output,std::string& error) noexcept {
    if(!width||!height||width>4096||height>4096){fail(error,"invalid preview dimensions");return false;}
    const std::uint64_t input_bytes=std::uint64_t(width)*height*4;
    const std::uint64_t row_bytes=(std::uint64_t(width)*3+3)&~std::uint64_t(3);
    const std::uint64_t pixel_bytes=row_bytes*height;
    const std::uint64_t file_bytes=54+pixel_bytes;
    if(input_bytes>max_bytes||file_bytes>max_bytes||bgra.size()!=input_bytes){
        fail(error,"invalid preview byte count");return false;
    }
    try {
        std::vector<std::uint8_t> pending(static_cast<std::size_t>(file_bytes),0);
        pending[0]='B';pending[1]='M';
        u32(pending,2,static_cast<std::uint32_t>(file_bytes));
        u32(pending,10,54);u32(pending,14,40);
        u32(pending,18,width);
        u32(pending,22,static_cast<std::uint32_t>(-static_cast<std::int32_t>(height)));
        u16(pending,26,1);u16(pending,28,24);
        u32(pending,34,static_cast<std::uint32_t>(pixel_bytes));
        for(std::uint32_t y=0;y<height;++y){
            const std::size_t source=std::size_t(y)*width*4;
            const std::size_t target=54+std::size_t(y)*static_cast<std::size_t>(row_bytes);
            for(std::uint32_t x=0;x<width;++x){
                pending[target+3*x]=bgra[source+4*x];
                pending[target+3*x+1]=bgra[source+4*x+1];
                pending[target+3*x+2]=bgra[source+4*x+2];
            }
        }
        output.swap(pending);error.clear();return true;
    }catch(...){fail(error,"preview BMP allocation failure");return false;}
}

bool encode_preview_status(const PreviewExportStatus& status,
    std::string& output,std::string& error) noexcept {
    const bool frame=status.state=="frame";
    if(status.state!="waiting"&&status.state!="unavailable"&&!frame&&status.state!="stopped"){
        fail(error,"invalid preview state");return false;
    }
    if(status.reason.size()>512||
       (frame?(!status.width||!status.height||status.width>4096||status.height>4096||
               (status.slot!='a'&&status.slot!='b')):
              (status.width||status.height||status.slot))){
        fail(error,"invalid preview status fields");return false;
    }
    try {
        std::string pending;pending.reserve(768);
        pending+="{\"state\":";quoted(pending,status.state);
        pending+=",\"reason\":";quoted(pending,status.reason);
        pending+=",\"serial\":";quoted(pending,std::to_string(status.serial));
        pending+=",\"frame\":";quoted(pending,std::to_string(status.frame));
        pending+=",\"source_epoch\":";quoted(pending,std::to_string(status.source_epoch));
        pending+=",\"source_ticket\":"+std::to_string(status.source_ticket);
        pending+=",\"objects\":"+std::to_string(status.objects);
        pending+=",\"width\":"+std::to_string(status.width);
        pending+=",\"height\":"+std::to_string(status.height);
        if(frame){pending+=",\"slot\":\"";pending.push_back(status.slot);pending.push_back('"');}
        pending+='}';
        output.swap(pending);error.clear();return true;
    }catch(...){fail(error,"preview status allocation failure");return false;}
}
} // namespace a2fo::native11
