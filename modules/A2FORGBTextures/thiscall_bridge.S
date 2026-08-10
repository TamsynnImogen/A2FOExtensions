	# ABI bridge for legacy texture calls into Armada's MSVC thiscall methods.
	# File-routing and conversion policy remains in module.cpp.

	.text

	# Armada's texture methods use MSVC thiscall: self is in ECX and the
	# callee removes its stack arguments. These cdecl wrappers give the C++
	# module an explicit, compiler-independent ABI boundary.
	.globl _a2fo_rgb_call_thiscall_0
_a2fo_rgb_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_rgb_call_thiscall_1
_a2fo_rgb_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_rgb_call_thiscall_2
_a2fo_rgb_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_rgb_call_thiscall_4
_a2fo_rgb_call_thiscall_4:
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
