// Host-only allocation-failure checks against the real Rust static archive.
// Link with -Wl,--wrap=malloc -Wl,--wrap=free to intercept CrtAllocator.
#include "../rust/runtime_modules_ffi.h"
#include "../rust/runtime_handlers_ffi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {
bool armed = false;
long remaining = -1;
size_t attempts = 0;
void* tracked[1024]{};
size_t tracked_count = 0;

void require(bool ok, const char* operation, size_t point = 0) {
    if (ok) return;
    std::fprintf(stderr, "%s failed at allocation %zu\n", operation, point);
    std::abort();
}
void arm(long fail_after) {
    require(tracked_count == 0, "previous operation leaked");
    attempts = 0;
    remaining = fail_after;
    armed = true;
}
size_t disarm() {
    armed = false;
    return attempts;
}
void check_no_leaks(const char* operation, size_t point = 0) {
    require(tracked_count == 0, operation, point);
}

constexpr uint8_t path_one[] = {'a', 0, 0xff, 'z'};
constexpr uint8_t path_two[] = {'b', 0xfe, 'q'};
constexpr uint8_t owner_one[] = {'o', 0, 0xff};
constexpr uint8_t owner_two[] = {'p', 0xfe};
constexpr uint8_t field_one[] = {'f', 0, 0xfc};
constexpr uint8_t field_two[] = {'g', 0xfd};
constexpr A2foRsHandlerFieldView fields[] = {
    {field_one, sizeof field_one}, {field_two, sizeof field_two},
};

void check_module(const void* store, size_t index, uintptr_t handle,
                  const uint8_t* path, size_t size) {
    A2foRsRuntimeModuleView view{};
    require(a2fo_rs_runtime_modules_get(store, index, &view) == 1 &&
            view.handle == handle && view.shutdown == handle + 1 &&
            view.path_size == size && std::memcmp(view.path, path, size) == 0 &&
            view.path[size] == 0, "module contents", index);
}
void* module_source() {
    void* store = a2fo_rs_runtime_modules_new();
    require(store != nullptr, "module new");
    require(a2fo_rs_runtime_modules_push(store, 0x1234, 0x1235,
            path_one, sizeof path_one) == 1, "module fixture push one");
    require(a2fo_rs_runtime_modules_push(store, 0x5678, 0x5679,
            path_two, sizeof path_two) == 1, "module fixture push two");
    return store;
}
void* full_module_source() {
    void* store = module_source();
    for (uintptr_t handle : {uintptr_t{0x9abc}, uintptr_t{0xdef0}})
        require(a2fo_rs_runtime_modules_push(store, handle, handle + 1,
                path_one, sizeof path_one) == 1, "module fixture fill");
    return store;
}
void check_handler(const void* store, size_t index, const uint8_t* owner,
                   size_t owner_size) {
    A2foRsHandlerView view{};
    require(a2fo_rs_runtime_handlers_get(store, index, &view) == 1,
            "handler get", index);
    const bool correct = view.lease && view.owner_size == owner_size &&
        std::memcmp(view.owner, owner, owner_size) == 0 &&
        view.owner[owner_size] == 0 && view.field_count == 2 &&
        view.handler == 0x1234 && view.user_data == 0x5678 && view.enabled == 1;
    require(correct, "handler contents", index);
    for (size_t i = 0; i != 2; ++i) {
        A2foRsHandlerFieldView field{};
        const uint8_t* expected = i == 0 ? field_one : field_two;
        const size_t size = i == 0 ? sizeof field_one : sizeof field_two;
        require(a2fo_rs_runtime_handlers_get_field(view.lease, i, &field) == 1 &&
                field.size == size && std::memcmp(field.bytes, expected, size) == 0 &&
                field.bytes[size] == 0, "handler field", i);
    }
    a2fo_rs_runtime_handlers_release(view.lease);
}
void* handler_source() {
    void* store = a2fo_rs_runtime_handlers_new();
    require(store != nullptr, "handler new");
    require(a2fo_rs_runtime_handlers_push(store, owner_one, sizeof owner_one,
            fields, 2, 0x1234, 0x5678, 1) == 1, "handler fixture push one");
    require(a2fo_rs_runtime_handlers_push(store, owner_two, sizeof owner_two,
            fields, 2, 0x1234, 0x5678, 1) == 1, "handler fixture push two");
    return store;
}
void* full_handler_source() {
    void* store = handler_source();
    for (int i = 0; i < 2; ++i)
        require(a2fo_rs_runtime_handlers_push(store, owner_one, sizeof owner_one,
                fields, 2, 0x1234, 0x5678, 1) == 1, "handler fixture fill");
    return store;
}

void check_new() {
    arm(-1);
    void* modules = a2fo_rs_runtime_modules_new();
    const size_t module_count = disarm();
    require(modules && module_count > 0, "module new count");
    a2fo_rs_runtime_modules_drop(modules);
    check_no_leaks("module new cleanup");
    for (size_t i = 0; i < module_count; ++i) {
        arm(static_cast<long>(i));
        modules = a2fo_rs_runtime_modules_new();
        disarm();
        require(modules == nullptr, "module new OOM", i);
        check_no_leaks("module new OOM cleanup", i);
    }
    arm(-1);
    void* handlers = a2fo_rs_runtime_handlers_new();
    const size_t handler_count = disarm();
    require(handlers && handler_count > 0, "handler new count");
    a2fo_rs_runtime_handlers_drop(handlers);
    check_no_leaks("handler new cleanup");
    for (size_t i = 0; i < handler_count; ++i) {
        arm(static_cast<long>(i));
        handlers = a2fo_rs_runtime_handlers_new();
        disarm();
        require(handlers == nullptr, "handler new OOM", i);
        check_no_leaks("handler new OOM cleanup", i);
    }
    std::puts("new:all-allocation-failures:PASS");
}
void check_module_push() {
    void* store = full_module_source();
    arm(-1);
    const uint8_t success = a2fo_rs_runtime_modules_push(store, 0xabcd, 0xabce,
                                                         path_two, sizeof path_two);
    const size_t count = disarm();
    require(success == 1 && count > 0, "module push count");
    a2fo_rs_runtime_modules_drop(store);
    check_no_leaks("module push cleanup");
    for (size_t i = 0; i < count; ++i) {
        store = full_module_source();
        arm(static_cast<long>(i));
        const uint8_t result = a2fo_rs_runtime_modules_push(store, 0xabcd, 0xabce,
                                                             path_two, sizeof path_two);
        disarm();
        require(result == 0 && a2fo_rs_runtime_modules_len(store) == 4,
                "module push OOM rollback", i);
        check_module(store, 0, 0x1234, path_one, sizeof path_one);
        check_module(store, 1, 0x5678, path_two, sizeof path_two);
        check_module(store, 2, 0x9abc, path_one, sizeof path_one);
        check_module(store, 3, 0xdef0, path_one, sizeof path_one);
        a2fo_rs_runtime_modules_drop(store);
        check_no_leaks("module push OOM cleanup", i);
    }
    std::puts("module-push:all-allocation-failures:PASS");
}
void check_module_clone() {
    void* source = module_source();
    arm(-1);
    void* clone = a2fo_rs_runtime_modules_clone(source);
    const size_t count = disarm();
    require(clone && count > 0, "module clone count");
    a2fo_rs_runtime_modules_drop(clone);
    a2fo_rs_runtime_modules_drop(source);
    check_no_leaks("module clone cleanup");
    for (size_t i = 0; i < count; ++i) {
        source = module_source();
        arm(static_cast<long>(i));
        clone = a2fo_rs_runtime_modules_clone(source);
        disarm();
        require(clone == nullptr && a2fo_rs_runtime_modules_len(source) == 2,
                "module clone OOM rollback", i);
        check_module(source, 0, 0x1234, path_one, sizeof path_one);
        check_module(source, 1, 0x5678, path_two, sizeof path_two);
        a2fo_rs_runtime_modules_drop(source);
        check_no_leaks("module clone OOM cleanup", i);
    }
    std::puts("module-clone:all-allocation-failures:PASS");
}
void check_handler_push() {
    void* store = full_handler_source();
    arm(-1);
    const uint8_t success = a2fo_rs_runtime_handlers_push(store, owner_one,
        sizeof owner_one, fields, 2, 0x1234, 0x5678, 1);
    const size_t count = disarm();
    require(success == 1 && count > 0, "handler push count");
    a2fo_rs_runtime_handlers_drop(store);
    check_no_leaks("handler push cleanup");
    for (size_t i = 0; i < count; ++i) {
        store = full_handler_source();
        A2foRsHandlerView lease{};
        require(a2fo_rs_runtime_handlers_get(store, 0, &lease) == 1,
                "handler pre-push lease");
        arm(static_cast<long>(i));
        const uint8_t result = a2fo_rs_runtime_handlers_push(store, owner_one,
            sizeof owner_one, fields, 2, 0x1234, 0x5678, 1);
        disarm();
        require(result == 0 && a2fo_rs_runtime_handlers_len(store) == 4,
                "handler push OOM rollback", i);
        check_handler(store, 0, owner_one, sizeof owner_one);
        check_handler(store, 1, owner_two, sizeof owner_two);
        check_handler(store, 2, owner_one, sizeof owner_one);
        check_handler(store, 3, owner_one, sizeof owner_one);
        a2fo_rs_runtime_handlers_clear(store);
        require(std::memcmp(lease.owner, owner_one, sizeof owner_one) == 0,
                "handler lease after clear", i);
        A2foRsHandlerFieldView field{};
        require(a2fo_rs_runtime_handlers_get_field(lease.lease, 1, &field) == 1 &&
                field.size == sizeof field_two &&
                std::memcmp(field.bytes, field_two, sizeof field_two) == 0,
                "handler field lease after clear", i);
        a2fo_rs_runtime_handlers_release(lease.lease);
        a2fo_rs_runtime_handlers_drop(store);
        check_no_leaks("handler push OOM cleanup", i);
    }
    std::puts("handler-push:all-allocation-failures:PASS");
}
void check_handler_clone() {
    void* source = handler_source();
    arm(-1);
    void* clone = a2fo_rs_runtime_handlers_clone(source);
    const size_t count = disarm();
    require(clone && count > 0, "handler clone count");
    a2fo_rs_runtime_handlers_drop(clone);
    a2fo_rs_runtime_handlers_drop(source);
    check_no_leaks("handler clone cleanup");
    for (size_t i = 0; i < count; ++i) {
        source = handler_source();
        A2foRsHandlerView lease{};
        require(a2fo_rs_runtime_handlers_get(source, 1, &lease) == 1,
                "handler pre-clone lease");
        arm(static_cast<long>(i));
        clone = a2fo_rs_runtime_handlers_clone(source);
        disarm();
        require(clone == nullptr && a2fo_rs_runtime_handlers_len(source) == 2,
                "handler clone OOM rollback", i);
        check_handler(source, 0, owner_one, sizeof owner_one);
        check_handler(source, 1, owner_two, sizeof owner_two);
        a2fo_rs_runtime_handlers_drop(source);
        require(std::memcmp(lease.owner, owner_two, sizeof owner_two) == 0,
                "handler lease after source drop", i);
        a2fo_rs_runtime_handlers_release(lease.lease);
        check_no_leaks("handler clone OOM cleanup", i);
    }
    std::puts("handler-clone:all-allocation-failures:PASS");
}
} // namespace

extern "C" void* __real_malloc(size_t);
extern "C" void __real_free(void*);
extern "C" void* __wrap_malloc(size_t size) {
    if (armed) {
        ++attempts;
        if (remaining == 0) return nullptr;
        if (remaining > 0) --remaining;
    }
    void* result = __real_malloc(size);
    if (armed && result) {
        if (tracked_count == sizeof tracked / sizeof tracked[0]) std::abort();
        tracked[tracked_count++] = result;
    }
    return result;
}
extern "C" void __wrap_free(void* pointer) {
    for (size_t i = 0; i < tracked_count; ++i) {
        if (tracked[i] == pointer) {
            tracked[i] = tracked[--tracked_count];
            break;
        }
    }
    __real_free(pointer);
}

int main() {
    check_new();
    check_module_push();
    check_module_clone();
    check_handler_push();
    check_handler_clone();
}
