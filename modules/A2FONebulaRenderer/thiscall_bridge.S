	.text

	.globl _a2fo_nebula_module_call_thiscall_0
_a2fo_nebula_module_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_nebula_module_call_thiscall_1
_a2fo_nebula_module_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_nebula_module_call_thiscall_2
_a2fo_nebula_module_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	/*
	 * cdecl wrapper for Armada's four-argument thiscall ParameterDB reader.
	 * The callee owns its four stack arguments; ECX carries ParameterDB.
	 */
	.globl _a2fo_nebula_module_call_thiscall_4
_a2fo_nebula_module_call_thiscall_4:
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

	.globl _a2fo_nebula_module_call_thiscall_3
_a2fo_nebula_module_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
