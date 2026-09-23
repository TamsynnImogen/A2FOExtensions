#include "../modules/A2FOStationRotation/rotation.hpp"
#include "../modules/A2FOStationRotation/footprint.hpp"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace a2fo::station_rotation;

int main() {
    for (unsigned turns = 0; turns != 4; ++turns) {
        float m[12] = {9,8,7,6,5,4,3,2,1, 123,-456,789};
        set_yaw(m, turns);
        assert(m[9] == 123 && m[10] == -456 && m[11] == 789);
        for (unsigned a = 0; a != 3; ++a) {
            for (unsigned b = 0; b != 3; ++b) {
                float dot = 0;
                for (unsigned i = 0; i != 3; ++i) dot += m[3*a+i]*m[3*b+i];
                assert(dot == (a == b ? 1.0f : 0.0f));
            }
        }
        const float determinant = m[0]*(m[4]*m[8]-m[5]*m[7]) -
            m[1]*(m[3]*m[8]-m[5]*m[6]) + m[2]*(m[3]*m[7]-m[4]*m[6]);
        assert(determinant == 1 && m[4] == 1);
        float copy[12]; std::memcpy(copy, m, sizeof(m));
        set_yaw(m, turns + 4);
        assert(std::memcmp(m, copy, sizeof(m)) == 0);
        assert(decode_wire_turns(static_cast<std::uint8_t>(encode_build(turns))) == turns);
        assert(parameter_turns(rotation_parameter(turns)) == turns);
    }
    for (unsigned command = 0; command != 256; ++command) {
        if (command < 0xb1 || command > 0xb3) assert(decode_wire_turns(command) == 0);
    }
    assert(parameter_turns(3) == 0);
    assert(parameter_turns(kRotationParameterTag | 0x81) == 0);
    assert(parameter_turns(0xffffffff) == 0);

    // Off-centre rectangular station, with different clearance on every side.
    // A simple width/depth swap would fail the half-turn and offset cases.
    const float box[6] = {-10,-3,-20, 30,7,50};
    const Margins margins{2,4,6,8};
    const Rectangle expected[4] = {
        {-14,-28,32,56}, {-28,-32,56,14},
        {-32,-56,14,28}, {-56,-14,28,32},
    };
    for (unsigned turns = 0; turns != 4; ++turns) {
        const auto r = rotate_rectangle(padded_footprint(box, margins), turns);
        assert(std::memcmp(&r, &expected[turns], sizeof(r)) == 0);
        float adjusted[6]; std::memcpy(adjusted, box, sizeof(box));
        planner_bounds(adjusted, margins, turns);
        const auto registered = padded_footprint(adjusted, margins);
        assert(std::memcmp(&registered, &expected[turns], sizeof(registered)) == 0);
        assert(adjusted[1] == box[1] && adjusted[4] == box[4]);
        float m[12]{}; set_yaw(m, turns);
        assert(cardinal_turns(m) == turns);
    }
    float non_cardinal[12]{}; set_yaw(non_cardinal, 1);
    non_cardinal[0] = 0.5f;
    assert(cardinal_turns(non_cardinal) == 0);
    non_cardinal[0] = NAN;
    assert(cardinal_turns(non_cardinal) == 0);

    PlacementControl c;
    assert(!c.key(0, true, false, false));
    assert(c.key(123, true, true, false) && c.turns == 1);
    assert(c.key(123, true, true, false) && c.turns == 1); // no repeat while held
    assert(c.key(123, true, true, true) && c.turns == 1);  // modifier change is not a press
    c.key(123, true, false, true);
    assert(c.key(123, true, true, true) && c.turns == 0);
    c.key(123, true, false, true);
    assert(c.key(123, true, true, true) && c.turns == 3); // reverse wrap
    c.key(123, true, false, false);
    assert(c.key(123, true, true, false) && c.turns == 0);
    assert(c.key(456, true, true, false) && c.turns == 0); // held across class switch
    assert(!c.key(0, true, true, false) && c.turns == 0);  // normal Repair restored
    assert(c.key(789, true, true, false) && c.turns == 0); // held on placement entry
    c.key(789, true, false, false);
    assert(!c.key(789, false, true, false) && c.turns == 0); // chat/background/modifiers
    assert(c.key(789, true, true, false) && c.turns == 0);   // returning needs a new press
    c.key(789, true, false, false);
    assert(c.key(789, true, true, false) && c.turns == 1);
    std::cout << "Station rotation policy tests passed\n";
}
