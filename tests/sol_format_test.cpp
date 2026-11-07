#include "modules/A2FOSOLMaps/sol_format.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: sol_format_test <map.sol>\n";
        return 2;
    }
    a2fo::sol::Map map;
    std::string error;
    if (!a2fo::sol::parse_file(argv[1], map, &error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "version=" << map.version << " title=" << map.title
              << " starts=" << map.start_locations.size()
              << " objects=" << map.objects.size()
              << " objectRecord=0x" << std::hex << map.object_count_record_offset
              << " missionRecord=0x" << map.runtime_mission_record_offset
              << std::dec << '\n';
    for (const auto& object : map.objects) {
        std::cout << object.odf << '\t' << object.instance_name << '\t'
                  << object.team << '\t' << object.position.x << ','
                  << object.position.y << ',' << object.position.z << '\n';
    }
    return 0;
}
