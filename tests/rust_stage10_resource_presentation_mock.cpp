#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>

int fixture_fail_new_after = -1;
void* operator new(std::size_t size) {
    if (fixture_fail_new_after == 0) {
        fixture_fail_new_after = -1;
        throw std::bad_alloc();
    }
    if (fixture_fail_new_after > 0) --fixture_fail_new_after;
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

#ifndef A2FO_STAGE10_RESOURCES_SOURCE
#error A2FO_STAGE10_RESOURCES_SOURCE is required
#endif
#include A2FO_STAGE10_RESOURCES_SOURCE

namespace {
std::int32_t fixture_local_index = 0;
std::int32_t fixture_team_for_team_index = -99;
std::int32_t fixture_team_for_team_calls = 0;
void* fixture_local_team = nullptr;
const char* fixture_translation = nullptr;
const char* fixture_expected_key = "KEY";
int fixture_lookup_calls = 0;

void check(bool ok, const char* label) {
    if (!ok) {
        std::fprintf(stderr, "FAIL:%s\n", label);
        std::exit(2);
    }
    std::printf("PASS:%s\n", label);
}

void install_stub(std::uintptr_t rva, const std::uint8_t* bytes,
                  std::size_t length) {
    DWORD previous = 0;
    void* destination = at(g_armada, rva);
    check(VirtualProtect(destination, length, PAGE_EXECUTE_READWRITE,
                         &previous) != 0, "stub-protect");
    std::memcpy(destination, bytes, length);
    FlushInstructionCache(GetCurrentProcess(), destination, length);
}

void put_address(std::uint8_t* bytes, std::size_t offset, const void* address) {
    const auto word = reinterpret_cast<std::uintptr_t>(address);
    static_assert(sizeof(word) == 4, "32-bit fixture only");
    std::memcpy(bytes + offset, &word, sizeof(word));
}

void expect_override(void* team, std::uint32_t resource, std::uint32_t kind,
                     bool expected, const char* expected_text,
                     const char* label) {
    std::string output = "sentinel";
    const bool actual = localized_presentation_override(
        team, resource, kind, &output);
    check(actual == expected && output == expected_text, label);
}
}

extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_0(void*, void*) {
    std::abort();
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_1(
    void* function, void*, std::uintptr_t key_address) {
    if (function != at(g_armada, kLocalizationLookupRva)) std::abort();
    const char* key = reinterpret_cast<const char*>(key_address);
    if (!key || std::strcmp(key, fixture_expected_key) != 0) std::abort();
    ++fixture_lookup_calls;
    return reinterpret_cast<std::uintptr_t>(fixture_translation);
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_3(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) {
    std::abort();
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_7(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) {
    std::abort();
}
extern "C" void a2fo_resources_deduct_hook() { std::abort(); }

namespace a2fo::resources {
bool parse_nonnegative(std::string_view, std::int32_t*) noexcept { std::abort(); }
bool can_afford(const Amounts&, const Costs&) noexcept { std::abort(); }
void debit(Amounts&, const Costs&) noexcept { std::abort(); }
void credit(Amounts&, const Costs&) noexcept { std::abort(); }
std::int64_t saturating_add(std::int64_t, std::int64_t) noexcept {
    std::abort();
}
}

int main() {
    void* memory = VirtualAlloc(nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
    check(memory != nullptr, "armada-allocation");
    g_armada = static_cast<HMODULE>(memory);
    std::uint8_t local_stub[] = {0xa1, 0, 0, 0, 0, 0xc3};
    put_address(local_stub, 1, &fixture_local_index);
    install_stub(kLocalTeamRva, local_stub, sizeof(local_stub));
    // increment call count; copy cdecl argument; remember it; return team.
    std::uint8_t team_stub[] = {
        0xff, 0x05, 0, 0, 0, 0, 0x8b, 0x44, 0x24, 0x04,
        0xa3, 0, 0, 0, 0, 0xa1, 0, 0, 0, 0, 0xc3};
    put_address(team_stub, 2, &fixture_team_for_team_calls);
    put_address(team_stub, 11, &fixture_team_for_team_index);
    put_address(team_stub, 16, &fixture_local_team);
    install_stub(kTeamForTeamRva, team_stub, sizeof(team_stub));

    std::array<std::uint8_t, 0x300> team{};
    fixture_local_team = team.data();
    for (std::int32_t index : {0, 15, -1, 16,
            std::numeric_limits<std::int32_t>::min(),
            std::numeric_limits<std::int32_t>::max()}) {
        fixture_local_index = index;
        fixture_team_for_team_calls = 0;
        fixture_team_for_team_index = -99;
        const bool valid = index >= 0 && index < 16;
        check(local_team_object() == (valid ? team.data() : nullptr) &&
              fixture_team_for_team_calls == (valid ? 1 : 0) &&
              fixture_team_for_team_index == (valid ? index : -99),
              (std::string("local-team-") + std::to_string(index)).c_str());
    }
    g_armada = nullptr;
    check(local_team_object() == nullptr, "local-team-no-module");
    g_armada = static_cast<HMODULE>(memory);

    InitializeCriticalSection(&g_lock);
    g_lock_ready = true;
    void* race = reinterpret_cast<void*>(0x1234);
    std::memcpy(team.data() + kTeamRaceOffset, &race, sizeof(race));
    g_race_presentations[race][6][A2FO_RESOURCE_PRESENTATION_RES] = "KEY";
    g_race_presentations[race][0][A2FO_RESOURCE_PRESENTATION_ICON] = "KEY";
    void* manager = reinterpret_cast<void*>(0x5678);
    std::memcpy(at(g_armada, kLocalizationManagerPointerRva), &manager,
                sizeof(manager));

    expect_override(nullptr, 6, 0, false, "sentinel", "missing-team");
    expect_override(team.data(), 10, 0, false, "sentinel", "invalid-resource");
    expect_override(team.data(), 6, 4, false, "sentinel", "invalid-kind");
    check(!localized_presentation_override(team.data(), 6, 0, nullptr),
          "null-output");
    void* empty_race = nullptr;
    std::memcpy(team.data() + kTeamRaceOffset, &empty_race, sizeof(empty_race));
    expect_override(team.data(), 6, 0, false, "sentinel", "missing-race");
    std::memcpy(team.data() + kTeamRaceOffset, &race, sizeof(race));
    g_lock_ready = false;
    expect_override(team.data(), 6, 0, false, "sentinel", "missing-lock");
    g_lock_ready = true;
    g_race_presentations[race][6][0].clear();
    expect_override(team.data(), 6, 0, false, "sentinel", "missing-key");
    g_race_presentations[race][6][0] = "KEY";
    std::memset(at(g_armada, kLocalizationManagerPointerRva), 0,
                sizeof(manager));
    expect_override(team.data(), 6, 0, true, "KEY", "literal-no-manager");
    std::memcpy(at(g_armada, kLocalizationManagerPointerRva), &manager,
                sizeof(manager));
    fixture_translation = nullptr;
    expect_override(team.data(), 6, 0, true, "KEY", "literal-null-translation");
    fixture_translation = "";
    expect_override(team.data(), 6, 0, true, "KEY", "literal-empty-translation");
    fixture_translation = "Translated";
    expect_override(team.data(), 6, 0, true, "Translated", "translated");
    check(fixture_lookup_calls == 3, "lookup-count");

    std::memset(at(g_armada, kLocalizationManagerPointerRva), 0,
                sizeof(manager));
    auto& stored_key = g_race_presentations[race][6][0];
    stored_key = "A long literal presentation key that requires allocation";
    expect_override(team.data(), 6, 0, true, stored_key.c_str(),
                    "long-literal");
    check(localized_presentation_override(team.data(), 6, 0, &stored_key) &&
          stored_key == "A long literal presentation key that requires allocation",
          "output-alias-stored-key");
    std::string failed_output = "sentinel";
    fixture_fail_new_after = 0;
    check(!localized_presentation_override(team.data(), 6, 0, &failed_output) &&
          failed_output == "sentinel", "key-copy-allocation-failure");
    failed_output = "sentinel";
    fixture_fail_new_after = 1;
    const bool failed_assignment = localized_presentation_override(
        team.data(), 6, 0, &failed_output);
    fixture_fail_new_after = -1;
    check(!failed_assignment && failed_output.empty(),
          "assignment-allocation-failure");
    std::memcpy(at(g_armada, kLocalizationManagerPointerRva), &manager,
                sizeof(manager));
    fixture_expected_key = stored_key.c_str();
    fixture_translation = "A translated presentation that requires allocation";
    failed_output = "sentinel";
    fixture_fail_new_after = 1;
    const bool failed_temporary = localized_presentation_override(
        team.data(), 6, 0, &failed_output);
    fixture_fail_new_after = -1;
    check(!failed_temporary && failed_output.empty(),
          "translation-temporary-allocation-failure");
    fixture_expected_key = "KEY";
    stored_key = "KEY";
    std::memcpy(at(g_armada, kLocalizationManagerPointerRva), &manager,
                sizeof(manager));
    fixture_translation = "Translated";

    check(std::strcmp(localize_presentation(nullptr, 6, 0), "Tritanium") == 0,
          "default-name");
    check(std::strcmp(localize_presentation(nullptr, 1, 3), "") == 0,
          "default-empty-icon");
    check(static_cast<unsigned char>(localize_presentation(nullptr, 6, 3)[0])
          == 0x87, "default-icon");
    check(std::strcmp(localize_presentation(team.data(), 6, 0),
                      "Translated") == 0, "localized-name");
    const char* previous = localize_presentation(team.data(), 6, 0);
    check(std::strcmp(previous, "Translated") == 0, "thread-local-first");
    fixture_translation = "Next";
    check(std::strcmp(localize_presentation(team.data(), 0, 3), "Next") == 0,
          "thread-local-reuse");
    check(std::strcmp(localize_presentation(team.data(), 10, 0), "") == 0,
          "localize-invalid-resource");
    check(std::strcmp(localize_presentation(team.data(), 0, 4), "") == 0,
          "localize-invalid-kind");
    g_runtime_alive = true;
    check(std::strcmp(A2FOResources_GetPresentationText(team.data(), 6, 0),
                      "Next") == 0, "public-api");
    g_runtime_alive = false;
    check(std::strcmp(A2FOResources_GetPresentationText(team.data(), 6, 0),
                      "") == 0, "public-api-inactive");
    DeleteCriticalSection(&g_lock);
    VirtualFree(memory, 0, MEM_RELEASE);
}
