#include "fpq_paths.hpp"
#include "odf_paths.hpp"
#include "race_menu_policy.hpp"
#include "team_color_policy.hpp"
#include "edit_menu_odf.hpp"
#include "upgrade_pod_config.hpp"
#include "art_texture_suffix_config.hpp"
#include "refit_policy.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>
namespace edit = a2fo::edit_menu;
namespace pods = a2fo::upgrade_pods;
namespace nebula = a2fo::nebula;
namespace refit = a2fo::refit;
static std::string hex(const std::string& s) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    for (unsigned char b : s) { out += digits[b>>4]; out += digits[b&15]; }
    return out;
}
static uint32_t bits(float value) { uint32_t b; std::memcpy(&b,&value,4); return b; }
static void write32(std::vector<uint8_t>& b, size_t at, uint32_t value) {
    for (int i=0;i<4;++i) b[at+i]=uint8_t(value>>(8*i));
}
static void paths(const std::vector<std::string>& samples) {
    std::map<std::string,std::string> aliases{{"foo.odf","ODF\\ships\\Foo.ODF"},{"a.odf",""}};
    for (const auto& p : samples) {
        std::string aliased="sentinel";
        bool found=a2fo::find_recursive_odf_alias(p,aliases,aliased);
        std::cout << "odf " << hex(p) << ' ' << hex(a2fo::recursive_odf_basename_key(p)) << ' ' << found << ' ' << hex(aliased) << '\n';
    }
}
static void fpq() {
    std::vector<uint8_t> valid(28+24+12+25+11);
    valid[0]='F';valid[1]='P';valid[2]='Q';
    write32(valid,4,2);write32(valid,8,1);write32(valid,12,1);write32(valid,16,2);write32(valid,24,11);
    write32(valid,52,0xffffffffu);write32(valid,56,0);write32(valid,60,3);
    write32(valid,64+12,3);write32(valid,64+16,8);write32(valid,64+21,0);
    std::memcpy(valid.data()+89,"odfShip.ODF",11);
    auto show=[](const std::vector<uint8_t>& b) {
        auto r=a2fo::parse_fpq_odf_directories(b);
        std::cout << "fpq " << r.ok << ',' << r.version << ',' << r.file_count << ',' << hex(r.error);
        for (const auto& d:r.odf_directories) std::cout << ',' << hex(d);
        std::cout << '\n';
    };
    show(valid);
    for (size_t i : {size_t(0),size_t(3),size_t(27),size_t(28),size_t(52),size_t(88),size_t(100)}) show({valid.begin(),valid.begin()+i});
    for (size_t at : {size_t(0),size_t(4),size_t(8),size_t(12),size_t(16),size_t(24),size_t(52),size_t(56),size_t(60),size_t(76),size_t(80),size_t(85),size_t(89)}) {
        auto b=valid;b[at]^=0xff;show(b);
    }
    std::mt19937 rng(0x465051);
    for (int i=0;i<100;++i) {
        auto b=valid;
        for (int j=0,n=int(rng()%5)+1;j<n;++j) b[rng()%b.size()] ^= uint8_t(rng());
        show(b);
    }
}
static void race() {
    std::vector<a1compat::LegacyRaceMenuInput> list(3);
    list[0]={true,1,true,false,true,"Alpha"};
    list[1]={true,0,false,false,true,"Beta"};
    list[2]={true,2,true,true,true,"Gamma"};
    for (size_t declared : {size_t(0),size_t(1),size_t(3),size_t(100)}) {
        for (int mode=0;mode<8;++mode) {
            auto copy=list;
            if (mode&1) copy[1].instant_action_slot=1;
            if (mode&2) copy[0].has_instant_action_slot=false;
            if (mode&4) copy[2].has_display_key=false;
            auto p=a1compat::build_legacy_race_menu_plan(copy,declared);
            std::cout << "race " << declared << ',' << mode << ',' << p.fallback_active << ',' << p.playable_count;
            for (const auto& e:p.entries) std::cout << ',' << e.instant_action_slot << ':' << e.replace_display_key << ':' << hex(e.display_key);
            std::cout << '\n';
        }
    }
}
static void team(const std::vector<std::string>& samples) {
    a1compat::TeamColorPalettePolicy palette;
    for (const auto& s : samples) {
        auto r=a1compat::merge_team_color_odf(s,palette);
        std::cout << "team " << hex(s) << ' ' << r.indexed_values << ',' << r.legacy_aliases;
        for (size_t i=0;i<a1compat::kPlayerTeamColorCount;++i) std::cout << ',' << palette.present[i] << ':' << bits(palette.colors[i].red) << ':' << bits(palette.colors[i].green) << ':' << bits(palette.colors[i].blue);
        std::cout << '\n';
    }
}
static void edit_menu(const std::vector<std::string>& samples) {
    for (const auto& s : samples) {
        edit::MenuNode n; n.title="prior"; std::string error="prior";
        bool ok=edit::parse_menu_node(s,"ODF\\Menus\\Main.ODF",&n,&error);
        std::cout << "edit " << hex(s) << ' ' << ok << ',' << hex(error) << ',' << hex(n.source_name) << ',' << hex(n.title) << ',' << n.force_to_neutral << ',' << n.is_submenu();
        for (size_t i=0;i<edit::kEntryCount;++i) std::cout << ',' << hex(n.build_items[i]) << ':' << hex(n.items[i]);
        std::cout << '\n';
    }
    edit::MenuNode n;std::string error;
    std::cout << "edit-null " << edit::parse_menu_node("x","x",nullptr,&error) << ':' << hex(error) << '\n';
    std::cout << "edit-empty " << edit::parse_menu_node("x","",&n,&error) << ':' << hex(error) << '\n';
    for (auto s:samples) std::cout << "edit-name " << hex(s) << ':' << hex(edit::normalize_odf_name(s)) << '\n';
}
static void upgrade(const std::vector<std::string>& samples) {
    for (const auto& s : samples) for (uint32_t initial : {0u,3u,16u}) {
        auto value=initial; auto status=pods::parse_maximum_tier_setting(s,&value);
        std::cout << "upgrade " << hex(s) << ',' << initial << ':' << int(status) << ',' << value << '\n';
    }
    std::cout << "upgrade-null " << int(pods::parse_maximum_tier_setting("upgradePodMaximumTier=4",nullptr)) << '\n';
}
static void art(const std::vector<std::string>& samples) {
    nebula::ArtTextureSuffixConfig config;
    for (const auto& s:samples) {
        auto r=nebula::parse_art_texture_suffix_config(s,&config);
        bool found=false;bool disabled=nebula::settings_disable_bump(s,&found);
        std::cout << "art " << hex(s) << ' ' << r.valid_assignments << ',' << r.invalid_assignments << ',' << r.emissive_suffix_found << ',' << r.bump_suffix_found << ',' << r.specular_suffix_found << ',' << r.emissive_bump_multiplier_found << ',' << r.bump_light_bias_found << ',' << r.emissive_diffuse_restore_found << ',' << hex(config.emissive_suffix) << ',' << hex(config.bump_suffix) << ',' << hex(config.specular_suffix) << ',' << bits(config.emissive_bump_multiplier) << ',' << bits(config.bump_light_bias) << ',' << bits(config.emissive_diffuse_restore) << ',' << found << ',' << disabled << '\n';
    }
    for (const auto& a : samples) {
        std::cout << "suffix " << hex(a) << ':' << hex(nebula::texture_name_with_suffix(a,"_x")) << ':' << hex(nebula::emissive_texture_name(a,"_e_","warp")) << '\n';
    }
    auto r=nebula::parse_art_texture_suffix_config("anything",nullptr);
    std::cout << "art-null " << r.valid_assignments << ',' << r.invalid_assignments << '\n';
}
static void refit_policy(const std::vector<std::string>& samples) {
    for (const auto& s:samples) std::cout << "refit-norm " << hex(s) << ':' << hex(refit::normalize_odf_value(s)) << '\n';
    refit::OdfFields fields={{"refitItem0"," \"A.odf\" "},{"REFITITEM1","a.ODF"},{"refitItem2","B.odf"},{"refitHardpoint"," hp1 "},{"buildHardpoint","hp1"},{"classLabel","ShipYard"},{"refitItem0","later"}};
    for (int mode=0;mode<6;++mode) {
        auto copy=fields;
        if (mode&1) copy[3].second="hp2";
        if (mode&2) copy[5].second="starbase";
        if (mode&4) copy.erase(copy.begin());
        auto p=refit::parse_class_policy(copy);
        std::cout << "refit " << mode << ':' << p.is_refit_source() << ',' << p.is_supported_yard() << ',' << hex(p.refit_hardpoint) << ',' << hex(p.build_hardpoint) << ',' << hex(p.class_label);
        for (const auto& item:p.refit_items) std::cout << ',' << hex(item);
        std::cout << '\n';
    }
}
int main() {
    std::vector<std::string> samples={"","foo.odf","ODF\\ships\\Foo.ODF","odf/ships/Foo.ODF","x/foo.odf","  Foo.ODF  ","/* comment */", "mpcolor01=1 0.5 0.25f;\nred=0 1 0\n","menuTitle = \"Hello\\nworld\"\nbuildItem1=\"ODF/ships/A.ODF\"\nitem12=target\nforceToNeutral=yes\n","upgradePodMaximumTier=4;upgradePodMaximumTier=16;","#define A2FO_EMISSIVE_SUFFIX \"_e_\"\n#define A2FO_BUMP_LIGHT_BIAS 0.55f\n","<disable_bump>True</disable_bump>",std::string("Hi\xff.ODF",6),std::string("A\0B",3),std::string("red=1 2 3\0junk",14),"\r\n//line\nred=1e-2f 2f 3f;","upgradePodMaximumTier = 17;",std::string("forceToNeutral=1\0junk",21),std::string("forceToNeutral=0\0junk",21), "a2fo_specular_suffix=\"_s\";"};
    std::mt19937 rng(0x12345);
    for (int i=0;i<100;++i) { std::string s;for (int j=0,n=int(rng()%32);j<n;++j) s.push_back(char(rng()%256)); samples.push_back(s); }
    paths(samples); fpq(); race(); team(samples); edit_menu(samples); upgrade(samples); art(samples); refit_policy(samples);
}
