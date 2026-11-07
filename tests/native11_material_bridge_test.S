	.text
	.globl _native11_fixture_call_tap
_native11_fixture_call_tap:
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
	# Fill all eight x87 registers and all eight XMM registers with known state.
	.rept 8
	flds _native11_fixture_fp
	.endr
	movdqu _native11_fixture_xmm+0, %xmm0
	movdqu _native11_fixture_xmm+16, %xmm1
	movdqu _native11_fixture_xmm+32, %xmm2
	movdqu _native11_fixture_xmm+48, %xmm3
	movdqu _native11_fixture_xmm+64, %xmm4
	movdqu _native11_fixture_xmm+80, %xmm5
	movdqu _native11_fixture_xmm+96, %xmm6
	movdqu _native11_fixture_xmm+112, %xmm7
	movl %esp, _native11_fixture_stack_before
	cmpl %eax, %eax
	pushfl
	popl _native11_fixture_flags_expected
	fxsave _native11_fixture_fx_expected
	pushl 12(%ebp)
	call _a2fo_native11_material_draw_tap
	movl %esp, _native11_fixture_stack_after
	# Capture the original return before compiler-generated code changes it.
	movl %eax, _native11_fixture_gpr_return+0
	movl %ebx, _native11_fixture_gpr_return+4
	movl %ecx, _native11_fixture_gpr_return+8
	movl %edx, _native11_fixture_gpr_return+12
	movl %esi, _native11_fixture_gpr_return+16
	movl %edi, _native11_fixture_gpr_return+20
	pushfl
	popl _native11_fixture_flags_return_actual
	fxsave _native11_fixture_fx_return_actual
	fxrstor _native11_fixture_fx_expected
	# Drop the eight known x87 values before returning to compiler-generated code.
	.rept 8
	fstp %st(0)
	.endr
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret

	# Controlled fake native material draw. Capture entry state before any code
	# changes flags or floating point state, then return with the native ret4 ABI.
	.globl _native11_fixture_original
_native11_fixture_original:
	movl %eax, _native11_fixture_gpr+0
	movl %ebx, _native11_fixture_gpr+4
	movl %ecx, _native11_fixture_gpr+8
	movl %edx, _native11_fixture_gpr+12
	movl %esi, _native11_fixture_gpr+16
	movl %edi, _native11_fixture_gpr+20
	pushfl
	popl _native11_fixture_flags_actual
	fxsave _native11_fixture_fx_actual
	movl 4(%esp), %eax
	movl %eax, _native11_fixture_camera_actual
	incl _native11_fixture_original_calls
	# Distinctive post-original state must survive the end callback unchanged.
	fstp %st(0)
	flds _native11_fixture_fp_post
	fldcw _native11_fixture_x87_cw_post
	ldmxcsr _native11_fixture_mxcsr_post
	movdqu _native11_fixture_xmm+16, %xmm0
	movl $0xaabbccdd, %eax
	movl $0x13579bdf, %ebx
	movl $0x2468ace0, %ecx
	movl $0xfedcba98, %edx
	movl $0x55aa55aa, %esi
	movl $0xaa55aa55, %edi
	stc
	pushfl
	popl _native11_fixture_flags_return_expected
	fxsave _native11_fixture_fx_return_expected
	ret $4

	.data
	.align 4
	.globl _native11_fixture_fp
_native11_fixture_fp:
	.long 0x3f800000
	.globl _native11_fixture_fp_post
_native11_fixture_fp_post:
	.long 0x40000000
	.globl _native11_fixture_x87_cw_post
_native11_fixture_x87_cw_post:
	.word 0x027f
	.align 4
	.globl _native11_fixture_mxcsr_post
_native11_fixture_mxcsr_post:
	.long 0x3f80
