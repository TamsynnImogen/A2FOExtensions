/* Isolated cdecl call probe. Every bridge/callee is supplied by the fixture;
 * no engine address, window, or input path is used. Single-threaded only. */
.data
.balign 4
.globl _rust_bridge_before_sp, _rust_bridge_after_sp
.globl _rust_bridge_after_ebx, _rust_bridge_after_esi, _rust_bridge_after_edi
.globl _rust_bridge_after_ebp, _rust_bridge_expected_ebp
_rust_bridge_before_sp: .long 0
_rust_bridge_after_sp: .long 0
_rust_bridge_after_ebx: .long 0
_rust_bridge_after_esi: .long 0
_rust_bridge_after_edi: .long 0
_rust_bridge_after_ebp: .long 0
_rust_bridge_expected_ebp: .long 0
_rust_bridge_saved_sp: .long 0
.text
.globl _rust_bridge_call
_rust_bridge_call:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl %esp, _rust_bridge_saved_sp
    movl %ebp, _rust_bridge_expected_ebp
    movl 16(%ebp), %ecx
    movl 12(%ebp), %edx
    testl %ecx, %ecx
    jz 2f
1:
    decl %ecx
    pushl (%edx,%ecx,4)
    testl %ecx, %ecx
    jnz 1b
2:
    movl %esp, _rust_bridge_before_sp
    movl $0x11223344, %ebx
    movl $0x55667788, %esi
    movl $0x99aabbcc, %edi
    call *8(%ebp)
    movl %esp, _rust_bridge_after_sp
    movl %ebx, _rust_bridge_after_ebx
    movl %esi, _rust_bridge_after_esi
    movl %edi, _rust_bridge_after_edi
    movl %ebp, _rust_bridge_after_ebp
    movl _rust_bridge_saved_sp, %esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret

.macro delphi_target name, registers
.globl _\name
_\name:
    movl %eax, _rust_bridge_seen
    .if \registers >= 2
    movl %edx, _rust_bridge_seen+4
    .endif
    .if \registers >= 3
    movl %ecx, _rust_bridge_seen+8
    .endif
    movl $0xaabbccdd, %eax
    ret
.endm
delphi_target rust_bridge_delphi1, 1
delphi_target rust_bridge_delphi2, 2
delphi_target rust_bridge_delphi3, 3
delphi_target rust_bridge_constructor, 3
.globl _rust_bridge_bounds
_rust_bridge_bounds:
    movl %eax, _rust_bridge_seen
    movl %edx, _rust_bridge_seen+4
    movl %ecx, _rust_bridge_seen+8
    movl 4(%esp), %eax
    movl %eax, _rust_bridge_seen+12
    movl 8(%esp), %eax
    movl %eax, _rust_bridge_seen+16
    movl $0xaabbccdd, %eax
    ret $8
