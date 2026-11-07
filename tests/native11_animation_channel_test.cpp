#include "../renderer/native11/animation_channel.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
bool close(float a,float b){return std::fabs(a-b)<0.00001f;}
unsigned short control(){unsigned short value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void set_control(unsigned short value){asm volatile("fldcw %0"::"m"(value):"memory");}
ScalarAnimationChannel scalar(){
    ScalarAnimationChannel c;c.timing.duration=1;c.timing.flags=1;c.timing.key_count=2;
    c.timing.timestamps={0,.5f};c.timing.elapsed=.25f;c.keys={10,20};return c;
}
MatrixAnimationChannel matrix(){
    MatrixAnimationChannel c;c.timing.duration=1;c.timing.flags=1;c.timing.key_count=2;
    c.timing.timestamps={0,.5f};c.timing.elapsed=.75f;
    for(unsigned key=0;key<2;++key){std::array<float,12> values{};
        for(unsigned i=0;i<12;++i)values[i]=float(key*100+i);
        c.keys.push_back(values);}
    return c;
}
void scalar_fail(const ScalarAnimationChannel& c,const char* why){
    ScalarAnimationSample out;out.phase={73,4,true};out.writes=true;out.value=99;
    std::string error;need(!evaluate_scalar_animation_channel(c,out,error),why);
    need(out.phase.key==73&&out.phase.fraction==4&&out.phase.finished&&out.writes&&out.value==99&&
         !error.empty(),"scalar failure atomicity");
}
void matrix_fail(const MatrixAnimationChannel& c,const char* why){
    MatrixAnimationSample out;out.phase={73,4,true};out.writes=true;out.value.fill(99);
    std::string error;need(!evaluate_matrix_animation_channel(c,out,error),why);
    need(out.phase.key==73&&out.phase.fraction==4&&out.phase.finished&&out.writes&&
         out.value[11]==99&&!error.empty(),"matrix failure atomicity");
}
void run(){
    auto c=scalar();ScalarAnimationSample out;std::string error="stale";
    c.interpolation=0;
    need(evaluate_scalar_animation_channel(c,out,error)&&!out.writes&&
         out.phase.key==0&&close(out.phase.fraction,.5f)&&error.empty(),"scalar mode 0 no write");
    c.interpolation=1;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.writes&&out.value==10,"scalar mode 1 step");
    c.interpolation=2;
    need(evaluate_scalar_animation_channel(c,out,error)&&close(out.value,15),"scalar mode 2 linear");
    c.timing.elapsed=.75f;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.phase.key==1&&close(out.value,15),
         "scalar mode 2 wraps to key zero");
    c.interpolation=3;c.keys.push_back(30);
    need(evaluate_scalar_animation_channel(c,out,error)&&close(out.value,25),"scalar mode 3 explicit guard");
    c.timing.flags=0;c.timing.elapsed=1;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.phase.finished&&out.writes&&out.value==20,
         "finished scalar mode 3 forces step");
    c.interpolation=0;c.keys.pop_back();
    need(evaluate_scalar_animation_channel(c,out,error)&&out.writes&&out.value==20,
         "finished scalar mode 0 forces step");
    c.timing.flags=5;c.timing.elapsed=.25f;c.interpolation=1;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.phase.key==1&&out.value==20,
         "reverse loop selection");
    c=scalar();c.timing.key_count=3;c.keys={10,20,30};c.timing.timestamps={0,.25f,.75f};
    c.timing.elapsed=.5f;c.interpolation=2;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.phase.key==1&&close(out.value,25),
         "nonuniform scalar keys");
    c=scalar();c.timing.timestamps.clear();c.timing.elapsed=.51f;c.interpolation=2;
    need(evaluate_scalar_animation_channel(c,out,error)&&out.phase.fraction>1&&out.value>20,
         "unclamped native uniform fraction");

    auto m=matrix();MatrixAnimationSample matrix_out;
    m.interpolation=0;
    need(evaluate_matrix_animation_channel(m,matrix_out,error)&&!matrix_out.writes,
         "matrix mode 0 no write");
    for(unsigned mode=1;mode<=3;++mode){m.interpolation=mode;
        need(evaluate_matrix_animation_channel(m,matrix_out,error)&&matrix_out.writes&&
             matrix_out.phase.key==1&&matrix_out.value==m.keys[1],
             "matrix modes 1/2/3 step native Matrix34 order");}
    m.timing.flags=0;m.timing.elapsed=1;m.interpolation=0;
    need(evaluate_matrix_animation_channel(m,matrix_out,error)&&matrix_out.phase.finished&&
         matrix_out.writes&&matrix_out.value==m.keys[1],"finished matrix forces step");

    c=scalar();c.interpolation=4;scalar_fail(c,"unsupported scalar mode");
    c=scalar();c.interpolation=3;scalar_fail(c,"missing scalar guard");
    c.keys.push_back(30);c.timing.flags=0;c.timing.elapsed=1;
    c.keys.pop_back();scalar_fail(c,"finished mode 3 still requires guard shape");
    c=scalar();c.keys[1]=std::numeric_limits<float>::quiet_NaN();
    scalar_fail(c,"NaN scalar key");
    c=scalar();c.timing.key_count=65536;scalar_fail(c,"scalar key count bound");
    c=scalar();c.keys={std::numeric_limits<float>::max(),-
        std::numeric_limits<float>::max()};c.timing.elapsed=.25f;
    c.timing.timestamps={0,.5f};c.interpolation=2;
    // Overshoot is allowed by the native phase policy, but a nonfinite final
    // binary32 result is not published.
    c.timing.timestamps.clear();c.timing.elapsed=.51f;
    if((control()&0x0c00u)<=0x0400u)scalar_fail(c,"interpolated scalar overflow");
    else need(evaluate_scalar_animation_channel(c,out,error)&&std::isfinite(out.value),
              "directed x87 rounding saturates negative overflow to finite float");
    m=matrix();m.interpolation=4;matrix_fail(m,"unsupported matrix mode");
    m=matrix();m.keys.push_back(m.keys[1]);matrix_fail(m,"matrix guard prohibited");
    m=matrix();m.keys[0][11]=std::numeric_limits<float>::infinity();
    matrix_fail(m,"nonfinite Matrix34 element");
    m=matrix();m.timing.elapsed=std::numeric_limits<float>::quiet_NaN();
    matrix_fail(m,"nonfinite matrix timing");
}
}
int main(){
    const auto original=control();
    for(unsigned rounding=0;rounding<4;++rounding){
        const auto selected=static_cast<unsigned short>((original&~0x0c00u)|(rounding<<10));
        set_control(selected);run();need(control()==selected,"x87 caller control word preserved");
    }
    set_control(original);
    std::puts("PASS native11 animation channels: scalar interpolation, Matrix34 step, timing and atomic rejection");
}
