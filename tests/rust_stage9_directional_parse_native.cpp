// Supplemental actual-source parse fixture. The stage-eight fixture is read-only.
#define main stage8_directional_unused_main
#include "rust_stage8_a_directional_native.cpp"
#undef main
#include <cstdlib>
#include <new>

static bool fail_next_fixture_allocation = false;
__attribute__((noinline)) void* operator new(std::size_t size) {
    if (fail_next_fixture_allocation) {
        fail_next_fixture_allocation = false;
        throw std::bad_alloc{};
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
__attribute__((noinline)) void* operator new[](std::size_t size) {
    return ::operator new(size);
}
__attribute__((noinline)) void operator delete(void* memory) noexcept {
    std::free(memory);
}
__attribute__((noinline)) void operator delete(void* memory,
                                              std::size_t) noexcept {
    std::free(memory);
}
__attribute__((noinline)) void operator delete[](void* memory) noexcept {
    ::operator delete(memory);
}
__attribute__((noinline)) void operator delete[](void* memory,
                                                std::size_t) noexcept {
    ::operator delete(memory);
}

void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}

int main() {
    const char name[] = "forwardShieldStrength";
    const char valid[] = "1.25f";
    const char later[] = "2.5";
    const A2FO_OdfFieldView fields[]{
        {{nullptr, sizeof(name) - 1}, {later, sizeof(later) - 1}},
        {{name, sizeof(name) - 2}, {later, sizeof(later) - 1}},
        {{name, sizeof(name) - 1}, {nullptr, 1}},
        {{name, sizeof(name) - 1}, {valid, sizeof(valid) - 1}},
        {{name, sizeof(name) - 1}, {later, sizeof(later) - 1}},
    };
    std::string selected;
    require(!field_value(nullptr, 5, reinterpret_cast<const char*>(1), &selected),
            "null fields rejected before name strlen");
    require(!field_value(fields, 5, nullptr, &selected),
            "null name rejected before strlen");
    require(!field_value(fields, 5, name, nullptr),
            "null output rejected before field reads");
    require(field_value(fields, 5, name, &selected) && selected == valid,
            "first valid duplicate wins after invalid earlier fields");
    std::printf("field-order:first-valid-duplicate\n");

    std::string allocation_value(1024, ' ');
    allocation_value[0] = '1';
    const A2FO_OdfFieldView allocation_field{{name, sizeof(name) - 1},
        {allocation_value.data(),
         static_cast<std::uint32_t>(allocation_value.size())}};
    std::string allocation_target;
    bool saw_bad_alloc = false;
    fail_next_fixture_allocation = true;
    try {
        (void)field_value(&allocation_field, 1, name, &allocation_target);
    } catch (const std::bad_alloc&) {
        saw_bad_alloc = true;
    }
    require(saw_bad_alloc && !fail_next_fixture_allocation &&
            allocation_target.empty(),
            "field_value propagates native bad_alloc without modifying output");
    float allocation_output = 77.0f;
    fail_next_fixture_allocation = true;
    const bool allocation_parsed =
        parse_float(&allocation_field, 1, name, &allocation_output);
    require(!allocation_parsed && !fail_next_fixture_allocation &&
            allocation_output == 77.0f,
            "parse_float contains native assign exception and preserves output");
    std::printf("field-exception:typed-bad-alloc:noexcept-contained\n");

    const auto parse_text = [](const std::string& text, float* output) {
        const char key[] = "x";
        const A2FO_OdfFieldView field{{key, 1}, {text.data(), text.size()}};
        return parse_float(&field, 1, key, output);
    };
    float unchanged = 77.0f;
    require(!parse_float(fields, 5, reinterpret_cast<const char*>(1), nullptr),
            "null float output rejected before field/name reads");
    for (const std::string text : {"", "1.25ff", "1.25\v", "-1", "nan", "inf", "1e9999"}) {
        unchanged = 77.0f;
        require(!parse_text(text, &unchanged) && unchanged == 77.0f,
                "invalid float leaves output untouched");
    }
    for (const std::string& text : std::array<std::string, 3>{
             "1.25 \t f \r\n", "1.25F", std::string("1.5\0bad", 7)}) {
        float output = -1.0f;
        require(parse_text(text, &output) && output > 0.0f,
                "suffix or embedded NUL accepted");
    }
    const unsigned int original = _controlfp(0, 0);
    for (const auto precision : {_PC_24, _PC_53, _PC_64}) {
        _controlfp(precision, _MCW_PC);
        float negative_zero = 9.0f;
        float subnormal = 9.0f;
        require(parse_text("-0.0", &negative_zero) && std::signbit(negative_zero),
                "negative zero accepted with sign bit");
        require(parse_text("1e-40", &subnormal) &&
                std::fpclassify(subnormal) == FP_SUBNORMAL,
                "subnormal accepted");
        std::printf("float-precision:%u:negative-zero:%u:subnormal:%u\n",
                    precision, bits(negative_zero), bits(subnormal));
    }
    _controlfp(original, _MCW_PC);
    std::printf("float-suffix:ascii-embedded-nul-overflow-finite\n");
    return 0;
}
