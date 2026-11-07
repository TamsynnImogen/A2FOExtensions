//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOMissionSelector/thiscall_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" .text
 # Invoke a no-argument MSVC thiscall target. The C wrapper supplies the
 # target and native object as ordinary cdecl arguments; the engine expects
 # its object in ECX.
 .globl _a2fo_mission_selector_call_thiscall_0
_a2fo_mission_selector_call_thiscall_0:
 movl 4(%esp), %eax
 movl 8(%esp), %ecx
 jmp *%eax

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 2, 1, 0x90
"###,
    options(att_syntax)
);
