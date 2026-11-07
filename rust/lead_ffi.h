#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Private ABI. Nonempty input ranges are readable for the whole call; outputs
 * are writable and disjoint. Visitors copy borrowed bytes synchronously and
 * never unwind through Rust. These are not public SDK exports. */
void a2fo_rs_lead_tokens(const uint32_t*, size_t, uint8_t, uint32_t*, size_t*);
uint8_t a2fo_rs_lead_fvf(uint32_t, uint32_t* fields27);
uint8_t a2fo_rs_lead_primitive(uint32_t, uint32_t, uint32_t*);
uint8_t a2fo_rs_lead_fan(uint32_t, const uint8_t*, uint32_t, uint32_t, uint32_t*, size_t);
typedef void (*A2foLeadBytes)(void*, uint32_t, const uint8_t*, size_t);
uint8_t a2fo_rs_lead_vs11(const uint32_t*, size_t, void*, A2foLeadBytes,
                         uint16_t*, uint8_t* masks12, size_t*);
void a2fo_rs_lead_vs_interface(void*, A2foLeadBytes);
void a2fo_rs_lead_fallback(const uint8_t*, size_t, const uint8_t*, size_t, void*, A2foLeadBytes);
uint32_t a2fo_rs_lead_shield_action(uint8_t, float, int32_t);
uint8_t a2fo_rs_lead_scan_due(uint32_t, uint32_t, uint8_t, uint32_t);
uint8_t a2fo_rs_lead_editor_valid(const uint8_t*, size_t);
uint8_t a2fo_rs_lead_editor_number(const char*, float*);
uint32_t a2fo_rs_lead_editor_decode(const uint8_t*, size_t, uint8_t, uint8_t, uint8_t*, size_t*);
#ifdef __cplusplus
}
#endif
