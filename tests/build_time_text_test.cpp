#include "build_time_text.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

namespace {

void expect_token(float seconds, bool compact, const char* expected) {
    char output[64]{};
    assert(a2fo::build_tooltips::format_build_time_token(
        "/", seconds, compact, output, sizeof(output)));
    assert(std::strcmp(output, expected) == 0);
}

}  // namespace

int main() {
    using a2fo::build_tooltips::reconcile_global_build_time;
    assert(reconcile_global_build_time(100.0f, 100.0f, 0.5f) == 50.0f);
    assert(reconcile_global_build_time(100.0f, 50.0f, 0.5f) == 50.0f);
    assert(reconcile_global_build_time(100.0f, 200.0f, 2.0f) == 200.0f);
    assert(reconcile_global_build_time(100.0f, 75.0f, 0.5f) == 75.0f);
    assert(reconcile_global_build_time(
               100.0f, 100.0f,
               std::numeric_limits<float>::quiet_NaN()) == 100.0f);

    expect_token(0.0f, true, " /0 \x8a");
    expect_token(1.0f, true, " /1 \x8a");
    expect_token(1.4f, true, " /1 \x8a");
    expect_token(1.5f, true, " /2 \x8a");
    expect_token(67.0f, true, " /67 \x8a");
    expect_token(1.0f, false, " /1 second");
    expect_token(67.0f, false, " /67 seconds");

    char output[64]{};
    assert(!a2fo::build_tooltips::format_build_time_token(
        "/", -1.0f, true, output, sizeof(output)));
    assert(!a2fo::build_tooltips::format_build_time_token(
        "/", std::numeric_limits<float>::infinity(), true, output,
        sizeof(output)));
    assert(!a2fo::build_tooltips::format_build_time_token(
        "/", std::numeric_limits<float>::quiet_NaN(), true, output,
        sizeof(output)));
    assert(!a2fo::build_tooltips::format_build_time_token(
        "/", 12.0f, true, output, 6));
    assert(!a2fo::build_tooltips::format_build_time_token(
        "/", 12.0f, true, nullptr, 0));

    assert(a2fo::build_tooltips::format_resource_cost_token(
        "/", "Tritanium", 75, true, output, sizeof(output)));
    assert(std::strcmp(output, " /75 T") == 0);
    assert(a2fo::build_tooltips::format_resource_cost_token(
        "/", "Collective connections", 10, false, output,
        sizeof(output)));
    assert(std::strcmp(output, " /10 Collective connections") == 0);
    assert(!a2fo::build_tooltips::format_resource_cost_token(
        "/", "Supply", 0, false, output, sizeof(output)));
}
