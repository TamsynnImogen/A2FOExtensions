#ifndef A2FO_RUST_RESUPPLY_FFI_H
#define A2FO_RUST_RESUPPLY_FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* All names and input strings are byte slices; no encoding conversion occurs. */
size_t a2fo_rs_resupply_normalize_type(const uint8_t* raw, size_t raw_len,
                                        uint8_t* output, size_t output_capacity);
size_t a2fo_rs_resupply_normalize_odf(const uint8_t* raw, size_t raw_len,
                                       uint8_t* output, size_t output_capacity);

/* kind: 0 = type, 1 = ODF. Return 0 if the receiver cannot store the rule. */
typedef uint8_t (*a2fo_rs_resupply_rule_visitor)(void* context, uint8_t kind,
                                                  const uint8_t* key, size_t key_len,
                                                  uint8_t allow);
/* Returns 1 only for a valid rule list and successful visitor calls. */
uint8_t a2fo_rs_resupply_parse(const uint8_t* raw, size_t raw_len,
                               void* context, a2fo_rs_resupply_rule_visitor visitor,
                               uint8_t* default_allow);

/* Lookup values: -1 = absent, 0 = deny, 1 = allow. Rust owns decision order. */
uint8_t a2fo_rs_resupply_allowed(uint8_t valid, uint8_t default_allow,
                                  int8_t type_rule, int8_t odf_rule,
                                  uint8_t same_object, uint8_t can_resupply_self);

#ifdef __cplusplus
}
#endif
#endif
