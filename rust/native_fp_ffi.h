#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#define A2FO_FP_NOEXCEPT noexcept
#else
#define A2FO_FP_NOEXCEPT
#endif
/* Private synchronous calls. saved must address 512 live bytes aligned to 16;
 * control_word must address a live 16-bit word. Restore uses a prior FXSAVE. */
void a2fo_rs_fp_save_reset(void* saved, const uint16_t* control_word) A2FO_FP_NOEXCEPT;
void a2fo_rs_fp_restore(const void* saved) A2FO_FP_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef A2FO_FP_NOEXCEPT
