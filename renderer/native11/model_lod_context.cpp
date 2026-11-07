#include "model_lod_context.hpp"
#include "native_arithmetic.hpp"
#include <cmath>
#include <cstring>

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try{error=message;}catch(...){error.clear();}return false;
}
struct Reader {
    void* context;Read read;
    bool bytes(std::uint32_t base,std::uint32_t offset,void* output,std::size_t size)const{
        return read&&base&&size&&offset<=UINT32_MAX-base&&size-1<=UINT32_MAX-base-offset&&
            read(context,base+offset,output,size);
    }
    template<class T>bool get(std::uint32_t base,std::uint32_t offset,T& value)const{
        return bytes(base,offset,&value,sizeof(value));
    }
};
struct Pass {
    ModelLodContext value;
    NativeMatrix34 raw_placement{};
    std::array<float,3> scale{{1,1,1}};
    std::int32_t global_requested=-1,instance_requested=-1;
    std::uint8_t scale_active=0,force=0;
};
bool capture_pass(const Object& object,std::uint32_t camera,ModelLodContextProfile profile,
    Reader reader,Pass& out,std::string& error){
    auto& value=out.value;std::uint32_t instance=0,database=0,vtable=0;
    if(!reader.get(object.entity,4,instance)||instance!=object.instance||
       !reader.get(instance,0,vtable)||vtable!=profile.instance_vtable||
       !reader.get(instance,0x80,database)||database!=object.database||
       !reader.get(database,0,vtable)||vtable!=profile.database_vtable||
       !reader.get(profile.engine_pointer,0,value.engine)||!value.engine||
       !reader.get(value.engine,0xfc,value.camera)||value.camera!=camera||
       !reader.get(value.engine,0x104,value.render_context)||value.render_context!=profile.render_context)
        return fail(error,"native LOD object or camera/context identity mismatch");
    if(!reader.get(value.engine,0xac,out.global_requested)||
       !reader.get(instance,0x14,out.instance_requested)||
       !reader.get(value.engine,0xec,out.force)||
       !reader.get(camera,0xf0,value.view)||
       !reader.get(value.render_context,0x30,value.thresholds)||
       !reader.get(value.render_context,0x40,value.quality_cap)||
       !reader.get(instance,0x44,out.raw_placement)||
       !reader.get(instance,0x74,out.scale_active)||
       (out.scale_active&&!reader.get(instance,0x28,out.scale)))
        return fail(error,"native LOD fresh inputs unavailable");
    for(float number:value.view)if(!std::isfinite(number))return fail(error,"nonfinite native LOD camera");
    for(float number:out.raw_placement)if(!std::isfinite(number))return fail(error,"nonfinite native LOD placement");
    for(float number:value.thresholds)if(!std::isfinite(number))return fail(error,"nonfinite native LOD thresholds");
    for(float number:out.scale)if(!std::isfinite(number)||number==0)return fail(error,"invalid native LOD placement scale");
    value.placement=out.raw_placement;
    if(out.scale_active)for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col){
        const unsigned i=row*3+col;
        asm volatile("flds %1; fmuls %2; fstps %0"
            :"=m"(value.placement[i]):"m"(out.raw_placement[i]),"m"(out.scale[row]):"st","memory");
        if(!std::isfinite(value.placement[i]))return fail(error,"nonfinite native LOD scaled placement");
    }
    // Object.world is the captured row-major column-vector transpose.
    float world[16]{};world[15]=1;
    for(unsigned row=0;row<3;++row){
        for(unsigned col=0;col<3;++col)world[row*4+col]=value.placement[col*3+row];
        world[row*4+3]=value.placement[9+row];
    }
    if(std::memcmp(world,object.world,sizeof(world))||
       std::memcmp(out.scale.data(),object.runtime_scale,sizeof(object.runtime_scale)))
        return fail(error,"native LOD placement changed since object capture");
    value.instance=instance;value.database=database;
    value.requested=out.global_requested==-1?out.instance_requested:out.global_requested;
    value.force_highest=out.force!=0;return true;
}
bool same(const Pass& a,const Pass& b) noexcept {
    return a.value.engine==b.value.engine&&a.value.camera==b.value.camera&&
        a.value.render_context==b.value.render_context&&a.value.instance==b.value.instance&&
        a.value.database==b.value.database&&a.value.quality_cap==b.value.quality_cap&&
        a.global_requested==b.global_requested&&a.instance_requested==b.instance_requested&&
        a.scale_active==b.scale_active&&a.force==b.force&&
        !std::memcmp(a.value.view.data(),b.value.view.data(),sizeof(a.value.view))&&
        !std::memcmp(a.raw_placement.data(),b.raw_placement.data(),sizeof(a.raw_placement))&&
        !std::memcmp(a.scale.data(),b.scale.data(),sizeof(a.scale))&&
        !std::memcmp(a.value.thresholds.data(),b.value.thresholds.data(),sizeof(a.value.thresholds));
}
}
bool capture_model_lod_context(const Object& object,std::uint32_t camera,
    std::uint32_t native_x87_control,ModelLodContextProfile profile,
    void* context,Read read,ModelLodContext& output,std::string& error) noexcept {
    try{
        NativeArithmeticScope arithmetic(native_x87_control);
        if(!arithmetic.valid()||!arithmetic.known()||!read||!camera||!profile.engine_pointer||
           !profile.render_context||!profile.instance_vtable||!profile.database_vtable||
           !object.entity||!object.instance||!object.database||!object.model_serial||
           object.model_identity!=ModelIdentity::plain_database_name||
           object.placement_identity!=PlacementIdentity::craft_runtime_scale||
           object.instance_vtable!=profile.instance_vtable||object.database_vtable!=profile.database_vtable)
            return fail(error,"invalid native LOD context request or original arithmetic unknown");
        Pass first,second;
        if(!capture_pass(object,camera,profile,{context,read},first,error)||
           !capture_pass(object,camera,profile,{context,read},second,error))return false;
        if(!same(first,second))return fail(error,"native LOD inputs changed during capture");
        second.value.native_x87_control=native_x87_control;
        output=second.value;error.clear();return true;
    }catch(...){return fail(error,"native LOD context capture failed");}
}
} // namespace a2fo::native11
