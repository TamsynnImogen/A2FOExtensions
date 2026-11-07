	.text
	.globl _asset_fixture_run
_asset_fixture_run:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	fninit
	movl %esp, _asset_stack_before
	movl $0x11223344, %ebx
	movl $0x55667788, %esi
	movl $0x99aabbcc, %edi
	movl %ebp, _asset_caller_frame
	movl $0x10203040, %ecx
	movl $0x13579bdf, %edx
	movl $0x3f800000, %eax
	cmpl $1,8(%ebp)
	jne 1f
	call _a2fo_native11_texture_parameters_tap
	jmp 2f
1:
	call _a2fo_native11_texture_pixels_tap
2:
	movl %esp, _asset_stack_after
	movl %eax, _asset_gpr_actual
	movl %ecx, _asset_gpr_actual+4
	movl %edx, _asset_gpr_actual+8
	movl %ebx, _asset_gpr_actual+12
	movl %esi, _asset_gpr_actual+16
	movl %edi, _asset_gpr_actual+20
	pushfl
	popl _asset_flags_actual
	fxsave _asset_fx_actual
	cld
	fninit
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret

	.globl _asset_fixture_original
_asset_fixture_original:
	movl %ecx, _asset_input
	movl %edx, _asset_input+4
	movl %eax, _asset_input+8
	incl _asset_original_calls
	# Fill every x87 slot. The observer must receive a fresh environment.
	.rept 8
	fld1
	.endr
	movdqu _asset_xmm, %xmm0
	movdqu _asset_xmm, %xmm1
	movdqu _asset_xmm, %xmm2
	movdqu _asset_xmm, %xmm3
	movdqu _asset_xmm, %xmm4
	movdqu _asset_xmm, %xmm5
	movdqu _asset_xmm, %xmm6
	movdqu _asset_xmm, %xmm7
	movl $0x2468ace0, %eax
	movl $0xaabbccdd, %ecx
	movl $0xdeadbeef, %edx
	cmpl %eax, %eax
	std
	pushfl
	popl _asset_flags_expected
	fxsave _asset_fx_expected
	ret
