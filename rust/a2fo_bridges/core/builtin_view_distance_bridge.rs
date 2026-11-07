//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: core/builtin_view_distance_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###"// Isolate the original bridge's symbols without changing its instructions.
/* Replaces MOV ECX,[cfgOBJECT_CULLING_DISTANCE]. Positive finite IEEE754
 * words have unsigned numeric order. Comparing bits preserves ALL original
 * x87/SSE state; negative, NaN and infinity inputs are left untouched.
 * The following native MOV stores the resulting ECX into the restoration
 * baseline. Original EFLAGS and all registers except ECX are preserved.
 */
.text
.globl _a2fo_builtin_view_bridge
_a2fo_builtin_view_bridge:
    pushfl
    pushal
    lock incl _a2fo_builtin_view_parser_visits
    mov _a2fo_builtin_view_value_bits, %edx
    test %edx, %edx
    jz 3f
    mov _a2fo_builtin_view_data_a, %eax
    mov (%eax), %ecx
    cmp $0x7f800000, %ecx
    jae 1f
    cmp %edx, %ecx
    jae 1f
    mov %edx, (%eax)
1:
    mov _a2fo_builtin_view_data_b, %eax
    mov (%eax), %ecx
    cmp $0x7f800000, %ecx
    jae 3f
    cmp %edx, %ecx
    jae 3f
    mov %edx, (%eax)
3:
    popal
    popfl
    push %eax
    mov _a2fo_builtin_view_data_b, %eax
    mov (%eax), %ecx
    pop %eax
    ret

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 3, 1, 0x90
"###,
    options(att_syntax)
);
