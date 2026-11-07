//! Exact FP environment entry/restore used by the recovery verifier.
//! These labels have no Rust function frame. Only integer pointer loads occur
//! before FXSAVE or after FXRSTOR; no compiler-generated SSE/x87 spill can
//! alter the captured/restored state. Caller owns a 16-byte-aligned 512-byte
//! buffer and a live control-word pointer. Calls are synchronous and private.

#[cfg(all(target_arch = "x86", target_os = "windows", target_env = "gnu"))]
core::arch::global_asm!(
    r#"
.text
.globl _a2fo_rs_fp_save_reset
_a2fo_rs_fp_save_reset:
    mov eax, dword ptr [esp + 4]
    fxsave [eax]
    fninit
    mov eax, dword ptr [esp + 8]
    fldcw word ptr [eax]
    ret
.globl _a2fo_rs_fp_restore
_a2fo_rs_fp_restore:
    mov eax, dword ptr [esp + 4]
    fxrstor [eax]
    ret
"#
);

#[cfg(all(target_arch = "x86_64", target_os = "linux"))]
core::arch::global_asm!(
    r#"
.text
.globl a2fo_rs_fp_save_reset
.type a2fo_rs_fp_save_reset,@function
a2fo_rs_fp_save_reset:
    fxsave [rdi]
    fninit
    fldcw word ptr [rsi]
    ret
.size a2fo_rs_fp_save_reset, .-a2fo_rs_fp_save_reset
.globl a2fo_rs_fp_restore
.type a2fo_rs_fp_restore,@function
a2fo_rs_fp_restore:
    fxrstor [rdi]
    ret
.size a2fo_rs_fp_restore, .-a2fo_rs_fp_restore
"#
);
