	.text
	.globl _native11_fixture_call_tap
_native11_fixture_call_tap:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl 8(%ebp), %ecx
	movl 12(%ebp), %esi
	movl 16(%ebp), %edi
	movl $0x11223344, %eax
	movl $0x55667788, %ebx
	movl $0x12345678, %edx
	# Seven occupied x87 slots leave one for the observer before FNINIT.
	.rept 7
	flds _native11_fixture_fp
	.endr
	movdqu _native11_fixture_xmm, %xmm0
	movl %esp, _native11_fixture_stack_before
	cmpl %eax, %eax
	pushfl
	popl _native11_fixture_flags_expected
	fxsave _native11_fixture_fx_expected
	pushl %edi
	call _a2fo_native11_entity_render_tap
	movl %esp, _native11_fixture_stack_after
	.rept 7
	fstp %st(0)
	.endr
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret

	# Fake original instance slot5: capture its exact entry ABI without first
	# calling C++ or changing the incoming flags/FPU state.
	.globl _native11_fixture_instance_render
_native11_fixture_instance_render:
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
	movl $0xaabbccdd, %eax
	ret $4
