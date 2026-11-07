// Stage-ten Squadrons native fixture. Inherited modes exercise the complete
// original module source with its actual native policy adapters.
#define main stage8_squadrons_fixture_main
#include "rust_stage8_a_squadrons_native.cpp"
#undef main

namespace {
unsigned value_order = 0;
unsigned colour_order = 0;
bool reentry_ok = false;
void* __attribute__((fastcall)) ordered_values(void* bar, void* edx,
                                                void* output) {
    const auto handle = object_handle(read_at<void*>(bar, 0x428, nullptr));
    ++value_order;
    trace("value-order:" + std::to_string(value_order) + ":" +
          std::to_string(handle));
    if (value_order == 1) {
        std::array<std::uint8_t, 0x500> other_bar{};
        std::array<float, 2> other_output{{123.0f, 456.0f}};
        reentry_ok = squad_bar_values_hook(other_bar.data(), nullptr,
            other_output.data()) == other_output.data() &&
            other_output[0] == 123.0f && other_output[1] == 456.0f;
        trace("value-reentry:" + std::to_string(reentry_ok));
    }
    void* result = values_provider(bar, edx, output);
    if (value_order == 1) put(bar, 0x428, static_cast<void*>(yard.data()));
    return result;
}
void* __attribute__((fastcall)) ordered_colour(void* bar, void* edx,
                                                void* output) {
    const auto handle = object_handle(read_at<void*>(bar, 0x428, nullptr));
    ++colour_order;
    trace("colour-order:" + std::to_string(colour_order) + ":" +
          std::to_string(handle));
    void* result = colour_provider(bar, edx, output);
    if (colour_order == 1) put(bar, 0x428, static_cast<void*>(yard.data()));
    return result;
}
void ordered_bar_case() {
    seed_squad();
    std::array<std::uint8_t, 0x500> bar{};
    put(bar.data(), 0x428, static_cast<void*>(station.data()));
    std::array<void*, 11> vtable{};
    vtable[8] = reinterpret_cast<void*>(&ordered_colour);
    vtable[9] = reinterpret_cast<void*>(&ordered_values);
    SquadBarOverride binding;
    binding.bar = bar.data();
    binding.original_vtable = vtable.data();
    g_active_bars = &binding;
    g_active_bar_count = 1;
    std::array<float, 2> values{};
    void* result = squad_bar_values_hook(bar.data(), nullptr, values.data());
    check(result == values.data(), "bar values return pointer");
    check(read_at<void*>(bar.data(), 0x428, nullptr) == station.data(),
          "bar values craft restoration");
    std::array<float, 3> colour{};
    result = squad_bar_colour_hook(bar.data(), nullptr, colour.data());
    check(result == colour.data(), "bar colour return pointer");
    check(read_at<void*>(bar.data(), 0x428, nullptr) == station.data(),
          "bar colour craft restoration");
    check(value_order == 2 && colour_order == 2 && reentry_ok,
          "bar provider count/reentry");
    trace("bar:" + bits(values[0]) + ":" + bits(values[1]));
    trace("colour:" + bits(colour[0]) + ":" + bits(colour[1]) +
          ":" + bits(colour[2]));
    g_active_bars = nullptr;
    g_active_bar_count = 0;
}
unsigned nested_depth = 0, outer_value_calls = 0, nested_value_calls = 0;
bool nested_restored = false;
std::array<float, 2> nested_output{};
void* __attribute__((fastcall)) same_bar_values(void* bar, void* edx,
                                                 void* output) {
    const auto handle = object_handle(read_at<void*>(bar, 0x428, nullptr));
    if (nested_depth) {
        ++nested_value_calls;
        trace("samebar:nested:" + std::to_string(nested_value_calls) + ":" +
              std::to_string(handle));
        return values_provider(bar, edx, output);
    }
    ++outer_value_calls;
    trace("samebar:outer:" + std::to_string(outer_value_calls) + ":" +
          std::to_string(handle));
    if (outer_value_calls == 2) {
        nested_depth = 1;
        const auto result = squad_bar_values_hook(bar, nullptr,
                                                   nested_output.data());
        nested_depth = 0;
        nested_restored = result == nested_output.data() &&
            read_at<void*>(bar, 0x428, nullptr) == peer.data();
        trace("samebar:restore-peer:" + std::to_string(nested_restored));
    }
    return values_provider(bar, edx, output);
}
void same_bar_reentry_case() {
    setup_image();
    setup_api();
    seed_objects();
    seed_squad();
    std::array<std::uint8_t, 0x500> bar{};
    put(bar.data(), 0x428, static_cast<void*>(station.data()));
    std::array<void*, 11> vtable{};
    vtable[9] = reinterpret_cast<void*>(&same_bar_values);
    SquadBarOverride binding;
    binding.bar = bar.data();
    binding.original_vtable = vtable.data();
    g_active_bars = &binding;
    g_active_bar_count = 1;
    std::array<float, 2> output{};
    const auto result = squad_bar_values_hook(bar.data(), nullptr,
                                               output.data());
    check(result == output.data() &&
          read_at<void*>(bar.data(), 0x428, nullptr) == station.data() &&
          nested_restored && outer_value_calls == 2 && nested_value_calls == 2,
          "same-bar nested restoration/order");
    trace("samebar:result:" + bits(output[0]) + ":" + bits(output[1]) +
          ":nested=" + bits(nested_output[0]) + ":" +
          bits(nested_output[1]));
    g_active_bars = nullptr;
    g_active_bar_count = 0;
}
unsigned short x87_control() {
    unsigned short value = 0;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
}
unsigned short x87_status() {
    unsigned short value = 0;
    __asm__ __volatile__("fnstsw %0" : "=m"(value));
    return value;
}
using RawValues = std::array<std::uint32_t, 2>;
using RawColour = std::array<std::uint32_t, 3>;
struct BarSample {
    const char* name;
    std::array<RawValues, 2> values;
    std::array<RawColour, 2> colour;
    RawValues weights;
};
const BarSample* active_sample = nullptr;
std::array<std::uint32_t, 2> value_handles{}, colour_handles{};
unsigned matrix_value_calls = 0, matrix_colour_calls = 0;
void* __attribute__((fastcall)) matrix_values(void* bar, void*, void* output) {
    const auto handle = object_handle(read_at<void*>(bar, 0x428, nullptr));
    const unsigned index = handle == 11 ? 0u : 1u;
    if (matrix_value_calls < value_handles.size())
        value_handles[matrix_value_calls] = handle;
    ++matrix_value_calls;
    std::memcpy(output, active_sample->values[index].data(), 2 * sizeof(float));
    return output;
}
void* __attribute__((fastcall)) matrix_colour(void* bar, void*, void* output) {
    const auto handle = object_handle(read_at<void*>(bar, 0x428, nullptr));
    const unsigned index = handle == 11 ? 0u : 1u;
    if (matrix_colour_calls < colour_handles.size())
        colour_handles[matrix_colour_calls] = handle;
    ++matrix_colour_calls;
    std::memcpy(output, active_sample->colour[index].data(), 3 * sizeof(float));
    return output;
}
std::uint32_t raw(float value) {
    std::uint32_t word = 0;
    std::memcpy(&word, &value, sizeof(word));
    return word;
}
void bar_matrix_case(const std::string& requested, unsigned short control) {
    using V = RawValues;
    using C = RawColour;
    constexpr std::uint32_t z = 0, one = 0x3f800000u, two = 0x40000000u;
    constexpr std::uint32_t three = 0x40400000u, neg = 0xbf800000u;
    constexpr std::uint32_t qnan = 0x7fc00001u, snan = 0x7f800001u;
    constexpr std::uint32_t inf = 0x7f800000u, sub = 1u;
    constexpr std::uint32_t huge = 0x7f7fffffu;
    const C normal{one, two, three};
    const std::array<BarSample, 26> samples{{
        {"normal", {V{one,two},V{one,two}}, {normal,normal}, V{one,two}},
        {"negative", {V{neg,two},V{neg,two}}, {normal,normal}, V{one,one}},
        {"clamp", {V{three,two},V{three,two}}, {normal,normal}, V{one,one}},
        {"zero-max", {V{one,z},V{one,two}}, {normal,normal}, V{one,one}},
        {"qnan-value", {V{qnan,two},V{one,two}}, {normal,normal}, V{one,one}},
        {"snan-value", {V{snan,two},V{one,two}}, {normal,normal}, V{one,one}},
        {"inf-value", {V{inf,two},V{one,two}}, {normal,normal}, V{one,one}},
        {"inf-max", {V{one,inf},V{one,two}}, {normal,normal}, V{one,one}},
        {"neg-inf-max", {V{one,0xff800000u},V{one,two}}, {normal,normal}, V{one,one}},
        {"subnormal", {V{sub,sub},V{sub,sub}}, {normal,normal}, V{sub,sub}},
        {"overflow", {V{huge,huge},V{huge,huge}}, {normal,normal}, V{one,one}},
        {"qnan-red", {V{one,two},V{one,two}}, {C{qnan,two,three},normal}, V{one,one}},
        {"qnan-green", {V{one,two},V{one,two}}, {C{one,qnan,three},normal}, V{one,one}},
        {"qnan-blue", {V{one,two},V{one,two}}, {C{one,two,qnan},normal}, V{one,one}},
        {"snan-colour", {V{one,two},V{one,two}}, {C{snan,two,three},normal}, V{one,one}},
        {"snan-green", {V{one,two},V{one,two}}, {C{one,snan,three},normal}, V{one,one}},
        {"snan-blue", {V{one,two},V{one,two}}, {C{one,two,snan},normal}, V{one,one}},
        {"inf-red", {V{one,two},V{one,two}}, {C{inf,two,three},normal}, V{one,one}},
        {"inf-colour", {V{one,two},V{one,two}}, {C{one,inf,three},normal}, V{one,one}},
        {"inf-blue", {V{one,two},V{one,two}}, {C{one,two,inf},normal}, V{one,one}},
        {"zero-weight", {V{one,two},V{one,two}}, {normal,normal}, V{z,one}},
        {"negative-weight", {V{one,two},V{one,two}}, {normal,normal}, V{neg,one}},
        {"nan-weight", {V{one,two},V{one,two}}, {normal,normal}, V{qnan,one}},
        {"snan-weight", {V{one,two},V{one,two}}, {normal,normal}, V{snan,one}},
        {"inf-weight", {V{one,two},V{one,two}}, {normal,normal}, V{inf,one}},
        {"huge-weight", {V{one,two},V{one,two}}, {normal,normal}, V{huge,huge}},
    }};
    std::array<std::uint8_t, 0x500> bar{};
    std::array<void*, 11> vtable{};
    vtable[8] = reinterpret_cast<void*>(&matrix_colour);
    vtable[9] = reinterpret_cast<void*>(&matrix_values);
    SquadBarOverride binding;
    binding.bar = bar.data();
    binding.original_vtable = vtable.data();
    g_active_bars = &binding;
    g_active_bar_count = 1;
    for (std::size_t index = 0; index < samples.size(); ++index) {
        active_sample = &samples[index];
        value_handles = {};
        colour_handles = {};
        matrix_value_calls = matrix_colour_calls = 0;
        put(bar.data(), 0x428, static_cast<void*>(station.data()));
        put(station.data(), 0x1cc, samples[index].weights[0]);
        put(peer.data(), 0x1cc, samples[index].weights[1]);
        std::array<float, 2> values{};
        __asm__ __volatile__("fnclex");
        const auto value_result = squad_bar_values_hook(bar.data(), nullptr,
                                                         values.data());
        const unsigned short value_sw = x87_status();
        const unsigned short value_cw = x87_control();
        const V value_bits{raw(values[0]), raw(values[1])};
        const bool value_restored = read_at<void*>(bar.data(), 0x428, nullptr) ==
                                    station.data();
        std::array<float, 3> colour{};
        __asm__ __volatile__("fnclex");
        const auto colour_result = squad_bar_colour_hook(bar.data(), nullptr,
                                                          colour.data());
        const unsigned short colour_sw = x87_status();
        const unsigned short colour_cw = x87_control();
        const C colour_bits{raw(colour[0]), raw(colour[1]), raw(colour[2])};
        const bool colour_restored = read_at<void*>(bar.data(), 0x428, nullptr) ==
                                     station.data();
        check(value_result == values.data() && colour_result == colour.data() &&
              value_restored && colour_restored, "matrix return/restore");
        check(matrix_value_calls == 2 && matrix_colour_calls == 2 &&
              value_handles == V{11,12} && colour_handles == V{11,12},
              "matrix provider order");
        check(value_cw == control && colour_cw == control,
              "matrix changed x87 control");
        std::printf("matrix:%s:%02lu:%s:v=%08lx,%08lx:%04x:%04x:%04x:"
                    "c=%08lx,%08lx,%08lx:%04x:%04x:%04x\n",
                    requested.c_str(), static_cast<unsigned long>(index),
                    samples[index].name,
                    static_cast<unsigned long>(value_bits[0]),
                    static_cast<unsigned long>(value_bits[1]),
                    value_sw & 0x023fu, value_sw & 0x3800u, value_cw,
                    static_cast<unsigned long>(colour_bits[0]),
                    static_cast<unsigned long>(colour_bits[1]),
                    static_cast<unsigned long>(colour_bits[2]),
                    colour_sw & 0x023fu, colour_sw & 0x3800u, colour_cw);
    }
    g_active_bars = nullptr;
    g_active_bar_count = 0;
    active_sample = nullptr;
}
struct NearSample {
    const char* name;
    std::array<std::uint32_t, 3> craft;
    std::array<std::uint32_t, 3> yard;
    std::uint32_t radius;
    int expected;
};
void near_matrix_case(const std::string& requested, unsigned short control) {
    constexpr std::uint32_t z = 0, edge = 0x44000000u;
    constexpr std::uint32_t next = 0x44000001u;
    constexpr std::uint32_t qnan = 0x7fc00001u;
    constexpr std::uint32_t snan = 0x7f800001u;
    constexpr std::uint32_t inf = 0x7f800000u;
    const std::array<NearSample, 20> samples{{
        {"origin", {z,z,z}, {z,z,z}, z, 1},
        {"edge-x", {edge,z,z}, {z,z,z}, z, 1},
        {"outside-x", {next,z,z}, {z,z,z}, z, 0},
        {"edge-negative", {0xc4000000u,z,z}, {z,z,z}, z, 1},
        {"outside-negative", {0xc4000001u,z,z}, {z,z,z}, z, 0},
        {"edge-y", {z,edge,z}, {z,z,z}, z, 1},
        {"outside-y", {z,next,z}, {z,z,z}, z, 0},
        {"edge-z", {z,z,edge}, {z,z,z}, z, 1},
        {"outside-z", {z,z,next}, {z,z,z}, z, 0},
        {"diagonal-inside", {0x43960000u,0x43c80000u,z}, {z,z,z}, z, 1},
        {"diagonal-outside", {0x43c80000u,0x43c80000u,z}, {z,z,z}, z, 0},
        {"negative-radius", {edge,z,z}, {z,z,z}, 0xc2c80000u, 1},
        {"nan-radius", {edge,z,z}, {z,z,z}, qnan, 1},
        {"inf-radius", {edge,z,z}, {z,z,z}, inf, 1},
        {"snan-radius", {edge,z,z}, {z,z,z}, snan, 1},
        {"huge-radius", {next,z,z}, {z,z,z}, 0x7f7fffffu, 1},
        {"nan-coordinate", {qnan,z,z}, {z,z,z}, z, 0},
        {"snan-coordinate", {snan,z,z}, {z,z,z}, z, 0},
        {"inf-coordinate", {inf,z,z}, {z,z,z}, z, 0},
        {"sub-coordinate", {1u,z,z}, {z,z,z}, z, 1},
    }};
    const auto run_sample = [&](const NearSample& sample, std::size_t index) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            put(station_geometry.data(), 0x68 + axis * 4, sample.craft[axis]);
            put(yard_geometry.data(), 0x68 + axis * 4, sample.yard[axis]);
        }
        put(yard_geometry.data(), 0x40, sample.radius);
        __asm__ __volatile__("fnclex");
        const bool result = near_repair_yard(station.data(), yard.data());
        const unsigned short status = x87_status();
        const unsigned short observed_control = x87_control();
        check(observed_control == control, "near changed x87 control");
        if (sample.expected >= 0)
            check(result == (sample.expected != 0), "near expected boundary");
        std::printf("near:%s:%02lu:%s:%08lx,%08lx,%08lx:"
                    "%08lx,%08lx,%08lx:%08lx:%u:%04x:%04x:%04x\n",
                    requested.c_str(), static_cast<unsigned long>(index),
                    sample.name,
                    static_cast<unsigned long>(sample.craft[0]),
                    static_cast<unsigned long>(sample.craft[1]),
                    static_cast<unsigned long>(sample.craft[2]),
                    static_cast<unsigned long>(sample.yard[0]),
                    static_cast<unsigned long>(sample.yard[1]),
                    static_cast<unsigned long>(sample.yard[2]),
                    static_cast<unsigned long>(sample.radius), result,
                    status & 0x023fu, status & 0x3800u, observed_control);
    };
    for (std::size_t index = 0; index < samples.size(); ++index)
        run_sample(samples[index], index);
    std::uint32_t generated = 0x672c3d91u;
    for (std::size_t index = 0; index < 24; ++index) {
        std::array<std::uint32_t, 3> craft{}, yard{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            generated ^= generated << 13;
            generated ^= generated >> 17;
            generated ^= generated << 5;
            craft[axis] = raw(static_cast<float>(static_cast<int>(generated % 1601u) - 800));
            generated ^= generated << 13;
            generated ^= generated >> 17;
            generated ^= generated << 5;
            yard[axis] = raw(static_cast<float>(static_cast<int>(generated % 1601u) - 800));
        }
        const NearSample sample{"generated", craft, yard,
                                raw(static_cast<float>(generated % 129u)), -1};
        run_sample(sample, samples.size() + index);
    }
}
struct RetrySample {
    const char* name;
    std::uint32_t initial;
    std::uint32_t seconds;
    bool pending;
};
void retry_matrix_case(const std::string& requested, unsigned short control) {
    constexpr std::uint32_t quarter = 0x3e800000u;
    constexpr std::uint32_t eighth = 0x3e000000u;
    constexpr std::uint32_t inf = 0x7f800000u;
    constexpr std::uint32_t qnan = 0x7fc00001u;
    constexpr std::uint32_t sub = 1u;
    const std::array<RetrySample, 14> samples{{
        {"pending", quarter, eighth, true},
        {"exact", quarter, quarter, false},
        {"past", quarter, 0x3e800001u, false},
        {"sub-exact", sub, sub, false},
        {"sub-pending", 2u, sub, true},
        {"initial-nan", qnan, eighth, false},
        {"initial-inf", inf, eighth, true},
        {"seconds-nan", quarter, qnan, true},
        {"seconds-inf", quarter, inf, true},
        {"seconds-negative-inf", quarter, 0xff800000u, true},
        {"seconds-zero", quarter, 0u, true},
        {"seconds-negative", quarter, 0xbe000000u, true},
        {"initial-negative-zero", 0x80000000u, eighth, false},
        {"initial-zero", 0u, 0u, false},
    }};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        put(station_geometry.data(), 0x68 + axis * 4, std::uint32_t{0});
        put(yard_geometry.data(), 0x68 + axis * 4,
            axis == 0 ? 0x41000000u : std::uint32_t{0});
    }
    put(yard_geometry.data(), 0x40, 0x42800000u);
    g_repair_ready = true;
    g_push_repair_job = &push_repair;
    g_cancel_repair_job = &cancel_repair;
    g_yard_build_items[yard_class.data()].insert("squad");
    std::array<std::uint8_t, 0x20> queue{};
    put(queue.data(), 0, static_cast<void*>(at(kRepairQueueVtableRva)));
    put(yard.data(), 0x2ac, static_cast<void*>(queue.data()));
    std::array<std::uint8_t, 0x80> command{};
    put(station.data(), 0x44, static_cast<void*>(command.data()));
    put(command.data(), 0x3c, std::uint32_t{0});
    put(station.data(), kCraftQueueStageOffset, std::uint32_t{0});
    for (std::size_t index = 0; index < samples.size(); ++index) {
        g_repair_visits.clear();
        g_registry = a2fo::squadrons::Registry{};
        const auto id = seed_squad(true);
        RepairVisit visit;
        visit.yard = 21;
        visit.serviced.insert(11);
        std::memcpy(&visit.retry_seconds, &samples[index].initial,
                    sizeof(visit.retry_seconds));
        g_repair_visits.emplace(id, visit);
        a2fo::squadrons::RepairContext observed{};
        bool waiting = false;
        check(observe_repair(id, g_repair_visits.at(id), &observed, &waiting) &&
              !waiting && observed.serviced_members.size() == 1,
              "retry native observation");
        float seconds = 0.0f;
        std::memcpy(&seconds, &samples[index].seconds, sizeof(seconds));
        __asm__ __volatile__("fnclex");
        update_reinforcement(station.data(), A2FO_CRAFT_EVENT_SIMULATE_POST,
                             seconds);
        const unsigned short status = x87_status();
        const unsigned short observed_control = x87_control();
        const auto found = g_repair_visits.find(id);
        check(found != g_repair_visits.end(), "retry visit retained");
        const auto result = raw(found->second.retry_seconds);
        const auto queue_id = found->second.queue_id;
        check((queue_id == 0) == samples[index].pending,
              "retry pending/queued status");
        check(observed_control == control, "retry changed x87 control");
        std::printf("retry:%s:%02lu:%s:%08lx,%08lx:%08lx:%lu:%04x:%04x:%04x\n",
                    requested.c_str(), static_cast<unsigned long>(index),
                    samples[index].name,
                    static_cast<unsigned long>(samples[index].initial),
                    static_cast<unsigned long>(samples[index].seconds),
                    static_cast<unsigned long>(result),
                    static_cast<unsigned long>(queue_id),
                    status & 0x023fu, status & 0x3800u,
                    observed_control);
    }
    g_repair_visits.clear();
}
void numeric_case(const std::string& requested) {
    const auto divider = requested.find("-rc");
    check(divider != std::string::npos, "numeric rounding mode");
    const std::string pc = requested.substr(8, divider - 8);
    const std::string rc = requested.substr(divider + 3);
    check(pc == "pc24" || pc == "pc53" || pc == "pc64", "numeric precision");
    check(rc == "near" || rc == "down" || rc == "up" || rc == "chop",
          "numeric rounding");
    __asm__ __volatile__("fninit");
    unsigned short control = x87_control();
    control = static_cast<unsigned short>(control & ~0x0f00u);
    if (pc == "pc53") control |= 0x0200u;
    if (pc == "pc64") control |= 0x0300u;
    if (rc == "down") control |= 0x0400u;
    if (rc == "up") control |= 0x0800u;
    if (rc == "chop") control |= 0x0c00u;
    __asm__ __volatile__("fldcw %0" : : "m"(control));
    setup_image();
    setup_api();
    seed_objects();
    const auto before = x87_status();
    // Keep the requested CW: inherited exact econ-pc modes call a legacy
    // precision helper that only recognizes bar-pc53/bar-pc64.
    mode = "econ-numeric-" + pc;
    class_case();
    const auto after_economics = x87_status();
    check(x87_control() == control, "economics changed x87 control");
    check((before & 0x3800u) == (after_economics & 0x3800u),
          "economics changed x87 stack top");
    mode = "bar-" + pc;
    ordered_bar_case();
    const auto after_bar = x87_status();
    bar_matrix_case(requested, control);
    near_matrix_case(requested, control);
    retry_matrix_case(requested, control);
    check(x87_control() == control, "numeric mode changed x87 control");
    check((before & 0x3800u) == (after_bar & 0x3800u),
          "bar changed x87 stack top");
    std::printf("numeric:%s:cw=%04x:sw=%04x,%04x,%04x\n",
                requested.c_str(), control, before, after_economics, after_bar);
}
void costs_case() {
    setup_image();
    setup_api();
    seed_objects();
    put(peer_class.data(), kClassBuildTimeOffset, 1.0f);
    const std::array<std::pair<std::string, std::int32_t>, 9> samples{{
        {"0", 0}, {"-0", 0}, {"-000", 0}, {"42", 42},
        {"+42", 0}, {"42junk", 0}, {"2147483648", 0},
        {"-1", 0}, {std::string("4\0" "2", 3), 0},
    }};
    for (std::size_t index = 0; index < samples.size(); ++index) {
        OdfFields fields;
        fields.emplace_back("tritaniumCost", samples[index].first);
        capture_member_economics(peer_class.data(), fields);
        const auto found = g_member_economics.find(peer_class.data());
        check(found != g_member_economics.end(), "member economics captured");
        const auto value = found->second.costs[6];
        check(value == samples[index].second, "additional cost parser");
        trace("cost:" + std::to_string(index) + ":" +
              std::to_string(value));
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "fixture:mode argument\n");
        return 1;
    }
    std::printf("ready:%s\n", argv[1]);
    std::fflush(stdout);
    if (std::strncmp(argv[1], "numeric-pc", 10) == 0) {
        try {
            numeric_case(argv[1]);
            std::printf("completed:numeric:%s\n", argv[1]);
            std::fflush(stdout);
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "fixture:%s\n", error.what());
            return 1;
        }
    }
    if (std::strcmp(argv[1], "costs-parse") == 0) {
        try {
            costs_case();
            std::puts("completed:costs-parse");
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "fixture:%s\n", error.what());
            return 1;
        }
    }
    if (std::strcmp(argv[1], "bar-reentry") == 0) {
        try {
            same_bar_reentry_case();
            std::puts("completed:bar-reentry");
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "fixture:%s\n", error.what());
            return 1;
        }
    }
    const int result = stage8_squadrons_fixture_main(argc, argv);
    if (result == 0) {
        std::printf("completed:inherited:%s\n", argv[1]);
        std::fflush(stdout);
    }
    return result;
}
