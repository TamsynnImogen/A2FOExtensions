#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foRsA1BznFieldWidth {
    uint32_t declared;
    uint32_t serialized;
} A2foRsA1BznFieldWidth;

// Native context owns the SOL result and all reader state. Each operation
// completes before control returns to Rust. No Rust view spans a callback.
typedef uint64_t (*A2foRsA1BznDispatchStep)(void* context,
                                              uint32_t operation);

uint8_t a2fo_rs_stage9_a1_bzn_inspect_width(uint32_t requested);
// Caller guards an eight-byte readable field and a writable out pointer.
void a2fo_rs_stage9_a1_bzn_field_width(
    const uint8_t* field, uint32_t requested,
    A2foRsA1BznFieldWidth* out);
uint8_t a2fo_rs_stage9_a1_bzn_pad(
    uint8_t a1_stream, uint32_t serialized, uint32_t requested);
// Caller supplies twelve previously copied readable bytes.
uint8_t a2fo_rs_stage9_a1_bzn_count_record(const uint8_t* record);
uint8_t a2fo_rs_stage9_a1_bzn_unique_marker(
    uint32_t matches, uint32_t distance);
// Inspection flags do not affect the source's cursor subtraction.
size_t a2fo_rs_stage9_a1_bzn_consumed(
    uint8_t have_before, uint8_t have_after,
    size_t before, size_t after);
// Caller supplies the exact length of already-lowercased readable bytes.
uint8_t a2fo_rs_stage9_a1_bzn_sol_suffix(
    const uint8_t* lowered, size_t length);
uint32_t a2fo_rs_stage9_a1_bzn_last_start(uint32_t count);
// Operations 0 SOL handled, 1 stored SOL result,
// 2 native guard and inspection: 0 non-A1, 1 A1, 2 invalid reader;
// 3 native original result, 4 A1 cursor/log workflow.
size_t a2fo_rs_stage9_a1_bzn_table_dispatch(
    void* context, A2foRsA1BznDispatchStep step);

#ifdef __cplusplus
}
#endif
