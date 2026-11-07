/* Only two fully pinned no-argument, no-call bodies use this bridge.
 * Their maximum original call depth below entry is 24 bytes. The saved
 * private-frame pointer at entry ESP-40 is outside that footprint.
 */
.text
.macro recovery_bridge name, kind, gateway
.globl _\name
_\name:
    pushfl
    pushal
    mov %esp, %ebp
    sub $943, %esp
    and $-16, %esp
    mov %ebp, 896(%esp)
    fxsave (%esp)
    cld
    fninit
    ldmxcsr recovery_default_mxcsr
    mov %esp, %eax
    push %eax
    lea 512(%eax), %edx
    push %edx
    push 24(%ebp)
    push $\kind
    call _a2fo_recovery_before
    add $16, %esp
    mov %esp, %eax
    mov 896(%esp), %edx
    mov %eax, -4(%edx)
    fxrstor (%esp)
    mov 896(%esp), %esp
    popal
    popfl
    call *_\gateway
    pushfl
    pushal
    mov %esp, %eax
    mov -4(%esp), %ebp
    mov %eax, 900(%ebp)
    fxsave (%ebp)
    cld
    fninit
    ldmxcsr recovery_default_mxcsr
    mov %ebp, %esp
    sub $8, %esp
    push 28(%eax)
    lea 512(%ebp), %edx
    push %edx
    call _a2fo_recovery_after
    add $16, %esp
    fxrstor (%esp)
    mov 900(%esp), %esp
    popal
    popfl
    ret
.endm
recovery_bridge a2fo_recovery_context_bridge, 0, a2fo_recovery_context_gateway
recovery_bridge a2fo_recovery_node_bridge, 1, a2fo_recovery_node_gateway
.section .rdata,"dr"
.balign 4
recovery_default_mxcsr:
    .long 0x1f80
