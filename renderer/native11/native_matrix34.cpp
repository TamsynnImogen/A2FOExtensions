#include "native_matrix34.hpp"
#include <cmath>

#if !defined(__i386__) && !defined(__x86_64__)
#error Native Matrix34 parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
bool finite(const NativeMatrix34& matrix) noexcept {
    for(float value:matrix)if(!std::isfinite(value))return false;
    return true;
}
float dot(float a,float x,float b,float y,float c,float z) noexcept {
    float result;
    asm volatile("flds %1; fmuls %2; flds %3; fmuls %4; faddp; "
                 "flds %5; fmuls %6; faddp; fstps %0"
        :"=m"(result):"m"(a),"m"(x),"m"(b),"m"(y),"m"(c),"m"(z)
        :"st","st(1)","memory");
    return result;
}
float translated_dot(float a,float x,float b,float y,float c,float z,float t) noexcept {
    float result;
    asm volatile("flds %1; fmuls %2; flds %3; fmuls %4; faddp; "
                 "flds %5; fmuls %6; faddp; fadds %7; fstps %0"
        :"=m"(result):"m"(a),"m"(x),"m"(b),"m"(y),"m"(c),"m"(z),"m"(t)
        :"st","st(1)","memory");
    return result;
}
}
bool native_matrix34_multiply(const NativeMatrix34& local,
    const NativeMatrix34& parent,NativeMatrix34& output,std::string& error) noexcept {
    if(!finite(local)||!finite(parent))return fail(error,"nonfinite native Matrix34 operand");
    // VA 465940..465a7a in the pinned 4300ab71 engine. Sum association differs
    // between elements; a generic loop accumulating k=0..2 changes LOD edges.
    constexpr unsigned order[4][3][3]={
        {{1,0,2},{1,2,0},{1,2,0}},
        {{1,2,0},{2,1,0},{2,1,0}},
        {{0,2,1},{2,1,0},{2,1,0}},
        {{1,2,0},{2,1,0},{2,1,0}}};
    NativeMatrix34 candidate{};
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<3;++col){
        const auto* k=order[row][col];const auto offset=row*3;
        const float a=local[offset+k[0]],b=local[offset+k[1]],c=local[offset+k[2]];
        const float x=parent[k[0]*3+col],y=parent[k[1]*3+col],z=parent[k[2]*3+col];
        candidate[offset+col]=row==3?translated_dot(a,x,b,y,c,z,parent[9+col]):
            dot(a,x,b,y,c,z);
    }
    if(!finite(candidate))return fail(error,"nonfinite native Matrix34 product");
    output=candidate;error.clear();return true;
}
bool native_matrix34_scale(const NativeMatrix34& local,float scale,
    NativeMatrix34& output,std::string& error) noexcept {
    if(!finite(local)||!std::isfinite(scale))return fail(error,"nonfinite native Matrix34 scale operand");
    NativeMatrix34 candidate=local;
    if(scale!=1.f)for(unsigned i=0;i<9;++i)
        asm volatile("flds %1; fmuls %2; fstps %0"
            :"=m"(candidate[i]):"m"(local[i]),"m"(scale):"st","memory");
    if(!finite(candidate))return fail(error,"nonfinite native Matrix34 scale result");
    output=candidate;error.clear();return true;
}
bool native_matrix34_camera_translation(const NativeMatrix34& world,
    const NativeMatrix34& view,std::array<float,3>& output,std::string& error) noexcept {
    if(!finite(world)||!finite(view))return fail(error,"nonfinite native camera operand");
    std::array<float,3> candidate{};
    for(unsigned col=0;col<3;++col){
        candidate[col]=translated_dot(world[9],view[col],world[11],view[6+col],
            world[10],view[3+col],view[9+col]);
        if(!std::isfinite(candidate[col]))return fail(error,"nonfinite native camera translation");
    }
    output=candidate;error.clear();return true;
}
} // namespace a2fo::native11
