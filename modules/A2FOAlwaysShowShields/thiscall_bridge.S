	# ABI bridge for A2FOAlwaysShowShields calls into Armada's MSVC thiscall
	# ParameterDB and Craft shield-effect methods.

	.text

	.globl _a2fo_shields_call_thiscall_0
_a2fo_shields_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_shields_call_thiscall_1
_a2fo_shields_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_shields_call_thiscall_4
_a2fo_shields_call_thiscall_4:
	pushl %ebp
	movl %esp, %ebp
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
