#include "squadron_config.hpp"
#include "texture_variants.hpp"
#include "firing_cycle.hpp"
#include "identity_selection.hpp"
#include "system_icon_state.hpp"
#include "directional_shield_display_config.hpp"
#include "directional_shield_fill.hpp"
#include "additional_resources.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace sq = a2fo::squadrons;
namespace tx = a2fo::texture_variants;
namespace pd = a2fo::point_defense;
namespace ci = a2fo::craft_identity;
namespace rs = a2fo::resources;

static std::string hex(std::string_view value) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned char byte : value) {
        result += digits[byte >> 4]; result += digits[byte & 15];
    }
    return result;
}
static std::uint32_t bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof result);
    return result;
}
static std::uint64_t bits(double value) {
    std::uint64_t result = 0;
    std::memcpy(&result, &value, sizeof result);
    return result;
}
static void rect(const ci::RectangleF& r) {
    std::cout << bits(r.x) << ',' << bits(r.y) << ',' << bits(r.width)
              << ',' << bits(r.height);
}
static void squadron() {
    const std::vector<std::string> keys = {"  \"Fed_Frigate.odf\"  ", "Fed/ship.odf",
        "  ',', ", "A", "abc.odf", "abc.ODF", std::string("A\0B",3)};
    for (auto& value : keys) std::cout << "key " << hex(value) << ' ' << hex(sq::odf_key(value)) << '\n';
    const std::vector<sq::OdfFields> fields = {
        {{"classLabel","squadron"},{"squadMember0","ship.odf"},{"squadMemberCount0","2"}},
        {{"classLabel","squadron"},{"squadMember00","ship"},{"squadMemberCount0","1"}},
        {{"classLabel","squadron"},{"squadMember0","ship"},{"squadMemberCount0","33"}},
        {{"classLabel","squadron"},{"squadMember0","ship"},{"squadMemberCount0","1"},{"squadMember1","ship2"}},
        {{"classLabel","squadron"},{"squadMember0","ship"},{"squadMemberCount0","1"},{"squadMemberCount0","2"}},
        {{"classLabel","SQUADRON"},{"squadMember0"," ship "},{"squadMemberCount0","1"},{"squadReinforceAtYard","0"}},
        {{"classLabel","craft"}},
    };
    for (const auto& input : fields) {
        auto result = sq::parse_definition("squad.odf",input);
        std::cout << "parse " << hex(result.error) << ' ' << bool(result);
        if (result) {
            std::cout << ' ' << hex(result.definition->odf) << ' '
                      << result.definition->reinforce_at_yard << ' ' << result.definition->size();
            for (const auto& row : result.definition->members)
                std::cout << ' ' << row.row << ',' << hex(row.odf) << ',' << row.count;
        }
        std::cout << '\n';
    }
    auto def = sq::parse_definition("squad",fields[0]).definition.value();
    for (auto kind : {sq::MemberClass::mobile_craft,sq::MemberClass::missing,
                      sq::MemberClass::squadron,sq::MemberClass::unsupported}) {
        std::cout << "class " << hex(sq::validate_definition(def,[&](const std::string&){ return kind; })) << '\n';
    }
    sq::BuildEconomics output{}; output.costs[0] = 99; output.seconds = 7;
    for (int mode=0; mode<5; ++mode) {
        const auto error = sq::sum_build_economics(def,[&](const std::string&) -> std::optional<sq::BuildEconomics> {
            if (mode == 0) return std::nullopt;
            sq::BuildEconomics v{};
            v.costs[0] = mode == 2 ? -1 : mode == 3 ? std::numeric_limits<std::int32_t>::max() : 6;
            v.seconds = mode == 1 ? -1.0f : 1.25f;
            return v;
        },&output);
        std::cout << "econ " << hex(error) << ' ' << output.costs[0] << ' ' << bits(output.seconds) << '\n';
    }
    // GNU++17 keeps the final row extended, but spills the previous total
    // as double before the next resolver callback.
    sq::Definition tie;
    tie.odf = "rounding";
    tie.members = {{0,"a",1},{1,"b",1},{2,"c",1}};
    sq::BuildEconomics tie_output{};
    const auto tie_error = sq::sum_build_economics(tie,[](const std::string& name) {
        sq::BuildEconomics value{};
        value.seconds = name=="a" ? 1.0f : name=="b" ? 0x1p-24f : 0x1p-60f;
        return std::optional<sq::BuildEconomics>(value);
    }, &tie_output);
    std::cout << "econ_double_tie " << hex(tie_error) << ' ' << bits(tie_output.seconds) << '\n';
    try { (void)sq::validate_definition(def,[](const std::string&)->sq::MemberClass { throw std::runtime_error("class"); }); }
    catch (const std::runtime_error& e) { std::cout << "class_throw " << e.what() << '\n'; }
}
static void textures() {
    for (std::string value : {" Node_A ","bad/node","\tA-B\r", "", "Textures/Ship.TGA", "Café"}) {
        std::string out="unchanged";
        const bool ok = tx::normalize_faction_node_name(value,&out);
        std::cout << "node " << hex(value) << ' ' << ok << ' ' << hex(out) << '\n';
        out="unchanged";
        const bool suffix_ok = tx::normalize_faction_suffix(value,&out);
        std::cout << "suffix " << hex(value) << ' ' << suffix_ok << ' ' << hex(out) << '\n';
    }
    for (auto name : {"  Textures/Ship.TGA  ","hull_k.dds","","ship"}) {
        std::cout << "tex " << hex(tx::faction_texture_name(name,"_k")) << ' '
                  << hex(tx::texture_asset_path(name,".dds")) << '\n';
    }
    std::cout << "texslash " << hex(tx::faction_texture_name("dir/item", "/item"))
              << ' ' << hex(tx::texture_asset_path("Textures/Ship", ".DDS")) << '\n';
    for (int i=0; i<400; ++i) {
        const std::uint32_t x = 0x9e3779b9u * static_cast<std::uint32_t>(i+1);
        const double hp = (i%41) - 10.25;
        const float timer = i%31 == 0 ? std::numeric_limits<float>::quiet_NaN() : (i%17)-8.0f;
        std::cout << "sub " << i << ' ' << unsigned(tx::subsystem_condition(i&1,i&2,100,hp,timer))
                  << ' ' << tx::subsystem_mesh_choice(x,i,static_cast<std::uint32_t>(i),7)
                  << ' ' << bits(tx::subsystem_rebuild_scale(100,hp))
                  << ' ' << bits(tx::subsystem_repair_sample(x,i,i,2))
                  << ' ' << tx::subsystem_damage_policy_active(i%3,timer,i%5,i%7) << '\n';
    }
    std::cout << "flags " << tx::faction_node_flags(0xffffffff,true) << ' '
              << tx::faction_node_flags(0,false) << '\n';
}
static void cycles() {
    pd::OptionalNumber numbered[] = {{true,true,1.5f},{true,true,2.25f},{false,false,0}};
    for (int mode=0; mode<8; ++mode) {
        auto save = pd::OptionalNumber{mode>=4,true,mode==5 ? 1.5f : 1.0f};
        auto reset = pd::OptionalNumber{mode>=6,true,2.0f};
        numbered[1].present = mode != 2;
        numbered[1].valid = mode != 3;
        pd::CyclePolicy policy;
        const auto status = pd::build_policy(mode==0 ? nullptr : numbered,mode==0 ? 0 : 3,mode==0 ? 0.7f : -1.0f,
                                             save,reset,&policy);
        std::cout << "cycle_policy " << mode << ' ' << int(status) << ' ' << policy.delays.size()
                  << ' ' << policy.save_fire_cycle_point << ' ' << bits(policy.shot_cycle_reset_time)
                  << ' ' << policy.uses_numbered_delays;
        for (float v : policy.delays) std::cout << ' ' << bits(v);
        std::cout << '\n';
    }
    numbered[1] = {true,true,2.25f};
    pd::CyclePolicy policy;
    pd::build_policy(numbered,2,0,{true,true,1.0f},{true,true,2.0f},&policy);
    pd::CycleState state{99,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<std::uint64_t>::max()};
    for (int i=0;i<5;++i) {
        float delay = pd::consume_delay(policy,&state);
        pd::update_idle_reset(policy,&state,true,0,0.75f);
        std::cout << "cycle_state " << bits(delay) << ' ' << state.next_delay_index << ' '
                  << bits(state.idle_reset_remaining) << ' ' << state.fire_count << '\n';
    }
}
static void identity_and_shields() {
    for (auto index : {-1,0,1,4}) {
        std::size_t output=99;
        std::cout << "identity " << index << ' ' << ci::aligned_identity_index(index,2,&output)
                  << ' ' << output << '\n';
    }
    for (double hp : {-1.0,0.0,25.0,50.0,75.0,std::numeric_limits<double>::quiet_NaN()}) {
        ci::SystemIconState state = ci::SystemIconState::healthy;
        bool valid = ci::classify_system_icon_state(true,false,100,hp,0,&state);
        std::cout << "icon " << valid << ' ' << int(state) << '\n';
    }
    auto colour = ci::tint_system_icon_colour({0.2f,0.5f,0.7f},{0.7f,0.2f,0.1f});
    std::cout << "tint " << bits(colour[0]) << ' ' << bits(colour[1]) << ' ' << bits(colour[2]) << '\n';
    for (auto label : {" utilityWEAPON ","weapon","UtilityWeapon\t"})
        std::cout << "passive " << ci::is_passive_weapon_classlabel(label) << '\n';
    for (int k=0;k<3;++k) for (int t=0;t<3;++t)
        std::cout << "weapon " << k << t << ' ' << int(ci::weapon_icon_presentation(
            static_cast<ci::WeaponIconKind>(k),static_cast<ci::WeaponTechnologyState>(t),true)) << '\n';
    std::cout << "source " << int(ci::weapon_icon_colour_source(true,true)) << '\n';
    ci::RectangleF source{10,20,100,30}, destination{1,2,80,20};
    for (float ratio : {-1.0f,0.0f,0.3f,1.0f,2.0f,std::numeric_limits<float>::quiet_NaN()}) {
        auto fill = ci::centered_directional_shield_fill(source,destination,ratio,true);
        std::cout << "fill "; rect(fill.source); std::cout << ' '; rect(fill.destination); std::cout << '\n';
    }
    std::uint32_t seed = 0x9750a2f0u;
    for (int i=0;i<160;++i) {
        seed = seed*1664525u + 1013904223u;
        const float ratio = static_cast<float>(seed & 0xffffu) / 65535.0f;
        const auto fill = ci::centered_directional_shield_fill(source,destination,ratio,i&1);
        std::cout << "fillrandom " << i << ' '; rect(fill.source);
        std::cout << ' '; rect(fill.destination); std::cout << '\n';
        const float c1 = static_cast<float>((seed>>16)&255u)/255.0f;
        const float c2 = static_cast<float>(seed&255u)/255.0f;
        const auto tint = ci::tint_system_icon_colour({ratio,c1,c2},{c2,ratio,c1});
        std::cout << "tintrandom " << i << ' ' << bits(tint[0]) << ' '
                  << bits(tint[1]) << ' ' << bits(tint[2]) << '\n';
    }
    std::array<ci::RectangleF,4> rectangle{{{0,0,10,4},{20,0,4,10},{0,20,10,4},{0,0,4,10}}};
    for (auto p : {std::array<float,2>{1,1},{9,3},{99,99}})
        std::cout << "segment " << ci::directional_shield_segment_at(rectangle,p[0],p[1]) << '\n';
    ci::DirectionalShieldDisplayConfig config;
    for (auto text : {"directionalShieldDisplayMode=2;directionalShieldForwardPosition=1;",
                      "/*x*/ directionalShieldAftPosition=-1; //bad\n directionalShieldPortPosition=3;",
                      "directionalShieldStarboardPosition = 0;"}) {
        auto report=ci::parse_directional_shield_display_config(text,&config);
        std::cout << "shieldparse " << report.valid_assignments << ' ' << report.invalid_assignments
                  << ' ' << config.display_mode << ' ' << config.position_mapping_configured;
        for (int p : config.facing_positions) std::cout << ' ' << p;
        std::cout << '\n';
    }
    std::array<ci::RectangleF,4> mapped{};
    std::array<std::size_t,4> indices{};
    bool ok=ci::remap_directional_shield_positions(config,rectangle,&mapped,&indices);
    std::cout << "remap " << ok;
    for (auto i : indices) std::cout << ' ' << i;
    std::cout << '\n';
    ci::scale_directional_shield_default_positions(&rectangle,{false,true,false,true},256,64);
    for (const auto& r : rectangle) { std::cout << "scaled "; rect(r); std::cout << '\n'; }
    for (int mode=0;mode<3;++mode) {
        char buffer[64]{};
        bool formatted=ci::format_directional_shield_value(
            static_cast<ci::DirectionalShieldValueDisplayMode>(mode),24.5f,100,buffer,sizeof buffer);
        std::cout << "shieldvalue " << mode << ' ' << formatted << ' ' << hex(buffer) << '\n';
    }
}
static void resources() {
    for (auto value : {"0","-0","-000"," +1","-1","2147483647","2147483648"," 12\r\n","1x",""}) {
        std::int32_t out=99;
        std::cout << "parseint " << hex(value) << ' ' << rs::parse_nonnegative(value,&out)
                  << ' ' << out << '\n';
    }
    rs::Amounts amount{{10,20,-5,std::numeric_limits<std::int64_t>::max()-1}};
    rs::Costs cost{{5,30,-1,5}};
    std::cout << "afford " << rs::can_afford(amount,cost) << '\n';
    rs::credit(amount,cost);
    for (auto value : amount) std::cout << "credit " << value << '\n';
    rs::debit(amount,cost);
    for (auto value : amount) std::cout << "debit " << value << '\n';
    std::cout << "sat " << rs::saturating_add(std::numeric_limits<std::int64_t>::min(),-1) << '\n';
}

// The i686 game build uses x87. Exercise all three precision controls, then
// restore the caller's control word before another profile is run.
class X87Precision {
public:
    explicit X87Precision(unsigned precision) noexcept {
#if defined(__i386__) && defined(__GNUC__)
        __asm__ __volatile__("fnstcw %0" : "=m"(saved_));
        const unsigned short selected = static_cast<unsigned short>(
            (saved_ & ~0x0300u) | (precision << 8));
        __asm__ __volatile__("fldcw %0" : : "m"(selected));
#else
        (void)precision;
#endif
    }
    ~X87Precision() noexcept {
#if defined(__i386__) && defined(__GNUC__)
        __asm__ __volatile__("fldcw %0" : : "m"(saved_));
#endif
    }
    X87Precision(const X87Precision&) = delete;
    X87Precision& operator=(const X87Precision&) = delete;
private:
#if defined(__i386__) && defined(__GNUC__)
    unsigned short saved_ = 0;
#endif
};

static void economics_rounding_rows(unsigned precision) {
    for (unsigned rows : {2u,3u,4u}) {
        sq::Definition def;def.odf="rounding";
        for(unsigned i=0;i<rows;++i)def.members.push_back({i,std::string(1,char('a'+i)),1});
        sq::BuildEconomics out{};
        const auto error=sq::sum_build_economics(def,[](const std::string& name){
            sq::BuildEconomics v{};v.seconds=name=="a"?1.0f:name=="b"?0x1p-24f:name=="c"?0x1p-60f:0.0f;
            return std::optional<sq::BuildEconomics>(v);
        },&out);
        std::cout<<"econ_row_spills "<<precision<<' '<<rows<<' '<<hex(error)<<' '<<bits(out.seconds)<<'\n';
    }
}

static std::uint32_t next_seed(std::uint32_t& state) noexcept {
    state = state * 1664525u + 1013904223u;
    return state;
}
static float unit_float(std::uint32_t& state) noexcept {
    return static_cast<float>(next_seed(state) & 0xffffu) / 65535.0f;
}
static void numeric_sweep_case(unsigned precision, int index) {
    std::uint32_t seed = 0x7252d4a3u ^
        (static_cast<std::uint32_t>(index) * 0x9e3779b9u);
    const float a = unit_float(seed), b = unit_float(seed);
    const float c = unit_float(seed), d = unit_float(seed);
    const float ratio = index % 19 == 0 ? -0.0f :
        index % 19 == 1 ? std::numeric_limits<float>::quiet_NaN() :
        index % 19 == 2 ? 1.0f :
        index % 19 == 3 ? -a : a;
    const ci::RectangleF source{-75.0f + a * 150.0f,
        -40.0f + b * 80.0f, 0.001f + c * 1024.0f, 0.001f + d * 512.0f};
    const ci::RectangleF destination{a * 350.0f, b * -100.0f,
        0.001f + d * 800.0f, 0.001f + c * 600.0f};
    const auto fill = ci::centered_directional_shield_fill(
        source, destination, ratio, (index & 1) != 0);
    if (index == 1 || index == 6 || index == 10 || index == 132) {
        std::cout << "sweep.fill_input " << precision << ' ' << index
                  << ' ' << bits(ratio) << ',' << (index & 1) << ' ';
        rect(source); std::cout << ' '; rect(destination); std::cout << '\n';
    }
    std::cout << "sweep.fill " << precision << ' ' << index << ' ';
    rect(fill.source); std::cout << ' '; rect(fill.destination);
    std::cout << '\n';

    std::array<ci::RectangleF, 4> rectangles{{
        {a * -20.0f, b * 10.0f, 5.0f + c * 20.0f, 3.0f + d * 6.0f},
        {a * 20.0f, b * -10.0f, 3.0f + d * 6.0f, 5.0f + c * 20.0f},
        {a * -5.0f, b * 30.0f, 5.0f + d * 20.0f, 3.0f + c * 6.0f},
        {a * -30.0f, b * 5.0f, 3.0f + c * 6.0f, 5.0f + d * 20.0f}}};
    const float cursor_x = index % 7 == 0
        ? std::numeric_limits<float>::quiet_NaN()
        : index % 2 == 0 ? rectangles[0].x + rectangles[0].width : a * 80.0f - 40.0f;
    const float cursor_y = index % 2 == 0
        ? rectangles[0].y + rectangles[0].height : b * 80.0f - 40.0f;
    if (index == 1 || index == 6 || index == 10 || index == 132) {
        std::cout << "sweep.geometry_input " << precision << ' ' << index
                  << ' ' << bits(cursor_x) << ',' << bits(cursor_y) << ' ';
        for (const auto& r : rectangles) { rect(r); std::cout << ' '; }
        std::cout << '\n';
    }
    const auto hit = ci::directional_shield_segment_at(rectangles, cursor_x,
                                                         cursor_y);
    const std::array<bool, 4> authored{{(index & 1) != 0, (index & 2) != 0,
                                        (index & 4) != 0, (index & 8) != 0}};
    ci::scale_directional_shield_default_positions(
        &rectangles, authored, 0.125f + a * 1024.0f,
        0.125f + b * 1024.0f);
    std::cout << "sweep.geometry " << precision << ' ' << index << ' '
              << hit << ' ';
    for (const auto& r : rectangles) { rect(r); std::cout << ' '; }
    std::cout << '\n';

    const float channel = index % 23 == 0
        ? std::numeric_limits<float>::quiet_NaN() : c * 1.75f - 0.375f;
    const auto tint = ci::tint_system_icon_colour(
        {{a * 1.5f, channel, b}}, {{d, c * 1.75f, a * -0.25f}});
    const std::int32_t maximum = index % 31 == 0 ? 0 :
        1 + static_cast<std::int32_t>(next_seed(seed) % 100000u);
    const double max_double = static_cast<double>(maximum);
    const double hp = index % 29 == 0
        ? std::numeric_limits<double>::quiet_NaN() :
        index % 13 == 0 ? std::nextafter(max_double * 0.25, 0.0) :
        index % 13 == 1 ? std::nextafter(max_double * 0.5,
                                         std::numeric_limits<double>::infinity()) :
        index % 13 == 2 ? max_double - 0.0001 :
        index % 13 == 3 ? 0.0 :
        max_double * static_cast<double>(a * 1.25f);
    const float disable = index % 17 == 0
        ? std::numeric_limits<float>::infinity() :
        index % 3 == 0 ? -b : d;
    if (index == 1 || index == 6 || index == 10 || index == 132)
        std::cout << "sweep.system_input " << precision << ' ' << index
                  << ' ' << (index & 1) << ',' << bool(index & 2) << ','
                  << maximum << ',' << bits(hp) << ',' << bits(disable)
                  << '\n';
    ci::SystemIconState icon = ci::SystemIconState::healthy;
    const bool icon_valid = ci::classify_system_icon_state(
        index & 1, index & 2, maximum, hp, disable, &icon);
    const auto condition = tx::subsystem_condition(
        index & 1, index & 2, maximum, hp, disable);
    const auto rebuild = tx::subsystem_rebuild_scale(maximum, hp);
    const auto sample = tx::subsystem_repair_sample(
        next_seed(seed), static_cast<std::size_t>(index), next_seed(seed),
        static_cast<std::size_t>(index % 9));
    std::cout << "sweep.system " << precision << ' ' << index << ' '
              << bits(tint[0]) << ',' << bits(tint[1]) << ',' << bits(tint[2])
              << ' ' << icon_valid << ',' << int(icon) << ',' << int(condition)
              << ' ' << bits(rebuild) << ',' << bits(sample) << '\n';

    // Two canonical members keep the count weighted economics valid while
    // regularly probing exact float and native-cost overflow boundaries.
    sq::Definition def;
    def.odf = "squad";
    const std::uint32_t count_one = 1u + (next_seed(seed) % 16u);
    const std::uint32_t count_two = 1u + (next_seed(seed) % 16u);
    def.members = {{0, "alpha", count_one}, {1, "beta", count_two}};
    const float seconds_one = index % 43 == 0
        ? std::numeric_limits<float>::max() / 16.0f :
        index % 11 == 0 ? std::nextafter(0.5f, 1.0f) :
        0.0001f + a * 100000.0f;
    const float seconds_two = index % 47 == 0
        ? std::numeric_limits<float>::max() / 32.0f :
        index % 11 == 1 ? std::nextafter(1.5f, 0.0f) :
        0.0001f + b * 100000.0f;
    const std::int32_t cost_one = index % 37 == 0
        ? std::numeric_limits<std::int32_t>::max() / 16 :
        static_cast<std::int32_t>(next_seed(seed) % 100000u);
    if (index == 1 || index == 6 || index == 10 || index == 132)
        std::cout << "sweep.economics_input " << precision << ' ' << index
                  << ' ' << count_one << ',' << count_two << ','
                  << bits(seconds_one) << ',' << bits(seconds_two) << ','
                  << cost_one << '\n';
    sq::BuildEconomics total{};
    total.costs[0] = 93;
    total.seconds = 7.0f;
    const auto error = sq::sum_build_economics(def,
        [&](const std::string& name) -> std::optional<sq::BuildEconomics> {
            sq::BuildEconomics value{};
            value.seconds = name == "alpha" ? seconds_one : seconds_two;
            value.costs[0] = name == "alpha" ? cost_one :
                static_cast<std::int32_t>(next_seed(seed) % 100000u);
            return value;
        }, &total);
    std::cout << "sweep.economics " << precision << ' ' << index << ' '
              << hex(error) << ' ' << total.costs[0] << ','
              << bits(total.seconds) << '\n';

    const float half = static_cast<float>(index % 1000) + 0.5f;
    const float amount = index % 5 == 0
        ? std::nextafter(half, 0.0f) :
        index % 5 == 1 ? half :
        index % 5 == 2 ? std::nextafter(half,
                                        std::numeric_limits<float>::infinity()) :
        index % 5 == 3 ? std::numeric_limits<float>::quiet_NaN() :
        std::numeric_limits<float>::infinity();
    const float value_max = index % 41 == 0
        ? std::numeric_limits<float>::max() :
        index % 41 == 1 ? 0.0f : 1000.0f + c * 1000.0f;
    char display[160]{};
    const auto display_mode = static_cast<ci::DirectionalShieldValueDisplayMode>(
        index % 3);
    const std::size_t display_length = index % 13 == 0 ? 4u : sizeof display;
    const bool formatted = ci::format_directional_shield_value(
        display_mode, amount, value_max, display, display_length);
    std::cout << "sweep.shieldvalue " << precision << ' ' << index << ' '
              << formatted << ' ' << hex(display) << '\n';

    pd::OptionalNumber delays[3]{{true,true,0.0001f + a * 10.0f},
                                 {true,true,0.0001f + b * 10.0f},
                                 {true,true,0.0001f + c * 10.0f}};
    const std::size_t delay_count = 1u + (next_seed(seed) % 3u);
    const pd::OptionalNumber save{true,true,
        static_cast<float>(next_seed(seed) % delay_count)};
    const pd::OptionalNumber reset{true,true,
        index % 7 == 0 ? 0.0f : 0.0001f + d * 3.0f};
    pd::CyclePolicy policy;
    const auto status = pd::build_policy(delays, delay_count, 1.0f,
                                         save, reset, &policy);
    pd::CycleState state{static_cast<std::size_t>(index % 5),
        index % 17 == 0 ? std::numeric_limits<float>::quiet_NaN() :
        a * 5.0f, std::numeric_limits<std::uint64_t>::max() -
        static_cast<std::uint64_t>(index % 3)};
    pd::normalize_state(policy, &state);
    const float consumed = pd::consume_delay(policy, &state);
    pd::update_idle_reset(policy, &state, (index & 1) != 0,
                          index % 3 == 0 ? 0.0f : -b,
                          index % 5 == 0 ? 0.0f : 0.0001f + c * 2.0f);
    std::cout << "sweep.clock " << precision << ' ' << index << ' '
              << int(status) << ',' << policy.delays.size() << ','
              << policy.save_fire_cycle_point << ','
              << bits(policy.shot_cycle_reset_time) << ' ' << bits(consumed)
              << ',' << state.next_delay_index << ','
              << bits(state.idle_reset_remaining) << ',' << state.fire_count
              << '\n';
}

static void numeric_sweeps() {
#if defined(__i386__) && defined(__GNUC__)
    constexpr std::array<unsigned, 3> precisions{{0, 2, 3}};
#else
    constexpr std::array<unsigned, 1> precisions{{3}};
#endif
    for (const unsigned precision : precisions) {
        X87Precision control(precision);
        economics_rounding_rows(precision);
        for (int index = 0; index < 768; ++index)
            numeric_sweep_case(precision, index);
    }
}
int main() {
    squadron(); textures(); cycles(); identity_and_shields(); resources();
    numeric_sweeps();
}
