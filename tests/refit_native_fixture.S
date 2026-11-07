/* Private instruction-fixture runner. Saves the test caller's entire FP and
 * integer state. No game routine, window or input path is executed.
 */
.text
.globl _refit_run_fixture
_refit_run_fixture:
    pushfl
    pushal
    mov %esp, _refit_saved_sp
    sub $528, %esp
    and $-16, %esp
    mov %esp, _refit_saved_fx
    fxsave (%esp)
    fninit
    fldcw _refit_control_word
    fld1
    cmpl $0, _refit_pan_fixture
    je 1f
    flds _refit_parsed_value
1:
    mov _refit_capture, %eax
    fxsave 48(%eax)
    mov $0x11223344, %eax
    mov $0x22334455, %ecx
    mov $0x33445566, %edx
    mov $0x44556677, %ebx
    mov $0x55667788, %ebp
    mov $0x66778899, %esi
    mov $0x778899aa, %edi
    push $0x646
    popfl
    call *_refit_target
    pushfl
    pushal
    cld
    mov %esp, %esi
    mov _refit_capture, %edi
    mov $9, %ecx
    rep movsl
    mov _refit_capture, %eax
    fxsave 560(%eax)
    mov _refit_saved_fx, %esp
    fxrstor (%esp)
    mov _refit_saved_sp, %esp
    popal
    popfl
    ret
