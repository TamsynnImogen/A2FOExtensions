// Headless: map fixtures as data, never execute Armada or automate input.
#include "../sdk/include/a2fo_supported_armada.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
using a2fo::supported_armada::Identity;
namespace {
using Bytes = std::vector<char>;
unsigned checks = 0;
bool write(const std::string& path, const std::string& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(bytes.data(), bytes.size()); return bool(f);
}
bool write(const std::string& path, const Bytes& bytes) {
    return write(path, std::string(bytes.begin(), bytes.end()));
}
bool check(const std::string& path, Identity wanted, const char* label) {
    HMODULE image = LoadLibraryExA(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!image) { std::fprintf(stderr,"FAIL map %s: %lu\n",label,GetLastError()); return false; }
    auto actual = a2fo::supported_armada::identify(image);
    FreeLibrary(image); ++checks;
    std::printf("%s: %s\n",actual == wanted ? "PASS" : "FAIL",label);
    return actual == wanted;
}
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc != 2) { std::fprintf(stderr,"Usage: roots_profile_smoke.exe OCT03_ARMADA_EXE\n"); return 2; }
    std::ifstream input(argv[1],std::ios::binary);
    Bytes original((std::istreambuf_iterator<char>(input)),{});
    if(original.size()<0x382999) return 3;
    const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(original.data());
    const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(original.data()+dos->e_lfanew);
    const auto* sections=IMAGE_FIRST_SECTION(nt);
    std::string profile="A2FO_ARMADA_PROFILES_V1\nroots_native_rotation 3c4c76bd 00405000";
    auto crc=[&](unsigned off,unsigned size){
        return a2fo::supported_armada::update_crc32(0xffffffffu,
            reinterpret_cast<const std::uint8_t*>(original.data()+off),size)^0xffffffffu;
    };
    char hex[16]{};
    std::snprintf(hex,sizeof(hex)," %08x",crc(0,0x1000)); profile+=hex;
    for(unsigned i=0;i<7;++i){std::snprintf(hex,sizeof(hex)," %08x",crc(sections[i].PointerToRawData,sections[i].SizeOfRawData));profile+=hex;}
    profile+='\n';
    char temp[MAX_PATH]{},dir[MAX_PATH]{};
    if(!GetTempPathA(MAX_PATH,temp)||!GetTempFileNameA(temp,"acp",0,dir))return 4;
    DeleteFileA(dir); if(!CreateDirectoryA(dir,nullptr))return 4;
    std::string image=std::string(dir)+"\\ArmadaL.exe",manifest=std::string(dir)+"\\A2FOArmadaProfiles.txt";
    bool ok=write(image,original)&&check(image,Identity::unsupported,"unapproved release rejected");
    ok=write(manifest,profile)&&check(image,Identity::roots_profile,"approved exact release")&&ok;
    const bool capability=a2fo::supported_armada::uses_native_station_rotation(Identity::roots_profile)&&
        a2fo::supported_armada::uses_native_station_rotation(Identity::janb_20260930)&&
        !a2fo::supported_armada::uses_native_station_rotation(Identity::janb_20260905);
    ok=capability&&ok;
    // Exercise every complete raw section, including padding beyond virtual size.
    for(unsigned i=0;i<7;++i){
        Bytes changed=original; changed[sections[i].PointerToRawData+sections[i].SizeOfRawData-1]^=1;
        char label[100];std::snprintf(label,sizeof(label),"changed %.8s raw tail rejected",sections[i].Name);
        ok=write(image,changed)&&check(image,Identity::unsupported,label)&&ok;
    }
    Bytes changed=original; changed[0x30]^=1;
    ok=write(image,changed)&&check(image,Identity::unsupported,"changed file header fingerprint rejected")&&ok;
    changed=original;
    const std::size_t flags=reinterpret_cast<const char*>(&sections[0].Characteristics)-original.data();
    changed[flags]^=1;
    ok=write(image,changed)&&check(image,Identity::unsupported,"changed section flags rejected")&&ok;
    ok=write(image,original)&&ok;
    const std::vector<std::string> bad={
        "", "A2FO_ARMADA_PROFILES_V2\n"+profile,
        "A2FO_ARMADA_PROFILES_V1\n", profile+"bad row\n",
        profile+"A2FO_ARMADA_PROFILES_V1\n",
        profile.substr(0,profile.size()-10)+"\n",
        profile+std::string(8200,'#'),
        profile+std::string("\0hidden",7),
    };
    for(const auto& b:bad)ok=write(manifest,b)&&check(image,Identity::unsupported,"malformed profile rejected")&&ok;
    auto corrupt=profile;auto at=corrupt.find("roots_native_rotation");corrupt.replace(at,21,"unchecked_capability");
    ok=write(manifest,corrupt)&&check(image,Identity::unsupported,"unknown capability rejected")&&ok;
    corrupt=profile;at=corrupt.find("3c4c76bd");corrupt[at]='4';
    ok=write(manifest,corrupt)&&check(image,Identity::unsupported,"unsupported family timestamp rejected")&&ok;
    corrupt=profile;at=corrupt.find("00405000");corrupt[at+7]='1';
    ok=write(manifest,corrupt)&&check(image,Identity::unsupported,"unsupported family size rejected")&&ok;
    corrupt=profile;corrupt[corrupt.size()-2]=corrupt[corrupt.size()-2]=='0'?'1':'0';
    ok=write(manifest,corrupt)&&check(image,Identity::unsupported,"stale profile fingerprint rejected")&&ok;
    const auto row=profile.substr(profile.find("roots_native_rotation"));
    std::string too_many="A2FO_ARMADA_PROFILES_V1\n";
    for(unsigned i=0;i<33;++i)too_many+=row;
    ok=write(manifest,too_many)&&check(image,Identity::unsupported,"excess profile rows rejected")&&ok;
    // A second reviewed resource-only update needs a new record, no DLL rebuild.
    changed=original;
    changed[sections[4].PointerToRawData+sections[4].SizeOfRawData-1]^=1;
    auto newer=profile;
    char previous[16]{},next[16]{};
    std::snprintf(previous,sizeof(previous),"%08x",crc(sections[4].PointerToRawData,sections[4].SizeOfRawData));
    auto newer_crc=a2fo::supported_armada::update_crc32(0xffffffffu,
        reinterpret_cast<const std::uint8_t*>(changed.data()+sections[4].PointerToRawData),
        sections[4].SizeOfRawData)^0xffffffffu;
    std::snprintf(next,sizeof(next),"%08x",newer_crc);
    newer.replace(newer.find(previous),8,next);
    const auto newer_row=newer.substr(newer.find("roots_native_rotation"));
    ok=write(image,changed)&&write(manifest,profile+newer_row)&&
        check(image,Identity::roots_profile,"second exact profile accepts reviewed resource update")&&ok;
    ok=write(image,original)&&check(image,Identity::roots_profile,"multiple profiles retain original approval")&&ok;
    ok=write(manifest,"# reviewed build\r\n"+profile+"# comment\r\n")&&check(image,Identity::roots_profile,"comments and CRLF accepted")&&ok;
    // No identity cache: removing approval must reject even in the same process.
    DeleteFileA(manifest.c_str());ok=check(image,Identity::unsupported,"removed approval rejected")&&ok;
    DeleteFileA(image.c_str());RemoveDirectoryA(dir);
    HMODULE invalid=reinterpret_cast<HMODULE>(1);
    ok=(a2fo::supported_armada::identify(invalid)==Identity::unsupported)&&ok;
    // Corrupt loaded headers cannot drive an out-of-bounds read.
    HMODULE mapped=LoadLibraryExA(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
    if(!mapped)return 5;
    auto* header=reinterpret_cast<IMAGE_DOS_HEADER*>(mapped);
    DWORD old=0;if(!VirtualProtect(header,0x1000,PAGE_READWRITE,&old))return 6;
    const LONG saved=header->e_lfanew;header->e_lfanew=0xfff;
    ok=(a2fo::supported_armada::identify(mapped)==Identity::unsupported)&&ok;
    header->e_lfanew=saved;
    auto* loaded_nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(reinterpret_cast<char*>(mapped)+saved);
    auto optional=loaded_nt->FileHeader.SizeOfOptionalHeader;
    loaded_nt->FileHeader.SizeOfOptionalHeader=0xfff;
    ok=(a2fo::supported_armada::identify(mapped)==Identity::unsupported)&&ok;
    loaded_nt->FileHeader.SizeOfOptionalHeader=optional;
    DWORD unused;VirtualProtect(header,0x1000,old,&unused);FreeLibrary(mapped);
    std::printf("%u mapped-image checks; capability and malformed-memory checks: %s\n",checks,ok?"PASS":"FAIL");
    return ok?0:1;
}
