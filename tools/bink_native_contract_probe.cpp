// Headless oracle for the pinned legacy Bink DLL. Never linked into the product.
// Registers a factory returning no audio provider; no windows or input hooks.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static void* __stdcall no_audio_provider(std::uint32_t) { return nullptr; }
template<class T> T symbol(HMODULE dll, const char* name, bool optional=false) {
    auto raw=GetProcAddress(dll,name);
    if(!raw&&!optional) { std::fprintf(stderr,"missing export %s\n",name); std::exit(2); }
    T value; static_assert(sizeof(value)==sizeof(raw)); std::memcpy(&value,&raw,sizeof(value)); return value;
}
int wmain(int argc,wchar_t** argv) {
    if(argc!=6) { std::fprintf(stderr,"DLL MOVIE NEW_OUTPUT_DIR FRAMES COPY_FLAGS\n"); return 2; }
    const unsigned frames=std::wcstoul(argv[4],nullptr,0), flags=std::wcstoul(argv[5],nullptr,0);
    if(frames>100000) return 2;
    // BinkOpen takes an ANSI name. Refuse lossy paths in this diagnostic.
    BOOL lossy=FALSE;
    int length=WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,argv[2],-1,nullptr,0,nullptr,&lossy);
    if(!length||lossy) return 2;
    std::string movie(length,'\0');
    if(!WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,argv[2],-1,movie.data(),length,nullptr,&lossy)||lossy) return 2;
    HMODULE dll=LoadLibraryW(argv[1]);
    if(!dll) { std::fprintf(stderr,"LoadLibrary error %lu\n",GetLastError()); return 2; }
    auto sound=symbol<int(__stdcall*)(void*,std::uint32_t)>(dll,"_BinkSetSoundSystem@8");
    auto open=symbol<void*(__stdcall*)(const char*,std::uint32_t)>(dll,"_BinkOpen@8");
    auto close=symbol<void(__stdcall*)(void*)>(dll,"_BinkClose@4");
    auto decode=symbol<int(__stdcall*)(void*)>(dll,"_BinkDoFrame@4");
    auto next=symbol<void(__stdcall*)(void*)>(dll,"_BinkNextFrame@4");
    auto go=symbol<void(__stdcall*)(void*,std::uint32_t,std::uint32_t)>(dll,"_BinkGoto@12");
    auto copy=symbol<int(__stdcall*)(void*,void*,std::int32_t,std::uint32_t,std::uint32_t,std::uint32_t,std::uint32_t)>(dll,"_BinkCopyToBuffer@28");
    auto get_error=symbol<const char*(__stdcall*)()>(dll,"_BinkGetError@0");
    auto open_track=symbol<void*(__stdcall*)(void*,std::uint32_t)>(dll,"_BinkOpenTrack@8",true);
    auto close_track=symbol<void(__stdcall*)(void*)>(dll,"_BinkCloseTrack@4",true);
    auto track_data=symbol<std::uint32_t(__stdcall*)(void*,void*)>(dll,"_BinkGetTrackData@8",true);
    const int sound_result=sound(reinterpret_cast<void*>(&no_audio_provider),0);
    void* handle=open(movie.c_str(),0);
    if(!handle) { std::fprintf(stderr,"BinkOpen: %s\n",get_error()); return 3; }
    auto* fields=static_cast<std::uint32_t*>(handle);
    const unsigned width=fields[0],height=fields[1],total=fields[2];
    if(!width||!height||width>4096||height>4096||!total) {close(handle);return 3;}
    if(!CreateDirectoryW(argv[3],nullptr)) {close(handle);return 2;}
    std::wstring prefix=argv[3];
    FILE* pixels=_wfopen((prefix+L"/pixels.bin").c_str(),L"wb");
    FILE* record=_wfopen((prefix+L"/metadata.json").c_str(),L"wb");
    if(!pixels||!record) {if(pixels)std::fclose(pixels);if(record)std::fclose(record);close(handle);return 2;}
    auto state=[&] {std::fprintf(record,"[");for(unsigned k=0;k<13;++k)std::fprintf(record,"%s%u",k?",":"",fields[k]);std::fprintf(record,"]");};
    std::fprintf(record,"{\"sound_factory_result\":%d,\"copy_flags\":%u,\"open_prefix\":",sound_result,flags);state();
    void* track=(open_track&&close_track&&track_data)?open_track(handle,0):nullptr;
    FILE* pcm=nullptr;
    std::vector<unsigned char> audio;
    if(track) {
        auto* tf=static_cast<std::uint32_t*>(track);
        if(!tf[3]||tf[3]>(16u<<20)) {close_track(track);close(handle);return 3;}
        audio.resize(tf[3]);
        pcm=_wfopen((prefix+L"/audio.bin").c_str(),L"wb");
        if(!pcm) {close_track(track);close(handle);return 2;}
        std::fprintf(record,",\"track_prefix\":[%u,%u,%u,%u]",tf[0],tf[1],tf[2],tf[3]);
    }
    std::fprintf(record,",\"frames\":[");
    const std::size_t pitch=width*4,body=pitch*height;
    std::vector<unsigned char> output(body+128,0xcd);
    int status=0;
    for(unsigned i=0;i<(frames?frames:total)&&i<total;++i) {
        const int dr=decode(handle);
        const std::uint32_t audio_count=track ? track_data(track,audio.data()) : 0;
        if(audio_count>audio.size() || (audio_count && std::fwrite(audio.data(),1,audio_count,pcm)!=audio_count)){status=4;break;}
        std::fill(output.begin(),output.end(),0xcd);
        const bool do_copy=flags!=0xffffffffu;
        const int cr=do_copy ? copy(handle,output.data()+64,static_cast<int>(pitch),height,0,0,flags) : 0;
        bool guards=true;for(unsigned j=0;j<64;++j)guards=guards&&output[j]==0xcd&&output[body+64+j]==0xcd;
        std::fprintf(record,"%s{\"decode_return\":%d,\"copy_return\":%d,\"audio_bytes\":%u,\"guards\":%s,\"prefix\":",i?",":"",dr,cr,audio_count,guards?"true":"false");state();std::fprintf(record,"}");
        if(!guards||(do_copy&&std::fwrite(output.data()+64,1,body,pixels)!=body)){status=4;break;}
        next(handle);
    }
    std::fprintf(record,"],\"after_next_prefix\":");state();
    go(handle,1,0);
    std::fprintf(record,",\"after_goto_one_prefix\":");state();std::fprintf(record,"}\n");
    if(pcm)std::fclose(pcm);if(track)close_track(track);
    std::fclose(pixels);std::fclose(record);close(handle);FreeLibrary(dll);return status;
}
