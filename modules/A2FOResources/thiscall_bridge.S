	.text

	.globl _a2fo_resources_call_thiscall_0
_a2fo_resources_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_resources_call_thiscall_1
_a2fo_resources_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_resources_call_thiscall_3
_a2fo_resources_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_resources_call_thiscall_7
_a2fo_resources_call_thiscall_7:
	pushl %ebp
	movl %esp, %ebp
	pushl 40(%ebp)
	pushl 36(%ebp)
	pushl 32(%ebp)
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Fleet Operations calls Producer::mDeductResourcesForBuild with its
	# Delphi register ABI (EAX=producer, EDX=target class). Check the four
	# sidecar balances first, let the native six-resource transaction run, and
	# commit the four extra costs only when the native transaction succeeds.
	.globl _a2fo_resources_deduct_hook
_a2fo_resources_deduct_hook:
	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp
	movl %eax, -4(%ebp)
	movl %edx, -8(%ebp)
	pushl %edx
	pushl %eax
	call _a2fo_resources_can_pay
	addl $8, %esp
	testb %al, %al
	jz 1f
	movl -4(%ebp), %eax
	movl -8(%ebp), %edx
	call *_g_a2fo_resources_deduct_gateway
	testb %al, %al
	jz 1f
	pushl -8(%ebp)
	pushl -4(%ebp)
	call _a2fo_resources_commit_payment
	addl $8, %esp
	movb $1, %al
	leave
	ret
1:
	xorl %eax, %eax
	leave
	ret
