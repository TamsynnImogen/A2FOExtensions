.text
.globl _refit_menu_test_call
_refit_menu_test_call:
    push %ebp
    mov %esp, %ebp
    mov 12(%ebp), %eax
    mov 16(%ebp), %edx
    call *8(%ebp)
    leave
    ret
.globl _refit_menu_test_call_unaligned
_refit_menu_test_call_unaligned:
    push %ebp
    mov %esp, %ebp
    sub 20(%ebp), %esp
    mov 12(%ebp), %eax
    mov 16(%ebp), %edx
    mov %esp, _refit_menu_skin_caller_stack
    call *8(%ebp)
    leave
    ret
.globl _refit_menu_test_shell_call
_refit_menu_test_shell_call:
    push %ebp
    mov %esp, %ebp
    sub 20(%ebp), %esp
    push 16(%ebp)
    mov 12(%ebp), %ecx
    mov %esp, _refit_menu_shell_caller_stack
    call *8(%ebp)
    mov %esp, _refit_menu_shell_after_stack
    leave
    ret
.globl _refit_menu_test_shell_original0
_refit_menu_test_shell_original0:
    incl _refit_menu_shell_seen_calls
    jmp 1f
.globl _refit_menu_test_shell_original1
_refit_menu_test_shell_original1:
    incl _refit_menu_shell_seen_calls+4
    jmp 1f
.globl _refit_menu_test_shell_original2
_refit_menu_test_shell_original2:
    incl _refit_menu_shell_seen_calls+8
1:
    mov %ecx, _refit_menu_shell_seen_self
    mov 4(%esp), %eax
    mov %eax, _refit_menu_shell_seen_dc
    mov %esp, _refit_menu_shell_seen_stack
    ret $4
.globl _refit_menu_test_original
_refit_menu_test_original:
    mov %eax, _refit_menu_seen_self
    mov %edx, _refit_menu_seen_sender
    incl _refit_menu_seen_calls
    ret
.globl _refit_menu_test_bitmap_original
_refit_menu_test_bitmap_original:
    mov %eax, _refit_menu_skin_seen_bitmap
    mov %edx, _refit_menu_skin_seen_name
    mov %esp, _refit_menu_skin_seen_stack
    incl _refit_menu_skin_seen_calls
    mov _refit_menu_skin_return, %eax
    ret
.globl _refit_menu_test_bitmap_width
_refit_menu_test_bitmap_width:
    mov _refit_menu_skin_width, %eax
    ret
.globl _refit_menu_test_bitmap_height
_refit_menu_test_bitmap_height:
    mov _refit_menu_skin_height, %eax
    ret
.globl _refit_menu_test_bitmap_canvas
_refit_menu_test_bitmap_canvas:
    mov $1, %eax
    ret
.globl _refit_menu_test_bitmap_dc
_refit_menu_test_bitmap_dc:
    mov _refit_menu_skin_dc, %eax
    ret
.globl _refit_menu_test_dispatch
_refit_menu_test_dispatch:
    mov %ax, _refit_menu_seen_dialog
    ret
.globl _refit_menu_test_running
_refit_menu_test_running:
    mov %eax, _refit_menu_seen_running
    ret
.globl _refit_menu_test_close
_refit_menu_test_close:
    mov %eax, _refit_menu_seen_closed
    ret
