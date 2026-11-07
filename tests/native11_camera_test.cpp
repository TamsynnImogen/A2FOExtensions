#include "../renderer/native11/engine_camera.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

using a2fo::native11::EngineCameraView;
using a2fo::native11::convert_engine_cpu_camera;
namespace {
void need(bool condition,const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL %s\n",message); std::exit(1); }
}
void close(double actual,double expected,double tolerance,const char* message) {
    if (std::fabs(actual-expected)>tolerance) {
        std::fprintf(stderr,"FAIL %s: %.9g vs %.9g\n",message,actual,expected);
        std::exit(1);
    }
}
using Matrix=std::array<float,16>;
Matrix row_product(const Matrix& left,const Matrix& right) {
    Matrix product{};
    for (int r=0;r<4;++r) for (int c=0;c<4;++c)
        for (int k=0;k<4;++k) product[r*4+c]+=left[r*4+k]*right[k*4+c];
    return product;
}
std::array<double,4> row_project(const Matrix& matrix,const std::array<float,4>& point) {
    std::array<double,4> projected{};
    for (int c=0;c<4;++c) for (int r=0;r<4;++r)
        projected[c]+=double(point[r])*matrix[r*4+c];
    return projected;
}
std::array<double,4> column_project(const float matrix[16],const std::array<float,4>& point) {
    std::array<double,4> projected{};
    for (int r=0;r<4;++r) for (int c=0;c<4;++c)
        projected[r]+=double(matrix[r*4+c])*point[c];
    return projected;
}
void check_projection(const Matrix& screen,const std::array<float,4>& viewport,
                      const std::array<float,4>& point) {
    EngineCameraView adapted{};
    need(convert_engine_cpu_camera(screen.data(),viewport.data(),adapted),"valid camera accepted");
    need(std::equal(viewport.begin(),viewport.end(),adapted.viewport),"viewport copied");
    const auto engine=row_project(screen,point);
    const auto clip=column_project(adapted.world_to_clip,point);
    need(engine[3]>0 && clip[3]>0,"positive homogeneous W");
    const double reference_x=viewport[0]+(viewport[2]-1)*engine[0]/engine[3];
    const double reference_y=viewport[1]+(viewport[3]-1)*engine[1]/engine[3];
    const double d3d_x=viewport[0]+viewport[2]*(clip[0]/clip[3]+1)/2;
    const double d3d_y=viewport[1]+viewport[3]*(1-clip[1]/clip[3])/2;
    // The adapter stores floats; the 16K case magnifies ulp differences.
    close(d3d_x,reference_x,0.02,"engine and native11 X pixel");
    close(d3d_y,reference_y,0.02,"engine and native11 Y pixel");
    close(clip[2]/clip[3],engine[2]/engine[3],0.000001,"depth preserved");
}
void invalid(const Matrix& screen,const std::array<float,4>& viewport,const char* name) {
    EngineCameraView result{};
    std::fill(std::begin(result.world_to_clip),std::end(result.world_to_clip),7.25f);
    std::fill(std::begin(result.viewport),std::end(result.viewport),-3.5f);
    EngineCameraView before=result;
    need(!convert_engine_cpu_camera(screen.data(),viewport.data(),result),name);
    need(std::memcmp(&result,&before,sizeof(result))==0,"failed conversion preserves output");
}
}
int main() {
    // Row-vector world-view with rotation around Y and translation.
    const Matrix view={0.8f,0.f,-0.6f,0.f,
                       0.f,1.f,0.f,0.f,
                       0.6f,0.f,0.8f,0.f,
                       -1.25f,0.75f,4.f,1.f};
    // Perspective CPU screen matrix: asymmetric focal lengths and offsets.
    // U,V are normalized screen coordinates before homogeneous division.
    constexpr float near=0.25f,far=80.f;
    const Matrix projection={0.73f,0.f,0.f,0.f,
                             0.f,0.41f,0.f,0.f,
                             0.37f,0.62f,far/(far-near),1.f,
                             0.f,0.f,-near*far/(far-near),0.f};
    const Matrix screen=row_product(view,projection);
    for (const std::array<float,4> viewport: {
            std::array<float,4>{0,0,640,480}, {19,37,1001,457}, {3,9,2,2},
            {120,45,16384,16384}, {7,13,1,480}, {7,13,640,1}}) {
        for (const std::array<float,4> point: {
                std::array<float,4>{0,0,-4.5f,1}, {1.3f,-0.5f,2.5f,1},
                {-3.2f,1.7f,20.f,1}, {0.7f,0.1f,65.f,1}})
            check_projection(screen,viewport,point);
    }
    const std::array<float,4> ordinary{0,0,640,480};
    auto broken=screen;
    broken[0]=std::numeric_limits<float>::quiet_NaN(); invalid(broken,ordinary,"NaN matrix rejected");
    broken=screen;broken[5]=std::numeric_limits<float>::infinity();invalid(broken,ordinary,"infinite matrix rejected");
    broken=screen;std::copy(broken.begin(),broken.begin()+4,broken.begin()+4);
    invalid(broken,ordinary,"singular matrix rejected");
    broken={3e38f,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    invalid(broken,ordinary,"nonfinite converted matrix rejected");
    invalid(screen,{-1,0,640,480},"negative viewport origin rejected");
    invalid(screen,{0,-1,640,480},"negative viewport Y rejected");
    invalid(screen,{0,0,0,480},"zero width rejected");
    invalid(screen,{0,0,640,16385},"oversize height rejected");
    invalid(screen,{0,0,std::numeric_limits<float>::quiet_NaN(),480},"NaN viewport rejected");
    EngineCameraView output{};
    need(!convert_engine_cpu_camera(nullptr,ordinary.data(),output),"null matrix rejected");
    need(!convert_engine_cpu_camera(screen.data(),nullptr,output),"null viewport rejected");
    std::puts("PASS native11 CPU camera pixel/depth parity and invalid inputs");
}
