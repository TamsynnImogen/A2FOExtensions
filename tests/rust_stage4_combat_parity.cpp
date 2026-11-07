#include "runtime_config.hpp"
#include "turret_combat.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace fa = a2fo::fire_arcs;
namespace tc = a2fo::turrets;

static void parser_case(const std::string& source) {
    for (int initial = 0; initial != 2; ++initial) {
        bool value = initial != 0;
        const auto status = fa::parse_firearc_setting(source, &value);
        std::cout << static_cast<int>(status) << ',' << value << '\n';
    }
    std::cout << static_cast<int>(fa::parse_firearc_setting(source, nullptr)) << '\n';
}

int main() {
    const std::vector<std::string> cases = {
        "", "firearc=0;", "firearc = 1;", "int firearc = 0;",
        "firearc=1;firearc=0;", "firearc=1;firearc=no;",
        "firearc=no;firearc=1;", "firearc=0;firearc=1x;",
        "firearc=0;//firearc=1\n", "/*firearc=1*/firearc=0;",
        "firearc=1/*x*/;", "firearc=1/*x\ny*/;",
        "firearc=1;/*", "//firearc=0\r firearc=1;",
        "x_firearc=1;", "firearc_extra=1;", "FIREARC=1;",
        "firearc=2;", "firearc=10;", "firearc=-1;",
        "firearc=1\0;", "firearc=\0;",
        "firearc=1\t\n\r\v\f;", "firearc=1\xA0;",
        "\xFF" "firearc=1;", "firearc\xFF=1;",
        "firearc=1//x\n;", "firearc=1/*x*/firearc=0;",
    };
    for (const auto& source : cases) parser_case(source);
    // Explicit lengths retain embedded NULs that C strings would truncate.
    parser_case(std::string("firearc=1\0;", 11));
    parser_case(std::string("firearc=\0;", 10));
    parser_case(std::string("firearc=0;\0firearc=1;", 21));
    std::uint32_t seed = 0x7f4a7c15u;
    const std::array<unsigned char, 23> alphabet = {
        'f','i','r','e','a','c','=','0','1','2',';','/','*',
        '\n','\r',' ','\t',0,0xa0,0xff,'x','_','+'};
    for (int n = 0; n < 2048; ++n) {
        std::string source;
        const int length = n % 79;
        for (int i = 0; i < length; ++i) {
            seed = seed * 1664525u + 1013904223u;
            source.push_back(static_cast<char>(alphabet[(seed >> 16) % alphabet.size()]));
        }
        parser_case(source);
    }
    constexpr std::array<std::uint32_t, 6> values = {
        0, 1, 2, 6, 0x80000000u, 0xffffffffu};
    for (auto command : values) for (auto form : values)
        for (auto handle : values) for (int exists = 0; exists != 2; ++exists)
            std::cout << tc::attack_order_target_handle(command, form, handle,
                                                        exists != 0) << '\n';
    for (auto previous : values) for (auto next : values)
        for (auto visual : values) {
            const auto state = tc::update_order_target(previous, next, visual);
            std::cout << state.ordered_target_handle << ','
                      << state.visual_target_handle << '\n';
        }
}
