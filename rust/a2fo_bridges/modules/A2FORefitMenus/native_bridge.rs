//! Exact i686 Windows ABI bridge, owned and assembled by Rust.
//! No compiler-generated entry/exit sequence may surround these labels.
//! Registers, flags, stack cleanup and x87/SSE instructions are contractual.
//! Historical source: modules/A2FORefitMenus/native_bridge.S.
#![no_std]
#[cfg(not(all(target_os = "windows", target_arch = "x86", target_env = "gnu")))]
compile_error!("This bridge requires the verified i686 Windows GNU ABI");
core::arch::global_asm!(
    r###"/* Delphi event: EAX=self, EDX=sender; no stack parameters. Original eight
 * bytes are whole instructions without relative/absolute address operands.
 */
.text
.globl _a2fo_refit_menu_show_bridge
_a2fo_refit_menu_show_bridge:
    push %edx
    push %eax
    call _a2fo_refit_menu_show
    add $8, %esp
    ret
/* Shared shell asset loaders use the same EAX/EDX register parameters.
 * Keep their full return register, including the GR32 PNG false result. */
.globl _a2fo_refit_bitmap_bridge
_a2fo_refit_bitmap_bridge:
    push %edx
    push %eax
    call _a2fo_refit_bitmap_load
    add $8, %esp
    ret
.globl _a2fo_refit_bitmap32_bridge
_a2fo_refit_bitmap32_bridge:
    push %edx
    push %eax
    call _a2fo_refit_bitmap32_load
    add $8, %esp
    ret
/* TImage.Paint: EAX=self, no parameters. */
.globl _a2fo_refit_image_paint_bridge
_a2fo_refit_image_paint_bridge:
    push %eax
    call _a2fo_refit_image_paint
    add $4, %esp
    ret
/* C ABI -> native Delphi register ABI, including a real sender. */
.globl _a2fo_refit_menu_call
_a2fo_refit_menu_call:
    push %ebp
    mov %esp, %ebp
    mov 12(%ebp), %eax
    mov 16(%ebp), %edx
    call *8(%ebp)
    leave
    ret

# Preserve historical COFF end padding as single-byte NOPs.
.text
.fill 3, 1, 0x90
"###,
    options(att_syntax)
);
