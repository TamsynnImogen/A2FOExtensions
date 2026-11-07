// Direct stage-ten M3D policy checks, paired against frozen and current module sources.
#define main stage8_m3d_fixture_main
#include STAGE8_M3D_FIXTURE
#undef main

#include <array>
#include <atomic>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

std::atomic<int> fail_allocation_after{-1};
void* operator new(std::size_t size) {
    int remaining = fail_allocation_after.load();
    if (remaining >= 0) {
        if (remaining == 0) {
            fail_allocation_after.store(-1);
            throw std::bad_alloc();
        }
        fail_allocation_after.store(remaining - 1);
    }
    if (void* pointer = std::malloc(size)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace {
void name_case(const char* label, void* node, const std::string& expected) {
    const auto actual = bounded_node_name(node);
    check(actual == expected, label);
    line(std::string("bounded:") + label + ":" + std::to_string(actual.size()));
}
void candidate_case(const char* label, const std::string& authored,
                    const std::vector<std::string>& expected) {
    const auto actual = texture_lookup_candidates(authored);
    check(actual == expected, label);
    std::string row = std::string("candidate:") + label;
    for (const auto& item : actual) row += ":" + item;
    line(row);
}
void completion() {
    std::array<std::uint8_t, 0x20> node{};
    name_case("null-node", nullptr, "");
    name_case("null-name", node.data(), "");
    char empty[]{0};
    put_pointer(node.data(), 0x08, empty);
    name_case("empty", node.data(), "");
    std::array<char, 128> exact{};
    exact.fill('A');
    exact[127] = 0;
    put_pointer(node.data(), 0x08, exact.data());
    name_case("nul127", node.data(), std::string(127, 'A'));
    exact[127] = 'A';
    name_case("no-nul128", node.data(), "");
    char high[]{static_cast<char>(0x80), static_cast<char>(0xff), 0};
    put_pointer(node.data(), 0x08, high);
    name_case("high-byte", node.data(), std::string(high, 2));
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    auto* pages = static_cast<std::uint8_t*>(VirtualAlloc(nullptr,
        2 * system.dwPageSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(pages != nullptr, "guarded page allocation");
    DWORD old_protection = 0;
    check(VirtualProtect(pages + system.dwPageSize, system.dwPageSize,
                         PAGE_NOACCESS, &old_protection) != 0, "guarded page protection");
    pages[system.dwPageSize - 1] = 'Z';
    put_pointer(node.data(), 0x08, pages + system.dwPageSize - 1);
    name_case("unreadable-boundary", node.data(), "");
    check(VirtualFree(pages, 0, MEM_RELEASE) != 0, "guarded page release");
    candidate_case("empty", "", {});
    candidate_case("path", "Textures\\MiXeD", {"MiXeD", "MiXeD.dds", "MiXeD.tga"});
    candidate_case("ext", "Textures\\MiXeD.DDS", {"MiXeD.DDS"});
    candidate_case("case", "textures/UPPER.tga", {"UPPER.tga"});
    char short_name[]{'X', 0};
    put_pointer(node.data(), 0x08, short_name);
    fail_allocation_after.store(0);
    name_case("reserve-badalloc", node.data(), "");
    check(fail_allocation_after.load() == -1, "reserve allocation fault consumed");
    std::array<char, 81> long_name{};
    long_name.fill('X');
    long_name[80] = 0;
    put_pointer(node.data(), 0x08, long_name.data());
    fail_allocation_after.store(1);
    name_case("append-badalloc", node.data(), "");
    check(fail_allocation_after.load() == -1, "append allocation fault consumed");
    fail_allocation_after.store(0);
    bool propagated = false;
    try { (void)texture_lookup_candidates("A"); }
    catch (const std::bad_alloc&) { propagated = true; }
    fail_allocation_after.store(-1);
    check(propagated, "candidate emission allocation fault propagated");
    line("candidate:emission-badalloc");
    line("m3d-stage10-completion:PASS");
}
} // namespace

int main() {
    try { completion(); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "completion: %s\n", error.what());
        return 1;
    }
}
