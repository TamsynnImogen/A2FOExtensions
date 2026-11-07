	.text
	.globl _refit_mesh_fixture_call
_refit_mesh_fixture_call:
	pushl %ebp
	movl %esp,%ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	cmpl $0,8(%ebp)
	jne 1f
	movl $_a2fo_refit_mesh_fast_tap,_refit_mesh_fixture_target
	jmp 2f
1:	movl $_a2fo_refit_mesh_legacy_tap,_refit_mesh_fixture_target
2:	movl 12(%ebp),%ecx
	movl $0x11223344,%eax
	movl $0x55667788,%ebx
	movl $0x12345678,%edx
	movl $0x89abcdef,%esi
	movl $0x76543210,%edi
	.rept 8
	flds _refit_mesh_fixture_fp
	.endr
	fldcw _refit_mesh_fixture_cw_entry
	ldmxcsr _refit_mesh_fixture_mxcsr_entry
	movdqu _refit_mesh_fixture_xmm+0,%xmm0
	movdqu _refit_mesh_fixture_xmm+16,%xmm1
	movdqu _refit_mesh_fixture_xmm+32,%xmm2
	movdqu _refit_mesh_fixture_xmm+48,%xmm3
	movdqu _refit_mesh_fixture_xmm+64,%xmm4
	movdqu _refit_mesh_fixture_xmm+80,%xmm5
	movdqu _refit_mesh_fixture_xmm+96,%xmm6
	movdqu _refit_mesh_fixture_xmm+112,%xmm7
	movl %esp,_refit_mesh_fixture_stack_before
	movl %ebp,_refit_mesh_fixture_ebp_expected
	stc
	pushfl
	popl _refit_mesh_fixture_flags_expected
	fxsave _refit_mesh_fixture_fx_expected
	pushl 20(%ebp)
	pushl 16(%ebp)
	call *_refit_mesh_fixture_target
	movl %esp,_refit_mesh_fixture_stack_after
	movl %eax,_refit_mesh_fixture_gpr_return+0
	movl %ebx,_refit_mesh_fixture_gpr_return+4
	movl %ecx,_refit_mesh_fixture_gpr_return+8
	movl %edx,_refit_mesh_fixture_gpr_return+12
	movl %esi,_refit_mesh_fixture_gpr_return+16
	movl %edi,_refit_mesh_fixture_gpr_return+20
	movl %ebp,_refit_mesh_fixture_ebp_return
	pushfl
	popl _refit_mesh_fixture_flags_return_actual
	fxsave _refit_mesh_fixture_fx_return_actual
	fxrstor _refit_mesh_fixture_fx_expected
	.rept 8
	fstp %st(0)
	.endr
	popl %edi
	popl %esi
	popl %ebx
	leave
	movl _refit_mesh_fixture_gpr_return,%eax
	ret

	.globl _a2fo_refit_observe_mesh
_a2fo_refit_observe_mesh:
	fxsave _refit_mesh_fixture_fx_observer
	jmp _refit_mesh_fixture_observer_body
	.globl _a2fo_refit_mesh_finished
_a2fo_refit_mesh_finished:
	fxsave _refit_mesh_fixture_fx_post_observer
	jmp _refit_mesh_fixture_post_body

	.globl _refit_mesh_fixture_fast_original
_refit_mesh_fixture_fast_original:
	movl $0,_refit_mesh_fixture_original_route
	jmp _refit_mesh_fixture_original_common
	.globl _refit_mesh_fixture_legacy_original
_refit_mesh_fixture_legacy_original:
	movl $1,_refit_mesh_fixture_original_route
	jmp _refit_mesh_fixture_original_common
_refit_mesh_fixture_original_common:
	movl %eax,_refit_mesh_fixture_gpr_entry+0
	movl %ebx,_refit_mesh_fixture_gpr_entry+4
	movl %ecx,_refit_mesh_fixture_gpr_entry+8
	movl %edx,_refit_mesh_fixture_gpr_entry+12
	movl %esi,_refit_mesh_fixture_gpr_entry+16
	movl %edi,_refit_mesh_fixture_gpr_entry+20
	movl %ebp,_refit_mesh_fixture_ebp_entry
	pushfl
	popl _refit_mesh_fixture_flags_actual
	fxsave _refit_mesh_fixture_fx_actual
	movl _refit_mesh_fixture_observer_calls,%eax
	movl %eax,_refit_mesh_fixture_observations_at_original
	movl 4(%esp),%eax
	movl %eax,_refit_mesh_fixture_pass_actual
	movl 8(%esp),%eax
	movl %eax,_refit_mesh_fixture_material_actual
	incl _refit_mesh_fixture_original_calls
	incl _refit_mesh_fixture_events
	movl _refit_mesh_fixture_events,%eax
	movl %eax,_refit_mesh_fixture_original_event
	# A distinctive fake native return state must reach the assembly caller.
	fstp %st(0)
	flds _refit_mesh_fixture_fp_post
	fldcw _refit_mesh_fixture_cw_post
	ldmxcsr _refit_mesh_fixture_mxcsr_post
	movdqu _refit_mesh_fixture_xmm+16,%xmm0
	movl $0xaabbccdd,%eax
	movl $0x13579bdf,%ebx
	movl $0x2468ace0,%ecx
	movl $0xfedcba98,%edx
	movl $0x55aa55aa,%esi
	movl $0xaa55aa55,%edi
	clc
	pushfl
	popl _refit_mesh_fixture_flags_return_expected
	fxsave _refit_mesh_fixture_fx_return_expected
	ret $8

	.data
	.align 4
	.globl _refit_mesh_fixture_fp
_refit_mesh_fixture_fp:
	.long 0x3f800000
	.globl _refit_mesh_fixture_fp_post
_refit_mesh_fixture_fp_post:
	.long 0x40000000
	.globl _refit_mesh_fixture_cw_entry
_refit_mesh_fixture_cw_entry:
	.word 0x027f
	.globl _refit_mesh_fixture_cw_post
_refit_mesh_fixture_cw_post:
	.word 0x037f
	.align 4
	.globl _refit_mesh_fixture_mxcsr_entry
_refit_mesh_fixture_mxcsr_entry:
	.long 0x3f80
	.globl _refit_mesh_fixture_mxcsr_post
_refit_mesh_fixture_mxcsr_post:
	.long 0x1f80
