// Standalone i386/Windows call-ABI regression for the core and built-in
// FeaturePack wrappers. Link with rust_bridge_call_fixture.S and both bridges.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static_assert(sizeof(void *) == 4, "Call bridge fixture requires i386");

extern "C" {
std::uint32_t rust_bridge_call(void *, const std::uint32_t *, std::uint32_t);
extern std::uint32_t rust_bridge_before_sp, rust_bridge_after_sp;
extern std::uint32_t rust_bridge_after_ebx, rust_bridge_after_esi;
extern std::uint32_t rust_bridge_after_edi, rust_bridge_after_ebp;
extern std::uint32_t rust_bridge_expected_ebp;
std::uint32_t rust_bridge_seen[8] = {};

void rust_bridge_delphi1();
void rust_bridge_delphi2();
void rust_bridge_delphi3();
void rust_bridge_constructor();
void rust_bridge_bounds();
void rust_callback0();
void rust_callback1();
void rust_callback2();
void rust_callback3();
void rust_callback4();
void rust_callback7();

void a2fo_call_get_file_from_hash();
void a2fo_call_delphi_one_register();
void a2fo_call_delphi_two_registers();
void a2fo_call_delphi_constructor();
void a2fo_call_delphi_set_bounds();
void a2fo_call_evolver_dtor();
void a2fo_call_craft_explode();
void a2fo_call_game_object_method();
void a2fo_call_evolver_build_class();
void a2fo_call_game_object_argument();
void a2fo_call_thiscall_1();
void a2fo_call_thiscall_2();
void a2fo_call_game_object_construct();
void a2fo_parameter_db_get_string();
void a2fo_parameter_db_get_scalar();
void a2fo_load_sod();
void a2fo_builtin_feature_pack_call_thiscall_0();
void a2fo_builtin_feature_pack_call_thiscall_1();
void a2fo_builtin_feature_pack_call_thiscall_2();
void a2fo_builtin_feature_pack_call_thiscall_3();
void a2fo_builtin_feature_pack_call_thiscall_4();
void a2fo_builtin_feature_pack_call_thiscall_7();

// Required relocations for unused mid-function hooks in the two linked
// objects. Reaching one is a fixture error; none is called by this test.
void a2fo_dispatch_race_odf_loaded() { std::abort(); }
void a2fo_graphics_options_form_show_hook_cpp() { std::abort(); }
void a2fo_jvg_checkbox_set_checked_hook_cpp() { std::abort(); }
void a2fo_mod_settings_form_show_hook_cpp() { std::abort(); }
void a2fo_mod_settings_launch_hook_cpp() { std::abort(); }
void a2fo_module_menu_jet_button_click_hook_cpp() { std::abort(); }
void a2fo_renderer_combo_change_hook_cpp() { std::abort(); }
void a2fo_select_cocoon() { std::abort(); }
void *g_a2fo_race_parameter_db_destructor = nullptr;
void *a2fo_cocoon_resume = nullptr;
void *a2fo_cocoon_update_resume = nullptr;
void a2fo_buildyard_localization_lookup_handler() { std::abort(); }
void a2fo_buildyard_project_id_call_handler() { std::abort(); }
void a2fo_buildyard_tree_begin() { std::abort(); }
void a2fo_buildyard_tree_commit() { std::abort(); }
void a2fo_buildyard_tree_text_handler() { std::abort(); }
void *g_a2fo_buildyard_tree_dispatch = nullptr;
}

namespace {
constexpr std::uint32_t result_word = 0xaabbccddu;
class NativeOwner {
public:
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback0() asm("_rust_callback0");
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback1(std::uint32_t) asm("_rust_callback1");
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback2(std::uint32_t, std::uint32_t) asm("_rust_callback2");
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback3(std::uint32_t, std::uint32_t, std::uint32_t) asm("_rust_callback3");
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback4(std::uint32_t, std::uint32_t, std::uint32_t,
            std::uint32_t) asm("_rust_callback4");
  std::uint32_t __attribute__((thiscall, noinline, used))
  callback7(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
            std::uint32_t, std::uint32_t, std::uint32_t) asm("_rust_callback7");
};
NativeOwner self_storage;
unsigned cases = 0;
std::uint64_t digest = 14695981039346656037ull;

void require(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "case %u: %s\n", cases, message);
    std::exit(1);
  }
}
void hash_word(std::uint32_t value) {
  for (unsigned shift = 0; shift != 32; shift += 8) {
    digest ^= static_cast<std::uint8_t>(value >> shift);
    digest *= 1099511628211ull;
  }
}
template <typename Function> std::uint32_t address(Function function) {
  return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function));
}
std::uint32_t self() {
  return static_cast<std::uint32_t>(
      reinterpret_cast<std::uintptr_t>(&self_storage));
}
void record_self(void *object, std::initializer_list<std::uint32_t> arguments) {
  rust_bridge_seen[0] =
      static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
  unsigned index = 1;
  for (std::uint32_t value : arguments)
    rust_bridge_seen[index++] = value;
}

// Explicit assembly labels give the probe raw function addresses without
// interpreting a C++ pointer-to-member representation.
std::uint32_t NativeOwner::callback0() {
  record_self(this, {});
  return result_word;
}
std::uint32_t NativeOwner::callback1(std::uint32_t a) {
  record_self(this, {a});
  return result_word;
}
std::uint32_t NativeOwner::callback2(std::uint32_t a, std::uint32_t b) {
  record_self(this, {a, b});
  return result_word;
}
std::uint32_t NativeOwner::callback3(std::uint32_t a, std::uint32_t b,
                                     std::uint32_t c) {
  record_self(this, {a, b, c});
  return result_word;
}
std::uint32_t NativeOwner::callback4(std::uint32_t a, std::uint32_t b,
                                     std::uint32_t c, std::uint32_t d) {
  record_self(this, {a, b, c, d});
  return result_word;
}
std::uint32_t NativeOwner::callback7(std::uint32_t a, std::uint32_t b,
                                     std::uint32_t c, std::uint32_t d,
                                     std::uint32_t e, std::uint32_t f,
                                     std::uint32_t g) {
  record_self(this, {a, b, c, d, e, f, g});
  return result_word;
}

void check(void (*bridge)(), std::initializer_list<std::uint32_t> arguments,
           std::initializer_list<std::uint32_t> expected,
           std::uint32_t expected_result, bool has_self) {
  std::array<std::uint32_t, 9> words{};
  require(arguments.size() <= words.size(), "call argument capacity");
  unsigned index = 0;
  for (std::uint32_t value : arguments)
    words[index++] = value;
  for (auto &value : rust_bridge_seen)
    value = 0xdecafbad;
  const std::uint32_t actual =
      rust_bridge_call(reinterpret_cast<void *>(bridge), words.data(), index);
  ++cases;
  require(actual == expected_result, "return value");
  require(rust_bridge_before_sp == rust_bridge_after_sp, "cdecl stack balance");
  require(rust_bridge_after_ebx == 0x11223344u &&
              rust_bridge_after_esi == 0x55667788u &&
              rust_bridge_after_edi == 0x99aabbccu &&
              rust_bridge_after_ebp == rust_bridge_expected_ebp,
          "callee-saved registers");
  index = 0;
  for (std::uint32_t value : expected) {
    require(index < 8, "seen capacity");
    require(rust_bridge_seen[index] == value, "target argument order");
    hash_word(has_self && index == 0 ? 0x53454c46u : rust_bridge_seen[index]);
    ++index;
  }
  hash_word(expected_result);
  hash_word(index);
}

void delphi_cases() {
  constexpr std::uint32_t a = 0x10203040u, b = 0x50607080u;
  constexpr std::uint32_t c = 0x90abcdefu, d = 0x13572468u;
  check(a2fo_call_get_file_from_hash, {address(rust_bridge_delphi3), a, b, c},
        {a, b, c}, result_word, false);
  check(a2fo_call_delphi_one_register, {address(rust_bridge_delphi1), a}, {a},
        result_word, false);
  check(a2fo_call_delphi_two_registers, {address(rust_bridge_delphi2), a, b},
        {a, b}, result_word, false);
  check(a2fo_call_delphi_constructor, {address(rust_bridge_constructor), a, b},
        {a, 1, b}, result_word, false);
  // The existing SetBounds wrapper pushes width then height. The target
  // fixture records the resulting entry stack at seen[3] and seen[4].
  check(a2fo_call_delphi_set_bounds,
        {address(rust_bridge_bounds), a, b, c, d, 0x24681357u},
        {a, b, c, 0x24681357u, d}, result_word, false);
}

void thiscall_cases() {
  constexpr std::uint32_t a = 0x10203040u, b = 0x50607080u;
  constexpr std::uint32_t c = 0x90abcdefu, d = 0x13572468u;
  constexpr std::uint32_t e = 0x11224488u, f = 0x88774422u;
  constexpr std::uint32_t g = 0x02468aceu;
  const auto s = self();
  const auto c0 = address(rust_callback0), c1 = address(rust_callback1);
  const auto c2 = address(rust_callback2), c3 = address(rust_callback3);
  const auto c4 = address(rust_callback4), c7 = address(rust_callback7);
  for (auto bridge : {a2fo_call_evolver_dtor, a2fo_call_craft_explode,
                      a2fo_call_game_object_method,
                      a2fo_builtin_feature_pack_call_thiscall_0})
    check(bridge, {c0, s}, {s}, result_word, true);
  for (auto bridge :
       {a2fo_call_evolver_build_class, a2fo_call_game_object_argument,
        a2fo_call_thiscall_1, a2fo_builtin_feature_pack_call_thiscall_1})
    check(bridge, {c1, s, a}, {s, a}, result_word, true);
  for (auto bridge :
       {a2fo_call_thiscall_2, a2fo_builtin_feature_pack_call_thiscall_2})
    check(bridge, {c2, s, a, b}, {s, a, b}, result_word, true);
  check(a2fo_call_game_object_construct, {c4, s, a, b, c, d}, {s, a, b, c, d},
        result_word, true);
  check(a2fo_parameter_db_get_string, {c4, s, a, b, c, d}, {s, a, b, c, d},
        0xddu, true);
  check(a2fo_parameter_db_get_scalar, {c3, s, a, b, 0x3f800000u},
        {s, a, b, 0x3f800000u}, 0xddu, true);
  check(a2fo_load_sod, {c2, s, a}, {s, a, 0x3f800000u}, result_word, true);
  check(a2fo_builtin_feature_pack_call_thiscall_3, {c3, s, a, b, c},
        {s, a, b, c}, result_word, true);
  check(a2fo_builtin_feature_pack_call_thiscall_4, {c4, s, a, b, c, d},
        {s, a, b, c, d}, result_word, true);
  check(a2fo_builtin_feature_pack_call_thiscall_7, {c7, s, a, b, c, d, e, f, g},
        {s, a, b, c, d, e, f, g}, result_word, true);
}
} // namespace

int main() {
  delphi_cases();
  thiscall_cases();
  std::printf("calls cases=%u digest=%016llx\n", cases,
              static_cast<unsigned long long>(digest));
  return 0;
}
