#include "bc_catalog.hpp"
#include "../../../rust/render_cache_state_ffi.h"

#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <unordered_set>
#include <utility>
#include <vector>

namespace a2fo::renderer::game {
namespace {
struct Entry {
    std::string name;
    UINT width=0,height=0,mips=0;
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    bool normal=false;
    std::uint64_t top_hash=0,file_hash=0;
    std::vector<std::uint64_t> mip_hashes;
};
std::vector<Entry> catalog;
std::vector<A2FoBcEntryView> catalog_views;
std::string root,root_identity;
bool enabled=false;
std::string span_string(A2FoRenderSpan span){return span.size?
    std::string(reinterpret_cast<const char*>(span.data),span.size):std::string{};}
A2FoRenderSpan span(const std::string& value){return {
    reinterpret_cast<const std::uint8_t*>(value.data()),value.size()};}

bool read_file(const std::string& path,std::size_t max_size,
               std::vector<std::uint8_t>& bytes) {
    // The native handle bypasses the game's hooked CRT fopen.
    HANDLE file=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ,
                            nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};
    if(!GetFileSizeEx(file,&length)||length.QuadPart<0||
       static_cast<std::uint64_t>(length.QuadPart)>max_size){CloseHandle(file);return false;}
    bytes.resize(static_cast<std::size_t>(length.QuadPart));
    DWORD received=0;
    const bool ok=bytes.empty()||
        (ReadFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&received,nullptr)&&
         received==bytes.size());
    CloseHandle(file);return ok;
}
struct AssetContext {
    textures::CompressedImage& out;
    bool& normal;
    std::string& asset;
    std::string& error;
    std::vector<std::uint8_t> file;
    textures::CompressedImage image;
    std::string parse_error;
    std::exception_ptr exception;
};
int asset_call(void* raw,std::uint32_t operation,A2FoRenderSpan input,
               A2FoBcReply* reply) noexcept {
    auto& c=*static_cast<AssetContext*>(raw);
    try {
        switch(operation) {
        case A2FO_BC_SET_ERROR:c.error=span_string(input);break;
        case A2FO_BC_READ_CANONICAL: {
            if(input.size!=sizeof(A2FoBcReadRequest))return 0;
            A2FoBcReadRequest request{};std::memcpy(&request,input.data,sizeof(request));
            c.file.clear();
            reply->value=read_file(span_string(request.path),request.max_size,c.file);
            reply->span={c.file.data(),c.file.size()};break;
        }
        case A2FO_BC_PARSE_DDS: {
            c.parse_error.clear();
            reply->image.hr=textures::parse_compressed_dds(c.file.data(),c.file.size(),
                                                           c.image,c.parse_error);
            reply->image.format=static_cast<UINT>(c.image.format);
            reply->image.width=c.image.width;reply->image.height=c.image.height;
            reply->image.alpha=static_cast<UINT>(c.image.alpha_mode);
            reply->image.mip_count=c.image.mips.size();
            for(std::size_t i=0;i<c.image.mips.size()&&i<15;++i){const auto& m=c.image.mips[i];
                reply->image.mips[i]={m.width,m.height,m.row_pitch,m.slice_pitch,m.offset};}
            reply->image.bytes={c.image.bytes.data(),c.image.bytes.size()};
            reply->image.parse_error=span(c.parse_error);break;
        }
        case A2FO_BC_PUBLISH_IMAGE: {
            if(input.size!=sizeof(A2FoBcPublishImage))return 0;
            A2FoBcPublishImage published{};std::memcpy(&published,input.data,sizeof(published));
            c.out=std::move(c.image);c.normal=published.normal!=0;
            c.asset=span_string(published.name);break;
        }
        case A2FO_BC_PUBLISH_CHAIN: {
            if(input.size!=sizeof(A2FoBcChain))return 0;
            A2FoBcChain chain{};std::memcpy(&chain,input.data,sizeof(chain));
            textures::CompressedImage image;
            image.format=static_cast<DXGI_FORMAT>(chain.format);
            image.width=chain.width;image.height=chain.height;
            image.alpha_mode=static_cast<textures::AlphaMode>(chain.alpha);
            if(chain.bytes.size)image.bytes.assign(chain.bytes.data,
                chain.bytes.data+chain.bytes.size);
            image.mips.reserve(chain.mip_count);
            for(std::size_t i=0;i<chain.mip_count;++i){const auto& m=chain.mips[i];
                image.mips.push_back({m.width,m.height,m.row_pitch,m.slice_pitch,m.offset});}
            c.out=std::move(image);c.normal=chain.normal!=0;
            c.asset=span_string(chain.name);break;
        }
        default:return 0;
        }
        return 1;
    }catch(...){c.exception=std::current_exception();return 0;}
}
}
bool load_bc_catalog(const std::string& modroot,void (*log)(const std::string&),
                     std::string& error) {
    struct Context {
        std::ifstream input;
        std::string line;
        std::vector<Entry> parsed;
        std::unordered_set<std::string> names;
        std::string& error;
        void (*log)(const std::string&);
        std::exception_ptr exception;
    } c{{},{},{},{},error,log,{}};
    auto call=[](void* raw,std::uint32_t operation,A2FoRenderSpan input,
                 A2FoBcReply* reply) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);
        try {
            switch(operation) {
            case A2FO_BC_RESET:
                enabled=false;catalog.clear();catalog_views.clear();root.clear();break;
            case A2FO_BC_OPEN_CATALOG: {
                const std::string path=span_string(input)+".a2fo-dx11\\catalog.txt";
                c.input.open(path,std::ios::binary);reply->value=bool(c.input);break;
            }
            case A2FO_BC_NEXT_LINE:
                reply->value=bool(std::getline(c.input,c.line));
                if(reply->value)reply->span=span(c.line);
                break;
            case A2FO_BC_UNIQUE_NAME:
                reply->value=c.names.insert(span_string(input)).second;break;
            case A2FO_BC_STAGE_ENTRY: {
                if(input.size!=sizeof(A2FoBcEntry))return 0;
                A2FoBcEntry parsed{};std::memcpy(&parsed,input.data,sizeof(parsed));
                Entry entry;entry.name=span_string(parsed.name);
                entry.width=parsed.width;entry.height=parsed.height;entry.mips=parsed.mips;
                entry.format=static_cast<DXGI_FORMAT>(parsed.format);
                entry.normal=parsed.normal!=0;entry.top_hash=parsed.top_hash;
                entry.file_hash=parsed.file_hash;
                entry.mip_hashes.assign(parsed.mip_hashes,parsed.mip_hashes+parsed.mips);
                c.parsed.push_back(std::move(entry));break;
            }
            case A2FO_BC_PUBLISH_LOAD: {
                if(input.size!=sizeof(A2FoBcPublication))return 0;
                A2FoBcPublication publication{};
                std::memcpy(&publication,input.data,sizeof(publication));
                std::vector<A2FoBcEntryView> views;
                views.reserve(c.parsed.size());
                for(const Entry& entry:c.parsed){views.push_back({span(entry.name),
                    entry.width,entry.height,entry.mips,static_cast<UINT>(entry.format),
                    entry.normal?1u:0u,entry.top_hash,entry.file_hash,
                    entry.mip_hashes.data()});}
                root=span_string(publication.directory);
                root_identity=span_string(publication.identity);
                catalog.swap(c.parsed);catalog_views.swap(views);enabled=true;
                c.error.clear();reply->value=1;
                if(c.log)c.log("[D3D8Compat] BC catalog loaded: 407 entries");
                break;
            }
            case A2FO_BC_SET_ERROR:c.error=span_string(input);break;
            default:return 0;
            }
            return 1;
        }catch(...){c.exception=std::current_exception();return 0;}
    };
    const int loaded=a2fo_rs_bc_load(span(modroot),enabled,span(root_identity),&c,call);
    if(c.exception)std::rethrow_exception(c.exception);
    if(loaded)error.clear();
    return loaded!=0;
}
bool load_bc_asset(const char* filename,textures::CompressedImage& out,
                   bool& normal,std::string& asset,std::string& error) {
    error.clear();
    AssetContext c{out,normal,asset,error,{},{},{},{}};
    const A2FoRenderSpan name{reinterpret_cast<const std::uint8_t*>(filename),
                              filename?std::strlen(filename):0};
    const int loaded=a2fo_rs_bc_asset(name,span(root),
        enabled?catalog_views.data():nullptr,enabled?catalog_views.size():0,&c,asset_call);
    if(c.exception)std::rethrow_exception(c.exception);
    return loaded!=0;
}
std::string bc_transport_path(const char* requestedfilename) {
    std::string path;
    struct Context {std::string& path;std::exception_ptr exception;} c{path,{}};
    auto call=[](void* raw,std::uint32_t operation,A2FoRenderSpan input,
                 A2FoBcReply*) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);
        if(operation!=A2FO_BC_PUBLISH_PATH)return 0;
        try {c.path=span_string(input);return 1;}
        catch(...){c.exception=std::current_exception();return 0;}
    };
    const A2FoRenderSpan name{reinterpret_cast<const std::uint8_t*>(requestedfilename),
                              requestedfilename?std::strlen(requestedfilename):0};
    a2fo_rs_bc_transport(name,span(root),enabled?catalog_views.data():nullptr,
                          enabled?catalog_views.size():0,&c,call);
    if(c.exception)std::rethrow_exception(c.exception);
    return path;
}
bool match_bc_texture(UINT width,UINT height,UINT mips,const void* base,
                      std::size_t size,textures::CompressedImage& out,
                      bool& normal,std::string& asset,std::string& error) {
    error.clear();
    AssetContext c{out,normal,asset,error,{},{},{},{}};
    const A2FoRenderSpan source{static_cast<const std::uint8_t*>(base),size};
    const int matched=a2fo_rs_bc_match(width,height,mips,source,span(root),
        enabled?catalog_views.data():nullptr,enabled?catalog_views.size():0,&c,asset_call);
    if(c.exception)std::rethrow_exception(c.exception);
    return matched!=0;
}
} // namespace a2fo::renderer::game
