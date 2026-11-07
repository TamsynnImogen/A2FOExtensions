//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FOM3D/thiscall_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###" .text
 # cdecl wrapper around Armada's MSVC thiscall SOD loader.
 # args: function, this, name, scale
 .globl _a2fo_m3d_call_load_sod
_a2fo_m3d_call_load_sod:
 pushl %ebp
 movl %esp, %ebp
 pushl 20(%ebp)
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # cdecl wrapper around the native Storm3D stream parser at Armada
 # RVA 0x23fa60.
 # args: function, stream, scale, database_name
 .globl _a2fo_m3d_parse_sod_stream
_a2fo_m3d_parse_sod_stream:
 pushl %ebp
 movl %esp, %ebp
 pushl 20(%ebp)
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # cdecl wrapper around a zero-argument MSVC thiscall method.
 # args: function, this
 .globl _a2fo_m3d_call_thiscall_0
_a2fo_m3d_call_thiscall_0:
 pushl %ebp
 movl %esp, %ebp
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # cdecl wrapper around a one-argument MSVC thiscall method.
 # args: function, this, argument1
 .globl _a2fo_m3d_call_thiscall_1
_a2fo_m3d_call_thiscall_1:
 pushl %ebp
 movl %esp, %ebp
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # cdecl wrapper around a two-argument MSVC thiscall method.
 # args: function, this, argument1, argument2
 .globl _a2fo_m3d_call_thiscall_2
_a2fo_m3d_call_thiscall_2:
 pushl %ebp
 movl %esp, %ebp
 pushl 20(%ebp)
 pushl 16(%ebp)
 movl 12(%ebp), %ecx
 call *8(%ebp)
 leave
 ret
 # cdecl wrapper around a four-argument MSVC thiscall method.
 # args: function, this, argument1, argument2, argument3, argument4
 .globl _a2fo_m3d_call_thiscall_4
_a2fo_m3d_call_thiscall_4:
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

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 1, 1, 0x90
"###,
    options(att_syntax)
);
