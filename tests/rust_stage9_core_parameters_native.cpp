// Actual frozen/current core source, synthetic engine ParameterDB bridge only.
#include <windows.h>
#include <cstdio>
static BOOL WINAPI fixture_write(HANDLE, const void*, DWORD, DWORD*, OVERLAPPED*);
#define WriteFile fixture_write
#include CORE_SOURCE
#undef WriteFile
#include <cstdlib>
#include <new>
#include <exception>

static int budget = -1;
__attribute__((noinline)) void* operator new(std::size_t size) {
    if (budget == 0) { budget = -1; std::puts("allocation:fired"); throw std::bad_alloc{}; }
    if (budget > 0) --budget;
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc{};
}
__attribute__((noinline)) void operator delete(void* value) noexcept { std::free(value); }
__attribute__((noinline)) void operator delete(void* value, std::size_t) noexcept { std::free(value); }
__attribute__((noinline)) void* operator new[](std::size_t size) { return ::operator new(size); }
__attribute__((noinline)) void operator delete[](void* value) noexcept { ::operator delete(value); }
__attribute__((noinline)) void operator delete[](void* value, std::size_t) noexcept { ::operator delete(value); }

static unsigned calls, kind, scenario;
static int live;
static bool log_thrown;
static const void* retained;
static char caller_text[64] = "caller";
static const char* caller_items[] = {"caller-one", "caller-two"};
static ParameterDbStringVectorAbi caller_vector;
struct Failure final : std::exception {
    unsigned code;
    explicit Failure(unsigned value) : code(value) { ++live; }
    Failure(const Failure& value) noexcept : code(value.code) { ++live; }
    ~Failure() noexcept override { --live; std::printf("destroy:%u:live=%d\n", code, live); }
};
static void dump_vector(const void* value) {
    const auto& vector = *static_cast<const ParameterDbStringVectorAbi*>(value);
    const auto count = vector.end - vector.begin;
    if (count < 0 || count > 2 || vector.capacity != vector.end || vector.allocator != 0)
        std::abort();
    std::printf("vector:%u:%ld", vector.allocator, static_cast<long>(count));
    for (auto item = vector.begin; item != vector.end; ++item) std::printf(":%s", *item);
    std::puts("");
}
static BOOL WINAPI fixture_write(HANDLE, const void* bytes, DWORD count,
                                 DWORD* written, OVERLAPPED*) {
    std::printf("log:live=%d:", live);
    std::fwrite(bytes, 1, count, stdout);
    const auto* text = static_cast<const char*>(bytes);
    const bool policy = std::strstr(text, "Classlabel ODF default applied:") != nullptr;
    if (retained && policy) {
        if (kind == 5) dump_vector(retained);
        else std::printf("retained:%s\n", static_cast<const char*>(retained));
    }
    if ((scenario == 4 && kind < 4 && policy && !log_thrown) ||
        (scenario == 12 && policy && !log_thrown) ||
        (scenario == 8 && !policy && !log_thrown)) {
        log_thrown = true;
        throw Failure{4};
    }
    if (written) *written = count;
    return TRUE;
}
static void change_gateway(void* value) {
    switch (kind) {
    case 0: g_parameter_db_get_int_hook.gateway = value; break;
    case 1: g_parameter_db_get_int_alternative_hook.gateway = value; break;
    case 2: g_parameter_db_get_float_hook.gateway = value; break;
    case 3: g_parameter_db_get_bool_hook.gateway = value; break;
    case 4: g_parameter_db_get_owned_string_hook.gateway = value; break;
    default: g_parameter_db_get_string_vector_hook.gateway = value; break;
    }
}
extern "C" bool a2fo_parameter_db_get_scalar(
    void* gateway, void*, const char* key, void* output, std::uint32_t bits) {
    ++calls;
    std::printf("call:%u:g=%lu:key=%s:out=%u:live=%d:", calls,
        static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(gateway)),
        key ? key : "<null>", output != nullptr, live);
    if (kind < 4) std::printf("bits=%08x\n", bits);
    else if (!bits) std::puts("default:null");
    else if (kind == 4) std::printf("default:%s\n", reinterpret_cast<const char*>(bits));
    else dump_vector(reinterpret_cast<const void*>(bits));
    if (scenario == 3 && calls == 1) throw Failure{1};
    if (kind >= 4 && (scenario == 4 || scenario == 8 || scenario == 13) && calls == 2) {
        retained = nullptr;
        throw Failure{2};
    }
    if (kind >= 4 && scenario == 5 && calls == 2) throw Failure{3};
    if (kind >= 4 && scenario == 13 && calls == 3) throw Failure{3};
    if (scenario == 6 && calls == 1) {
        change_gateway(reinterpret_cast<void*>(0x303));
        std::strcpy(caller_text, "caller-mutated");
        g_classlabel_odf_defaults["hybridbuild"].defaults["key"] = "77";
    }
    if (scenario == 7 && calls == 1) budget = 0;
    if (output) {
        if (kind == 4) *static_cast<char**>(output) = const_cast<char*>("engine-owned");
        else if (kind == 5) *static_cast<unsigned*>(output) = calls;
        else if (kind == 2) std::memcpy(output, &bits, sizeof(bits));
        else if (kind == 3) *static_cast<std::uint8_t*>(output) = 9;
        else *static_cast<int*>(output) = -19;
    }
    if (kind >= 4 && calls == 2 && scenario != 1 && scenario != 5 && scenario != 10)
        retained = reinterpret_cast<const void*>(bits);
    return scenario == 0 || (kind >= 4 && scenario == 1 && calls == 2);
}
static bool invoke(void* self, const char* key, void* output) {
    switch (kind) {
    case 0: return parameter_db_get_int_hook(self, nullptr, key, static_cast<int*>(output), -123);
    case 1: return parameter_db_get_int_alternative_hook(self, nullptr, key, static_cast<int*>(output), 43);
    case 2: {
        std::uint32_t bits = scenario == 9 ? 0x7fc12345u : 0x80000000u;
        float value; std::memcpy(&value, &bits, sizeof(value));
        return parameter_db_get_float_hook(self, nullptr, key, static_cast<float*>(output), value);
    }
    case 3: return parameter_db_get_bool_hook(self, nullptr, key, static_cast<std::uint8_t*>(output), 0x12345678);
    case 4: return parameter_db_get_owned_string_hook(self, nullptr, key, static_cast<char**>(output), caller_text);
    default: return parameter_db_get_string_vector_hook(self, nullptr, key, output, &caller_vector);
    }
}
static DWORD WINAPI check_lock(void*) {
    if (!TryEnterCriticalSection(&g_state_lock)) return 3;
    LeaveCriticalSection(&g_state_lock); return 0;
}
int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    InitializeCriticalSection(&g_state_lock); g_log = reinterpret_cast<HANDLE>(1);
    caller_vector.begin = const_cast<char**>(caller_items);
    caller_vector.end = caller_vector.begin + 2; caller_vector.capacity = caller_vector.end;
    for (kind = 0; kind < 6; ++kind) for (scenario = 0; scenario < 14; ++scenario) {
        budget = -1; calls = 0; log_thrown = false; retained = nullptr;
        g_original_classlabels.clear(); g_classlabel_odf_defaults.clear();
        void* self = reinterpret_cast<void*>(0x99);
        g_original_classlabels[self] = scenario == 7
            ? "a class label longer than the native SSO capacity" : "hybridbuild";
        const bool policy = scenario != 1 && scenario != 5 && scenario != 10;
        if (policy) g_classlabel_odf_defaults[g_original_classlabels[self]].defaults["key"] = "3";
        change_gateway(reinterpret_cast<void*>(0x101)); std::strcpy(caller_text, "caller");
        std::uint64_t output = 0; void* target = scenario == 11 ? nullptr : &output;
        std::printf("case:%u:%u\n", kind, scenario);
        try {
            const bool found = invoke(scenario == 10 ? nullptr : self,
                                      scenario == 10 ? nullptr : "key", target);
            retained = nullptr;
            if (kind == 4) std::printf("result:%u:%s\n", found,
                target && *reinterpret_cast<char**>(target) ? *reinterpret_cast<char**>(target) : "<null>");
            else std::printf("result:%u:%016llx\n", found, static_cast<unsigned long long>(output));
        } catch (const Failure& error) {
            retained = nullptr; std::printf("caught:%u:live=%d\n", error.code, live);
        } catch (const std::bad_alloc&) {
            retained = nullptr; std::printf("caught:allocation:live=%d\n", live);
        }
        budget = -1; std::printf("after:calls=%u:live=%d\n", calls, live);
        if (live != 0) return 7;
    }
    HANDLE thread = CreateThread(nullptr, 0, check_lock, nullptr, 0, nullptr);
    if (!thread || WaitForSingleObject(thread, 5000) != WAIT_OBJECT_0) return 8;
    DWORD status = 3; GetExitCodeThread(thread, &status); CloseHandle(thread);
    if (status) return 9;
    std::puts("core parameter fallback fixture: PASS");
    DeleteCriticalSection(&g_state_lock);
}
