	# ABI bridge for A1Fallbacks calls into Armada's MSVC thiscall methods.

	.text

	.globl _a1_fallbacks_call_thiscall_1
_a1_fallbacks_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a1_fallbacks_call_thiscall_2
_a1_fallbacks_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
