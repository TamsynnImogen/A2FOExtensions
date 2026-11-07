#include "quad_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {

bool fail(std::string& error, const char* message) noexcept {
    try { error=message; } catch (...) { error.clear(); }
    return false;
}

bool uv_valid(const UvRect& uv) noexcept {
    return std::isfinite(uv.u0) && std::isfinite(uv.v0) &&
           std::isfinite(uv.u1) && std::isfinite(uv.v1) &&
           uv.u0>=0 && uv.v0>=0 && uv.u1<=1 && uv.v1<=1 &&
           uv.u1>=uv.u0 && uv.v1>=uv.v0;
}

bool float_fits(double value) noexcept {
    return std::isfinite(value) &&
           std::abs(value)<=static_cast<double>(std::numeric_limits<float>::max());
}

Vertex vertex(double x, double y, double z, double nx, double ny, double nz,
              double u, double v) noexcept {
    return {{static_cast<float>(x),static_cast<float>(y),static_cast<float>(z)},
            {static_cast<float>(nx),static_cast<float>(ny),static_cast<float>(nz)},
            {static_cast<float>(u),static_cast<float>(v)}};
}

void put_quad(QuadGeometry& geometry, const Vertex& a, const Vertex& b,
              const Vertex& c, const Vertex& d) {
    geometry.vertices={a,b,c,d};
    geometry.indices={0,1,2,0,2,3};
}

} // namespace

bool build_screen_sprite(const ScreenSprite& sprite, std::uint32_t target_width,
                         std::uint32_t target_height, QuadGeometry& out,
                         std::string& error) noexcept {
    if (!target_width || !target_height || target_width>16384 || target_height>16384 ||
        !std::isfinite(sprite.left) || !std::isfinite(sprite.top) ||
        !std::isfinite(sprite.width) || !std::isfinite(sprite.height) ||
        sprite.width<=0 || sprite.height<=0 || !std::isfinite(sprite.depth) ||
        sprite.depth<0 || sprite.depth>1 || !uv_valid(sprite.uv))
        return fail(error,"invalid screen sprite");
    if (sprite.has_clip &&
        (!std::isfinite(sprite.clip.left) || !std::isfinite(sprite.clip.top) ||
         !std::isfinite(sprite.clip.right) || !std::isfinite(sprite.clip.bottom) ||
         sprite.clip.right<sprite.clip.left || sprite.clip.bottom<sprite.clip.top))
        return fail(error,"invalid sprite clip rectangle");

    try {
        const double left=sprite.left, top=sprite.top;
        const double right=left+static_cast<double>(sprite.width);
        const double bottom=top+static_cast<double>(sprite.height);
        if (!std::isfinite(right) || !std::isfinite(bottom))
            return fail(error,"sprite bounds overflow");
        double x0=std::max(0.0,left), y0=std::max(0.0,top);
        double x1=std::min(static_cast<double>(target_width),right);
        double y1=std::min(static_cast<double>(target_height),bottom);
        if (sprite.has_clip) {
            x0=std::max(x0,static_cast<double>(sprite.clip.left));
            y0=std::max(y0,static_cast<double>(sprite.clip.top));
            x1=std::min(x1,static_cast<double>(sprite.clip.right));
            y1=std::min(y1,static_cast<double>(sprite.clip.bottom));
        }
        QuadGeometry candidate;
        if (x0<x1 && y0<y1) {
            const double tx0=(x0-left)/sprite.width, tx1=(x1-left)/sprite.width;
            const double ty0=(y0-top)/sprite.height, ty1=(y1-top)/sprite.height;
            const double u0=sprite.uv.u0+(sprite.uv.u1-sprite.uv.u0)*tx0;
            const double u1=sprite.uv.u0+(sprite.uv.u1-sprite.uv.u0)*tx1;
            const double v0=sprite.uv.v0+(sprite.uv.v1-sprite.uv.v0)*ty0;
            const double v1=sprite.uv.v0+(sprite.uv.v1-sprite.uv.v0)*ty1;
            const double cx0=2*x0/target_width-1, cx1=2*x1/target_width-1;
            const double cy0=1-2*y0/target_height, cy1=1-2*y1/target_height;
            if (!float_fits(cx0) || !float_fits(cx1) || !float_fits(cy0) ||
                !float_fits(cy1) || !float_fits(u0) || !float_fits(u1) ||
                !float_fits(v0) || !float_fits(v1))
                return fail(error,"sprite geometry overflow");
            put_quad(candidate,
                     vertex(cx0,cy0,sprite.depth,0,0,-1,u0,v0),
                     vertex(cx1,cy0,sprite.depth,0,0,-1,u1,v0),
                     vertex(cx1,cy1,sprite.depth,0,0,-1,u1,v1),
                     vertex(cx0,cy1,sprite.depth,0,0,-1,u0,v1));
        }
        out=std::move(candidate);
        error.clear();
        return true;
    } catch (...) { return fail(error,"sprite allocation failed"); }
}

bool build_world_billboard(const WorldBillboard& billboard, QuadGeometry& out,
                           std::string& error) noexcept {
    if (!std::isfinite(billboard.width) || !std::isfinite(billboard.height) ||
        billboard.width<=0 || billboard.height<=0 || !uv_valid(billboard.uv))
        return fail(error,"invalid billboard size or UV rectangle");
    double right[3], up[3], center[3];
    double right_length=0, up_length=0, dot=0;
    for (int i=0;i<3;++i) {
        if (!std::isfinite(billboard.center[i]) ||
            !std::isfinite(billboard.camera_right[i]) ||
            !std::isfinite(billboard.camera_up[i]))
            return fail(error,"non-finite billboard basis or center");
        center[i]=billboard.center[i];
        right[i]=billboard.camera_right[i];
        up[i]=billboard.camera_up[i];
        right_length+=right[i]*right[i];
        up_length+=up[i]*up[i];
        dot+=right[i]*up[i];
    }
    constexpr double epsilon=1e-4;
    if (std::abs(std::sqrt(right_length)-1)>epsilon ||
        std::abs(std::sqrt(up_length)-1)>epsilon ||
        std::abs(dot)>epsilon)
        return fail(error,"billboard basis is not unit orthonormal");
    const double normal[3]={-(right[1]*up[2]-right[2]*up[1]),
                            -(right[2]*up[0]-right[0]*up[2]),
                            -(right[0]*up[1]-right[1]*up[0])};
    const double half_width=static_cast<double>(billboard.width)/2;
    const double half_height=static_cast<double>(billboard.height)/2;
    double corners[4][3];
    for (int i=0;i<4;++i) {
        const double sx=(i==0 || i==3) ? -1 : 1;
        const double sy=(i==0 || i==1) ? 1 : -1;
        for (int axis=0;axis<3;++axis) {
            corners[i][axis]=center[axis]+sx*half_width*right[axis]+
                             sy*half_height*up[axis];
            if (!float_fits(corners[i][axis]))
                return fail(error,"billboard position overflow");
        }
    }
    try {
        QuadGeometry candidate;
        put_quad(candidate,
                 vertex(corners[0][0],corners[0][1],corners[0][2],normal[0],normal[1],normal[2],billboard.uv.u0,billboard.uv.v0),
                 vertex(corners[1][0],corners[1][1],corners[1][2],normal[0],normal[1],normal[2],billboard.uv.u1,billboard.uv.v0),
                 vertex(corners[2][0],corners[2][1],corners[2][2],normal[0],normal[1],normal[2],billboard.uv.u1,billboard.uv.v1),
                 vertex(corners[3][0],corners[3][1],corners[3][2],normal[0],normal[1],normal[2],billboard.uv.u0,billboard.uv.v1));
        out=std::move(candidate);
        error.clear();
        return true;
    } catch (...) { return fail(error,"billboard allocation failed"); }
}

} // namespace a2fo::native11
