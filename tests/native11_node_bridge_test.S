	.text
	.globl _node_fixture_run
_node_fixture_run:
	pushl %ebp
	movl %esp,%ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	subl $16,%esp
	movl %ebp,(%esp)
	movl 8(%ebp),%eax
	movl %eax,4(%esp)
	movl 12(%ebp),%eax
	movl %eax,8(%esp)
	movl 16(%ebp),%eax
	movl %eax,12(%esp)
	# Select a synthetic native frame with camera at EBP+8.
	movl $0x24681357,_node_fixture_native_frame+8
	leal _node_fixture_native_frame,%ebp
	movl $0x11223344,%eax
	movl $0x55667788,%ebx
	movl 12(%esp),%ecx
	movl $0x12345678,%edx
	movl 8(%esp),%esi
	movl $0x76543210,%edi
	.rept 8
	flds _node_fixture_fp
	.endr
	movdqu _node_fixture_xmm+0,%xmm0
	movdqu _node_fixture_xmm+16,%xmm1
	movdqu _node_fixture_xmm+32,%xmm2
	movdqu _node_fixture_xmm+48,%xmm3
	movdqu _node_fixture_xmm+64,%xmm4
	movdqu _node_fixture_xmm+80,%xmm5
	movdqu _node_fixture_xmm+96,%xmm6
	movdqu _node_fixture_xmm+112,%xmm7
	# Derive expected post-ADD status flags from the actual instance address.
	addl $0x44,%esi
	pushfl
	popl _node_fixture_flags_expected
	subl $0x44,%esi
	movl %esp,_node_fixture_stack_before
	fxsave _node_fixture_fx_expected
	cmpl %eax,%eax
	cmpl $0,4(%esp)
	jne 1f
	call _a2fo_native11_node_branch_a
	jmp 2f
1:
	call _a2fo_native11_node_branch_b
2:
	movl %esp,_node_fixture_stack_after
	movl %ebp,_node_fixture_ebp_actual
	movl %eax,_node_fixture_gpr_actual+0
	movl %ebx,_node_fixture_gpr_actual+4
	movl %ecx,_node_fixture_gpr_actual+8
	movl %edx,_node_fixture_gpr_actual+12
	movl %esi,_node_fixture_gpr_actual+16
	movl %edi,_node_fixture_gpr_actual+20
	pushfl
	popl _node_fixture_flags_actual
	fxsave _node_fixture_fx_actual
	fninit
	movl (%esp),%ebp
	addl $16,%esp
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret

	.data
	.align 16
	.globl _node_fixture_native_frame
_node_fixture_native_frame:
	.long 0,0,0
	.globl _node_fixture_fp
_node_fixture_fp:
	.long 0x3f800000
