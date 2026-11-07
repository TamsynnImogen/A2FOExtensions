.data
.p2align 2
.globl _test_wrapper_sp, _test_before_sp, _test_after_sp
.globl _test_after_eax, _test_after_ebx, _test_after_esi, _test_after_edi, _test_after_ecx
.globl _test_path, _test_native_ecx, _test_cont_edx, _test_menu_entry_eax
_test_wrapper_sp: .long 0
_test_before_sp: .long 0
_test_after_sp: .long 0
_test_after_eax: .long 0
_test_after_ebx: .long 0
_test_after_esi: .long 0
_test_after_edi: .long 0
_test_after_ecx: .long 0
_test_path: .long 0
_test_native_ecx: .long 0
_test_cont_edx: .long 0
_test_menu_entry_eax: .long 0

.text
.p2align 2
.globl _test_call_stub
_test_call_stub:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, _test_wrapper_sp
    movl 16(%ebp), %ebx
    movl 20(%ebp), %esi
    movl 24(%ebp), %edi
    movl 28(%ebp), %ecx
    movl 12(%ebp), %edx
    movl 8(%ebp), %eax
    movl %edx, %ebp
    movl %esp, _test_before_sp
    call *%eax
    movl %esp, _test_after_sp
    movl %eax, _test_after_eax
    movl %ebx, _test_after_ebx
    movl %esi, _test_after_esi
    movl %edi, _test_after_edi
    movl %ecx, _test_after_ecx
    movl _test_wrapper_sp, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

.globl _test_capture
_test_capture:
    movl %edx, _test_cont_edx
    ret
.globl _test_capture_one, _test_capture_many, _test_capture_check, _test_capture_accept
_test_capture_one:
    movl $1, _test_path
    ret
_test_capture_many:
    movl $2, _test_path
    ret
_test_capture_check:
    movl $3, _test_path
    ret
_test_capture_accept:
    movl $4, _test_path
    ret
.globl _test_capture_cancel_empty
_test_capture_cancel_empty:
    movl $5, _test_path
    ret
.globl _test_capture_route_loop
_test_capture_route_loop:
    movl $6, _test_path
    ret
.globl _test_capture_menu_open, _test_capture_menu_exit
_test_capture_menu_open:
    movl %eax, _test_menu_entry_eax
    movl $7, _test_path
    ret
_test_capture_menu_exit:
    movl %eax, _test_menu_entry_eax
    movl $8, _test_path
    ret
.globl _test_queue_gateway
_test_queue_gateway:
    pushl 8(%esp)
    pushl 8(%esp)
    pushl %ecx
    call _test_queue_handler
    addl $12, %esp
    ret $8
.globl _test_native_qsize
_test_native_qsize:
    movl %ecx, _test_native_ecx
    movl $77, %eax
    ret
