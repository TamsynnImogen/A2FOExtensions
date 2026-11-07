#ifndef A2FO_RUST_HYBRID_FFI_H
#define A2FO_RUST_HYBRID_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct { uint32_t queue_id, project_id; uint8_t method, active; } A2foHybridJob;
typedef struct { uint8_t method; size_t index; uint32_t project_id; } A2foHybridEntry;
typedef struct { uint8_t result; size_t index; uint8_t clear; uint8_t activate; } A2foHybridPlan;

/* Returned strings have static lifetime, are NUL terminated, and must not be freed.
 * Unknown method bytes yield "unknown" for names and "" for prefixes. */
const char* a2fo_rs_hybrid_method_name(uint8_t method);
const char* a2fo_rs_hybrid_command_prefix(uint8_t method);
/* ABI contract: all rows are aligned, initialized POD; all nonempty byte
 * spans have readable pointers. The caller keeps them valid and unmodified
 * until return. A zero count accepts a null input pointer. The Rust side
 * borrows input synchronously and retains no pointers. */
/* Queue ops: 0 enqueue, 1 start, 2 finish, 3 cancel, 4 active,
 * 5 find, 6 barrier, 7 accepts. result is enum for enqueue and 0/1 otherwise.
 * index identifies an existing row; clear means erase all after row removal.
 * jobs may be null only if count is zero. */
A2foHybridPlan a2fo_rs_hybrid_queue(uint8_t op, const A2foHybridJob* jobs,
    size_t count, size_t capacity, A2foHybridJob candidate, uint32_t queue_id);
/* List ops: 0 add, 1 at, 2 project, 3 resolve. result is enum for add,
 * found flag for at/project, and method byte for resolve.
 * entries may be null only if count is zero. */
A2foHybridPlan a2fo_rs_hybrid_list(uint8_t op, const A2foHybridEntry* entries,
    size_t count, uint8_t method, size_t index, uint32_t project_id);
/* method and index must be writable; failed parses leave both untouched. */
uint8_t a2fo_rs_hybrid_parse_command(const uint8_t* bytes, size_t length,
    uint8_t* method, size_t* index);
/* Normalize writes at most capacity bytes, without a terminator, and returns
 * the written byte count. A nonempty result needs writable output storage;
 * zero also denotes an empty normalized name or insufficient capacity. */
size_t a2fo_rs_hybrid_normalize(const uint8_t* bytes, size_t length,
    uint8_t* output, size_t capacity);
/* The callback's value span lives only until the callback returns. It must
 * return normally (never unwind across this C ABI); zero aborts parsing.
 * The caller handles any partial results transactionally. */
typedef uint8_t (*A2foHybridSubmenuVisitor)(void* context, size_t parent,
    size_t child, const uint8_t* value, size_t length);
uint8_t a2fo_rs_hybrid_parse_submenu(const uint8_t* bytes, size_t length,
    void* context, A2foHybridSubmenuVisitor visitor);

#ifdef __cplusplus
}
#endif
#endif
