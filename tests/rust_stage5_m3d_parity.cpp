#include "m3d_conversion.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct W {
    std::vector<std::uint8_t> b;
    void u8(std::uint8_t x) { b.push_back(x); }
    void u16(std::uint16_t x) { u8(x); u8(x >> 8); }
    void u32(std::uint32_t x) { for (int i=0;i<4;++i) u8(x >> (8*i)); }
    void f32(float x) { std::uint32_t bits; std::memcpy(&bits,&x,4); u32(bits); }
    void id(const std::string& x) { u16(static_cast<std::uint16_t>(x.size())); b.insert(b.end(),x.begin(),x.end()); }
    void matrix() { for (float x : std::array<float,12>{1,0,0,0,1,0,0,0,1,0,0,0}) f32(x); }
};
std::vector<std::uint8_t> valid196(bool mesh, bool second_root=false,
                                  bool empty_faces=false, bool bad_index=false) {
    W w; for (auto x : std::array<std::uint8_t,10>{'M','a','d','3','D','_','S','W',0,0}) w.u8(x);
    w.f32(1.96f); w.u16(1); w.id(std::string("Mat\0",4));
    for (float x : std::array<float,9>{0.1f,0.2f,0.3f,1,1,1,0.4f,0.5f,0.6f}) w.f32(x);
    w.f32(16); w.u8(2); w.u8(1); w.u16(second_root?2:1);
    w.id(mesh?"mesh":"root"); w.u32(1); w.u16(mesh?1:0); w.u32(0); w.matrix();
    if (mesh) {
        w.u32(0); w.u16(0); w.u16(3); w.u16(3); w.u16(1);
        for (float x : std::array<float,9>{0,0,0,1,0,0,0,1,0}) w.f32(x);
        for (int i=0;i<3;++i) {w.f32(0);w.f32(0);w.f32(1);}
        for (float x : std::array<float,6>{0,0,1,0,0,1}) w.f32(x);
        w.u16(empty_faces?0:1);w.id("Mat");w.u32(2);w.id("Textures\\Ship.DDS");w.u32(0);w.id("Normal.TGA");w.u32(0x200);w.f32(0.75f);w.id("AlphaThreshold");
        if (!empty_faces) for (std::uint16_t i=0;i<3;++i) {w.u16(bad_index&&i==1?9:i);w.u16(i);}w.u8(1);w.u16(0);
    }
    if (second_root) {w.id("other");w.u32(2);w.u16(11);w.u32(0);w.matrix();}
    w.u16(0);w.u32(42);w.u16(0);return w.b;
}
std::vector<std::uint8_t> valid194() {
    W w;for (auto x : std::array<std::uint8_t,10>{'M','a','d','3','D','_','S','W',0,0}) w.u8(x);
    w.f32(1.94f);w.u16(0);w.u16(1);w.u16(12);w.id(std::string("ve\0nt",5));w.id("");w.matrix();w.id(std::string("smoke\0",6));w.u16(0);w.u16(0);return w.b;
}
std::vector<std::uint8_t> valid206(bool huge_bounds=false,
                                  bool huge_records=false) {
    W w;for (auto x : std::array<std::uint8_t,10>{'M','a','d','3','D','_','S','W',0,0}) w.u8(x);
    w.f32(2.06f);w.u16(0);w.u16(1);w.id("wide");w.u32(3);w.u16(1);w.u32(0);w.matrix();
    w.u32(2);w.u16(0);w.u16(3);w.u16(3);w.u16(3);w.u16(1);
    for (float x : std::array<float,9>{0,0,0,1,0,0,0,1,0}) w.f32(x);
    for (int i=0;i<3;++i) {w.f32(0);w.f32(0);w.f32(1);}
    for (float x : std::array<float,6>{0,0,1,0,0,1}) w.f32(x);
    w.u16(1);w.id("mat");for (int i=0;i<5;++i) {w.id(i==0?"ship.png":"");w.u32(0);}w.id("add");w.id("extra");
    for (std::uint32_t i=0;i<3;++i) w.u32(i);w.u32(huge_records?10000001:3);
    if (huge_records) return w.b;
    for (std::uint32_t i=0;i<3;++i) {w.u32(i);w.u32(i);w.u32(i);}
    for (int i=0;i<3*24;++i) w.u8(0);for (int i=0;i<24;++i) w.u8(0);w.u32(huge_bounds?10000001:0);w.u32(0);w.u16(0);w.u8(1);
    w.u16(0);w.u32(0);w.u16(0);return w.b;
}
std::vector<std::uint8_t> damage(bool ambiguous) {
    W w;for (auto x : std::array<std::uint8_t,10>{'M','a','d','3','D','_','S','W',0,0}) w.u8(x);
    w.f32(1.96f);w.u16(0);w.u16(ambiguous?7:6);
    struct N {const char* name;std::uint32_t id,parent;std::uint16_t kind;float x;};
    std::vector<N> nodes={{"root",1,0,0,0},{"damage",2,1,0,0},{"engines",3,2,0,2},{"l_engine",4,1,0,4},{"break",5,4,0,1},{"vent",6,5,12,3}};
    if (ambiguous) nodes.push_back({"engines",7,2,0,0});
    for (const auto& n:nodes) {w.id(n.name);w.u32(n.id);w.u16(n.kind);w.u32(n.parent);w.matrix();
        // Identity matrix translation x is the first of the final three f32 values.
        if (n.x!=0) {std::uint32_t bits;std::memcpy(&bits,&n.x,4);const auto p=w.b.size()-12;for (int i=0;i<4;++i) w.b[p+i]=bits>>(8*i);}
        if (n.kind==12) w.id("plasma");
    }
    w.u16(0);w.u32(0);w.u16(0);return w.b;
}
std::string hex(const std::uint8_t* data,std::size_t n) {std::ostringstream o;o<<std::hex<<std::setfill('0');for (std::size_t i=0;i<n;++i)o<<std::setw(2)<<unsigned(data[i]);return o.str();}
std::string hex(const std::string& s) {return hex(reinterpret_cast<const std::uint8_t*>(s.data()),s.size());}
std::string hex(const std::vector<std::uint8_t>& b) {return hex(b.data(),b.size());}
void row(const std::string& label,const std::vector<std::uint8_t>& input) {
    std::vector<std::uint8_t> output{0xaa,0xbb};a2fo::m3d::ConversionStats s;s.materials=99;s.warnings.push_back("old");std::string error="old";
    const bool ok=a2fo::m3d::convert_bytes_to_sod(input,output,s,error);
    std::cout<<label<<'|'<<ok<<'|'<<hex(output)<<'|'<<hex(error)<<'|'<<hex(reinterpret_cast<const std::uint8_t*>(&s.source_version),4)
             <<'|'<<s.materials<<','<<s.source_nodes<<','<<s.output_nodes<<','<<s.meshes<<','<<s.groups<<','<<s.triangles<<','<<s.animations<<','<<s.texture_references<<','<<s.source_roots<<','<<s.synthesized_root<<','<<s.reordered_nodes<<','<<s.disambiguated_node_names<<','<<s.damage_emitters_adapted<<','<<s.sod_materials_end<<','<<s.sod_nodes_end<<','<<s.sod_animations_end<<','<<s.sod_references_end<<','<<s.legacy_sod_passthrough;
    for (const auto& v:s.warnings) std::cout<<"|W:"<<hex(v);
    for (const auto& v:s.hierarchy_bindings) std::cout<<"|H:"<<hex(v.node_name)<<':'<<hex(v.parent_name)<<':'<<hex(v.authored_name);
    for (const auto& v:s.texture_bindings) {std::cout<<"|T:"<<hex(v.mesh_name)<<':'<<hex(v.diffuse_texture)<<':'<<hex(v.bump_texture)<<':'<<v.has_local_bounds;for (auto x:v.local_bounds_min)std::cout<<':'<<hex(reinterpret_cast<const std::uint8_t*>(&x),4);for (auto x:v.local_bounds_max)std::cout<<':'<<hex(reinterpret_cast<const std::uint8_t*>(&x),4);}
    std::cout<<'\n';
}
} // namespace
int main() {
    auto mesh=valid196(true);row("mesh196",mesh);row("roots196",valid196(false,true));row("vent194",valid194());row("wide206",valid206());row("wide-huge-bounds",valid206(true));row("wide-huge-records",valid206(false,true));row("damage",damage(false));row("damage-ambiguous",damage(true));row("empty-group",valid196(true,false,true));row("bad-index",valid196(true,false,false,true));
    row("sod",{'S','t','o','r','m','3','D','_','S','W',0,1});row("db",{'S','t','a','r','T','r','e','k','D','B',7});row("badmagic",{'B','a','d'});
    for (std::size_t n=0;n<mesh.size();++n) row("trunc"+std::to_string(n),{mesh.begin(),mesh.begin()+n});
    auto wide=valid206();for (std::size_t n=0;n<wide.size();++n) row("wide-trunc"+std::to_string(n),{wide.begin(),wide.begin()+n});
    auto bad=mesh;bad[14]=0xff;bad[15]=0xff;row("material-count",bad);bad=mesh;bad.push_back(0);row("trailing",bad);bad=mesh;bad[10]=0;bad[11]=0;bad[12]=0;bad[13]=0;row("versionzero",bad);
    bad=mesh;bad[14]=0;bad[15]=0;row("shifted",bad);
}
