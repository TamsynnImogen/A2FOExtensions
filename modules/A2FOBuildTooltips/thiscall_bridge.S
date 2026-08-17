	# ABI bridges for A2FOBuildTooltips. Armada methods use MSVC thiscall;
	# module helpers use MinGW cdecl. Float-returning methods leave their value
	# in x87 ST(0), so that bridge returns the raw float bits in EAX.

	.text

	.globl _a2fo_build_tooltips_call_thiscall_0
_a2fo_build_tooltips_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_build_tooltips_call_thiscall_1
_a2fo_build_tooltips_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_build_tooltips_call_thiscall_float_1_bits
_a2fo_build_tooltips_call_thiscall_float_1_bits:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	subl $4, %esp
	fstps (%esp)
	popl %eax
	leave
	ret

	.globl _a2fo_build_tooltips_call_thiscall_float_0_bits
_a2fo_build_tooltips_call_thiscall_float_0_bits:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	subl $4, %esp
	fstps (%esp)
	popl %eax
	leave
	ret

	.extern _a2fo_build_tooltips_button_text_hook_cpp
	.globl _a2fo_build_tooltips_button_text_bridge
_a2fo_build_tooltips_button_text_bridge:
	pushl 4(%esp)
	pushl %ecx
	call _a2fo_build_tooltips_button_text_hook_cpp
	addl $8, %esp
	ret $4

	.extern _a2fo_build_tooltips_button_verbose_hook_cpp
	.globl _a2fo_build_tooltips_button_verbose_bridge
_a2fo_build_tooltips_button_verbose_bridge:
	pushl 4(%esp)
	pushl %ecx
	call _a2fo_build_tooltips_button_verbose_hook_cpp
	addl $8, %esp
	ret $4
