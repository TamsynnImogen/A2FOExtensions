#include "../core/native11_preview_export.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace a2fo;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::vector<std::uint8_t> file(const std::string& path){
    HANDLE handle=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,nullptr);
    need(handle!=INVALID_HANDLE_VALUE,"open private export file");
    const DWORD length=GetFileSize(handle,nullptr);need(length!=INVALID_FILE_SIZE&&length<1024*1024,
        "bounded private export file");
    std::vector<std::uint8_t> out(length);DWORD got=0;
    need(ReadFile(handle,out.data(),length,&got,nullptr)&&got==length,"read complete private export file");
    CloseHandle(handle);return out;
}
std::string text(const std::string& path){const auto bytes=file(path);return {bytes.begin(),bytes.end()};}
void write(const std::string& path,const char* value){
    HANDLE handle=CreateFileA(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,nullptr);need(handle!=INVALID_HANDLE_VALUE,"create private test file");
    DWORD written=0;const DWORD size=static_cast<DWORD>(std::strlen(value));
    need(WriteFile(handle,value,size,&written,nullptr)&&written==size,"write private test file");
    CloseHandle(handle);
}
void remove_if_file(const std::string& path){
    const DWORD attributes=GetFileAttributesA(path.c_str());
    if(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY))
        need(DeleteFileA(path.c_str())!=0,"remove exact private test file");
}
struct PrivateRoot {
    std::string path,export_dir,unowned,bad;
    explicit PrivateRoot(const char* supplied){
        char temp[MAX_PATH]{};
        std::string base;
        if(supplied){base=supplied;need(!base.empty()&&base.size()<MAX_PATH-1,"bounded supplied base directory");
            if(base.back()!='\\'&&base.back()!='/')base+='\\';
        }else {need(GetTempPathA(MAX_PATH,temp)>0,"private temp path");base=temp;}
        char unique[MAX_PATH]{};need(GetTempFileNameA(base.c_str(),"n11",0,unique)!=0,"private temp name");
        need(DeleteFileA(unique)&&CreateDirectoryA(unique,nullptr),"private root created");
        path=unique;export_dir=path+"\\owned";unowned=path+"\\unowned";bad=path+"\\bad";
    }
    void clean(){
        for(const auto& leaf:{"frame-a.bmp","frame-b.bmp","status.json","capture.lock",
                              "write.tmp",".a2fo-preview-v1"})remove_if_file(export_dir+"\\"+leaf);
        need(RemoveDirectoryA(export_dir.c_str())!=0,"remove owned export directory");
        remove_if_file(unowned+"\\keep.txt");need(RemoveDirectoryA(unowned.c_str())!=0,"remove unowned test directory");
        remove_if_file(bad+"\\.a2fo-preview-v1");need(RemoveDirectoryA(bad.c_str())!=0,"remove bad-marker test directory");
        need(RemoveDirectoryA(path.c_str())!=0,"remove private root");path.clear();
    }
    ~PrivateRoot(){if(!path.empty())std::fprintf(stderr,"private test root retained: %s\n",path.c_str());}
};
struct Source {
    unsigned reads=0,current_calls=0,reject_at=0;
    bool active=true,revoke_in_read=false;
    static bool read(void* raw,std::vector<std::uint8_t>& bytes,std::uint32_t& width,
        std::uint32_t& height,std::string&){
        auto& source=*static_cast<Source*>(raw);++source.reads;
        bytes={0,0,255,255,255,0,0,255};width=2;height=1;
        if(source.revoke_in_read)source.active=false;
        return true;
    }
    static bool current(void* raw) noexcept {
        auto& source=*static_cast<Source*>(raw);++source.current_calls;
        return source.active&&source.current_calls!=source.reject_at;
    }
};
native11::PreviewExportStatus frame(std::uint64_t number){
    native11::PreviewExportStatus status;status.state="frame";status.reason="synthetic owned preview";
    status.frame=number;status.source_epoch=20;status.source_ticket=3;status.objects=2;return status;
}
struct Foreign {Source* source;std::uint64_t tick;};
DWORD WINAPI foreign_sample(void* raw){
    auto& input=*static_cast<Foreign*>(raw);
    native11_preview_export::sample(frame(9),Source::read,Source::current,input.source,input.tick);
    return 0;
}
}
int main(int argc,char** argv){
    need(argc==1||argc==2,"usage: native11_live_preview_export_native.exe [BASE_DIRECTORY]");
    PrivateRoot root(argc==2?argv[1]:nullptr);std::string error;
    const bool configured=native11_preview_export::configure_at(root.export_dir,1000,nullptr,error);
    if(!configured)std::fprintf(stderr,"configure_at(%s): %s\n",root.export_dir.c_str(),error.c_str());
    need(configured&&
         native11_preview_export::requested()&&
         text(root.export_dir+"\\status.json").find("\"state\":\"waiting\"")!=std::string::npos,
         "owned empty directory starts with waiting status");
    Source source;
    native11_preview_export::sample(frame(1),Source::read,Source::current,&source,0);
    auto status=text(root.export_dir+"\\status.json");
    const auto first=file(root.export_dir+"\\frame-a.bmp");
    need(status.find("\"state\":\"frame\"")!=std::string::npos&&
         status.find("\"slot\":\"a\"")!=std::string::npos&&
         status.find("\"frame\":\"1\"")!=std::string::npos&&
         first.size()==62&&first[0]=='B'&&first[1]=='M'&&
         first[54]==0&&first[55]==0&&first[56]==255&&
         first[57]==255&&first[58]==0&&first[59]==0,
         "two-pixel top-down BMP and frame-a status published");
    const auto reads=source.reads;
    native11_preview_export::sample(frame(2),Source::read,Source::current,&source,100);
    need(source.reads==reads&&text(root.export_dir+"\\status.json")==status,
         "sub-interval sample does not read or republish");
    native11_preview_export::sample(frame(2),Source::read,Source::current,&source,1000);
    status=text(root.export_dir+"\\status.json");
    need(source.reads==reads+1&&status.find("\"slot\":\"b\"")!=std::string::npos&&
         file(root.export_dir+"\\frame-b.bmp")==first,
         "cadence publishes alternating second BMP");
    source.active=false;const auto before_false=source.reads;
    native11_preview_export::sample(frame(3),Source::read,Source::current,&source,2000);
    status=text(root.export_dir+"\\status.json");
    need(source.reads==before_false&&status.find("\"state\":\"waiting\"")!=std::string::npos&&
         status.find("\"slot\"")==std::string::npos,
         "refuted source before read cannot advertise a frame");
    source.active=true;source.revoke_in_read=true;
    native11_preview_export::sample(frame(4),Source::read,Source::current,&source,3000);
    status=text(root.export_dir+"\\status.json");
    need(status.find("\"state\":\"waiting\"")!=std::string::npos&&
         status.find("\"slot\"")==std::string::npos,
         "source revocation during read cannot advertise a frame");
    source.active=true;source.revoke_in_read=false;source.current_calls=0;source.reject_at=3;
    native11_preview_export::sample(frame(5),Source::read,Source::current,&source,4000);
    status=text(root.export_dir+"\\status.json");
    need(source.current_calls>=3&&status.find("\"state\":\"waiting\"")!=std::string::npos&&
         status.find("\"slot\"")==std::string::npos,
         "third publication checkpoint revokes written but unadvertised BMP");
    source.reject_at=0;source.current_calls=0;
    need(DeleteFileA((root.export_dir+"\\frame-a.bmp").c_str())&&
         CreateDirectoryA((root.export_dir+"\\frame-a.bmp").c_str(),nullptr),
         "replace next BMP target with directory");
    native11_preview_export::sample(frame(6),Source::read,Source::current,&source,5000);
    status=text(root.export_dir+"\\status.json");
    need(status.find("\"state\":\"unavailable\"")!=std::string::npos&&
         status.find("\"slot\"")==std::string::npos,
         "non-ordinary BMP target refuses publication");
    need(RemoveDirectoryA((root.export_dir+"\\frame-a.bmp").c_str())!=0,
         "remove private BMP target directory");
    const auto before_foreign=source.reads;
    Foreign foreign{&source,6000};HANDLE thread=CreateThread(nullptr,0,foreign_sample,&foreign,0,nullptr);
    need(thread!=nullptr&&WaitForSingleObject(thread,5000)==WAIT_OBJECT_0,
         "bounded foreign thread sample attempt");
    CloseHandle(thread);
    need(source.reads==before_foreign&&text(root.export_dir+"\\status.json")==status,
         "foreign thread cannot sample or publish");
    need(native11_preview_export::stop()&&!native11_preview_export::requested()&&
         text(root.export_dir+"\\status.json").find("\"state\":\"stopped\"")!=std::string::npos,
         "stop writes status and releases exporter");
    need(CreateDirectoryA(root.unowned.c_str(),nullptr),"create unowned test directory");
    write(root.unowned+"\\keep.txt","preserve");
    need(!native11_preview_export::configure_at(root.unowned,1000,nullptr,error)&&
         text(root.unowned+"\\keep.txt")=="preserve"&&
         !native11_preview_export::requested(),"unowned directory refused without mutation");
    need(CreateDirectoryA(root.bad.c_str(),nullptr),"create bad-marker test directory");
    write(root.bad+"\\.a2fo-preview-v1","wrong owner\n");
    need(!native11_preview_export::configure_at(root.bad,1000,nullptr,error)&&
         text(root.bad+"\\.a2fo-preview-v1")=="wrong owner\n",
         "foreign marker rejected and retained");
    const auto alias=root.path+"\\output-link";
    if(CreateSymbolicLinkA(alias.c_str(),root.export_dir.c_str(),
        SYMBOLIC_LINK_FLAG_DIRECTORY|SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)){
        const auto attributes=GetFileAttributesA(alias.c_str());
        if(attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT)){
            const auto retained=text(root.export_dir+"\\status.json");
            need(!native11_preview_export::configure_at(alias,1000,nullptr,error)&&
                 text(root.export_dir+"\\status.json")==retained,
                 "reparse output directory is refused without writing its target");
        }else std::printf("SKIP symbolic-link API did not materialize a reparse point: %s attrs=%08lx\n",
            alias.c_str(),static_cast<unsigned long>(attributes));
        if(attributes!=INVALID_FILE_ATTRIBUTES)
            need((attributes&FILE_ATTRIBUTE_DIRECTORY)?RemoveDirectoryA(alias.c_str()):DeleteFileA(alias.c_str()),
                 "remove exact private output link");
    }else std::printf("SKIP output-link creation unavailable: %lu\n",static_cast<unsigned long>(GetLastError()));
    const auto lock_path=root.export_dir+"\\capture.lock";
    HANDLE held=CreateFileA(lock_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,nullptr);
    need(held!=INVALID_HANDLE_VALUE,"hold private capture lock");
    need(!native11_preview_export::configure_at(root.export_dir,1000,nullptr,error)&&
         !native11_preview_export::requested(),"held lock denies concurrent exporter");
    CloseHandle(held);
    need(native11_preview_export::configure_at(root.export_dir,1000,nullptr,error),
         "released lock permits owned exporter restart");
    need(native11_preview_export::stop(),"restart stops cleanly");
    root.clean();std::puts("PASS native11 private preview exporter WinFS guard");
}
