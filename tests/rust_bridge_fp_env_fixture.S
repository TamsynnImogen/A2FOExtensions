/* A synthetic interrupted FP environment. The probe supplies owned globals;
 * the two private Rust environment operations must round-trip it exactly. */
.text
.globl _a2fo_refit_bridge
_a2fo_refit_bridge:
    pushfl
    pushal
    lock incl _a2fo_refit_parser_visits
    pushl $_rust_fp_requested_cw
    pushl $_rust_fp_saved_state
    call _a2fo_rs_fp_save_reset
    addl $8, %esp
    fxsave _rust_fp_reset_state
    pushl $_rust_fp_saved_state
    call _a2fo_rs_fp_restore
    addl $4, %esp
    popal
    popfl
    ret

#ifdef A2FO_FP_REFERENCE
/* Reference instructions from ScopedX87's original FXSAVE/FNINIT/FLDCW and
 * FXRSTOR sequence, with only the cdecl pointer-load boundary added. */
.globl _a2fo_rs_fp_save_reset
_a2fo_rs_fp_save_reset:
    movl 4(%esp), %eax
    fxsave (%eax)
    fninit
    movl 8(%esp), %eax
    fldcw (%eax)
    ret
.globl _a2fo_rs_fp_restore
_a2fo_rs_fp_restore:
    movl 4(%esp), %eax
    fxrstor (%eax)
    ret
#endif
