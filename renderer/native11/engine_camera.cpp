#include "engine_camera.hpp"
#include <cmath>

namespace a2fo::native11 {
namespace {
bool invertible(const float matrix[16]) {
    // Partial-pivot elimination in double avoids an arbitrary scale cutoff.
    double a[4][4];
    for (int row=0; row<4; ++row)
        for (int col=0; col<4; ++col)
            a[row][col]=matrix[row*4+col];
    for (int col=0; col<4; ++col) {
        int pivot=col;
        for (int row=col+1; row<4; ++row)
            if (std::fabs(a[row][col])>std::fabs(a[pivot][col])) pivot=row;
        if (a[pivot][col]==0.0) return false;
        if (pivot!=col)
            for (int k=0; k<4; ++k) {
                const double value=a[col][k];
                a[col][k]=a[pivot][k];
                a[pivot][k]=value;
            }
        for (int row=col+1; row<4; ++row) {
            const double factor=a[row][col]/a[col][col];
            if (!std::isfinite(factor)) return false;
            for (int k=col+1; k<4; ++k) a[row][k]-=factor*a[col][k];
            a[row][col]=0.0;
        }
    }
    return true;
}
} // namespace

bool convert_engine_cpu_camera(const float engine_screen[16],
                               const float viewport[4], EngineCameraView& output) {
    if (!engine_screen || !viewport) return false;
    for (int i=0; i<16; ++i) if (!std::isfinite(engine_screen[i])) return false;
    if (!invertible(engine_screen)) return false;
    for (int i=0; i<4; ++i) if (!std::isfinite(viewport[i])) return false;
    const double x=viewport[0], y=viewport[1], w=viewport[2], h=viewport[3];
    if (x<0 || y<0 || w<1 || h<1 || w>16384 || h>16384) return false;

    EngineCameraView next{};
    for (int i=0; i<4; ++i) next.viewport[i]=viewport[i];
    // Transpose converts the engine's row-vector matrix to column-vector form.
    // Scale/flip NDC axes so ordinary viewport mapping reproduces the CPU
    // ProjectPoint pixel rule, including its width-1/height-1 endpoints.
    const double sx=2.0*(w-1.0)/w, sy=2.0*(h-1.0)/h;
    for (int col=0; col<4; ++col) {
        const double u=engine_screen[col*4];
        const double v=engine_screen[col*4+1];
        const double depth=engine_screen[col*4+2];
        const double homogeneous=engine_screen[col*4+3];
        next.world_to_clip[col]=static_cast<float>(sx*u-homogeneous);
        next.world_to_clip[4+col]=static_cast<float>(homogeneous-sy*v);
        next.world_to_clip[8+col]=static_cast<float>(depth);
        next.world_to_clip[12+col]=static_cast<float>(homogeneous);
    }
    for (float value:next.world_to_clip) if (!std::isfinite(value)) return false;
    output=next;
    return true;
}
} // namespace a2fo::native11
