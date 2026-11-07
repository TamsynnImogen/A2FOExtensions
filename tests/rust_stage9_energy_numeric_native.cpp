// Build-only extension of the existing full EnergySystems native fixture.
#define main energy_fixture_main
#include "rust_stage8_a_energy_native.cpp"
#undef main

#include <cfloat>
#include <limits>

namespace {
using RangeEntry = std::uintptr_t (*)(void*, std::uint32_t, std::uintptr_t) noexcept;
RangeEntry volatile range_entry = &energy_range_step;

std::uint16_t control_word() {
    std::uint16_t value;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
}

std::uint16_t status_word() {
    std::uint16_t value;
    __asm__ __volatile__("fnstsw %0" : "=am"(value));
    return value;
}

float from_bits(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

std::uint32_t next_bits(std::uint32_t& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

void one(const char* mode, unsigned case_number, CraftFixture& provider,
         CraftPolicy& policy, const std::array<float, 3>& receiver_position,
         const std::array<float, 3>& provider_position, float range,
         int known = -1) {
    provider.position(provider_position);
    policy.provider_range = range;
    EnergyRangeContext context{nullptr};
    context.provider = provider.get();
    context.provider_policy = &policy;
    context.position = receiver_position;
    const std::uint16_t before_cw = control_word();
    const std::uint16_t before_top = static_cast<std::uint16_t>((status_word() >> 11) & 7);
    __asm__ __volatile__("fnclex");
    const auto admitted = range_entry(&context, 7, 0);
    const std::uint16_t after_status = status_word();
    const std::uint16_t after_top = static_cast<std::uint16_t>((after_status >> 11) & 7);
    const std::uint16_t after_cw = control_word();
    assert(context.exception == nullptr);
    assert(before_cw == after_cw && before_top == after_top);
    assert(admitted <= 1);
    if (known >= 0) assert(admitted == static_cast<unsigned>(known));
    std::printf("%s %u %08x %08x %08x %08x %08x %08x %08x %u cw=%04x top=%u flags=%02x\n",
                mode, case_number,
                float_bits(receiver_position[0]), float_bits(receiver_position[1]),
                float_bits(receiver_position[2]), float_bits(provider_position[0]),
                float_bits(provider_position[1]), float_bits(provider_position[2]),
                float_bits(range), static_cast<unsigned>(admitted), after_cw, after_top,
                static_cast<unsigned>(after_status & 0x3f));
}

void cases(const char* mode) {
    int object_class = 0;
    CraftFixture provider(&object_class, 1, {{0, 0, 0}});
    CraftPolicy policy{};
    unsigned number = 0;
    const std::array<float, 3> zero{{0, 0, 0}};
    one(mode, number++, provider, policy, {{3, 4, 0}}, zero, 5, 1);
    one(mode, number++, provider, policy, {{3, 4, 1}}, zero, 5, 0);
    one(mode, number++, provider, policy, {{3, 4, 0}}, zero, -5, 1);
    one(mode, number++, provider, policy, zero, zero, 0, 1);
    one(mode, number++, provider, policy, {{1, 0, 0}}, zero, 0, 0);
    one(mode, number++, provider, policy,
        {{std::numeric_limits<float>::quiet_NaN(), 0, 0}}, zero, 1, 1);
    one(mode, number++, provider, policy,
        {{std::numeric_limits<float>::infinity(), 0, 0}}, zero,
        std::numeric_limits<float>::infinity(), 1);
    one(mode, number++, provider, policy,
        {{std::numeric_limits<float>::infinity(), 0, 0}}, zero, 1, 0);
    for (float value : {std::nextafter(5.0f, 0.0f), 5.0f,
                        std::nextafter(5.0f, 6.0f)}) {
        one(mode, number++, provider, policy, {{value, 0, 0}}, zero, 5);
        one(mode, number++, provider, policy, {{3, 4, 0}}, zero, value);
    }
    // A tiny provider offset can survive an extended x87 local difference
    // even when the nearby float radius rounds to the adjacent value.
    const float near_one = std::nextafter(1.0f, 0.0f);
    for (float small : {std::nextafter(0x1p-24f, 0.0f), 0x1p-24f,
                        std::nextafter(0x1p-24f, 1.0f),
                        std::nextafter(0x1p-23f, 0.0f), 0x1p-23f,
                        std::nextafter(0x1p-23f, 1.0f)}) {
        one(mode, number++, provider, policy, {{1, 0, 0}}, {{small, 0, 0}}, near_one);
        one(mode, number++, provider, policy, {{1, 0, 0}}, {{-small, 0, 0}}, near_one);
        one(mode, number++, provider, policy, {{-1, 0, 0}}, {{-small, 0, 0}}, near_one);
        one(mode, number++, provider, policy, {{1, small / 2, -small / 2}},
            {{small, 0, 0}}, near_one);
    }
    std::uint32_t state = 0x9e3779b9u;
    for (unsigned i = 0; i < 512; ++i) {
        float values[7];
        for (float& value : values) {
            const auto raw = next_bits(state);
            value = from_bits((raw & 0x80000000u) | (raw & 0x7f7fffffu));
        }
        one(mode, number++, provider, policy,
            {{values[0], values[1], values[2]}},
            {{values[3], values[4], values[5]}}, values[6]);
    }
    assert(number == 550);
}
} // namespace

int main() {
    const unsigned old = _controlfp(0, 0);
    struct Precision { const char* name; unsigned value; };
    const Precision precisions[] = {{"pc24", _PC_24}, {"pc53", _PC_53},
                                    {"pc64", _PC_64}};
    const Precision roundings[] = {{"near", _RC_NEAR}, {"down", _RC_DOWN},
                                   {"up", _RC_UP}, {"chop", _RC_CHOP}};
    for (const auto& precision : precisions) {
        for (const auto& rounding : roundings) {
            _controlfp(precision.value | rounding.value, _MCW_PC | _MCW_RC);
            const std::string label = std::string(precision.name) + "/" + rounding.name;
            cases(label.c_str());
        }
    }
    _controlfp(old, _MCW_PC | _MCW_RC);
}
