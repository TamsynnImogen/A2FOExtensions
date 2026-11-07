/* FleetOps Delphi register ABI. Preserve the interrupted x87 environment too. */
.text
.globl _a2fo_collective_award_bridge
_a2fo_collective_award_bridge:
    pushfl
    pushal
    subl $108, %esp
    fnsave (%esp)
    movl 128(%esp), %edx
    movl 124(%esp), %eax
    pushl %edx
    pushl %eax
    call _a2fo_collective_award
    addl $8, %esp
    testl %eax, %eax
    jz 1f
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    movl 4(%edx), %edx
    jmp *_a2fo_collective_award_continue
1:
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    /* Exactly the original XP write; executed only for stock owners. */
    flds 0x10(%eax)
    movl 4(%edx), %edx
    fadds 0xb4(%edx)
    fstps 0x10(%eax)
    fwait
    jmp *_a2fo_collective_award_continue

.macro SPECIAL_AWARD name, recipient, continuation
.globl \name
\name:
    pushfl
    pushal
    subl $108, %esp
    fnsave (%esp)
    movl 136(%esp), %edx
    movl \recipient(%esp), %eax
    pushl %edx
    pushl %eax
    call _a2fo_collective_class_award
    addl $8, %esp
    testl %eax, %eax
    jz 1f
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    /* Consume the two XP-address temporaries, just like native code. */
    popl %eax
    popl %eax
    jmp *\continuation
1:
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    flds 0xb4(%eax)
    popl %eax
    fadds (%eax)
    popl %eax
    fstps (%eax)
    fwait
    jmp *\continuation
.endm
SPECIAL_AWARD _a2fo_collective_sleeper_bridge, 112, _a2fo_collective_sleeper_continue
SPECIAL_AWARD _a2fo_collective_saboteur_bridge, 124, _a2fo_collective_saboteur_continue

.globl _a2fo_collective_level_bridge
_a2fo_collective_level_bridge:
    pushfl
    pushal
    subl $108, %esp
    fnsave (%esp)
    pushl 136(%esp)
    call _a2fo_collective_level
    addl $4, %esp
    testl %eax, %eax
    jz 2f
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    ret
2:
    frstor (%esp)
    addl $108, %esp
    popal
    popfl
    jmp *_a2fo_collective_level_gateway

.globl _a2fo_collective_delphi2
_a2fo_collective_delphi2:
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

.globl _a2fo_collective_thiscall1
_a2fo_collective_thiscall1:
    pushl %ebp
    movl %esp, %ebp
    movl 12(%ebp), %ecx
    pushl 16(%ebp)
    call *8(%ebp)
    leave
    ret
