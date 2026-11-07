// Compile against the frozen or current complete module source. Existing
// fixtures provide the actual native module and required engine mocks.
#if defined(TEST_SHIELDS)
#define main stage8_fixture_main
#include "rust_stage8_a_shields_native.cpp"
#undef main
#elif defined(TEST_SWARM)
#define main stage8_fixture_main
#include "rust_stage8_a_swarm_native.cpp"
#undef main
#else
#error "Select TEST_SHIELDS or TEST_SWARM"
#endif

#include <climits>
#include <clocale>

int main() {
#if defined(TEST_SHIELDS)
    std::setlocale(LC_CTYPE, "C");
    require(lower_ascii(std::string("A\0Z\x80\xff", 5)) ==
                std::string("a\0z\x80\xff", 5), "lower full bytes");
    require(lower_ascii("").empty(), "lower empty");
    require(join_path("", "") == "", "join both empty");
    require(join_path("", "right") == "right", "join left empty");
    require(join_path("left", "") == "left", "join right empty");
    require(join_path("left/", "right") == "left/right", "join slash");
    require(join_path("left\\", "right") == "left\\right", "join backslash");
    require(join_path("left", "right") == "left\\right", "join separator");
    require(join_path(std::string("a\0b", 3), "c") ==
                std::string("a\0b\\c", 5), "join embedded nul");
    std::puts("visibility native target functions PASS");
#else
    const std::vector<Hardpoint> empty;
    require(hardpoint_at(empty, SIZE_MAX) == nullptr, "empty hardpoints");
    const std::vector<Hardpoint> hardpoints{{"zero", nullptr},
                                             {"one", nullptr},
                                             {"two", nullptr}};
    require(hardpoint_at(hardpoints, 0) == &hardpoints[0], "first hardpoint");
    require(hardpoint_at(hardpoints, 3) == &hardpoints[0], "wrap hardpoint");
    require(hardpoint_at(hardpoints, SIZE_MAX) ==
                &hardpoints[SIZE_MAX % hardpoints.size()], "full width index");
    std::puts("swarm native target function PASS");
#endif
    return 0;
}
