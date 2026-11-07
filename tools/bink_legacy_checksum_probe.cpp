// Headless oracle: invoke only madExcept.CalcCheckSum, never DLL/game startup.
// The caller hash-gates the DLL. DONT_RESOLVE_DLL_REFERENCES avoids entry points.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
static std::uint32_t native_sum(void* entry,const unsigned char* bytes,
                                std::uint32_t count,std::uint32_t initial) {
    std::uint32_t result;
    __asm__ volatile("call *%3" : "=a"(result), "+d"(count), "+c"(initial)
                     : "r"(entry), "0"(bytes) : "memory", "cc");
    return result;
}
int main(int argc,char**argv) {
    if(argc!=2)return 2;
    std::ifstream input(argv[1],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)),{});
    if(bytes.size()<0x1000||bytes.size()>16*1024*1024)return 2;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||std::size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS32)>bytes.size())return 2;
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||nt->OptionalHeader.SizeOfImage!=0x322000)return 2;
    const std::size_t field=std::size_t(dos->e_lfanew)+24+64;
    std::uint32_t stored;std::memcpy(&stored,bytes.data()+field,4);
    HMODULE module=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!module){std::fprintf(stderr,"map failed %lu\n",GetLastError());return 3;}
    auto* entry=reinterpret_cast<unsigned char*>(module)+0x6c14c;
    const unsigned char signature[]={0x53,0x56,0x8b,0xda,0xd1,0xeb};
    if(std::memcmp(entry,signature,sizeof(signature))||entry[91]!=0xc3){FreeLibrary(module);return 3;}
    auto sum=native_sum(entry,bytes.data(),static_cast<std::uint32_t>(field),0);
    sum=native_sum(entry,bytes.data()+field+4,static_cast<std::uint32_t>(bytes.size()-field-4),sum);
    sum+=static_cast<std::uint32_t>(bytes.size());
    // The pinned CheckFileCrc sets BL=1 on unequal checksum, and its final
    // test/jne skips the error path when BL=1. No error UI is invoked here.
    std::printf("{\"stored\":%u,\"native_computed\":%u,\"legacy_error_path\":%s}\n",stored,sum,stored==sum?"true":"false");
    FreeLibrary(module);return 0;
}
