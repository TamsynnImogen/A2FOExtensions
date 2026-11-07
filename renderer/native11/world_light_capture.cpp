#include "world_light_capture.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_objects=4096;
struct Node { std::uint32_t next=0,prev=0,object=0; };
struct Observation {
    std::uint32_t node=0,next=0,prev=0,object=0,object_class=0,instance=0;
    float matrix[12]{};
    std::shared_ptr<const LightClassRecord> owner;
};
struct Pass {
    std::uint32_t container=0,sentinel=0,head=0,tail=0;
    std::vector<Observation> objects;
};
bool at(void* ctx,Read read,std::uint32_t base,std::uint32_t offset,
    void* out,std::size_t size) {
    if (!read||!base||!size||offset>std::numeric_limits<std::uint32_t>::max()-base||
        size-1>std::numeric_limits<std::uint32_t>::max()-base-offset) return false;
    return read(ctx,base+offset,out,size);
}
bool word(void* ctx,Read read,std::uint32_t base,std::uint32_t offset,std::uint32_t& out) {
    return at(ctx,read,base,offset,&out,sizeof(out));
}
bool pose(const float m[12]) {
    for (int i=0;i<12;++i) if (!std::isfinite(m[i])) return false;
    const double determinant=
        double(m[0])*(double(m[4])*m[8]-double(m[5])*m[7])-
        double(m[1])*(double(m[3])*m[8]-double(m[5])*m[6])+
        double(m[2])*(double(m[3])*m[7]-double(m[4])*m[6]);
    return std::isfinite(determinant)&&determinant!=0;
}
void world(const float m[12],float out[16]) {
    for (int row=0;row<3;++row)
        for (int col=0;col<3;++col) out[row*4+col]=m[col*3+row];
    for (int row=0;row<3;++row) out[row*4+3]=m[9+row];
    out[12]=out[13]=out[14]=0;out[15]=1;
}
bool pass(WorldLightReadProfile profile,void* read_context,Read read,
    void* class_context,PinLightClass pin,Pass& out,const char*& failure) {
    if (!profile.object_list_pointer||!read||!pin) {
        failure="missing world light reader/profile";return false;
    }
    if (!word(read_context,read,profile.object_list_pointer,0,out.container)||!out.container||
        !word(read_context,read,out.container,0x4,out.sentinel)||!out.sentinel||
        !word(read_context,read,out.sentinel,0,out.head)||!out.head||
        !word(read_context,read,out.sentinel,4,out.tail)||!out.tail) {
        failure="missing world object list";return false;
    }
    std::set<std::uint32_t> nodes,objects;
    std::uint32_t current=out.head,previous=out.sentinel;
    while (current!=out.sentinel) {
        if (!current||out.objects.size()==max_objects||!nodes.insert(current).second) {
            failure="world object list cycle or capacity exceeded";return false;
        }
        Node node{};
        if (!at(read_context,read,current,0,&node,sizeof(node))||
            node.prev!=previous||!node.next||!node.object||
            !objects.insert(node.object).second) {
            failure="invalid world object link or duplicate object";return false;
        }
        Observation item{};
        item.node=current;item.next=node.next;item.prev=node.prev;item.object=node.object;
        if (!word(read_context,read,item.object,0x40,item.object_class)||!item.object_class) {
            failure="missing world object class";return false;
        }
        item.owner=pin(class_context,item.object_class);
        if (!item.owner||item.owner->class_id!=item.object_class) {
            failure="unknown or changed world light class";return false;
        }
        if(!word(read_context,read,item.object,0x4,item.instance)){
            failure="unreadable world object instance identity";return false;
        }
        if (item.owner->config.enabled) {
            if (!item.instance||
                !at(read_context,read,item.instance,0x44,item.matrix,sizeof(item.matrix))||
                !pose(item.matrix)) {
                failure="invalid enabled world light pose";return false;
            }
        }
        out.objects.push_back(std::move(item));
        previous=current;current=node.next;
    }
    if (previous!=out.tail) { failure="world object list tail mismatch";return false; }
    return true;
}
bool same(const Pass& a,const Pass& b) {
    if (a.container!=b.container||a.sentinel!=b.sentinel||a.head!=b.head||
        a.tail!=b.tail||a.objects.size()!=b.objects.size()) return false;
    for (std::size_t i=0;i<a.objects.size();++i) {
        const auto& x=a.objects[i];const auto& y=b.objects[i];
        if (x.node!=y.node||x.next!=y.next||x.prev!=y.prev||
            x.object!=y.object||x.object_class!=y.object_class||
            x.instance!=y.instance||x.owner.get()!=y.owner.get()||
            x.owner->version!=y.owner->version||
            std::memcmp(x.matrix,y.matrix,sizeof(x.matrix))!=0) return false;
    }
    return true;
}
}

bool capture_world_lights(WorldLightReadProfile profile,void* read_context,Read read,
    void* class_context,PinLightClass pin,WorldLightCapture& output,
    std::string& error) noexcept {
    try {
        const char* failure=nullptr;
        Pass initial{};
        if (!pass(profile,read_context,read,class_context,pin,initial,failure)) {
            error=failure;return false;
        }
        Pass verification{};
        if (!pass(profile,read_context,read,class_context,pin,verification,failure)) {
            error=failure;return false;
        }
        if (!same(initial,verification)) { error="world light list changed during capture";return false; }
        WorldLightCapture candidate{};
        candidate.sources.reserve(initial.objects.size());
        candidate.class_owners.reserve(initial.objects.size());
        candidate.members.reserve(initial.objects.size());
        for (const auto& observed:initial.objects) {
            PlacedLight placed{};
            placed.source_id=observed.object;
            placed.config=observed.owner->config;
            if (placed.config.enabled) world(observed.matrix,placed.local_to_world);
            candidate.sources.push_back(placed);
            candidate.class_owners.push_back(observed.owner);
            candidate.members.push_back({observed.object,observed.instance,observed.object_class,observed.owner->version});
        }
        output=std::move(candidate);
        error.clear();
        return true;
    } catch (...) {
        try { error="world light capture allocation or callback failure"; } catch (...) { error.clear(); }
        return false;
    }
}
} // namespace a2fo::native11
