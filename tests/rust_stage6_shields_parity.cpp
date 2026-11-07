#include "directional_shields.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>

using namespace a2fo::directional_shields;

static std::uint32_t bits(float x) {
    std::uint32_t n;
    std::memcpy(&n, &x, sizeof n);
    return n;
}
static float value(std::uint32_t n) {
    float x;
    std::memcpy(&x, &n, sizeof x);
    return x;
}
static std::uint32_t seed = 0x53484945u;
static std::uint32_t next_word() {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}
static void print_stores(const ShieldStores& stores) {
    for (float x : stores.current) std::cout << ',' << bits(x);
}
static void print_policy(const ShieldPolicy& policy) {
    std::cout << static_cast<std::uint32_t>(policy.mode);
    for (float x : policy.maximum) std::cout << ',' << bits(x);
}

#if defined(__i386__) && defined(__GNUC__)
static unsigned short control_word() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
static unsigned short stack_top() {
    unsigned short sw;
    __asm__ volatile("fnstsw %0" : "=a"(sw));
    return sw & 0x3800u;
}
struct Precision {
    unsigned short old;
    explicit Precision(unsigned n) : old(control_word()) {
        unsigned short next = (old & ~0x300u) | (n << 8);
        __asm__ volatile("fldcw %0" : : "m"(next));
    }
    ~Precision() { __asm__ volatile("fldcw %0" : : "m"(old)); }
};
#else
static unsigned short control_word() { return 0; }
static unsigned short stack_top() { return 0; }
struct Precision { explicit Precision(unsigned) {} };
#endif

static void trace(unsigned precision, unsigned case_id, const ShieldPolicy& policy,
                  const ShieldStores& initial, float native) {
    const auto normalized = normalize_policy(policy);
    ShieldStores initialized = initial, reconciled = initial;
    initialize_from_total(&initialized, policy, native);
    reconcile_total(&reconciled, policy, native);
    std::cout << "policy " << precision << ' ' << case_id << ' ';
    print_policy(policy);
    std::cout << ' ';
    print_policy(normalized);
    std::cout << ' ' << valid_policy(policy) << ',' << valid_policy(normalized)
              << ',' << bits(total_capacity(policy)) << ',' << bits(total_capacity(normalized))
              << ',' << bits(total_current(initial)) << ',' << bits(native);
    print_stores(initial);
    print_stores(initialized);
    print_stores(reconciled);
    for (unsigned f : {0u, 1u, 2u, 3u, 4u, 0xffffffffu}) {
        std::cout << ',' << bits(effect_aggregate_for_facing(policy, initial,
                                                             static_cast<Facing>(f)));
    }
    std::cout << '\n';
}

static void directions(unsigned precision, unsigned case_id, Mode mode,
                       const Matrix34& matrix, const float attacker[3]) {
    Facing world = static_cast<Facing>(0xfaceu);
    Facing local = world, legacy_world = world, legacy_local = world;
    const bool w = select_facing(mode, matrix, attacker, &world);
    const bool l = select_local_effect_facing(mode, matrix, &local);
    const bool lw = select_facing(matrix, attacker, &legacy_world);
    const bool ll = select_local_effect_facing(matrix, &legacy_local);
    std::cout << "direction " << precision << ' ' << case_id << ' '
              << static_cast<std::uint32_t>(mode);
    for (float x : matrix.values) std::cout << ',' << bits(x);
    for (unsigned i = 0; i < 3; ++i) std::cout << ',' << bits(attacker[i]);
    std::cout << ' ' << w << ',' << static_cast<std::uint32_t>(world)
              << ',' << l << ',' << static_cast<std::uint32_t>(local)
              << ',' << lw << ',' << static_cast<std::uint32_t>(legacy_world)
              << ',' << ll << ',' << static_cast<std::uint32_t>(legacy_local) << '\n';
}

int main() {
    const std::array<float, 22> special{{
        0.0f, -0.0f, 1.0f, -1.0f, 0.0001f,
        std::nextafter(0.0001f, 0.0f), std::nextafter(0.0001f, 1.0f),
        0.001f, 0.5f, 2.0f, 100.0f, 1000000.0f,
        std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
        value(0x7fc12345u), value(0xffc12345u), value(0x7f812345u), value(0xff812345u)
    }};
#if defined(__i386__) && defined(__GNUC__)
    const unsigned precisions[] = {0, 2, 3};
#else
    const unsigned precisions[] = {3};
#endif
    for (unsigned precision : precisions) {
        Precision mode(precision);
        for (unsigned f : {0u, 1u, 2u, 3u, 4u, 0xffffffffu}) {
            std::cout << "index " << precision << ' ' << f << ' '
                      << facing_index(static_cast<Facing>(f));
            for (unsigned m : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 0xffffffffu})
                std::cout << ',' << facing_active(static_cast<Mode>(m), static_cast<Facing>(f));
            std::cout << '\n';
        }
        for (unsigned i = 0; i < 600; ++i) {
            ShieldPolicy policy{};
            ShieldStores stores{};
            policy.mode = static_cast<Mode>(i < 8 ? i : next_word() % 9);
            for (unsigned n = 0; n < 4; ++n) {
                if (i < special.size()) {
                    policy.maximum[n] = special[(i + 3 * n) % special.size()];
                    stores.current[n] = special[(i + 5 * n + 7) % special.size()];
                } else if (i % 3 == 0) {
                    policy.maximum[n] = float(next_word() % 100000 + 1) / 100.0f;
                    stores.current[n] = float(next_word() % 120000) / 100.0f;
                } else {
                    policy.maximum[n] = value(next_word());
                    stores.current[n] = value(next_word());
                }
            }
            float native = i < special.size() ? special[special.size() - i - 1]
                         : i % 3 == 0 ? float(next_word() % 250000) / 100.0f
                                      : value(next_word());
            trace(precision, i, policy, stores, native);
            if (i < 32 || i % 7 == 0) {
                // The same store is taken through native recharge and damage.
                ShieldStores sequence = stores;
                for (float target : {native, 0.0f, 47.125f, 900.0f, -0.0f, native}) {
                    reconcile_total(&sequence, policy, target);
                    std::cout << "sequence " << precision << ' ' << i << ',' << bits(target);
                    print_stores(sequence);
                    std::cout << '\n';
                }
            }
        }
        Matrix34 identity{{1,0,0, 0,1,0, 0,0,1, 0,0,0}};
        unsigned case_id = 0;
        for (unsigned m : {0u,1u,2u,3u,4u,5u,6u,0xffffffffu}) {
            for (float x : special) {
                float attacker[3]{x, 0.0f, 1.0f};
                identity.values[9] = x;
                identity.values[11] = -0.0f;
                directions(precision, case_id++, static_cast<Mode>(m), identity, attacker);
            }
            // Exact seams and one-ULP neighbors in both signs.
            for (float x : {-2.0f, -1.0f, -0.0f, 0.0f, 1.0f, 2.0f})
                for (float z : {-2.0f, -1.0f, -0.0f, 0.0f, 1.0f, 2.0f}) {
                    Matrix34 transform{{1,0,0, 0,1,0, 0,0,1, x,7,z}};
                    float attacker[3]{x + x, 7, z + z};
                    directions(precision, case_id++, static_cast<Mode>(m), transform, attacker);
                }
        }
        for (unsigned i = 0; i < 500; ++i) {
            Matrix34 transform{};
            float attacker[3]{};
            for (float& x : transform.values)
                x = i % 4 ? float(int(next_word() % 20001) - 10000) / 97.0f
                            : value(next_word());
            for (float& x : attacker)
                x = i % 4 ? float(int(next_word() % 20001) - 10000) / 97.0f
                            : value(next_word());
            directions(precision, case_id++, static_cast<Mode>(i % 8), transform, attacker);
        }
        for (float remaining : special)
            for (int effect : {-1, 0, 1, 7, 0x7fffffff}) {
                std::cout << "effect " << precision << ' ' << bits(remaining) << ',' << effect
                          << ' ' << should_clear_native_collapse_effect(remaining, effect);
                for (int shield : {-1, 0, 1, 7, 0x7fffffff})
                    for (bool resolved : {false, true})
                        std::cout << ',' << should_suppress_native_impact_effect(
                            shield, resolved, remaining);
                std::cout << '\n';
            }
        Facing sentinel = static_cast<Facing>(0xfaceu);
        std::cout << "null " << precision << ' '
                  << select_facing(Mode::quad, identity, nullptr, &sentinel) << ','
                  << static_cast<std::uint32_t>(sentinel) << ','
                  << select_facing(Mode::quad, identity, identity.values, nullptr) << ','
                  << select_local_effect_facing(Mode::quad, identity, nullptr) << ','
                  << select_facing(identity, nullptr, &sentinel) << ','
                  << select_facing(identity, identity.values, nullptr) << ','
                  << select_local_effect_facing(identity, nullptr) << '\n';
        ShieldStores untouched{{1,2,3,4}};
        ShieldPolicy policy{{1,2,3,4}, Mode::quad};
        initialize_from_total(nullptr, policy, 2.0f);
        reconcile_total(nullptr, policy, 2.0f);
        std::cout << "null_stores " << precision;
        print_stores(untouched);
        std::cout << '\n';
        const auto cw = control_word(), top = stack_top();
        for (unsigned i = 0; i < 2000; ++i) {
            volatile float result = total_capacity(policy) + total_current(untouched)
                + effect_aggregate_for_facing(policy, untouched, Facing::forward);
            (void)result;
            reconcile_total(&untouched, policy, float(i % 11));
            initialize_from_total(&untouched, policy, float(i % 11));
            Facing facing = Facing::forward;
            float position[3]{float(i % 3), 0, float(i % 5)};
            select_facing(Mode::tri_forward, identity, position, &facing);
            select_local_effect_facing(Mode::tri_rear, identity, &facing);
            if (control_word() != cw || stack_top() != top) {
                std::cerr << "x87 control word or stack changed at " << precision << ':' << i << '\n';
                return 1;
            }
        }
        std::cout << "x87_preserved " << precision << ' ' << (control_word() == cw)
                  << ',' << (stack_top() == top) << '\n';
    }
}
