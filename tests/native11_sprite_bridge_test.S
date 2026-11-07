    .text
    .macro seed_fp_and_xmm
    fninit
    fldcw _sprite_fixture_cw
    ldmxcsr _sprite_fixture_mxcsr
    flds _sprite_fixture_fp_seed+0
    flds _sprite_fixture_fp_seed+4
    flds _sprite_fixture_fp_seed+8
    flds _sprite_fixture_fp_seed+12
    flds _sprite_fixture_fp_seed+16
    flds _sprite_fixture_fp_seed+20
    flds _sprite_fixture_fp_seed+24
    flds _sprite_fixture_fp_seed+28
    movdqu _sprite_fixture_xmm_seed+0,%xmm0
    movdqu _sprite_fixture_xmm_seed+16,%xmm1
    movdqu _sprite_fixture_xmm_seed+32,%xmm2
    movdqu _sprite_fixture_xmm_seed+48,%xmm3
    movdqu _sprite_fixture_xmm_seed+64,%xmm4
    movdqu _sprite_fixture_xmm_seed+80,%xmm5
    movdqu _sprite_fixture_xmm_seed+96,%xmm6
    movdqu _sprite_fixture_xmm_seed+112,%xmm7
    cmpl %eax,%eax
    stc
    std
    .endm

    .macro capture_result
    movl %esp,_sprite_fixture_stack_after
    movl %eax,_sprite_fixture_result_gpr+0
    movl %ebx,_sprite_fixture_result_gpr+4
    movl %ecx,_sprite_fixture_result_gpr+8
    movl %edx,_sprite_fixture_result_gpr+12
    movl %esi,_sprite_fixture_result_gpr+16
    movl %edi,_sprite_fixture_result_gpr+20
    movl %ebp,_sprite_fixture_result_gpr+24
    fxsave _sprite_fixture_result_fx
    cld
    fninit
    ldmxcsr _sprite_fixture_clean_mxcsr
    movl (%esp),%ebp
    addl $4,%esp
    popl %edi
    popl %esi
    popl %ebx
    leave
    ret
    .endm

    .globl _sprite_fixture_run
_sprite_fixture_run:
    pushl %ebp
    movl %esp,%ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    subl $4,%esp
    movl %ebp,(%esp)
    movl 8(%ebp),%ecx
    movl 12(%ebp),%edx
    movl $0x11111111,%eax
    movl $0x22222222,%ebx
    movl $0x55555555,%esi
    movl $0x66666666,%edi
    movl $0x24681357,%ebp
    seed_fp_and_xmm
    movl %esp,_sprite_fixture_stack_before
    pushl _sprite_fixture_args+24
    pushl _sprite_fixture_args+20
    pushl _sprite_fixture_args+16
    pushl _sprite_fixture_args+12
    pushl _sprite_fixture_args+8
    pushl _sprite_fixture_args+4
    pushl _sprite_fixture_args+0
    pushfl
    popl _sprite_fixture_expected_flags
    fxsave _sprite_fixture_expected_fx
    call _a2fo_native11_sprite_dispatch_tap
    capture_result

    .macro run_end name,tap
    .globl \name
\name:
    pushl %ebp
    movl %esp,%ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    subl $4,%esp
    movl %ebp,(%esp)
    movl 8(%ebp),%esi
    movl 12(%ebp),%ecx
    movl $0x11111111,%eax
    movl $0x22222222,%ebx
    movl $0x44444444,%edx
    movl $0x66666666,%edi
    movl $0x24681357,%ebp
    seed_fp_and_xmm
    movl %esp,_sprite_fixture_stack_before
    pushl _end_sprite_fixture_comthis
    pushfl
    popl _sprite_fixture_expected_flags
    fxsave _sprite_fixture_expected_fx
    call \tap
    capture_result
    .endm

    run_end _end_sprite_fixture_run,_a2fo_native11_sprite_end_scene_tap
    run_end _fleet_end_sprite_fixture_run,_a2fo_native11_sprite_fleet_end_scene_tap

    .macro record_original
    movl %esp,_sprite_fixture_original_stack
    movl %eax,_sprite_fixture_original_gpr+0
    movl %ebx,_sprite_fixture_original_gpr+4
    movl %ecx,_sprite_fixture_original_gpr+8
    movl %edx,_sprite_fixture_original_gpr+12
    movl %esi,_sprite_fixture_original_gpr+16
    movl %edi,_sprite_fixture_original_gpr+20
    movl %ebp,_sprite_fixture_original_gpr+24
    pushfl
    popl _sprite_fixture_original_flags
    fxsave _sprite_fixture_original_fx
    .endm

    .globl _sprite_fixture_original
_sprite_fixture_original:
    record_original
    movl 4(%esp),%eax
    movl %eax,_sprite_fixture_original_args+0
    movl 8(%esp),%eax
    movl %eax,_sprite_fixture_original_args+4
    movl 12(%esp),%eax
    movl %eax,_sprite_fixture_original_args+8
    movl 16(%esp),%eax
    movl %eax,_sprite_fixture_original_args+12
    movl 20(%esp),%eax
    movl %eax,_sprite_fixture_original_args+16
    movl 24(%esp),%eax
    movl %eax,_sprite_fixture_original_args+20
    movl 28(%esp),%eax
    movl %eax,_sprite_fixture_original_args+24
    movl _sprite_fixture_observer_calls,%eax
    movl %eax,_sprite_fixture_original_saw_observer
    incl _sprite_fixture_original_calls
    fninit
    flds _sprite_fixture_fp_sprite
    movdqu _sprite_fixture_xmm_result,%xmm0
    ldmxcsr _sprite_fixture_result_mxcsr
    movl $0xa0a0a0a0,%eax
    movl $0xc0c0c0c0,%ecx
    ret $28

    .globl _end_sprite_fixture_original
_end_sprite_fixture_original:
    record_original
    movl 4(%esp),%eax
    movl %eax,_sprite_fixture_original_args+0
    movl _end_sprite_fixture_observer_calls,%eax
    movl %eax,_sprite_fixture_original_saw_observer
    incl _sprite_fixture_original_calls
    fninit
    flds _sprite_fixture_fp_end
    movdqu _sprite_fixture_xmm_result,%xmm0
    ldmxcsr _sprite_fixture_result_mxcsr
    movl $0xb0b0b0b0,%eax
    movl $0xd0d0d0d0,%ecx
    ret $4

    .globl _fleet_end_sprite_fixture_original
_fleet_end_sprite_fixture_original:
    record_original
    movl 4(%esp),%eax
    movl %eax,_sprite_fixture_original_args+0
    movl _end_sprite_fixture_observer_calls,%eax
    movl %eax,_sprite_fixture_original_saw_observer
    incl _sprite_fixture_original_calls
    fninit
    flds _sprite_fixture_fp_fleet
    movdqu _sprite_fixture_xmm_result,%xmm0
    ldmxcsr _sprite_fixture_result_mxcsr
    movl $0xe0e0e0e0,%eax
    movl $0xf0f0f0f0,%ecx
    ret $4
