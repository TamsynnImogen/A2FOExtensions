	# ABI bridge for no-argument Armada MSVC thiscall methods.

	.text

	.globl _a2fo_directional_shields_call_thiscall_0
_a2fo_directional_shields_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
