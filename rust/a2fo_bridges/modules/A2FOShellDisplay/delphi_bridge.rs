//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOShellDisplay/delphi_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" .text
 # cdecl(target, form, value) -> Delphi EAX=form, EDX=value.
 .globl _a2fo_shell_set_position
_a2fo_shell_set_position:
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
 # Delphi EAX=TWinControl -> cdecl handler(control), no stack arguments.
 .globl _a2fo_shell_bounds_entry
_a2fo_shell_bounds_entry:
 pushl %ebp
 movl %esp, %ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 pushl %eax
 call _a2fo_shell_bounds_handler
 addl $4, %esp
 popl %edi
 popl %esi
 popl %ebx
 leave
 ret
 # Delphi EAX=form EDX=HWND ECX=left; stack height,width,top.
 .globl _a2fo_shell_register_entry
_a2fo_shell_register_entry:
 pushl %ebp
 movl %esp, %ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 pushl 8(%ebp)
 pushl 12(%ebp)
 pushl 16(%ebp)
 pushl %ecx
 pushl %edx
 pushl %eax
 call _a2fo_shell_register_handler
 addl $24, %esp
 popl %edi
 popl %esi
 popl %ebx
 leave
 ret $12
 # cdecl(target,form,HWND,left,top,width,height) -> native RET12.
 .globl _a2fo_shell_register_call
_a2fo_shell_register_call:
 pushl %ebp
 movl %esp, %ebp
 pushl %ebx
 pushl %esi
 pushl %edi
 pushl 24(%ebp)
 pushl 28(%ebp)
 pushl 32(%ebp)
 movl 12(%ebp), %eax
 movl 16(%ebp), %edx
 movl 20(%ebp), %ecx
 call *8(%ebp)
 popl %edi
 popl %esi
 popl %ebx
 leave
 ret

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 3, 1, 0x90
"###,
    options(att_syntax)
);
