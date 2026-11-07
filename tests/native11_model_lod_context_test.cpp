#include "../renderer/native11/model_lod_context.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <map>
#include <vector>

using namespace a2fo::native11;
namespace {
constexpr std::uint32_t entity=0x1000,instance=0x2000,database=0x3000;
constexpr std::uint32_t engine_pointer=0x4000,engine=0x5000,camera=0x6000;
constexpr std::uint32_t render_context=0x7000,instance_vtable=0x8100,database_vtable=0x8200;
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint16_t control_word(){std::uint16_t word=0;asm volatile("fnstcw %0":"=m"(word));return word;}
struct Fake {
    std::map<std::uint32_t,std::uint8_t> memory;
    std::vector<std::pair<std::uint32_t,std::size_t>> reads;
    std::vector<std::pair<std::uint32_t,std::size_t>> protected_ranges;
    std::function<void(Fake&)> second_pass_mutation;
    unsigned entity_reads=0;
    template<class T>void put(std::uint32_t address,const T& value){
        const auto* source=reinterpret_cast<const std::uint8_t*>(&value);
        for(std::size_t i=0;i<sizeof(T);++i)memory[address+static_cast<std::uint32_t>(i)]=source[i];
    }
    void deny(std::uint32_t address,std::size_t size){protected_ranges.push_back({address,size});}
    static bool read(void* context,std::uint32_t address,void* output,std::size_t size){
        auto& self=*static_cast<Fake*>(context);
        if(address==entity+4&&++self.entity_reads==2&&self.second_pass_mutation)
            self.second_pass_mutation(self);
        self.reads.push_back({address,size});
        for(const auto& range:self.protected_ranges)
            if(static_cast<std::uint64_t>(address)<static_cast<std::uint64_t>(range.first)+range.second&&
               static_cast<std::uint64_t>(range.first)<static_cast<std::uint64_t>(address)+size)return false;
        auto* destination=static_cast<std::uint8_t*>(output);
        for(std::size_t i=0;i<size;++i){
            const auto item=self.memory.find(address+static_cast<std::uint32_t>(i));
            if(item==self.memory.end())return false;
            destination[i]=item->second;
        }
        return true;
    }
};
struct Fixture {
    Fake fake;
    Object object;
    ModelLodContextProfile profile;
    std::uint32_t original_cw=encode_native_x87(control_word());
    NativeMatrix34 raw{{1,2,3,4,5,6,7,8,9,10,11,12}};
    NativeMatrix34 view{{1,0,0,0,1,0,0,0,1,0,0,0}};
    std::array<float,3> scale{{2,3,4}};
    Fixture(){
        profile={engine_pointer,instance_vtable,database_vtable,render_context};
        object.entity=entity;object.instance=instance;object.instance_vtable=instance_vtable;
        object.database=database;object.database_vtable=database_vtable;
        object.model_serial=9;object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::craft_runtime_scale;
        for(unsigned row=0;row<3;++row){
            for(unsigned col=0;col<3;++col)object.world[row*4+col]=raw[col*3+row]*scale[col];
            object.world[row*4+3]=raw[9+row];
        }
        object.world[15]=1;
        std::memcpy(object.runtime_scale,scale.data(),sizeof(object.runtime_scale));
        fake.put(entity+4,instance);fake.put(instance,instance_vtable);
        fake.put(instance+0x80,database);fake.put(database,database_vtable);
        fake.put(engine_pointer,engine);fake.put(engine+0xfc,camera);
        fake.put(engine+0x104,render_context);
        const std::int32_t automatic=-1,requested=2,quality=3;
        fake.put(engine+0xac,automatic);fake.put(instance+0x14,requested);
        const std::uint8_t force=0,active=1;
        fake.put(engine+0xec,force);fake.put(camera+0xf0,view);
        fake.put(render_context+0x30,std::array<float,4>{{30,20,10,8}});
        fake.put(render_context+0x40,quality);fake.put(instance+0x44,raw);
        fake.put(instance+0x74,active);fake.put(instance+0x28,scale);
        fake.deny(engine+0x100,4); // mutable current engine instance
        fake.deny(0x9000+0xbc,4); // native node flags
    }
    bool capture(ModelLodContext& output,std::string& error){
        return capture_model_lod_context(object,camera,original_cw,profile,
            &fake,&Fake::read,output,error);
    }
};
void rejected(Fixture& fixture,const char* why){
    ModelLodContext output;output.engine=77;output.placement[0]=123;
    std::string error;
    const auto before=control_word();
    need(!fixture.capture(output,error)&&!error.empty()&&output.engine==77&&
         output.placement[0]==123&&control_word()==before,why);
}
}
int main(){
    Fixture fixture;ModelLodContext output;std::string error="old error";
    const auto before=control_word();
    need(fixture.capture(output,error)&&error.empty()&&control_word()==before,
         "two-pass checked capture restores caller control word");
    need(output.engine==engine&&output.camera==camera&&output.render_context==render_context&&
         output.instance==instance&&output.database==database&&
         output.native_x87_control==fixture.original_cw&&output.requested==2&&
         output.quality_cap==3&&!output.force_highest,
         "identity and instance request fallback copied");
    const NativeMatrix34 expected{{2,4,6,12,15,18,28,32,36,10,11,12}};
    need(output.placement==expected&&output.view==fixture.view&&fixture.fake.entity_reads==2,
         "fresh placement scales each basis row and keeps translation");
    const std::vector<std::pair<std::uint32_t,std::size_t>> expected_reads{
        {entity+4,4},{instance,4},{instance+0x80,4},{database,4},
        {engine_pointer,4},{engine+0xfc,4},{engine+0x104,4},
        {engine+0xac,4},{instance+0x14,4},{engine+0xec,1},
        {camera+0xf0,sizeof(NativeMatrix34)},
        {render_context+0x30,sizeof(std::array<float,4>)},{render_context+0x40,4},
        {instance+0x44,sizeof(NativeMatrix34)},{instance+0x74,1},
        {instance+0x28,sizeof(std::array<float,3>)}
    };
    need(fixture.fake.reads.size()==expected_reads.size()*2,
         "only verified fields read in two passes");
    for(std::size_t i=0;i<fixture.fake.reads.size();++i)
        need(fixture.fake.reads[i]==expected_reads[i%expected_reads.size()],
             "read log excludes last-node globals and native flags");
    for(const auto& read:fixture.fake.reads)
        for(const auto& range:fixture.fake.protected_ranges)
            need(!(static_cast<std::uint64_t>(read.first)<static_cast<std::uint64_t>(range.first)+range.second&&
                   static_cast<std::uint64_t>(range.first)<static_cast<std::uint64_t>(read.first)+read.second),
                 "protected mutable state was not read");

    Fixture global;global.fake.put(engine+0xac,std::int32_t{1});
    const std::uint8_t truthy=0x80;global.fake.put(engine+0xec,truthy);
    need(global.capture(output,error)&&output.requested==1&&output.force_highest,
         "global request overrides instance and non-one force byte is truthy");
    for(unsigned force=1;force<=255;++force){
        Fixture each;each.fake.put(engine+0xec,static_cast<std::uint8_t>(force));
        need(each.capture(output,error)&&output.force_highest,"every nonzero force byte truthy");
    }

    Fixture unknown;unknown.original_cw=0;rejected(unknown,"unknown original control word refused");
    Fixture invalid_cw;invalid_cw.original_cw=encode_native_x87(0x0100);
    rejected(invalid_cw,"invalid original control word refused");
    Fixture changed_cw;
    changed_cw.original_cw=encode_native_x87(static_cast<std::uint16_t>(
        (control_word()&static_cast<std::uint16_t>(~0x0f00u))|0x0800u));
    const auto caller_cw=control_word();
    need(changed_cw.capture(output,error)&&control_word()==caller_cw&&
         output.native_x87_control==changed_cw.original_cw,
         "different captured rounding mode restores original caller control word");
    Fixture bad;bad.fake.put(engine+0xfc,std::uint32_t{0x6004});
    rejected(bad,"engine camera mismatch refused");
    bad=Fixture();bad.fake.put(engine+0x104,std::uint32_t{0x7004});
    rejected(bad,"engine render context mismatch refused");
    bad=Fixture();bad.fake.put(entity+4,std::uint32_t{0x2004});
    rejected(bad,"entity instance mismatch refused");
    bad=Fixture();bad.fake.put(instance,std::uint32_t{0x8104});
    rejected(bad,"instance vtable mismatch refused");
    bad=Fixture();bad.fake.put(instance+0x80,std::uint32_t{0x3004});
    rejected(bad,"instance database mismatch refused");
    bad=Fixture();bad.fake.put(database,std::uint32_t{0x8204});
    rejected(bad,"database vtable mismatch refused");
    bad=Fixture();bad.fake.deny(instance+0x44,sizeof(NativeMatrix34));
    rejected(bad,"unreadable placement refused");
    bad=Fixture();bad.scale[1]=0;bad.fake.put(instance+0x28,bad.scale);
    rejected(bad,"zero native runtime scale refused");
    bad=Fixture();bad.scale[1]=std::numeric_limits<float>::infinity();
    bad.fake.put(instance+0x28,bad.scale);
    rejected(bad,"nonfinite native runtime scale refused");

    struct Mutation {const char* why;std::function<void(Fake&)> apply;};
    const std::vector<Mutation> mutations{
        {"second-pass signed-zero view mutation",[](Fake& f){
            auto view=NativeMatrix34{{1,-0.0f,0,0,1,0,0,0,1,0,0,0}};
            f.put(camera+0xf0,view);}},
        {"second-pass requested override mutation",[](Fake& f){f.put(engine+0xac,std::int32_t{3});}},
        {"second-pass force mutation",[](Fake& f){f.put(engine+0xec,std::uint8_t{1});}},
        {"second-pass threshold mutation",[](Fake& f){
            f.put(render_context+0x30,std::array<float,4>{{30,20,10,9}});}},
        {"second-pass quality mutation",[](Fake& f){f.put(render_context+0x40,std::int32_t{2});}},
        {"second-pass camera identity mutation",[](Fake& f){f.put(engine+0xfc,std::uint32_t{0x6004});}}
    };
    for(const auto& mutation:mutations){
        Fixture changed;changed.fake.second_pass_mutation=mutation.apply;
        rejected(changed,mutation.why);
    }
    std::puts("PASS native11 model LOD context checked reader");
}
