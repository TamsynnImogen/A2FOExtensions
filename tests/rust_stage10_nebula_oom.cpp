// Focused allocation-failure observations for the complete native module source.
#define main nebula_existing_fixture_main
#include "rust_stage8_c_nebula_native.cpp"
#undef main
#include <new>
#include <malloc.h>

static std::string g_query_value;
static bool g_query_found = false;
static std::uintptr_t __attribute__((thiscall)) mock_query_string(
    void*, const char*, char* output, std::uintptr_t capacity, const char*) {
    if (!g_query_found || g_query_value.size() >= capacity) return 0;
    std::memcpy(output, g_query_value.data(), g_query_value.size());
    output[g_query_value.size()] = 0;
    return 1;
}

namespace {
int allocation_budget = -1;
unsigned failures_consumed = 0;
void* watched_buffer = nullptr;
std::size_t watched_size = 0;
int watched_free_uncaught = -1;
unsigned watched_frees = 0;

void arm(int allowed_allocations) noexcept {
    allocation_budget = allowed_allocations;
    failures_consumed = 0;
    watched_buffer = nullptr;
    watched_size = 0;
    watched_free_uncaught = -1;
    watched_frees = 0;
}
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void consumed(const char* message) {
    check(allocation_budget == -1 && failures_consumed == 1, message);
}
void show_case(const char* name, bool result, bool unchanged) {
    std::printf("%s:%u:%u:%u:%u:%d\n", name, result, unchanged,
                failures_consumed, watched_frees, watched_free_uncaught);
}
}

void* operator new(std::size_t size) {
    if (allocation_budget == 0) {
        allocation_budget = -1;
        ++failures_consumed;
        throw std::bad_alloc();
    }
    const bool watch = allocation_budget > 0;
    if (watch) --allocation_budget;
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer) throw std::bad_alloc();
    if (watch && !watched_buffer) {
        watched_buffer = pointer;
        watched_size = size;
    }
    return pointer;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept {
    if (pointer == watched_buffer) {
        ++watched_frees;
        watched_free_uncaught = std::uncaught_exceptions();
        watched_buffer = nullptr;
    }
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    try {
        // Moving the argument avoids a setup allocation before substr.
        std::string trim_input(100, 'x');
        trim_input.front() = trim_input.back() = ' ';
        arm(0);
        bool trim_threw = false;
        try { (void)trim(std::move(trim_input)); }
        catch (const std::bad_alloc&) { trim_threw = true; }
        consumed("trim did not consume injected allocation");
        check(trim_threw, "trim did not rethrow bad_alloc");
        show_case("trim-substr", trim_threw, true);

        auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr, 0x00400000, MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        check(image != nullptr, "query image allocation failed");
        g_armada = reinterpret_cast<HMODULE>(image);
        install_callable(image, kParameterDbGetStringRva,
                         reinterpret_cast<void*>(&mock_query_string));
        g_query_found = true;
        g_query_value.assign(100, 'q');
        std::string output = "sentinel";
        int parameter = 0;
        arm(0);
        bool ok = read_parameter_string(&parameter, "edge", &output);
        consumed("query initial copy did not consume allocation");
        check(!ok && output == "sentinel", "query copy changed output");
        show_case("query-copy", ok, output == "sentinel");

        g_query_value = " " + std::string(100, 'q') + " ";
        arm(1);
        ok = read_parameter_string(&parameter, "edge", &output);
        consumed("query substr did not consume second allocation");
        check(!ok && output == "sentinel", "query substr changed output");
        show_case("query-substr", ok, output == "sentinel");
        g_armada = nullptr;
        VirtualFree(image, 0, MEM_RELEASE);

        const std::string texture(130, 'T');
        output = "sentinel";
        arm(0);
        ok = bounded_texture_name(texture.c_str(), &output);
        consumed("bounded reserve did not consume allocation");
        check(!ok && output == "sentinel", "bounded reserve changed output");
        show_case("bounded-reserve", ok, output == "sentinel");

        arm(1);
        ok = bounded_texture_name(texture.c_str(), &output);
        consumed("bounded growth did not consume second allocation");
        check(!ok && output == "sentinel", "bounded growth changed output");
        check(watched_size == 129 && watched_frees == 1,
              "bounded first heap buffer not freed once");
        show_case("bounded-growth", ok, output == "sentinel");

        const std::string node_text(62, 'N');
        MockNode node{};
        node.name = node_text.c_str();
        arm(1);
        ok = normalized_node_name(&node, &output);
        consumed("node growth did not consume second allocation");
        check(!ok && output == "sentinel", "node growth changed output");
        check(watched_size == 31 && watched_frees == 1,
              "node first heap buffer not freed once");
        show_case("node-growth", ok, output == "sentinel");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "nebula oom fixture: %s\n", error.what());
        return 1;
    }
}
