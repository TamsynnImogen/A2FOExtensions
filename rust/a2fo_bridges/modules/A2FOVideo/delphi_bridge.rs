//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOVideo/delphi_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" .text
 # cdecl(target, form, value) -> Delphi EAX=form, EDX=value.
 .globl _a2fo_video_set_client
_a2fo_video_set_client:
 pushl %ebp
 movl %esp, %ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 movl 12(%ebp), %eax
 movl 16(%ebp), %edx
 call *8(%ebp)
 popl %edi
 popl %esi
 popl %ebx
 leave
 ret
"###,
    options(att_syntax)
);
