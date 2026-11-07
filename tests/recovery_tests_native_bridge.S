/* Private Win32 call probe. The fixture supplies the copied pinned body or
 * the module bridge and checks its result without changing either body. */
.text
.globl _recovery_probe
_recovery_probe:
    push %ebp
    mov %esp, %ebp
    push %ebx
    push %esi
    push %edi
    fxsave _recovery_probe_saved_fx
    mov 16(%ebp), %eax
    fxrstor (%eax)
    mov 20(%ebp), %eax
    mov %esp, 36(%eax)
    mov $0x12345678, %ebx
    mov $0x23456789, %esi
    mov $0x3456789a, %edi
    mov $0xfeedface, %edx
    mov 12(%ebp), %ecx
    mov $0xdecafbad, %eax
    push 24(%ebp)
    popfl
    call *8(%ebp)
    pushal
    pushfl
    cld
    mov 20(%ebp), %edx
    mov 32(%esp), %eax
    mov %eax, 0(%edx)
    mov 20(%esp), %eax
    mov %eax, 4(%edx)
    mov 8(%esp), %eax
    mov %eax, 8(%edx)
    mov 4(%esp), %eax
    mov %eax, 12(%edx)
    mov 12(%esp), %eax
    mov %eax, 16(%edx)
    mov 16(%esp), %eax
    mov %eax, 20(%edx)
    mov 0(%esp), %eax
    mov %eax, 24(%edx)
    mov 28(%esp), %eax
    mov %eax, 28(%edx)
    mov 24(%esp), %eax
    mov %eax, 32(%edx)
    fxsave 48(%edx)
    fxrstor _recovery_probe_saved_fx
    add $36, %esp
    pop %edi
    pop %esi
    pop %ebx
    pop %ebp
    ret
