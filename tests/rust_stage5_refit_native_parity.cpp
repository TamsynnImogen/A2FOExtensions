#include "../rust/refit_native_ffi.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

namespace {
struct Cache {
    A2FORefitGpuCacheState state{};
    std::vector<std::uint8_t> pixels;
    unsigned creates{},uploads{};
};
struct Mock {
    std::string trace;
    Cache cache[2];
    A2FORefitGpuCaps caps{0,4096,4096};
    int fail_create=-1,fail_bind=-1;
    unsigned drawings{},restores{};
    A2FORefitGpuVertex drawn[4][4]{};
    static Mock& self(void* p){return *static_cast<Mock*>(p);}
    static int caps_op(void* p,A2FORefitGpuCaps* out){auto& m=self(p);m.trace+='C';*out=m.caps;return 1;}
    static int state_op(void* p,int s,A2FORefitGpuCacheState* out){auto& m=self(p);m.trace+='S';*out=m.cache[s].state;return 1;}
    static void reset_op(void* p,int s){auto& m=self(p);m.trace+='X';m.cache[s].state={};m.cache[s].pixels.clear();}
    static int create_op(void* p,int s,uint32_t tw,uint32_t th,uint32_t w,uint32_t h){
        auto& m=self(p);m.trace+='T';if(m.fail_create==s)return 0;
        auto& c=m.cache[s];c.pixels.assign(static_cast<std::size_t>(tw)*th*4,0);
        c.state.present=1;c.state.device_match=1;c.state.width=w;c.state.height=h;
        c.state.texture_width=tw;c.state.texture_height=th;++c.creates;return 1;
    }
    static int lock_op(void* p,int s,uint8_t** bits,int32_t* pitch){
        auto& m=self(p);m.trace+='L';*bits=m.cache[s].pixels.data();
        *pitch=static_cast<int32_t>(m.cache[s].state.texture_width*4);return 1;
    }
    static int unlock_op(void* p,int){self(p).trace+='U';return 1;}
    static void mark_op(void* p,int s,uint64_t revision,int preserved,int success){
        auto& m=self(p);m.trace+=success?'m':'u';auto& c=m.cache[s];c.state.uploaded=success;
        if(success){c.state.revision=revision;c.state.alpha_preserved=preserved;++c.uploads;}
    }
    static int capture_op(void* p){self(p).trace+='B';return 1;}
    static void restore_op(void* p){auto& m=self(p);m.trace+='E';++m.restores;}
    static int viewport_op(void* p,uint32_t,uint32_t){self(p).trace+='V';return 1;}
    static int render_op(void* p,int,int){self(p).trace+='R';return 1;}
    static int pixel_op(void* p){self(p).trace+='P';return 1;}
    static int vertex_op(void* p){self(p).trace+='F';return 1;}
    static int bind_op(void* p,int s){auto& m=self(p);m.trace+='N';return m.fail_bind==s?0:1;}
    static int stage_op(void* p,int,int){self(p).trace+='G';return 1;}
    static int draw_op(void* p,const A2FORefitGpuVertex* vertices){
        auto& m=self(p);m.trace+='D';assert(m.drawings<4);
        for(int i=0;i<4;++i)m.drawn[m.drawings][i]=vertices[i];
        ++m.drawings;return 1;
    }
    A2FORefitGpuOps ops(){return {this,caps_op,state_op,reset_op,create_op,lock_op,
        unlock_op,mark_op,capture_op,restore_op,viewport_op,render_op,pixel_op,
        vertex_op,bind_op,stage_op,draw_op};}
};
void require(bool test,const char* message){if(!test){std::fprintf(stderr,"%s\n",message);std::abort();}}
void check_order(const std::string& trace,const std::string& expected,const char* message){
    require(trace==expected,message);
}
std::string repeated(char c,int n){return std::string(static_cast<std::size_t>(n),c);}
int skin_operation(void* context,int32_t op){
    auto& trace=*static_cast<std::string*>(context);
    trace.push_back(static_cast<char>('0'+op));return op!=2;
}
void skin_checks(){
    std::vector<uint8_t> atlas(32+128*256*4);
    const uint32_t header[8]={0x31534955,128,256,64,64,8,0,0};
    for(int i=0;i<8;++i)for(int b=0;b<4;++b)
        atlas[static_cast<std::size_t>(i*4+b)]=static_cast<uint8_t>(header[i]>>(b*8));
    for(std::size_t i=32;i<atlas.size();i+=4)atlas[i+3]=255;
    require(a2fo_refit_skin_atlas_size_valid(atlas.size()),"atlas size accepted");
    require(a2fo_refit_skin_atlas_valid(atlas.data(),atlas.size()),"atlas accepted");
    atlas[3]=0;require(!a2fo_refit_skin_atlas_valid(atlas.data(),atlas.size()),"atlas header rejection");
    atlas[3]=0x31;
    atlas[32]=255;atlas[35]=0;require(!a2fo_refit_skin_atlas_valid(atlas.data(),atlas.size()),
                                      "non-premultiplied pixel rejection");
    atlas[32]=0;atlas[35]=255;
    std::vector<uint8_t> tile(17*13*4);
    require(a2fo_refit_skin_fill_tile(atlas.data()+32,tile.data(),0,17,13,4),"tile generation");
    require(a2fo_refit_skin_source_axis(0,17,4)==0&&
            a2fo_refit_skin_source_axis(3,17,4)==7&&
            a2fo_refit_skin_source_axis(4,17,4)==8&&
            a2fo_refit_skin_source_axis(16,17,4)==63,"tile axis samples");
    require(a2fo_refit_skin_cache_decision(17,13,0,0)==1&&
            a2fo_refit_skin_cache_decision(17,13,96,0)==2&&
            a2fo_refit_skin_cache_decision(4000,4000,0,0)==0,"tile cache policy");
    uint8_t pixels[51*39*4]{};
    A2FORefitSkinDirect info{};info.registered=1;info.dc_kind=10;info.map_mode=1;
    info.graphics_mode=1;info.clip_kind=2;info.clip_right=51;info.clip_bottom=39;
    info.object_valid=1;info.bits_match=1;info.section_null=1;
    info.header_size=40;info.planes=1;info.bit_count=32;info.compression=0;
    info.header_width=51;info.header_height=-39;info.width=51;info.height=39;
    info.stride=51*4;info.bitmap_planes=1;info.bitmap_bit_count=32;info.bits=pixels;
    const A2FORefitNativeRect rect{2,3,17,13};
    require(a2fo_refit_skin_direct_valid(rect,&info),"direct DIB accepted");
    info.map_mode=8;require(!a2fo_refit_skin_direct_valid(rect,&info),"mapping fallback");
    info.map_mode=1;info.clip_right=12;
    require(!a2fo_refit_skin_direct_valid(rect,&info),"clip fallback");
    std::string trace;
    require(a2fo_refit_skin_paint(1,rect,0,4,skin_operation,&trace),"paint fallback");
    require(trace=="0123","skin draw order");
    trace.clear();require(!a2fo_refit_skin_paint(0,rect,0,4,skin_operation,&trace)&&trace.empty(),
                          "invalid paint made no native calls");
}
}
int main(){
    skin_checks();
    const std::array<uint32_t,4> pixels{{0x11223344,0xaabbccdd,0x00112233,0xff556677}};
    const A2FORefitGpuImage image{2,2,pixels.data(),pixels.size()};
    {
        Mock m;auto ops=m.ops();
        require(!a2fo_refit_gpu_draw({0,2,pixels.data(),4},0,0,&ops),"invalid image");
        require(m.trace.empty(),"invalid image touched native API");
        require(a2fo_refit_gpu_draw(image,0,0,&ops),"transient draw");
        check_order(m.trace,"CTLUBV"+repeated('R',12)+"PFN"+repeated('G',13)+"DE",
                    "single-image native order");
        require(m.drawings==1&&m.restores==1,"single-image draw and restore");
        require(*reinterpret_cast<const uint32_t*>(m.cache[0].pixels.data())==0xff223344,
                "single-image opaque upload");
        require(m.drawn[0][1].u==1.0f&&m.drawn[0][3].v==1.0f,
                "single-image vertex UV");
    }
    {
        Mock m;auto ops=m.ops();
        require(a2fo_refit_gpu_draw(image,7,1,&ops),"cached initial draw");
        require(a2fo_refit_gpu_draw(image,7,1,&ops),"cached reuse");
        require(m.cache[0].creates==1&&m.cache[0].uploads==1,"cache reuse counters");
        require(a2fo_refit_gpu_draw(image,8,1,&ops),"cached revision upload");
        require(m.cache[0].uploads==2,"revision upload counter");
        require(m.drawings==3&&m.restores==3,"cached render count");
    }
    {
        Mock m;m.caps.texture_caps=0x22;auto ops=m.ops();
        const A2FORefitGpuImage wide{3,2,pixels.data(),4};
        require(!a2fo_refit_gpu_draw(wide,0,0,&ops),"bad pixel count");
        const std::array<uint32_t,6> source{{1,2,3,4,5,6}};
        require(a2fo_refit_gpu_draw({3,2,source.data(),6},0,0,&ops),"pow2 square draw");
        require(m.cache[0].state.texture_width==4&&m.cache[0].state.texture_height==4,
                "pow2 square dimensions");
        require(m.drawn[0][1].u==0.75f&&m.drawn[0][3].v==0.5f,"pow2 UV");
    }
    {
        Mock m;m.fail_create=1;auto ops=m.ops();
        require(!a2fo_refit_gpu_layers(image,image,2,2,1,2,0,0,0,&ops),
                "overlay allocation failure");
        require(m.drawings==0&&m.restores==1,"allocation preflight preserves target");
        require(m.trace.find("TLU")!=std::string::npos,"background upload before overlay allocation");
    }
    {
        Mock m;m.fail_bind=1;auto ops=m.ops();
        require(!a2fo_refit_gpu_layers(image,image,2,2,1,2,0,0,0,&ops),
                "overlay binding failure");
        require(m.drawings==0&&m.restores==1,"binding preflight preserves target");
    }
    {
        Mock m;auto ops=m.ops();
        require(!a2fo_refit_gpu_layers(image,image,2,2,1,2,1,1,1,&ops),"same cache rejection");
        require(m.trace.empty(),"same cache touched native API");
        require(a2fo_refit_gpu_layers(image,image,2,2,1,2,1,1,0,&ops),"layered cached draw");
        require(m.drawings==2&&m.restores==1,"two layers and restore");
        require(m.cache[0].uploads==1&&m.cache[1].uploads==1,"both layer uploads");
        require(*reinterpret_cast<const uint32_t*>(m.cache[0].pixels.data())==0xff223344,
                "background opaque pixels");
        require(*reinterpret_cast<const uint32_t*>(m.cache[1].pixels.data())==0x11223344,
                "overlay alpha pixels");
        require(m.drawn[0][0].colour==0xff707070&&m.drawn[0][2].colour==0xff5e5e5e&&
                m.drawn[1][0].colour==0xffffffff,"gradient and overlay colours");
    }
    std::puts("refit native GPU mock: validation, state order, cache, pixels, preflight, restore OK");
}
