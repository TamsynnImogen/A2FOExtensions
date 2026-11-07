//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOODFVariants/thiscall_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" # ABI bridges for A2FOODFVariants calls into Armada's MSVC thiscall code.
 .text
 .globl _a2fo_odf_variants_call_thiscall_0
_a2fo_odf_variants_call_thiscall_0:
 pushl %ebp
 movl %esp, %ebp
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 .globl _a2fo_odf_variants_call_thiscall_1
_a2fo_odf_variants_call_thiscall_1:
 pushl %ebp
 movl %esp, %ebp
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # GameObjectClass::Construct(transform, team, unknown, parent)
 .globl _a2fo_odf_variants_construct
_a2fo_odf_variants_construct:
 pushl %ebp
 movl %esp, %ebp
 pushl 28(%ebp)
 pushl 24(%ebp)
 pushl 20(%ebp)
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
"###,
    options(att_syntax)
);
