#include "../renderer/refit/lights.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo;
namespace {
A2FORefitLight registered_point(){
    A2FORefitLight p{};
    p.kind=A2FO_REFIT_POINT;p.position[0]=2;p.position[1]=3;p.position[2]=4;
    p.direction[1]=-1;p.colour[0]=.2f;p.colour[1]=.3f;p.colour[2]=.4f;
    p.intensity=5;p.start=6;p.range=7;return p;
}
native11::Light odf_directional(){
    native11::Light l{};l.type=native11::LightType::directional;
    l.direction[0]=.6f;l.direction[1]=-.8f;l.direction[2]=0;
    l.colour[0]=.1f;l.colour[1]=.2f;l.colour[2]=.3f;
    l.intensity=4;l.falloff_start=2;l.falloff_range=9;return l;
}
native11::Light odf_point(){
    native11::Light l{};l.type=native11::LightType::point;
    l.position[0]=11;l.position[1]=12;l.position[2]=13;
    l.colour[0]=.4f;l.colour[1]=.5f;l.colour[2]=.6f;
    l.intensity=7;l.falloff_start=8;l.falloff_range=9;return l;
}
native11::LightingFrame world(){
    native11::LightingFrame frame{};
    frame.source_ids={4,12};frame.lights={odf_directional(),odf_point()};
    frame.ambient[0]=.1f;frame.shadows.light_mask=3;
    return frame;
}
void refused(const std::vector<A2FORefitLight>& registered,
             const native11::LightingFrame& frame){
    std::vector<A2FORefitLight> output{registered_point()};
    std::string error="old";
    assert(!refit::append_world_lights2(registered,frame,output,error));
    assert(!error.empty()&&output.size()==1&&output[0].kind==A2FO_REFIT_POINT&&
           output[0].position[0]==2&&output[0].intensity==5);
}
}
int main(){
    auto frame=world();
    std::vector<A2FORefitLight> registered{registered_point()},output;
    std::string error="old";
    assert(refit::append_world_lights2(registered,frame,output,error)&&error.empty());
    assert(output.size()==3&&output[0].kind==A2FO_REFIT_POINT&&
           output[0].position[0]==2&&output[0].intensity==5);
    assert(output[1].kind==A2FO_REFIT_DIRECTIONAL&&output[1].direction[0]==.6f&&
           output[1].colour[2]==.3f&&output[1].intensity==4&&
           output[1].start==2&&output[1].range==9);
    assert(output[2].kind==A2FO_REFIT_POINT&&output[2].position[0]==11&&
           output[2].colour[1]==.5f&&output[2].intensity==7&&
           output[2].start==8&&output[2].range==9);
    registered[0].position[0]=99;frame.lights[0].colour[2]=99;
    assert(output[0].position[0]==2&&output[1].colour[2]==.3f);
    frame=world();
    frame.shadows.light_mask=0xffffffffu;
    assert(refit::append_world_lights2({},frame,output,error)&&output.size()==2);

    auto bad=world();bad.source_ids.pop_back();refused({},bad);
    bad=world();bad.source_ids={4,4};refused({},bad);
    bad=world();bad.source_ids={0,12};refused({},bad);
    bad=world();bad.source_ids={12,4};refused({},bad);
    bad=world();bad.lights[0].type=native11::LightType::spot;refused({},bad);
    bad=world();bad.lights[0].type=static_cast<native11::LightType>(99);refused({},bad);
    bad=world();bad.lights[0].direction[0]=bad.lights[0].direction[1]=0;
    refused({},bad);
    bad=world();bad.lights[1].falloff_range=0;refused({},bad);
    bad=world();bad.lights[1].position[0]=std::numeric_limits<float>::infinity();refused({},bad);
    bad=world();bad.lights[1].colour[0]=-1;refused({},bad);
    bad=world();bad.lights[1].intensity=1e12f;bad.lights[1].colour[0]=2;refused({},bad);
    bad=world();bad.lights[0].cone_inner_degrees=std::numeric_limits<float>::quiet_NaN();
    refused({},bad);
    bad=world();bad.ambient[0]=std::numeric_limits<float>::quiet_NaN();refused({},bad);
    bad=world();bad.ambient[0]=-1;refused({},bad);
    bad=world();bad.source_ids.resize(9);bad.lights.resize(9);refused({},bad);
    std::vector<A2FORefitLight> too_many(32,registered_point());refused(too_many,world());
    auto short_direction=registered_point();short_direction.kind=A2FO_REFIT_DIRECTIONAL;
    short_direction.direction[0]=short_direction.direction[1]=short_direction.direction[2]=0;
    refused({short_direction},world());
    auto bad_range=registered_point();bad_range.range=0;refused({bad_range},world());
    auto bad_coord=registered_point();bad_coord.position[0]=1e13f;
    refused({bad_coord},world());
    auto bad_energy=registered_point();bad_energy.intensity=1e12f;bad_energy.colour[0]=2;
    refused({bad_energy},world());
    auto unknown=registered_point();unknown.kind=99;refused({unknown},world());
    auto directional=registered_point();directional.kind=A2FO_REFIT_DIRECTIONAL;
    directional.direction[1]=-1;
    refused(std::vector<A2FORefitLight>(4,directional),world());
    assert(refit::append_world_lights2(std::vector<A2FORefitLight>(31,registered_point()),
                                      native11::LightingFrame{},output,error));
    assert(output.size()==31);
    native11::LightingFrame eight{};
    eight.lights.assign(8,odf_point());
    eight.source_ids={1,2,3,4,5,6,7,8};
    assert(refit::append_world_lights2(std::vector<A2FORefitLight>(24,registered_point()),
                                      eight,output,error));
    assert(output.size()==32);
    refused(std::vector<A2FORefitLight>(25,registered_point()),eight);
    assert(refit::append_world_lights2(std::vector<A2FORefitLight>(4,directional),
                                      native11::LightingFrame{},output,error));
    assert(output.size()==4);

    const auto baseline=world();
    assert(refit::equal_world_lighting2(baseline,baseline));
    auto changed=baseline;changed.source_ids[0]=3;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].position[0]=1;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].direction[0]=1;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].type=native11::LightType::point;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].colour[0]=1;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].intensity=2;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].falloff_start=3;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].falloff_range=3;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].cone_inner_degrees=31;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].cone_outer_degrees=61;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.ambient[1]=.9f;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.shadows.light_mask=7;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.shadows.resolution=1024;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.shadows.depth_bias=.02f;assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.shadows.filter=native11::ShadowFilter::hard;
    assert(!refit::equal_world_lighting2(baseline,changed));
    changed=baseline;changed.lights[0].colour[0]=std::numeric_limits<float>::quiet_NaN();
    assert(!refit::equal_world_lighting2(baseline,changed));

    // ABI3: spots become A2FORefitSpot3; other sources match the ABI2 conversion.
    {
        native11::Light spot{};spot.type=native11::LightType::spot;
        spot.position[0]=1;spot.position[1]=2;spot.position[2]=3;spot.direction[2]=-1;
        spot.colour[0]=.7f;spot.intensity=2;spot.falloff_start=4;spot.falloff_range=5;
        spot.cone_inner_degrees=30;spot.cone_outer_degrees=60;
        auto frame=world();frame.source_ids={4,7,12};frame.lights={odf_directional(),spot,odf_point()};
        std::vector<A2FORefitLight> v2_out,v3_out;std::vector<A2FORefitSpot3> spots;std::string e2,e3;
        assert(!refit::append_world_lights2({registered_point()},frame,v2_out,e2)&&!e2.empty()); // ABI2 still refuses
        assert(refit::append_world_lights3({registered_point()},frame,v3_out,spots,e3)&&e3.empty());
        assert(v3_out.size()==3&&v3_out[0].kind==A2FO_REFIT_POINT&&v3_out[1].kind==A2FO_REFIT_DIRECTIONAL&&
               v3_out[2].kind==A2FO_REFIT_POINT&&v3_out[2].position[0]==11);
        assert(spots.size()==1&&spots[0].position[2]==3&&spots[0].direction[2]==-1&&spots[0].colour[0]==.7f&&
               spots[0].intensity==2&&spots[0].start==4&&spots[0].range==5);
        assert(spots[0].inner>.5235f&&spots[0].inner<.5237f&&spots[0].outer>1.0471f&&spots[0].outer<1.0473f);
        // Without spots, ABI3 equals ABI2 exactly.
        std::vector<A2FORefitLight> a,b;std::vector<A2FORefitSpot3> none{A2FORefitSpot3{}};std::string ea,eb;
        assert(refit::append_world_lights2({registered_point()},world(),a,ea));
        assert(refit::append_world_lights3({registered_point()},world(),b,none,eb)&&none.empty()&&a.size()==b.size());
        for(std::size_t k=0;k<a.size();++k)assert(std::memcmp(&a[k],&b[k],sizeof(A2FORefitLight))==0);
        // Transactional rejection keeps both outputs.
        auto rejected3=[&](const native11::LightingFrame& bad){
            std::vector<A2FORefitLight> o{registered_point()};std::vector<A2FORefitSpot3> sp(2);std::string e="old";
            assert(!refit::append_world_lights3({},bad,o,sp,e)&&!e.empty()&&o.size()==1&&sp.size()==2);};
        auto bad=frame;bad.lights[1].cone_inner_degrees=70;rejected3(bad);           // inner > outer
        bad=frame;bad.lights[1].cone_outer_degrees=180;rejected3(bad);               // outer >= 180
        bad=frame;bad.lights[1].cone_inner_degrees=0;rejected3(bad);                 // zero inner
        bad=frame;bad.lights[1].falloff_range=0;rejected3(bad);                      // zero range
        bad=frame;bad.lights[1].direction[0]=bad.lights[1].direction[1]=bad.lights[1].direction[2]=0;rejected3(bad); // zero direction
        bad=frame;bad.lights[1].colour[1]=-1;rejected3(bad);                         // negative colour
        bad=frame;bad.lights[1].position[0]=std::numeric_limits<float>::infinity();rejected3(bad);
        bad=frame;bad.source_ids={4,4,12};rejected3(bad);                             // ID order
        bad=frame;bad.source_ids.pop_back();rejected3(bad);                           // count mismatch
        bad=frame;bad.lights={spot,spot,spot,spot,spot};bad.source_ids={1,2,3,4,5};rejected3(bad); // > 4 spots
        bad=frame;bad.lights[0].direction[0]=bad.lights[0].direction[1]=0;rejected3(bad); // ABI2 rule still applies
        auto four=frame;four.lights={spot,spot,spot,spot};four.source_ids={1,2,3,4};
        std::vector<A2FORefitLight> o4;std::vector<A2FORefitSpot3> s4;std::string e4;
        assert(refit::append_world_lights3({},four,o4,s4,e4)&&o4.empty()&&s4.size()==4);
    }
}
