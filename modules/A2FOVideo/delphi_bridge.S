	.text
	# cdecl(target, form, value) -> Delphi EAX=form, EDX=value.
	.globl _a2fo_video_set_client
_a2fo_video_set_client:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	popl %edi
	popl %esi
	popl %ebx
	leave
	ret
