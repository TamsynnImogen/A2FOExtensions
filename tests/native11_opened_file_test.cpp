#include "../renderer/native11/opened_file.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace a2fo::native11;
namespace {
void need(bool b,const char* message) { if (!b) { std::fprintf(stderr,"FAIL %s\n",message);std::exit(1); } }
struct Region { std::uint32_t address;std::vector<std::uint8_t> data; };
struct Memory {
    std::vector<Region> regions;
    bool throwing=false;
    static bool read(void* ctx,std::uint32_t address,void* destination,std::size_t size) {
        auto& self=*static_cast<Memory*>(ctx);
        if (self.throwing) throw std::runtime_error("reader failure");
        for (const auto& r:self.regions) {
            if (address>=r.address && std::uint64_t(address)-r.address+size<=r.data.size()) {
                std::memcpy(destination,r.data.data()+(address-r.address),size);
                return true;
            }
        }
        return false;
    }
    void word(std::uint32_t address,std::uint32_t value) {
        for (auto& r:regions) if (address>=r.address && std::uint64_t(address)-r.address+4<=r.data.size()) {
            for (int i=0;i<4;++i) r.data[address-r.address+i]=std::uint8_t(value>>(8*i));
            return;
        }
        need(false,"test word region");
    }
};
constexpr std::uint32_t stream=0x1000,name=0x2000,begin=0x3000,vtable=0x6bc874;
Memory fixture() {
    Memory m{{{stream,std::vector<std::uint8_t>(0x20)},
               {name,{'A','/','D','.','D','D','S',0}},
               {begin,{1,2,3,4,99,98,97,96}}}};
    m.word(stream,vtable);m.word(stream+0x14,begin);
    m.word(stream+0x18,begin+4);m.word(stream+0x1c,begin);
    return m;
}
void unchanged(Memory& m,OpenedFileStatus want,std::uint32_t s=stream,
               std::uint32_t n=name,std::uint32_t vt=vtable,std::uint32_t budget=16) {
    OpenedFile out;out.name="sentinel";out.bytes={7,8,9};
    out.stream=31;out.buffer_begin=32;out.buffer_end=33;
    const auto result=capture_opened_file(&m,Memory::read,s,n,vt,budget,out);
    static int case_number=0; ++case_number;
    if (result!=want) std::fprintf(stderr,"case %d: got %d expected %d\n",case_number,int(result),int(want));
    need(result==want,"expected failure status");
    need(out.name=="sentinel"&&out.bytes==std::vector<std::uint8_t>({7,8,9})&&
         out.stream==31&&out.buffer_begin==32&&out.buffer_end==33,"failure atomicity");
}
}
int main() {
    auto m=fixture();OpenedFile out;
    need(capture_opened_file(&m,Memory::read,stream,name,vtable,4,out)==OpenedFileStatus::ok,"capture");
    need(out.name=="A/D.DDS"&&out.bytes==std::vector<std::uint8_t>({1,2,3,4}),"name and actual span only");
    need(out.stream==stream&&out.buffer_begin==begin&&out.buffer_end==begin+4,"source coordinates");
    m.regions.clear();
    need(out.name=="A/D.DDS"&&out.bytes[0]==1&&out.bytes.size()==4,"owned after source removal");
    m=fixture();m.word(stream+0x1c,begin+2);
    need(capture_consumed_file(&m,Memory::read,stream,name,vtable,4,out)==OpenedFileStatus::ok&&out.bytes.size()==4,
         "consumed stream still snapshots complete actual-read span");
    m.word(stream+0x1c,begin+4);
    need(capture_consumed_file(&m,Memory::read,stream,name,vtable,4,out)==OpenedFileStatus::ok,"end cursor accepted after parsing");
    m.word(stream+0x1c,begin+5);
    need(capture_consumed_file(&m,Memory::read,stream,name,vtable,4,out)==OpenedFileStatus::invalid_span&&out.bytes.size()==4,
         "parser cursor beyond actual data rejected atomically");
    m=fixture();m.regions[0].data.resize(4);m.word(stream,vtable+1);
    need(capture_consumed_file(&m,Memory::read,stream,name,vtable,4,out)==OpenedFileStatus::wrong_type,
         "foreign type rejected before reading layout-specific fields");
    m=fixture();m.word(stream,vtable+1);unchanged(m,OpenedFileStatus::wrong_type);
    m=fixture();m.regions.erase(m.regions.begin());unchanged(m,OpenedFileStatus::unreadable);
    m=fixture();m.word(stream+0x1c,begin+1);unchanged(m,OpenedFileStatus::invalid_span);
    m=fixture();m.word(stream+0x18,begin);unchanged(m,OpenedFileStatus::invalid_span);
    m=fixture();m.word(stream+0x14,0);unchanged(m,OpenedFileStatus::invalid_span);
    m=fixture();m.word(stream+0x18,begin+9);unchanged(m,OpenedFileStatus::unreadable);
    m=fixture();unchanged(m,OpenedFileStatus::budget_exceeded,stream,name,vtable,3);
    m=fixture();unchanged(m,OpenedFileStatus::invalid_arguments,stream,name,vtable,0);
    m=fixture();unchanged(m,OpenedFileStatus::invalid_arguments,stream,name,vtable,256u*1024u*1024u+1);
    m=fixture();unchanged(m,OpenedFileStatus::invalid_arguments,0,name,vtable);
    m=fixture();unchanged(m,OpenedFileStatus::invalid_arguments,stream,name,0);
    m=fixture();unchanged(m,OpenedFileStatus::invalid_arguments,0xfffffff0u,name,vtable);
    m=fixture();m.regions[1].data={0};unchanged(m,OpenedFileStatus::invalid_name);
    m=fixture();m.regions[1].data={'A',0x7f,0};unchanged(m,OpenedFileStatus::invalid_name);
    m=fixture();m.regions[1].data=std::vector<std::uint8_t>(1024,'A');
    unchanged(m,OpenedFileStatus::invalid_name);
    m=fixture();m.regions.erase(m.regions.begin()+1);unchanged(m,OpenedFileStatus::unreadable);
    m=fixture();m.throwing=true;unchanged(m,OpenedFileStatus::unreadable);
    m=fixture();unchanged(m,OpenedFileStatus::unreadable,stream,0xffffffffu);
    need(capture_opened_file(&m,nullptr,stream,name,vtable,4,out)==OpenedFileStatus::invalid_arguments,"null reader");
    std::puts("native11 opened file: PASS");
}
