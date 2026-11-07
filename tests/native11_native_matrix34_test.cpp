#include "../renderer/native11/native_matrix34.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint16_t control(){std::uint16_t cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
std::uint32_t mxcsr(){std::uint32_t value=0;asm volatile("stmxcsr %0":"=m"(value));return value;}
constexpr NativeMatrix34 identity{{1,0,0,0,1,0,0,0,1,0,0,0}};
constexpr NativeMatrix34 rotation{{0,1,0,-1,0,0,0,0,1,0,0,0}};
bool bit_equal(const NativeMatrix34& a,const NativeMatrix34& b){return std::memcmp(a.data(),b.data(),sizeof(a))==0;}
void equal(const NativeMatrix34& actual,const NativeMatrix34& expected,const char* why){
    need(actual==expected,why);
}
void cases(){
    const auto caller=control();const auto sse=mxcsr();std::string error="old";
    NativeMatrix34 out{};
    need(native_matrix34_multiply(identity,identity,out,error)&&error.empty(),"identity multiply");
    equal(out,identity,"identity product exact");
    NativeMatrix34 translation=identity;translation[9]=2;translation[10]=3;translation[11]=4;
    need(native_matrix34_multiply(translation,identity,out,error),"left identity multiply");
    equal(out,translation,"left translation retained");
    need(native_matrix34_multiply(identity,translation,out,error),"right translation multiply");
    equal(out,translation,"right translation retained");
    NativeMatrix34 child=identity;child[9]=1;child[10]=0;child[11]=5;
    need(native_matrix34_multiply(child,rotation,out,error),"local translation under parent rotation");
    NativeMatrix34 expected=rotation;expected[9]=0;expected[10]=1;expected[11]=5;
    equal(out,expected,"local row translation rotates with parent basis");
    need(native_matrix34_multiply(rotation,translation,out,error),"local rotation under parent translation");
    expected=rotation;expected[9]=2;expected[10]=3;expected[11]=4;
    equal(out,expected,"parent translation does not rotate with local basis");
    need(native_matrix34_multiply(child,translation,out,error),"two translations compose");
    expected=identity;expected[9]=3;expected[10]=3;expected[11]=9;
    equal(out,expected,"translation composition follows local then parent");
    // Off-diagonal, asymmetric bases expose a transpose or operand reversal.
    NativeMatrix34 asym_local{{1,2,3,4,5,6,7,8,9,10,11,12}};
    NativeMatrix34 asym_parent{{2,3,5,7,11,13,17,19,23,29,31,37}};
    need(native_matrix34_multiply(asym_local,asym_parent,out,error),"asymmetric product");
    expected={{67,82,100,145,181,223,223,280,346,330,410,506}};
    equal(out,expected,"asymmetric native row-vector product order");
    auto left_alias=asym_local,right_alias=asym_parent;
    need(native_matrix34_multiply(left_alias,asym_parent,left_alias,error)&&
         bit_equal(left_alias,expected),"multiply output may alias local operand");
    need(native_matrix34_multiply(asym_local,right_alias,right_alias,error)&&
         bit_equal(right_alias,expected),"multiply output may alias parent operand");
    auto scaled=rotation;scaled[9]=2;scaled[10]=-3;scaled[11]=4;
    need(native_matrix34_scale(scaled,2,out,error),"positive scale");
    expected={{0,2,0,-2,0,0,0,0,2,2,-3,4}};
    equal(out,expected,"scale affects basis only");
    need(native_matrix34_scale(scaled,0,out,error),"zero scale accepted");
    expected={{0,0,0,0,0,0,0,0,0,2,-3,4}};
    equal(out,expected,"singular zero basis retains translation");
    need(native_matrix34_scale(scaled,-1,out,error),"negative scale accepted");
    expected={{0,-1,0,1,0,0,0,0,-1,2,-3,4}};
    equal(out,expected,"negative scale reverses basis");
    scaled[0]=-0.f;scaled[1]=std::numeric_limits<float>::denorm_min();
    need(native_matrix34_scale(scaled,1,out,error)&&bit_equal(out,scaled),
         "scale-one fast copy preserves signed zero and subnormal bits");
    auto scale_alias=scaled;
    need(native_matrix34_scale(scale_alias,1,scale_alias,error)&&bit_equal(scale_alias,scaled),
         "scale-one output may alias source");
    NativeMatrix34 zero{};
    need(native_matrix34_multiply(zero,identity,out,error)&&out==zero,
         "finite singular matrix multiply accepted");
    const NativeMatrix34 sentinel{{42,42,42,42,42,42,42,42,42,42,42,42}};
    auto reject_multiply=[&](const NativeMatrix34& a,const NativeMatrix34& b,const char* why){
        out=sentinel;error="old";
        need(!native_matrix34_multiply(a,b,out,error)&&bit_equal(out,sentinel)&&
             !error.empty()&&error!="old",why);
    };
    auto reject_scale=[&](const NativeMatrix34& a,float factor,const char* why){
        out=sentinel;error="old";
        need(!native_matrix34_scale(a,factor,out,error)&&bit_equal(out,sentinel)&&
             !error.empty()&&error!="old",why);
    };
    auto bad=identity;bad[0]=std::numeric_limits<float>::quiet_NaN();
    reject_multiply(bad,identity,"nonfinite local rejected atomically");
    reject_multiply(identity,bad,"nonfinite parent rejected atomically");
    reject_scale(bad,1,"nonfinite scale source rejected atomically");
    bad=identity;bad[4]=std::numeric_limits<float>::infinity();
    reject_multiply(bad,identity,"infinite local rejected atomically");
    reject_scale(identity,std::numeric_limits<float>::infinity(),"infinite scale rejected atomically");
    bad=identity;bad[0]=std::numeric_limits<float>::max();
    reject_multiply(bad,bad,"finite operands with overflowing product rejected atomically");
    reject_scale(bad,std::numeric_limits<float>::max(),"finite scale overflow rejected atomically");
    need(control()==caller,"kernel preserves caller x87 CW");
    need((mxcsr()&~0x3fu)==(sse&~0x3fu),"kernel preserves caller MXCSR control bits");
    double stack_value=0;asm volatile("fld1":::"st");
    need(native_matrix34_multiply(rotation,translation,out,error)&&
         native_matrix34_scale(rotation,-1,out,error),"kernel works with existing x87 stack value");
    asm volatile("fstpl %0":"=m"(stack_value)::"st","memory");
    need(stack_value==1&&control()==caller&&
         (mxcsr()&~0x3fu)==(sse&~0x3fu),"kernel balances x87 stack");
}
}
int main(){cases();std::puts("PASS native11 native Matrix34 kernel");}
