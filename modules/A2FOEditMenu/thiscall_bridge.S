	# ABI bridge for A2FOEditMenu's call to Armada's MSVC thiscall class lookup.

	.text

	.globl _a2fo_edit_menu_call_thiscall_0
_a2fo_edit_menu_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
