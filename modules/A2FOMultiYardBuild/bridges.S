.text
.p2align 2
.globl _multi_yard_call_thiscall_0
_multi_yard_call_thiscall_0:
    movl 4(%esp), %eax
    movl 8(%esp), %ecx
    jmp *%eax
.globl _multi_yard_call_thiscall_2
_multi_yard_call_thiscall_2:
    movl 4(%esp), %eax
    movl 8(%esp), %ecx
    pushl 16(%esp)
    pushl 16(%esp)
    call *%eax
    ret

.globl _multi_yard_enable_stub
_multi_yard_enable_stub:
    pushal
    pushl 0x8(%ebp)
    call _multi_yard_build_enable
    addl $4, %esp
    movl %eax, 28(%esp)
    popal
    jmp *_multi_yard_enable_continue

.globl _multi_yard_menu_stub
_multi_yard_menu_stub:
    pushal
    pushl -0x4(%ebp)
    pushl 0x8(%ebp)
    call _multi_yard_menu_permit
    addl $8, %esp
    testl %eax, %eax
    popal
    movl 0x8(%ebp), %eax
    jne 1f
    jmp *_multi_yard_menu_reject
1:  jmp *_multi_yard_menu_open

.globl _multi_yard_route_stub
_multi_yard_route_stub:
    pushal
    pushl 0xc(%edi)
    pushl -0x18(%ebp)
    pushl -0x20(%ebp)
    call _multi_yard_build_route
    addl $12, %esp
    movl %eax, 28(%esp)
    popal
    testl %eax, %eax
    je 1f
    jmp *_multi_yard_route_continue
1:  movl -0x18(%ebp), %eax
    xorl %esi, %esi
    testl %eax, %eax
    jle 2f
    jmp *_multi_yard_route_loop
2:  jmp *_multi_yard_route_continue

.globl _multi_yard_cancel_stub
_multi_yard_cancel_stub:
    pushal
    leal -0x28(%ebp), %eax
    pushl %eax
    pushl %esi
    call _multi_yard_cancel_filter
    addl $8, %esp
    popal
    movl -0x20(%ebp), %eax
    movl $0, -0x14(%ebp)
    testl %eax, %eax
    jle 1f
    jmp *_multi_yard_cancel_continue
1:  jmp *_multi_yard_cancel_empty

.globl _multi_yard_qsize_stub
_multi_yard_qsize_stub:
    pushl %ecx
    leal -0x28(%ebp), %eax
    pushl %eax
    call _multi_yard_longest_queue
    addl $4, %esp
    popl %ecx
    cmpl $-1, %eax
    jne 1f
    jmp *_multi_yard_native_qsize
1:  ret

// These sites lie inside Fleet Ops' stdcall Select frame. Leave the entry
// hook used by A2FOSquadrons alone; bypass only the station-exclusive rules
// when every existing selection member and the candidate is the same yard.
.globl _multi_yard_select_first_stub
_multi_yard_select_first_stub:
    pushal
    pushl 0xc(%ebp)
    pushl 0x8(%ebp)
    call _multi_yard_selection_permit
    addl $8, %esp
    movl %eax, 28(%esp)
    popal
    testl %eax, %eax
    jne 1f
    movl 0x8(%ebp), %eax
    cmpl $1, 0xb8(%eax)
    jne 1f
    jmp *_multi_yard_select_one
1:  jmp *_multi_yard_select_many

.globl _multi_yard_select_compat_stub
_multi_yard_select_compat_stub:
    pushal
    pushl 0xc(%ebp)
    pushl 0x8(%ebp)
    call _multi_yard_selection_permit
    addl $8, %esp
    movl %eax, 28(%esp)
    popal
    testl %eax, %eax
    jne 1f
    cmpb $0, 0x1c(%ebp)
    je 1f
    jmp *_multi_yard_select_check
1:  jmp *_multi_yard_select_accept
