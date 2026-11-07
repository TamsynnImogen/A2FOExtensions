// Link this same transcript to frozen stage-5 C++ or current Rust-backed C++.
#include "../modules/A2FOFireArcs/fire_arc.hpp"
#include "../modules/A2FOSwarmSystem/swarm_motion.hpp"
#include "../modules/A2FOTurrets/turret_math.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
using namespace a2fo;
static void f(float x) {
    std::uint32_t bits;
    std::memcpy(&bits, &x, 4);
    std::printf(" %08x", bits);
}
static void b(bool x) { std::printf(" %d", x ? 1 : 0); }
static void v(swarm::Vec3 x) {
    f(x.x);
    f(x.y);
    f(x.z);
}
static void a(turrets::AimAngles x) {
    f(x.yaw_degrees);
    f(x.pitch_degrees);
}
static void m(const float *x) {
    for (int i = 0; i < 12; ++i)
        f(x[i]);
}
static void tag(const char *s) { std::printf("%s", s); }
static void end() { std::putchar('\n'); }
static void run() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const swarm::Vec3 values[] = {
        {0, 0, 0}, {1, 2, 3}, {-4.25f, 0.5f, 8.f}, {nan, 1, 0}, {inf, 0, 0}};
    const swarm::Matrix34 sm{{1, 0, 0, 0, 2, 0, 0, 0, -3, 5, -7, 11}};
    const turrets::Matrix34 tm{{1, 0, 0, 0, 1, 0, 0, 0, 1, 5, -7, 11}};
    const fire_arcs::Matrix34 fm{{1, 0, 0, 0, 1, 0, 0, 0, 1, 5, -7, 11}};
    for (unsigned i = 0; i < 5; ++i) {
        auto x = values[i];
        tag("vec");
        std::printf(" %u", i);
        f(swarm::length_squared(x));
        f(swarm::length(x));
        v(swarm::normalized(x, {0, 1, 0}));
        v(swarm::add(x, {1, -2, 3}));
        v(swarm::subtract(x, {1, -2, 3}));
        v(swarm::multiply(x, -0.5f));
        v(swarm::project_outside_sphere(x, {1, 0, -2}, 2, {0, 1, 0}));
        v(swarm::transform_point(sm, x));
        v(swarm::transform_direction(sm, x));
        v(swarm::inverse_transform_point(sm, x));
        end();
    }
    for (std::uint32_t seed : {0u, 1u, 0x12345678u}) {
        swarm::Random r(seed);
        tag("random");
        std::printf(" %08x", seed);
        for (int i = 0; i < 4; ++i)
            std::printf(" %08x", r.next_u32());
        for (int i = 0; i < 4; ++i)
            f(r.unit());
        f(r.range(-5, 7));
        f(r.range(7, -5));
        v(swarm::random_shell_point(r, 0, 8));
        v(swarm::random_shell_point(r, 3, 5));
        std::printf(" %08x", r.next_u32());
        end();
    }
    for (unsigned i = 0; i < 4; ++i) {
        auto p = values[i], d = swarm::Vec3{0, 0, 1};
        tag("advance");
        std::printf(" %u", i);
        b(swarm::advance_agent(&p, &d, {10, 2, -3}, 3 + i, 0.5f + i, 0.12f + i * 0.04f,
                               0.1f));
        v(p);
        v(d);
        end();
    }
    {
        auto p = swarm::Vec3{0, 0, 0}, d = swarm::Vec3{1, 0, 0};
        tag("advance_null");
        b(swarm::advance_agent(nullptr, &d, p, 1, 1, 1, 1));
        v(d);
        end();
    }
    for (unsigned i = 0; i < 5; ++i) {
        auto p = values[i];
        tag("radius");
        std::printf(" %u", i);
        b(swarm::constrain_to_radius(&p, 2.5f));
        v(p);
        end();
    }
    for (unsigned i = 0; i < 5; ++i) {
        auto p = swarm::Vec3{(float)i - 2, 0, 0}, d = swarm::Vec3{-1, 0, 0};
        tag("sphere");
        std::printf(" %u", i);
        b(swarm::constrain_motion_outside_sphere({4, 0, 0}, &p, &d, {0, 0, 0}, 1.5f));
        v(p);
        v(d);
        end();
    }
    for (unsigned i = 0; i < 4; ++i) {
        auto lp = swarm::Vec3{(float)i, 0, 0}, rp = swarm::Vec3{0, 0, 0},
             ld = swarm::Vec3{1, 0, 0}, rd = swarm::Vec3{-1, 0, 0};
        tag("separate");
        std::printf(" %u", i);
        b(swarm::separate_pair(&lp, &ld, &rp, &rd, 2, {0, 1, 0}));
        v(lp);
        v(ld);
        v(rp);
        v(rd);
        end();
    }
    for (auto forward :
         {swarm::Vec3{0, 0, 1}, swarm::Vec3{0, 1, 0}, swarm::Vec3{1, 2, 3}}) {
        auto result = swarm::compose_facing_transform({2, -3, 4}, forward, {0, 1, 0});
        tag("facing");
        m(result.values);
        end();
    }
    for (auto value : {-721.f, -180.f, -0.f, 181.f, 359.f, nan}) {
        tag("turret_scalar");
        f(value);
        f(turrets::clamp_value(value, 2, -2));
        if (value == value)
            f(turrets::normalize_degrees(value));
        end();
    }
    const turrets::AimLimits limits[] = {{-180, 180, -30, 80, 90, 60},
                                         {-60, 45, -10, 35, 22, 13}};
    const float targets[][3] = {
        {5, -7, 11}, {5, -7, 15}, {9, -4, 14}, {-20, 12, -15}, {nan, 0, 0}};
    for (int l = 0; l < 2; ++l)
        for (int i = 0; i < 5; ++i) {
            tag("turret");
            std::printf(" %d %d", l, i);
            a(turrets::calculate_aim_angles(tm, targets[i], limits[l], {12, -7}));
            a(turrets::advance_aim_angles({175, -12}, {-170, 80}, limits[l], 0.25f));
            auto result =
                turrets::compose_turret_transform(tm, {(float)(i * 30 - 60), -15.f});
            m(result.values);
            end();
        }
    tag("turret_null");
    a(turrets::calculate_aim_angles(tm, nullptr, limits[0], {3, 4}));
    end();
    for (auto value : {-inf, -721.f, -180.f, -0.f, 181.f, 359.f, inf, nan}) {
        tag("arc_scalar");
        f(value);
        f(fire_arcs::normalize_degrees(value));
        end();
    }
    for (int mode = 0; mode < 2; ++mode)
        for (int i = 0; i < 6; ++i) {
            fire_arcs::ArcConfig c{};
            c.mode = mode ? fire_arcs::ArcMode::cone : fire_arcs::ArcMode::box;
            c.yaw_degrees = (i - 2) * 55.f;
            c.pitch_degrees = (i - 2) * 33.f;
            c.yaw_angle_degrees = i == 0 ? 360.f : i * 17.f;
            c.pitch_angle_degrees = i == 0 ? 180.f : i * 11.f;
            c.cone_angle_degrees = i == 0 ? 360.f : i * 29.f;
            if (i == 5) {
                c.yaw_degrees = nan;
                c.pitch_degrees = inf;
                c.cone_angle_degrees = -inf;
            }
            tag("arc");
            std::printf(" %d %d", mode, i);
            b(fire_arcs::defer_arc_to_candidate_selection(mode != 0, i % 2 == 0));
            fire_arcs::normalize_config(&c);
            std::printf(" %d", static_cast<int>(c.mode));
            f(c.yaw_degrees);
            f(c.pitch_degrees);
            f(c.yaw_angle_degrees);
            f(c.pitch_angle_degrees);
            f(c.cone_angle_degrees);
            for (auto &target : targets)
                b(fire_arcs::allows_target(c, fm, target));
            fire_arcs::ArcLine lines[97]{};
            const float origin[3]{1, 2, 3};
            for (std::size_t cap : {std::size_t{0}, std::size_t{4}, std::size_t{97}}) {
                auto count = fire_arcs::build_visualization_lines(c, fm, origin, 3.5f,
                                                                  lines, cap);
                std::printf(" %zu", count);
                for (std::size_t j = 0; j < count; ++j) {
                    for (float x : lines[j].start.values)
                        f(x);
                    for (float x : lines[j].end.values)
                        f(x);
                    std::printf(" %d", static_cast<int>(lines[j].style));
                }
            }
            end();
        }
    std::uint32_t state = 0x7182a931u;
    auto next = [&]() {
        state = state * 1664525u + 1013904223u;
        return state;
    };
    auto sample = [&]() {
        return static_cast<float>(static_cast<int>(next() % 40001u) - 20000) / 173.f;
    };
    for (int i = 0; i < 256; ++i) {
        const swarm::Vec3 x{sample(), sample(), sample()},
            y{sample(), sample(), sample()};
        const float scalar = sample(),
                    radius = static_cast<float>(next() % 1000u) / 57.f;
        swarm::Random random(next());
        tag("sweep_motion");
        std::printf(" %d", i);
        f(swarm::length_squared(x));
        f(swarm::length(x));
        v(swarm::normalized(x, y));
        v(swarm::add(x, y));
        v(swarm::subtract(x, y));
        v(swarm::multiply(x, scalar));
        v(swarm::project_outside_sphere(x, y, radius, {0, 1, 0}));
        v(swarm::transform_point(sm, x));
        v(swarm::transform_direction(sm, x));
        v(swarm::inverse_transform_point(sm, x));
        auto p = x, d = y;
        b(swarm::advance_agent(&p, &d, {2, -3, 4}, radius, scalar,
                               static_cast<float>(next() % 30u) / 100.f, 0.15f));
        v(p);
        v(d);
        p = x;
        b(swarm::constrain_to_radius(&p, radius));
        v(p);
        p = x;
        d = y;
        b(swarm::constrain_motion_outside_sphere({10, 0, 0}, &p, &d, {0, 0, 0},
                                                 radius));
        v(p);
        v(d);
        auto lp = x, ld = y, rp = swarm::Vec3{sample(), sample(), sample()},
             rd = swarm::Vec3{sample(), sample(), sample()};
        b(swarm::separate_pair(&lp, &ld, &rp, &rd, radius, {0, 1, 0}));
        v(lp);
        v(ld);
        v(rp);
        v(rd);
        const auto facing = swarm::compose_facing_transform(x, y, {0, 1, 0});
        m(facing.values);
        f(random.unit());
        f(random.range(-radius, radius));
        v(swarm::random_shell_point(random, 0, radius));
        std::printf(" %08x", random.next_u32());
        end();
        if (i >= 128)
            continue;
        const turrets::AimAngles current{sample(), sample()},
            desired{sample(), sample()};
        const turrets::AimLimits lim{-180, 180,    -80,
                                     80,   radius, static_cast<float>(next() % 100u)};
        const float target[3]{sample(), sample(), sample()};
        tag("sweep_turret");
        std::printf(" %d", i);
        f(turrets::clamp_value(scalar, -radius, radius));
        f(turrets::normalize_degrees(current.yaw_degrees));
        a(turrets::calculate_aim_angles(tm, target, lim, current));
        a(turrets::advance_aim_angles(current, desired, lim,
                                      static_cast<float>(next() % 100u) / 100.f));
        const auto composed = turrets::compose_turret_transform(tm, current);
        m(composed.values);
        end();
        fire_arcs::ArcConfig ac{};
        ac.mode = i % 2 ? fire_arcs::ArcMode::cone : fire_arcs::ArcMode::box;
        ac.yaw_degrees = sample();
        ac.pitch_degrees = sample();
        ac.yaw_angle_degrees = static_cast<float>(next() % 361u);
        ac.pitch_angle_degrees = static_cast<float>(next() % 181u);
        ac.cone_angle_degrees = static_cast<float>(next() % 361u);
        tag("sweep_arc");
        std::printf(" %d", i);
        fire_arcs::normalize_config(&ac);
        f(ac.yaw_degrees);
        f(ac.pitch_degrees);
        f(ac.yaw_angle_degrees);
        f(ac.pitch_angle_degrees);
        f(ac.cone_angle_degrees);
        b(fire_arcs::allows_target(ac, fm, target));
        if (i < 32) {
            fire_arcs::ArcLine lines[97]{};
            const float origin[3]{1, 2, 3};
            const auto count =
                fire_arcs::build_visualization_lines(ac, fm, origin, radius, lines, 97);
            std::printf(" %zu", count);
            for (std::size_t j = 0; j < count; ++j) {
                for (float z : lines[j].start.values)
                    f(z);
                for (float z : lines[j].end.values)
                    f(z);
                std::printf(" %d", static_cast<int>(lines[j].style));
            }
        }
        end();
    }
    fire_arcs::ArcConfig c{};
    tag("arc_null");
    b(fire_arcs::allows_target(c, fm, nullptr));
    std::printf(" %zu",
                fire_arcs::build_visualization_lines(c, fm, nullptr, 1, nullptr, 0));
    end();
}
int main() {
#if defined(_WIN32) && defined(__i386__)
    unsigned short saved;
    __asm__ __volatile__("fnstcw %0" : "=m"(saved));
    for (unsigned short precision :
         {(unsigned short)0x0000, (unsigned short)0x0200, (unsigned short)0x0300}) {
        unsigned short cw = (saved & ~0x0300) | precision;
        __asm__ __volatile__("fldcw %0" ::"m"(cw));
        std::printf("precision %04x\n", precision);
        run();
    }
    __asm__ __volatile__("fldcw %0" ::"m"(saved));
#else
    run();
#endif
}
