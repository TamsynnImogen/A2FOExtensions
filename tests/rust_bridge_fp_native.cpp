// Standalone i386/Windows bridge regression. Link with refit_native_fixture.S
// and exactly one camera or view bridge implementation.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#if (defined(A2FO_BRIDGE_CAMERA) + defined(A2FO_BRIDGE_VIEW) +                 \
     defined(A2FO_BRIDGE_FP_ENV)) != 1
#error Select exactly one A2FO bridge family or FP environment test
#endif
static_assert(sizeof(void *) == 4, "The instruction fixture requires i386");
static_assert(sizeof(float) == 4, "The bridge requires binary32");

struct alignas(16) Capture {
  std::uint32_t registers[9];
  std::uint8_t padding[12];
  std::uint8_t before[512];
  std::uint8_t after[512];
};
static_assert(offsetof(Capture, before) == 48 &&
              offsetof(Capture, after) == 560);

extern "C" {
void refit_run_fixture();
void a2fo_refit_bridge();
void *refit_target = nullptr;
Capture *refit_capture = nullptr;
std::uint32_t refit_parsed_value = 0;
int refit_pan_fixture = 0;
std::uint16_t refit_control_word = 0x37f;
void *refit_saved_sp = nullptr;
void *refit_saved_fx = nullptr;
alignas(4) volatile std::uint32_t a2fo_refit_value_bits = 0;
alignas(4) volatile std::uint32_t a2fo_refit_parser_visits = 0;
std::uint32_t a2fo_refit_data_a = 0;
std::uint32_t a2fo_refit_data_b = 0;
#if defined(A2FO_BRIDGE_FP_ENV)
alignas(16) std::uint8_t rust_fp_saved_state[512]{};
alignas(16) std::uint8_t rust_fp_reset_state[512]{};
std::uint16_t rust_fp_requested_cw = 0x37f;
#endif
}

namespace {
std::uint32_t data_a = 0;
std::uint32_t data_b = 0;
std::uint64_t digest = 14695981039346656037ull;
unsigned cases = 0;

void require(bool condition, const char *what) {
  if (!condition) {
    std::fprintf(stderr, "case %u: %s\n", cases, what);
    std::exit(1);
  }
}
void hash_word(std::uint32_t word) {
  for (unsigned byte = 0; byte != 4; ++byte) {
    digest ^= static_cast<std::uint8_t>(word >> (byte * 8));
    digest *= 1099511628211ull;
  }
}
std::uint32_t word_at(const std::uint8_t *bytes, unsigned offset) {
  std::uint32_t word;
  std::memcpy(&word, bytes + offset, sizeof(word));
  return word;
}
void check_common(const Capture &out, bool camera, std::uint32_t ecx) {
  const auto &r = out.registers;
  require(r[0] == 0x778899aa && r[1] == 0x66778899 && r[2] == 0x55667788 &&
              r[4] == 0x44556677 && r[5] == 0x33445566 && r[7] == 0x11223344,
          "GPR preservation");
  require(r[6] == (camera ? 0x22334455u : ecx), "ECX result");
  require((r[8] & 0xcd5u) == (0x646u & 0xcd5u), "EFLAGS and DF preservation");
  // CALL starts at saved_fx; PUSHF immediately after RET precedes PUSHAD.
  require(r[3] == reinterpret_cast<std::uintptr_t>(refit_saved_fx) - 4,
          "bridge stack balance");
  require(std::memcmp(out.before, out.after, 2) == 0 &&
              std::memcmp(out.before + 24, out.after + 24, 8) == 0,
          "x87 control word and MXCSR preservation");
  if (camera) {
    const unsigned top = (out.before[3] >> 3) & 7;
    require(((out.after[3] >> 3) & 7) == ((top + 1) & 7),
            "camera pops one x87 entry");
    require(out.after[4] == (out.before[4] & ~(1u << top)),
            "camera x87 tag stack");
    require(std::memcmp(out.after + 32, out.before + 48, 10) == 0,
            "camera retains 1.0 guard");
  } else {
    require(std::memcmp(out.before, out.after, 416) == 0,
            "non-arithmetic bridge preserves architectural FP state");
  }
}
Capture run(bool camera, std::uint32_t ecx) {
  Capture out{};
  refit_capture = &out;
  const std::uint32_t prior = a2fo_refit_parser_visits;
  refit_run_fixture();
  ++cases;
  require(a2fo_refit_parser_visits == prior + 1u,
          "exactly one parser visit, including wraparound");
  check_common(out, camera, ecx);
  hash_word(out.registers[8] & 0xcd5u);
  hash_word(word_at(out.after, 0)); // CW, status, and tag; no FP addresses.
  hash_word(out.after[4]);
  hash_word(word_at(out.after, 24)); // MXCSR
  return out;
}

#if defined(A2FO_BRIDGE_CAMERA)
void camera_cases() {
  refit_pan_fixture = 1;
  constexpr std::array<std::uint32_t, 18> values{
      {0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3ba3d70au, 0xc0200000u,
       0x00800000u, 0x80800000u, 0x00000001u, 0x80000001u, 0x007fffffu,
       0x807fffffu, 0x7f7fffffu, 0xff7fffffu, 0x7f800000u, 0xff800000u,
       0x7fc12345u, 0x7f812345u}};
  for (const std::uint16_t precision : {0u, 0x200u, 0x300u}) {
    for (const std::uint16_t rounding : {0u, 0x400u, 0x800u, 0xc00u}) {
      refit_control_word =
          static_cast<std::uint16_t>(0x7f | precision | rounding);
      for (const std::uint32_t factor : {0x3f800000u, 0x40200000u}) {
        a2fo_refit_value_bits = factor;
        for (const std::uint32_t parsed : values) {
          refit_parsed_value = parsed;
          data_a = 0xdeadbeefu;
          run(true, 0);
          if (factor == 0x3f800000u && (parsed & 0x7f800000u) != 0x7f800000u)
            require(data_a == parsed, "factor one preserves finite bits");
          if (factor == 0x40200000u && parsed == 0x3ba3d70au)
            require(data_a >= 0x3c4ccc00u && data_a <= 0x3c4ccd00u,
                    "finite camera scaling");
          hash_word(data_a);
          hash_word(refit_control_word);
          hash_word(factor);
          hash_word(parsed);
        }
      }
    }
  }
}
#elif defined(A2FO_BRIDGE_VIEW)
std::uint32_t raised(std::uint32_t current, std::uint32_t threshold) {
  return threshold && current < 0x7f800000u && current < threshold ? threshold
                                                                   : current;
}
void view_cases() {
  refit_pan_fixture = 0;
  constexpr std::array<std::uint32_t, 12> values{
      {0, 0x80000000u, 0x00000001u, 0x00800000u, 0x3f800000u, 0x452f0000u,
       0x461c4000u, 0x7f7fffffu, 0x7f800000u, 0xff800000u, 0x7fc12345u,
       0x7f812345u}};
  for (const std::uint16_t precision : {0u, 0x200u, 0x300u}) {
    for (const std::uint16_t rounding : {0u, 0x400u, 0x800u, 0xc00u}) {
      refit_control_word =
          static_cast<std::uint16_t>(0x7f | precision | rounding);
      for (const std::uint32_t threshold : {0u, 0x461c4000u}) {
        a2fo_refit_value_bits = threshold;
        for (const std::uint32_t first : values) {
          for (const std::uint32_t second : values) {
            data_a = first;
            data_b = second;
            const std::uint32_t expected_a = raised(first, threshold);
            const std::uint32_t expected_b = raised(second, threshold);
            const Capture out = run(false, expected_b);
            require(data_a == expected_a && data_b == expected_b,
                    "ordered view slot updates");
            hash_word(data_a);
            hash_word(data_b);
            hash_word(out.registers[6]);
            hash_word(threshold);
          }
        }
      }
    }
  }
}
#else
void environment_cases() {
  // Two live x87 values, including difficult loads. The bridge must retain
  // the original state despite installing a different empty environment.
  refit_pan_fixture = 1;
  for (const std::uint16_t original_pc : {0u, 0x200u, 0x300u})
    for (const std::uint16_t original_rc : {0u, 0x400u, 0x800u, 0xc00u})
      for (const std::uint16_t pc : {0u, 0x200u, 0x300u})
        for (const std::uint16_t rc : {0u, 0x400u, 0x800u, 0xc00u})
          for (const std::uint32_t value :
               {0x3ba3d70au, 1u, 0x7f812345u, 0x7f800000u}) {
            refit_control_word =
                static_cast<std::uint16_t>(0x7f | original_pc | original_rc);
            rust_fp_requested_cw = static_cast<std::uint16_t>(0x7f | pc | rc);
            refit_parsed_value = value;
            const auto capture = run(false, 0x22334455u);
            std::uint16_t actual_cw = 0;
            std::memcpy(&actual_cw, rust_fp_reset_state, 2);
            require(actual_cw == rust_fp_requested_cw,
                    "requested x87 control word");
            require(rust_fp_reset_state[4] == 0 &&
                        ((rust_fp_reset_state[3] >> 3) & 7) == 0,
                    "reset x87 environment is empty");
            require(std::memcmp(rust_fp_reset_state + 24, capture.before + 24,
                                8) == 0 &&
                        std::memcmp(rust_fp_reset_state + 160,
                                    capture.before + 160, 128) == 0,
                    "environment reset retains MXCSR and SSE registers");
            hash_word(actual_cw);
            hash_word(refit_control_word);
            hash_word(value);
          }
}
#endif
} // namespace

int main() {
  refit_target = reinterpret_cast<void *>(&a2fo_refit_bridge);
  a2fo_refit_data_a = reinterpret_cast<std::uintptr_t>(&data_a);
  a2fo_refit_data_b = reinterpret_cast<std::uintptr_t>(&data_b);
  a2fo_refit_parser_visits = 0xffffffffu;
#if defined(A2FO_BRIDGE_CAMERA)
  camera_cases();
  const char *family = "camera";
#elif defined(A2FO_BRIDGE_VIEW)
  view_cases();
  const char *family = "view";
#else
  environment_cases();
  const char *family = "environment";
#endif
  require(a2fo_refit_parser_visits ==
              static_cast<std::uint32_t>(0xffffffffu + cases),
          "final parser visit count");
  std::printf("%s cases=%u digest=%016llx\n", family, cases,
              static_cast<unsigned long long>(digest));
  return 0;
}
