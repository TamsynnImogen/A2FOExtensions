// Direct native D3D owner allocation and reentrancy fixture.
// Link with -Wl,--wrap=malloc -Wl,--wrap=free against liba2fo_core.a.
#include "../rust/d3d_native_ffi.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
constexpr std::int32_t e_fail = static_cast<std::int32_t>(0x80004005u);
constexpr std::int32_t e_outofmemory = static_cast<std::int32_t>(0x8007000eu);
constexpr std::int32_t e_pointer = static_cast<std::int32_t>(0x80004003u);
constexpr std::int32_t e_unexpected = static_cast<std::int32_t>(0x8000ffffu);

bool armed = false;
long remaining = -1;
unsigned attempts = 0;
void* allocations[32]{};
unsigned live = 0;

void require(bool ok, const char* label) {
    if (!ok) { std::fprintf(stderr, "FAIL %s\n", label); std::abort(); }
}
void arm(long fail_after) {
    require(!armed && live == 0, "clean allocation tracking start");
    remaining = fail_after; attempts = 0; armed = true;
}
void disarm() { armed = false; remaining = -1; }

// Rust reads slots 1 and 2; guards verify selection and do not invoke methods.
struct View {
    const std::uintptr_t* vtable;
    unsigned references;
    unsigned releases;
};
constexpr std::uintptr_t methods[3] = {1, 2, 3};
View view_a{methods, 0, 0};
View view_b{methods, 0, 0};
View view_c{methods, 0, 0};
void* owner = nullptr;
unsigned creates = 0;
unsigned addrefs = 0;
bool check_old_release_owner = false;
bool reenter_addref_drop = false;
std::int32_t addref_status = 1;
std::int32_t release_status = 1;

std::int32_t feature(std::uintptr_t, void*, std::uint32_t*, void*) noexcept { return 0; }
std::int32_t format(std::uintptr_t, void*, std::uint32_t,
                    std::uint32_t*, std::int32_t*, void*) noexcept { return 0; }
std::int32_t texture(std::uintptr_t, void*, const void*, const void*,
                     void**, std::int32_t*, void*) noexcept { return 0; }
std::int32_t view(std::uintptr_t, void*, void*, const void*,
                  void**, std::int32_t*, void*) noexcept { return 0; }
std::int32_t release_texture(std::uintptr_t, void*, void*) noexcept { return 0; }
std::int32_t release_view(std::uintptr_t method, void* raw, void*) noexcept {
    require(method == methods[2], "Release vtable slot");
    auto* v = static_cast<View*>(raw);
    require(v && v->references > 0, "Release live view");
    ++v->releases;
    --v->references;
    if (v == &view_a && check_old_release_owner) {
        require(owner != nullptr, "new owner published before old Release");
        void* observed = nullptr;
        require(a2fo_rs_d3d_native_handle_get(owner, &observed) == 0 &&
                observed == &view_c, "old Release observes new view");
        // Balance the extra reference returned by reentrant get_view.
        require(view_c.references > 1, "reentrant get added reference");
        --view_c.references;
        check_old_release_owner = false;
    }
    return release_status;
}
std::int32_t addref_view(std::uintptr_t method, void* raw, void*) noexcept {
    require(method == methods[1], "AddRef vtable slot");
    auto* v = static_cast<View*>(raw);
    require(v && v->references > 0, "AddRef live view");
    ++addrefs;
    if (addref_status != 1) return addref_status;
    if (reenter_addref_drop) {
        reenter_addref_drop = false;
        require(owner != nullptr, "reentrant owner exists");
        a2fo_rs_d3d_native_handle_drop(owner);
        owner = nullptr;
    }
    ++v->references;
    return 1;
}
constexpr A2FoD3DGuardApi guards{feature, format, texture, view,
                                  addref_view, release_view, release_texture};

struct CreateRequest { View* value; std::int32_t result; };
std::int32_t create(void* context, void** output) noexcept {
    ++creates;
    auto* request = static_cast<CreateRequest*>(context);
    require(request && output && request->value, "creator inputs");
    ++request->value->references;
    *output = request->value;
    return request->result;
}
void reset_views() {
    require(!armed && live == 0 && owner == nullptr, "reset only when idle");
    view_a = {methods, 0, 0}; view_b = {methods, 0, 0};
    view_c = {methods, 0, 0}; creates = addrefs = 0;
    check_old_release_owner = reenter_addref_drop = false;
    addref_status = release_status = 1;
}

// The upload guard table is separate from the owner table above. The fake
// device exposes only the slots native Rust reads; guards never invoke them.
constexpr std::uint32_t upload_format = 70;
constexpr std::uintptr_t upload_device_methods[38] = {
    0, 0, 0, 0, 0, 105, 0, 107, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 129, 0, 0, 0, 0, 0, 0, 0, 137
};
struct UploadObject { const std::uintptr_t* vtable; unsigned references; };
UploadObject upload_device{upload_device_methods, 1};
UploadObject upload_texture{methods, 0};
UploadObject upload_view{methods, 0};
unsigned upload_validations = 0;
unsigned upload_checks = 0;
unsigned upload_texture_creates = 0;
unsigned upload_view_creates = 0;
unsigned upload_texture_releases = 0;
unsigned upload_view_releases = 0;
constexpr std::uint8_t upload_bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr A2FoD3DMip upload_mip{4, 4, 8, 8, 0};
constexpr A2FoD3DImage upload_image{
    upload_format, 4, 4, 0, &upload_mip, 1, upload_bytes, sizeof(upload_bytes)};
constexpr A2FoD3DConstants upload_constants{
    e_pointer, e_fail, e_unexpected, e_fail, e_fail,
    80, 81, 82, 83, 84, 85, 10, 11, 1, 2, 4};

std::int32_t upload_validate(void*) noexcept { ++upload_validations; return 0; }
std::uintptr_t upload_read(void*, std::uint32_t field) noexcept {
    require(field == 0 || field == 1, "upload host read field");
    return field == 0 ? upload_format :
        reinterpret_cast<std::uintptr_t>(upload_bytes);
}
constexpr A2FoD3DNativeHost upload_host{nullptr, upload_validate, upload_read, nullptr};
std::int32_t upload_feature(std::uintptr_t, void*, std::uint32_t*, void*) noexcept {
    require(false, "non-BC45/7 format skips feature query"); return 0;
}
std::int32_t upload_check(std::uintptr_t method, void* object,
                          std::uint32_t format_value, std::uint32_t* support,
                          std::int32_t* hr, void*) noexcept {
    require(method == upload_device_methods[29] && object == &upload_device &&
            format_value == upload_format && support && hr,
            "upload format guard arguments");
    ++upload_checks; *support = 1 | 2 | 4; *hr = 0; return 1;
}
std::int32_t upload_create_texture(std::uintptr_t method, void* object,
    const void* descriptor, const void* subresources, void** output,
    std::int32_t* hr, void*) noexcept {
    require(method == upload_device_methods[5] && object == &upload_device &&
            descriptor && subresources && output && hr,
            "upload texture guard arguments");
    ++upload_texture_creates;
    require(upload_texture.references == 0, "upload texture slot unused");
    upload_texture.references = 1; *output = &upload_texture; *hr = 0; return 1;
}
std::int32_t upload_create_view(std::uintptr_t method, void* object,
    void* resource, const void* descriptor, void** output,
    std::int32_t* hr, void*) noexcept {
    require(method == upload_device_methods[7] && object == &upload_device &&
            resource == &upload_texture && descriptor && output && hr,
            "upload view guard arguments");
    ++upload_view_creates;
    require(upload_view.references == 0, "upload view slot unused");
    upload_view.references = 1; *output = &upload_view; *hr = 0; return 1;
}
std::int32_t upload_addref(std::uintptr_t, void*, void*) noexcept {
    require(false, "upload does not AddRef"); return 0;
}
std::int32_t upload_release_view(std::uintptr_t method, void* object, void*) noexcept {
    require(method == methods[2] && object == &upload_view &&
            upload_view.references == 1, "upload view Release");
    ++upload_view_releases; --upload_view.references; return 1;
}
std::int32_t upload_release_texture(std::uintptr_t method, void* object, void*) noexcept {
    require(method == methods[2] && object == &upload_texture &&
            upload_texture.references == 1, "upload texture Release");
    ++upload_texture_releases; --upload_texture.references; return 1;
}
constexpr A2FoD3DGuardApi upload_guards{
    upload_feature, upload_check, upload_create_texture, upload_create_view,
    upload_addref, upload_release_view, upload_release_texture};

void check_upload_allocation() {
    require(live == 0 && !armed, "upload allocation starts clean");
    A2FoD3DResult result{};
    void* output = &upload_view;
    arm(0);
    const auto oom = a2fo_rs_d3d_native_upload(
        &upload_image, &upload_constants, &upload_device, 1, &upload_host,
        &upload_guards, &output, &result);
    disarm();
    require(oom == 2 && output == nullptr && attempts == 1 && live == 0 &&
            upload_validations == 1 && upload_checks == 1 &&
            upload_texture_creates == 0 && upload_view_creates == 0 &&
            upload_texture_releases == 0 && upload_view_releases == 0,
            "upload Vec OOM after support before creation");
    std::puts("upload:vec_oom_after_check,no_creation,no_leak");

    output = &upload_texture;
    arm(-1);
    const auto success = a2fo_rs_d3d_native_upload(
        &upload_image, &upload_constants, &upload_device, 1, &upload_host,
        &upload_guards, &output, &result);
    disarm();
    require(success == 1 && result.hr == 0 && output == &upload_view &&
            attempts == 1 && live == 0 && upload_validations == 2 &&
            upload_checks == 2 && upload_texture_creates == 1 &&
            upload_view_creates == 1 && upload_texture_releases == 1 &&
            upload_texture.references == 0 && upload_view.references == 1,
            "upload success frees Vec and transfers one view reference");
    require(upload_release_view(methods[2], output, nullptr) == 1 &&
            upload_view.references == 0 && upload_view_releases == 1,
            "upload caller balances returned view");
    std::puts("upload:success,vec_freed,view_handoff_balanced");
}
} // namespace

extern "C" void* __real_malloc(std::size_t);
extern "C" void __real_free(void*);
extern "C" void* __wrap_malloc(std::size_t size) {
    if (armed) {
        ++attempts;
        if (remaining == 0) return nullptr;
        if (remaining > 0) --remaining;
    }
    void* p = __real_malloc(size);
    if (armed && p) {
        require(live < sizeof(allocations) / sizeof(allocations[0]), "tracking capacity");
        allocations[live++] = p;
    }
    return p;
}
extern "C" void __wrap_free(void* p) {
    for (unsigned i = 0; i < live; ++i) {
        if (allocations[i] == p) {
            allocations[i] = allocations[--live];
            break;
        }
    }
    __real_free(p);
}

int main() {
    reset_views();
    CreateRequest a{&view_a, 0}, b{&view_b, e_fail}, c{&view_c, 0};
    require(a2fo_rs_d3d_native_handle_initialize(nullptr, &guards, &a, create)
            == e_fail && creates == 0, "null owner rejected");
    require(a2fo_rs_d3d_native_handle_initialize(&owner, nullptr, &a, create)
            == e_fail && creates == 0 && owner == nullptr, "null guards rejected");
    require(a2fo_rs_d3d_native_handle_get(nullptr, nullptr) == e_pointer,
            "null output rejected");
    void* output = &view_b;
    require(a2fo_rs_d3d_native_handle_get(nullptr, &output) == e_unexpected &&
            output == nullptr, "uninitialized get clears output");
    std::puts("invalid:owner,guards,output,uninitialized");

    arm(0);
    require(a2fo_rs_d3d_native_handle_initialize(&owner, &guards, &a, create)
            == e_outofmemory, "first allocation OOM");
    disarm();
    require(attempts == 1 && live == 0 && creates == 0 && owner == nullptr,
            "OOM leaves owner and creator untouched");
    std::puts("allocation:first_failure=outofmemory,owner=unchanged");

    arm(-1);
    require(a2fo_rs_d3d_native_handle_initialize(&owner, &guards, &a, create)
            == 0 && owner && view_a.references == 1 && live == 1,
            "initial owner published");
    void* first = owner;
    require(a2fo_rs_d3d_native_handle_initialize(&owner, &guards, &b, create)
            == e_fail && owner == first && view_b.references == 0 &&
            view_b.releases == 1 && view_a.references == 1 && live == 1,
            "failed creator frees pending and preserves old owner");
    check_old_release_owner = true;
    require(a2fo_rs_d3d_native_handle_initialize(&owner, &guards, &c, create)
            == 0 && owner != first && !check_old_release_owner &&
            view_a.references == 0 && view_a.releases == 1 &&
            view_c.references == 1 && live == 1,
            "replacement publishes before old Release exactly once");
    std::puts("initialize:pending_failure_clean,replacement_published_before_release");

    output = &view_b;
    require(a2fo_rs_d3d_native_handle_get(owner, nullptr) == e_pointer,
            "get null output");
    addref_status = 2; // C++ firewall's std::bad_alloc status.
    require(a2fo_rs_d3d_native_handle_get(owner, &output) == e_fail &&
            output == nullptr && view_c.references == 1, "AddRef bad_alloc guard");
    addref_status = 0; // Other C++ exception status.
    output = &view_b;
    require(a2fo_rs_d3d_native_handle_get(owner, &output) == e_fail &&
            output == nullptr && view_c.references == 1, "AddRef other guard");
    addref_status = 1;
    reenter_addref_drop = true;
    require(a2fo_rs_d3d_native_handle_get(owner, &output) == 0 &&
            output == &view_c && owner == nullptr &&
            view_c.references == 1 && view_c.releases == 1,
            "AddRef snapshot survives owner destruction");
    require(release_view(methods[2], output, nullptr) == 1 &&
            view_c.references == 0, "release returned snapshot reference");
    require(live == 0, "reentrant drop frees owner allocation");
    disarm();
    std::puts("get:guard_failures_clear_output,reentrant_snapshot_balanced");

    reset_views();
    arm(-1);
    require(a2fo_rs_d3d_native_handle_initialize(&owner, &guards, &a, create)
            == 0 && live == 1, "owner for release guard fault");
    release_status = 2; // Guard reports swallowed std::bad_alloc after mock release.
    a2fo_rs_d3d_native_handle_drop(owner);
    owner = nullptr;
    require(view_a.references == 0 && view_a.releases == 1 && live == 0,
            "release guard fault still frees owner state");
    disarm();
    std::puts("drop:release_guard_fault_swallowed,state_freed");
    require(live == 0, "all tracked allocations freed");
    check_upload_allocation();
    std::puts("PASS platform_d3d11_allocations_native");
}
