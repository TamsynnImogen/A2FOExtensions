#include "native11_preview_export.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <limits>

namespace a2fo::native11_preview_export {
namespace {
volatile LONG busy=0,enabled=0;
HANDLE lock_file=INVALID_HANDLE_VALUE;
DWORD owner=0;
std::string directory,last_error;
Log logger=nullptr;
std::uint32_t interval=1000;
std::uint64_t last_tick=0,serial=0;
bool sampled=false;
char last_slot='b';
constexpr char marker_name[]=".a2fo-preview-v1";
constexpr char marker_bytes[]="A2FO Native11 observed preview v1\n";
struct Admission {bool entered=InterlockedCompareExchange(&busy,1,0)==0;
    ~Admission(){if(entered)InterlockedExchange(&busy,0);}};
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
void report(const std::string& error) noexcept {
    try {if(error!=last_error){last_error=error;if(logger){
        const auto message="Native11 preview export: "+error;logger(message.c_str());}}}catch(...){}
}
bool ordinary(const std::string& path,bool missing=true) noexcept {
    const auto flags=GetFileAttributesA(path.c_str());
    return flags==INVALID_FILE_ATTRIBUTES?missing:
        !(flags&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY));
}
bool atomic_write(const char* name,const void* bytes,std::size_t size,std::string& error){
    if(size>64u*1024u*1024u){error="export exceeds byte bound";return false;}
    const auto target=directory+"\\"+name,temp=directory+"\\write.tmp";
    if(!ordinary(target)||!ordinary(temp)){error="export leaf is not an ordinary file";return false;}
    if(GetFileAttributesA(temp.c_str())!=INVALID_FILE_ATTRIBUTES&&!DeleteFileA(temp.c_str())){
        error="old private export temporary is busy";return false;
    }
    File file;file.value=CreateFileA(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if(file.value==INVALID_HANDLE_VALUE){error="cannot create export temporary";return false;}
    DWORD written=0;
    const bool good=WriteFile(file.value,bytes,static_cast<DWORD>(size),&written,nullptr)&&
        written==size&&FlushFileBuffers(file.value);
    CloseHandle(file.value);file.value=INVALID_HANDLE_VALUE;
    if(!good){DeleteFileA(temp.c_str());error="cannot write complete export";return false;}
    if(!ordinary(target)||!MoveFileExA(temp.c_str(),target.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        DeleteFileA(temp.c_str());error="cannot publish export atomically";return false;
    }
    return true;
}
bool write_status(native11::PreviewExportStatus status,std::string& error){
    status.serial=serial;
    if(status.reason.size()>512)status.reason.resize(512);
    if(status.state!="frame"){status.width=status.height=0;status.slot=0;}
    std::string json;
    return native11::encode_preview_status(status,json,error)&&
        atomic_write("status.json",json.data(),json.size(),error);
}
void close_locked() noexcept {
    if(lock_file!=INVALID_HANDLE_VALUE){
        try {native11::PreviewExportStatus status;status.state="stopped";
            status.reason="Capture stopped";std::string error;
            if(!write_status(status,error))report(error);}catch(...){}
        CloseHandle(lock_file);lock_file=INVALID_HANDLE_VALUE;
    }
    InterlockedExchange(&enabled,0);owner=0;directory.clear();sampled=false;
}
bool prepare_directory(const std::string& path,std::string& error){
    if(path.size()<4||path.size()>32000||path[1]!=':'||path[2]!='\\'||
       path.find("..")!=std::string::npos||path.back()=='\\'){
        error="invalid absolute export directory";return false;
    }
    for(unsigned char c:path)if(c<32){error="invalid export directory character";return false;}
    for(std::size_t pos=3;pos<=path.size();++pos){
        if(pos!=path.size()&&path[pos]!='\\')continue;
        const auto prefix=path.substr(0,pos);auto flags=GetFileAttributesA(prefix.c_str());
        if(flags==INVALID_FILE_ATTRIBUTES&&pos==path.size()&&CreateDirectoryA(prefix.c_str(),nullptr))
            flags=GetFileAttributesA(prefix.c_str());
        // The executable's ancestors may be volume mount points/junctions.
        // Wine also marks Unix mounts as reparse points without a reparse tag.
        // The owned output directory itself must remain an ordinary directory;
        // atomic_write separately refuses every reparse-point output leaf.
        if(flags==INVALID_FILE_ATTRIBUTES||!(flags&FILE_ATTRIBUTE_DIRECTORY)||
           (pos==path.size()&&(flags&FILE_ATTRIBUTE_REPARSE_POINT))){
            error="invalid export directory component: "+prefix;return false;
        }
    }
    directory=path;const auto marker=directory+"\\"+marker_name;
    if(GetFileAttributesA(marker.c_str())==INVALID_FILE_ATTRIBUTES){
        WIN32_FIND_DATAA found{};const auto search=directory+"\\*";
        HANDLE scan=FindFirstFileA(search.c_str(),&found);bool empty=true;
        if(scan!=INVALID_HANDLE_VALUE){do{
            if(std::strcmp(found.cFileName,".")&&std::strcmp(found.cFileName,"..")){empty=false;break;}
        }while(FindNextFileA(scan,&found));FindClose(scan);}
        if(!empty){error="export directory contains unowned files";return false;}
        if(!atomic_write(marker_name,marker_bytes,sizeof(marker_bytes)-1,error))return false;
    }
    if(!ordinary(marker,false)){error="invalid export ownership marker";return false;}
    File file;file.value=CreateFileA(marker.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,nullptr);char bytes[sizeof(marker_bytes)]{};DWORD got=0;
    if(file.value==INVALID_HANDLE_VALUE||!ReadFile(file.value,bytes,sizeof(bytes),&got,nullptr)||
       got!=sizeof(marker_bytes)-1||std::memcmp(bytes,marker_bytes,got)){
        error="export ownership marker mismatch";return false;
    }
    const auto lock_path=directory+"\\capture.lock";
    if(!ordinary(lock_path)){error="invalid export lock leaf";return false;}
    lock_file=CreateFileA(lock_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
    if(lock_file==INVALID_HANDLE_VALUE){error="another process owns this export directory";return false;}
    return true;
}
}
bool requested() noexcept {return InterlockedCompareExchange(&enabled,0,0)!=0;}
bool configure_at(const std::string& path,std::uint32_t delay,Log log,std::string& error) noexcept {
    Admission guard;if(!guard.entered){error="preview exporter is busy";return false;}
    try {
        close_locked();logger=log;last_error.clear();
        if(delay<250||delay>10000){error="LivePreviewIntervalMs must be 250..10000";return false;}
        if(!prepare_directory(path,error)){close_locked();return false;}
        interval=delay;last_tick=0;sampled=false;last_slot='b';owner=0;
        FILETIME now{};GetSystemTimeAsFileTime(&now);
        serial=(std::uint64_t(now.dwHighDateTime)<<32)|now.dwLowDateTime;
        native11::PreviewExportStatus status;status.state="waiting";
        status.reason="Start a skirmish to inspect observed DX11 ships";
        if(!write_status(status,error)){close_locked();return false;}
        InterlockedExchange(&enabled,1);error.clear();return true;
    }catch(...){close_locked();try{error="preview exporter setup failed";}catch(...){}return false;}
}
bool configure(bool value,std::uint32_t delay,Log log) noexcept {
    if(!value)return stop();
    try {
        char executable[32768]{};
        const auto length=GetModuleFileNameA(nullptr,executable,sizeof(executable));
        if(!length||length>=sizeof(executable))return false;
        std::string path(executable,length);const auto slash=path.find_last_of('\\');
        if(slash==std::string::npos)return false;
        path.resize(slash);path+="\\Native11Preview";std::string error;
        if(configure_at(path,delay,log,error))return true;
        if(log)log(("Native11 preview export setup failed: "+error).c_str());
        return false;
    }catch(...){return false;}
}
void sample(native11::PreviewExportStatus status,Readback read,Current current,void* context,
    std::uint64_t now) noexcept {
    Admission guard;if(!guard.entered||!requested()||
       (owner&&owner!=GetCurrentThreadId()))return;
    owner=GetCurrentThreadId();
    if(sampled&&now>=last_tick&&now-last_tick<interval)return;
    sampled=true;last_tick=now;
    try {
        std::string error;
        if(status.state=="frame"){
            std::vector<std::uint8_t> bgra,bmp;std::uint32_t width=0,height=0;
            if(!read||!current||!current(context)){
                status.state="waiting";status.reason="Observed material source changed";
            }else if(!read(context,bgra,width,height,error)){
                status.state="unavailable";status.reason=error.empty()?"DX11 preview readback unavailable":error;
            }else if(!current(context)){
                status.state="waiting";status.reason="Observed material source changed during readback";
            }else if(!native11::encode_preview_bmp(bgra,width,height,bmp,error)){
                status.state="unavailable";status.reason=error;
            }else{
                status.width=width;status.height=height;status.slot=last_slot=='a'?'b':'a';
                if(serial==std::numeric_limits<std::uint64_t>::max()){
                    status.state="unavailable";status.reason="Preview sequence exhausted";
                }else if(!atomic_write(status.slot=='a'?"frame-a.bmp":"frame-b.bmp",bmp.data(),bmp.size(),error)){
                    status.state="unavailable";status.reason=error;
                }else if(!current(context)){
                    status.state="waiting";status.reason="Observed material source changed before publication";
                }else{
                    ++serial;
                    if(write_status(status,error)){
                        last_slot=status.slot;
                        if(current(context)){report("frame exported");return;}
                        status.state="waiting";status.reason="Observed material source revoked after publication";
                    }else {report(error);return;}
                }
            }
        }
        if(!write_status(status,error))report(error);
        else report(status.reason);
    }catch(...){report("preview export allocation or callback failure");}
}
bool stop() noexcept {
    Admission guard;if(!guard.entered)return false;
    close_locked();return true;
}
}
