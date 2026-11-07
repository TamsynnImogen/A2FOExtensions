//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOAnimatedHardpoints/thiscall_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" # Small ABI bridges between MinGW cdecl helpers and Armada's MSVC
 # thiscall methods. The function address is always the first cdecl argument.
 .text
 .globl _a2fo_animated_hardpoints_call_thiscall_0
_a2fo_animated_hardpoints_call_thiscall_0:
 pushl %ebp
 movl %esp, %ebp
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 .globl _a2fo_animated_hardpoints_call_thiscall_1
_a2fo_animated_hardpoints_call_thiscall_1:
 pushl %ebp
 movl %esp, %ebp
 movl 12(%ebp), %ecx
 pushl 16(%ebp)
 call *8(%ebp)
 leave
 ret
 .globl _a2fo_animated_hardpoints_call_thiscall_2
_a2fo_animated_hardpoints_call_thiscall_2:
 pushl %ebp
 movl %esp, %ebp
 movl 12(%ebp), %ecx
 pushl 20(%ebp)
 pushl 16(%ebp)
 call *8(%ebp)
 leave
 ret

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 2, 1, 0x90
"###,
    options(att_syntax)
);
