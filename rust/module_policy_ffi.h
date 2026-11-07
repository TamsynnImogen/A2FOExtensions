#pragma once

#include <stddef.h>
#include <stdint.h>

// All byte views may contain NUL/non-UTF8 bytes. A null pointer is valid only
// with length zero. Arrays and callback context remain caller-owned. Rust
// borrows inputs only for this call; callback views remain valid only until
// that callback returns. Callback function pointers must be non-null.
// Callbacks must copy data they retain and must not throw across this C ABI.
// Functions return 0 for malformed input, 1 on success.
#ifdef __cplusplus
extern "C" {
#endif

typedef struct A2foPolicyBytes { const uint8_t* data; size_t size; } A2foPolicyBytes;
typedef struct A2foPolicyByteArray { const A2foPolicyBytes* data; size_t size; } A2foPolicyByteArray;
typedef struct A2foPolicyRule {
    uint8_t has_section;
    A2foPolicyByteArray required, rejected, active, diagnostics;
} A2foPolicyRule;
typedef struct A2foPolicyInstalled {
    A2foPolicyBytes name, filename, path;
} A2foPolicyInstalled;
typedef struct A2foPolicyEntry {
    A2foPolicyBytes key, name, filename, path;
    uint32_t state; // order of a2fo::ModulePolicyState
    uint8_t installed;
} A2foPolicyEntry;

typedef void (*A2foPolicyBytesVisitor)(void*, A2foPolicyBytes);
typedef void (*A2foPolicyRuleVisitor)(void*, uint32_t, A2foPolicyBytes);
typedef void (*A2foPolicyEntryVisitor)(void*, const A2foPolicyEntry*);
typedef void (*A2foPolicyMetaVisitor)(void*, uint8_t, uint8_t);

uint8_t a2fo_rs_policy_normalize(A2foPolicyBytes, void*, A2foPolicyBytesVisitor);
uint8_t a2fo_rs_policy_key(A2foPolicyBytes, void*, A2foPolicyBytesVisitor);
// Rule visitor kinds: 0 has_section (empty view), 1 required,
// 2 rejected, 3 active, 4 diagnostic.
uint8_t a2fo_rs_policy_parse(A2foPolicyBytes, void*, A2foPolicyRuleVisitor);
uint8_t a2fo_rs_policy_evaluate(const A2foPolicyRule*, size_t,
                                const A2foPolicyInstalled*, size_t, void*,
                                A2foPolicyMetaVisitor,
                                A2foPolicyEntryVisitor,
                                A2foPolicyBytesVisitor);
uint8_t a2fo_rs_policy_rewrite(A2foPolicyBytes, A2foPolicyByteArray,
                               void*, A2foPolicyBytesVisitor);

#ifdef __cplusplus
}
#endif
