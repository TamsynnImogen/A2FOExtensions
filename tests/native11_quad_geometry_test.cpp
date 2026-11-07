#include "../renderer/native11/quad_geometry.hpp"

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace a2fo::native11;

namespace {
void require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
bool near(float actual, float expected) { return std::abs(actual-expected)<1e-5f; }
void same_sentinel(const QuadGeometry& geometry) {
    require(geometry.vertices.size()==1 && geometry.indices.size()==1 &&
            geometry.vertices[0].position[0]==42 && geometry.indices[0]==99,
            "failed build changed output");
}
QuadGeometry sentinel() {
    QuadGeometry result;
    result.vertices.push_back(Vertex{{42,0,0},{0,0,0},{0,0}});
    result.indices.push_back(99);
    return result;
}
}

int main() {
    std::string error;
    QuadGeometry geometry=sentinel();
    ScreenSprite sprite;
    sprite.left=-10; sprite.top=5; sprite.width=20; sprite.height=10;
    sprite.depth=0.25f; sprite.has_clip=true; sprite.clip={0,8,8,12};
    require(build_screen_sprite(sprite,100,100,geometry,error),"clipped sprite builds");
    require(error.empty() && geometry.vertices.size()==4 && geometry.indices.size()==6,
            "sprite geometry size and error");
    const Vertex& tl=geometry.vertices[0];
    const Vertex& tr=geometry.vertices[1];
    const Vertex& br=geometry.vertices[2];
    const Vertex& bl=geometry.vertices[3];
    require(near(tl.position[0],-1) && near(tl.position[1],0.84f) &&
            near(br.position[0],-0.84f) && near(br.position[1],0.76f),
            "sprite clipspace corners");
    require(near(tl.uv[0],0.5f) && near(tl.uv[1],0.3f) &&
            near(br.uv[0],0.9f) && near(br.uv[1],0.7f) &&
            near(tr.uv[1],0.3f) && near(bl.uv[0],0.5f),
            "clipping interpolates both UV axes");
    require(near(tl.position[2],0.25f) && near(br.position[2],0.25f) &&
            near(tl.normal[2],-1),"sprite depth and normal");
    require(geometry.indices==std::vector<std::uint32_t>({0,1,2,0,2,3}),
            "sprite triangle indices");
    const float signed_area=(tr.position[0]-tl.position[0])*(br.position[1]-tl.position[1])-
                            (tr.position[1]-tl.position[1])*(br.position[0]-tl.position[0]);
    require(signed_area<0,"sprite front is clockwise in clipspace");

    sprite.left=200; sprite.has_clip=false;
    require(build_screen_sprite(sprite,100,100,geometry,error) &&
            geometry.vertices.empty() && geometry.indices.empty(),
            "outside sprite succeeds with empty geometry");
    geometry=sentinel();
    sprite.width=0;
    require(!build_screen_sprite(sprite,100,100,geometry,error) && !error.empty(),
            "zero sprite width rejected");
    same_sentinel(geometry);
    sprite.width=20; sprite.depth=1.1f;
    require(!build_screen_sprite(sprite,100,100,geometry,error),"invalid depth rejected");
    same_sentinel(geometry);
    sprite.depth=0; sprite.uv.u1=1.1f;
    require(!build_screen_sprite(sprite,100,100,geometry,error),"out-of-atlas UV rejected");
    same_sentinel(geometry);
    sprite.uv.u1=1; sprite.has_clip=true; sprite.clip={8,0,7,10};
    require(!build_screen_sprite(sprite,100,100,geometry,error),"reversed clip rejected");
    same_sentinel(geometry);
    sprite.has_clip=false; sprite.left=0;
    require(!build_screen_sprite(sprite,0,100,geometry,error) &&
            !build_screen_sprite(sprite,16385,100,geometry,error),
            "target dimensions bounded");
    same_sentinel(geometry);
    sprite.left=std::numeric_limits<float>::quiet_NaN();
    require(!build_screen_sprite(sprite,100,100,geometry,error),"non-finite sprite rejected");
    same_sentinel(geometry);

    WorldBillboard billboard;
    billboard.center[0]=1; billboard.center[1]=2; billboard.center[2]=3;
    billboard.camera_right[0]=0; billboard.camera_right[1]=1;
    billboard.camera_up[0]=-1; billboard.camera_up[1]=0;
    billboard.width=2; billboard.height=4;
    require(build_world_billboard(billboard,geometry,error),"rotated billboard builds");
    require(error.empty() && geometry.vertices.size()==4 && geometry.indices.size()==6,
            "billboard geometry size and error");
    require(near(geometry.vertices[0].position[0],-1) &&
            near(geometry.vertices[0].position[1],1) &&
            near(geometry.vertices[2].position[0],3) &&
            near(geometry.vertices[2].position[1],3) &&
            near(geometry.vertices[2].position[2],3),
            "rotated billboard world corners");
    require(near(geometry.vertices[0].normal[0],0) &&
            near(geometry.vertices[0].normal[1],0) &&
            near(geometry.vertices[0].normal[2],-1),
            "billboard normal is negative basis cross");
    require(geometry.indices==std::vector<std::uint32_t>({0,1,2,0,2,3}),
            "billboard triangle indices");

    geometry=sentinel();
    billboard.camera_up[0]=0; billboard.camera_up[1]=1;
    require(!build_world_billboard(billboard,geometry,error),"parallel basis rejected");
    same_sentinel(geometry);
    billboard.camera_up[0]=-2; billboard.camera_up[1]=0;
    require(!build_world_billboard(billboard,geometry,error),"non-unit basis rejected");
    same_sentinel(geometry);
    billboard.camera_up[0]=-1;
    billboard.center[1]=FLT_MAX; billboard.width=FLT_MAX;
    require(!build_world_billboard(billboard,geometry,error),"float position overflow rejected");
    same_sentinel(geometry);
    billboard.center[1]=2; billboard.width=0;
    require(!build_world_billboard(billboard,geometry,error),"degenerate billboard rejected");
    same_sentinel(geometry);
    std::puts("native11 quad geometry: PASS");
}
