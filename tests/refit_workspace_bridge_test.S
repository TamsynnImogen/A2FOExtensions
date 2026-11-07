	.text
	.globl _refit_fixture_call_tap
_refit_fixture_call_tap:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl 8(%ebp), %ecx
	movl $0x11223344, %eax
	movl $0x55667788, %ebx
	movl $0x12345678, %edx
	movl $0x89abcdef, %esi
	movl $0x76543210, %edi
	.rept 8
	flds _refit_fixture_fp
	.endr
	movdqu _refit_fixture_xmm+0, %xmm0
	movdqu _refit_fixture_xmm+16, %xmm1
	movdqu _refit_fixture_xmm+32, %xmm2
	movdqu _refit_fixture_xmm+48, %xmm3
	movdqu _refit_fixture_xmm+64, %xmm4
	movdqu _refit_fixture_xmm+80, %xmm5
	movdqu _refit_fixture_xmm+96, %xmm6
	movdqu _refit_fixture_xmm+112, %xmm7
	movl %esp, _refit_fixture_stack_before
	cmpl %eax, %eax
	pushfl
	popl _refit_fixture_flags_expected
	fxsave _refit_fixture_fx_expected
	# thiscall ret8: push argument 2 then argument 1; ECX is this.
	pushl 16(%ebp)
	pushl 12(%ebp)
	call _a2fo_refit_workspace_ctor_tap
	movl %esp, _refit_fixture_stack_after
	# Restore clean caller FP state before returning to compiler-generated code.
	fxrstor _refit_fixture_fx_expected
	.rept 8
	fstp %st(0)
	.endr
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret

	.globl _refit_fixture_original
_refit_fixture_original:
	pushl %eax
	movl _refit_fixture_observations, %eax
	movl %eax, _refit_fixture_observations_at_original
	popl %eax
	movl %eax, _refit_fixture_gpr+0
	movl %ebx, _refit_fixture_gpr+4
	movl %ecx, _refit_fixture_gpr+8
	movl %edx, _refit_fixture_gpr+12
	movl %esi, _refit_fixture_gpr+16
	movl %edi, _refit_fixture_gpr+20
	pushfl
	popl _refit_fixture_flags_actual
	fxsave _refit_fixture_fx_actual
	movl 4(%esp), %eax
	movl %eax, _refit_fixture_arg1
	movl 8(%esp), %eax
	movl %eax, _refit_fixture_arg2
	incl _refit_fixture_calls
	movl $0xaabbccdd, %eax
	ret $8

	.data
	.align 4
	.globl _refit_fixture_fp
_refit_fixture_fp:
	.long 0x3f800000
