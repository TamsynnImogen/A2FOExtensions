	.text

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
