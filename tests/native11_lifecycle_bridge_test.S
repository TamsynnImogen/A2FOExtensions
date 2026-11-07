    .text
    .macro run_fixture name,tap
    .globl \name
\name:
    pushl %ebp
    movl %esp,%ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    subl $4,%esp
    movl %ebp,(%esp)
    movl 8(%ebp),%ecx
    movl $0x11111111,%eax
    movl $0x22222222,%ebx
    movl $0x44444444,%edx
    movl $0x55555555,%esi
    movl $0x66666666,%edi
    movl $0x24681357,%ebp
    fninit
    fldcw _lifecycle_input_cw
    ldmxcsr _lifecycle_input_mxcsr
    flds _lifecycle_fp_seed+0
    flds _lifecycle_fp_seed+4
    flds _lifecycle_fp_seed+8
    flds _lifecycle_fp_seed+12
    flds _lifecycle_fp_seed+16
    flds _lifecycle_fp_seed+20
    flds _lifecycle_fp_seed+24
    flds _lifecycle_fp_seed+28
    movdqu _lifecycle_xmm_seed+0,%xmm0
    movdqu _lifecycle_xmm_seed+16,%xmm1
    movdqu _lifecycle_xmm_seed+32,%xmm2
    movdqu _lifecycle_xmm_seed+48,%xmm3
    movdqu _lifecycle_xmm_seed+64,%xmm4
    movdqu _lifecycle_xmm_seed+80,%xmm5
    movdqu _lifecycle_xmm_seed+96,%xmm6
    movdqu _lifecycle_xmm_seed+112,%xmm7
    cmpl %eax,%eax
    stc
    std
    movl %esp,_lifecycle_stack_before
    pushfl
    popl _lifecycle_flags_expected
    fxsave _lifecycle_fx_expected
    call \tap
    movl %esp,_lifecycle_stack_after
    movl %eax,_lifecycle_result_gpr+0
    movl %ebx,_lifecycle_result_gpr+4
    movl %ecx,_lifecycle_result_gpr+8
    movl %edx,_lifecycle_result_gpr+12
    movl %esi,_lifecycle_result_gpr+16
    movl %edi,_lifecycle_result_gpr+20
    movl %ebp,_lifecycle_result_gpr+24
    fxsave _lifecycle_fx_result
    # Restore a clean C++ caller environment after recording native effects.
    cld
    fninit
    ldmxcsr _lifecycle_callback_clobber_mxcsr
    movl (%esp),%ebp
    addl $4,%esp
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret
    .endm

    .macro original name,fp,result_eax,result_ecx
    .globl \name
\name:
    movl %esp,_lifecycle_original_stack
    movl %eax,_lifecycle_original_gpr+0
    movl %ebx,_lifecycle_original_gpr+4
    movl %ecx,_lifecycle_original_gpr+8
    movl %edx,_lifecycle_original_gpr+12
    movl %esi,_lifecycle_original_gpr+16
    movl %edi,_lifecycle_original_gpr+20
    movl %ebp,_lifecycle_original_gpr+24
    pushfl
    popl _lifecycle_flags_original
    fxsave _lifecycle_fx_original
    movl _lifecycle_observer_calls,%eax
    movl %eax,_lifecycle_original_saw_observer
    incl _lifecycle_original_calls
    fninit
    flds \fp
    movdqu _lifecycle_xmm_output,%xmm0
    ldmxcsr _lifecycle_output_mxcsr
    movl $\result_eax,%eax
    movl $\result_ecx,%ecx
    ret
    .endm

    run_fixture _lifecycle_fixture_run_model,_a2fo_native11_model_destroy_tap
    run_fixture _lifecycle_fixture_run_texture,_a2fo_native11_texture_destroy_tap
    original _lifecycle_fixture_model_original,_lifecycle_fp_model,0xa0a0a0a0,0xc0c0c0c0
    original _lifecycle_fixture_texture_original,_lifecycle_fp_texture,0xb0b0b0b0,0xd0d0d0d0
