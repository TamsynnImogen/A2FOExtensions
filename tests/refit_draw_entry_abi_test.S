	.text
	.globl _refit_probe_draw
_refit_probe_draw:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl 8(%ebp), %ecx
	movl (%ecx), %edx
	movl 12(%ebp), %eax
	movl (%edx,%eax,4), %edx
	pushl %ebp
	movl %esp, _refit_probe_before
	movl $0x11223344, %ebx
	movl $0x55667788, %esi
	movl $0x12345678, %edi
	movl $0x89abcdef, %ebp
	cmpl $70, %eax
	je 1f
	cmpl $71, %eax
	je 2f
	cmpl $72, %eax
	je 3f
	cmpl $73, %eax
	je 4f
	jmp 9f
1:	movl $5f, _refit_probe_expected_caller
	pushl $0
	pushl $0
	pushl $4
	pushl %ecx
	call *%edx
5:	jmp 8f
2:	movl $6f, _refit_probe_expected_caller
	pushl $0
	pushl $0
	pushl $0
	pushl $0
	pushl $4
	pushl %ecx
	call *%edx
6:	jmp 8f
3:	movl $7f, _refit_probe_expected_caller
	pushl $36
	pushl $0
	pushl $0
	pushl $4
	pushl %ecx
	call *%edx
7:	jmp 8f
4:	movl $10f, _refit_probe_expected_caller
	pushl $36
	pushl $0
	pushl $101
	pushl $0
	pushl $0
	pushl $0
	pushl $0
	pushl $4
	pushl %ecx
	call *%edx
10:	jmp 8f
8:	movl %eax, _refit_probe_result
9:	movl %esp, _refit_probe_after
	movl %ebx, _refit_probe_ebx
	movl %esi, _refit_probe_esi
	movl %edi, _refit_probe_edi
	movl %ebp, _refit_probe_ebp
	popl %ebp
	popl %edi
	popl %esi
	popl %ebx
	leave
	movl _refit_probe_result, %eax
	ret
