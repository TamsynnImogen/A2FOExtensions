	# Small ABI bridges between MinGW cdecl helpers and Armada's MSVC
	# thiscall methods. The function address is always the first cdecl argument.

	.text

	.globl _a2fo_animated_hardpoints_call_thiscall_0
_a2fo_animated_hardpoints_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_animated_hardpoints_call_thiscall_1
_a2fo_animated_hardpoints_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	pushl 16(%ebp)
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_animated_hardpoints_call_thiscall_2
_a2fo_animated_hardpoints_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	pushl 20(%ebp)
	pushl 16(%ebp)
	call *8(%ebp)
	leave
	ret
