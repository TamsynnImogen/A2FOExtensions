//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOCameraControls/native_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###"/* Called in place of the six-byte FSTP after FASTSCROLL is parsed.
 * No C++ call, scratch FP stack entry or changed integer register/flags.
 * The original ST0 is multiplied then popped into the original destination.
 */
.text
.globl _a2fo_refit_bridge
_a2fo_refit_bridge:
    pushfl
    lock incl _a2fo_refit_parser_visits
    cmpl $0x3f800000, _a2fo_refit_value_bits
    je 1f
    popfl
    fmuls _a2fo_refit_value_bits
    jmp 2f
1:
    popfl
2:
    push %eax
    mov _a2fo_refit_data_a, %eax
    fstps (%eax)
    pop %eax
    ret
"###,
    options(att_syntax)
);
