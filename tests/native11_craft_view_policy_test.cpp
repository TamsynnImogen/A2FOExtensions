#include "../renderer/native11/craft_view_policy.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
bool near(float a,float b){return std::fabs(a-b)<0.0001f;}
std::uint16_t control(){std::uint16_t value=0;asm volatile("fnstcw %0":"=m"(value));return value;}
void rejected(CraftViewInput input,std::uint32_t encoded,const char* why){
    CraftViewResult output;output.weighted_distance=42;output.distance=43;
    output.alpha=44;output.culled=true;output.fade_written=true;
    const auto before=control();
    need(!native_craft_view(input,encoded,output)&&control()==before&&
         output.weighted_distance==42&&output.distance==43&&output.alpha==44&&
         output.culled&&output.fade_written,why);
}
CraftViewInput base(){CraftViewInput input;input.max_distance=10;input.detailed_range=true;return input;}
}
int main(){
    constexpr auto original=encode_native_x87(0x037f);
    auto input=base();input.position[0]=3;input.position[1]=4;
    CraftViewResult result;
    need(native_craft_view(input,original,result)&&near(result.weighted_distance,std::sqrt(10.f))&&
         near(result.distance,5)&&result.alpha==1&&!result.culled&&!result.fade_written,
         "weighted Y-quarter distance and Euclidean distance differ");
    input.instance_cull_flag=true;
    need(native_craft_view(input,original,result)&&result.culled,
         "mode zero instance cull applies in detailed range");
    input.reveal_all=true;
    need(native_craft_view(input,original,result)&&!result.culled,
         "reveal all bypasses instance flag cull when in range");
    input.reveal_all=false;input.mode=1;
    need(native_craft_view(input,original,result)&&!result.culled,
         "nonzero detailed mode bypasses instance flag cull");
    input.detailed_range=false;
    need(native_craft_view(input,original,result)&&result.culled,
         "nondetailed range enables instance flag cull");
    input.instance_cull_flag=false;input.position[0]=8;
    need(native_craft_view(input,original,result)&&result.culled,
         "strict distance cull uses reduced nondetailed threshold");
    input.position[0]=7;input.position[1]=0;
    need(native_craft_view(input,original,result)&&!result.culled,
         "distance equal to threshold is not culled");

    input=base();input.position[0]=9.5f;input.fade_enabled=true;
    need(native_craft_view(input,original,result)&&near(result.alpha,0.5f)&&
         result.fade_written&&!result.culled,
         "far fade writes alpha from extended nine-tenths start");
    input.fade_enabled=false;
    need(native_craft_view(input,original,result)&&result.alpha==1&&!result.fade_written,
         "disabled fade leaves initial alpha one");
    input=base();input.position[0]=1;input.radius=1;input.fade_enabled=true;input.mode=1;
    need(native_craft_view(input,original,result)&&near(result.alpha,0.708333333f)&&
         result.fade_written,"near fade uses radius over Euclidean distance");
    input.mode=2;
    need(native_craft_view(input,original,result)&&result.alpha==1&&!result.fade_written,
         "mode two skips near fade");
    input.mode=1;input.skip_near_fade=true;
    need(native_craft_view(input,original,result)&&result.alpha==1&&!result.fade_written,
         "explicit near-fade skip is honoured");
    input.skip_near_fade=false;input.position[0]=0;
    need(native_craft_view(input,original,result)&&result.alpha==0.125f&&result.fade_written,
         "positive radius at zero distance uses clamped ratio without division");
    input.radius=0;
    need(native_craft_view(input,original,result)&&result.alpha==1&&!result.fade_written,
         "zero radius and zero distance leave near fade unwritten");
    input=base();input.position[0]=19;input.radius=200;input.fade_enabled=true;
    need(native_craft_view(input,original,result)&&near(result.alpha,0.5f)&&
         result.fade_written&&!result.culled,
         "large radius scales threshold before far fade");
    input=base();input.radius=std::nextafter(100.f,INFINITY);
    input.max_distance=12345.678f;
    input.position[0]=std::nextafter(input.max_distance,INFINITY);
    input.fade_enabled=true;
    need(native_craft_view(input,original,result)&&result.culled&&
         result.fade_written&&result.alpha<0,
         "extended radius threshold remains below its binary32 rounded value");

    input=base();
    rejected(input,0,"unknown arithmetic rejected atomically");
    rejected(input,0x037f,"unmarked arithmetic rejected atomically");
    rejected(input,encode_native_x87(0x0100),"reserved precision rejected atomically");
    input.camera[0]=std::numeric_limits<float>::quiet_NaN();
    rejected(input,original,"nonfinite camera rejected atomically");
    input=base();input.position[2]=1000001.f;
    rejected(input,original,"out-of-range position rejected atomically");
    input=base();input.radius=-1;
    rejected(input,original,"negative radius rejected atomically");
    input=base();input.radius=1000001.f;
    rejected(input,original,"oversize radius rejected atomically");
    input=base();input.max_distance=0;
    rejected(input,original,"zero maximum distance rejected atomically");
    input=base();input.max_distance=1100000000.f;
    rejected(input,original,"oversize maximum distance rejected atomically");

    const auto caller=control();
    input=base();input.position[0]=3;input.position[1]=4;input.radius=1;
    input.fade_enabled=true;
    for(unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto selected=encode_native_x87(static_cast<std::uint16_t>(
            (caller&~0x0f00u)|(pc<<8)|(rc<<10)|0x003fu));
        const auto before=control();
        need(native_craft_view(input,selected,result)&&control()==before&&
             std::isfinite(result.weighted_distance)&&std::isfinite(result.distance)&&
             std::isfinite(result.alpha),
             "each precision and rounding mode restores caller CW");
    }
    std::puts("PASS native11 craft view policy");
}
