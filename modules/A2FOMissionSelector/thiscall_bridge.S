	.text

	# Invoke a no-argument MSVC thiscall target. The C wrapper supplies the
	# target and native object as ordinary cdecl arguments; the engine expects
	# its object in ECX.
	.globl _a2fo_mission_selector_call_thiscall_0
_a2fo_mission_selector_call_thiscall_0:
	movl 4(%esp), %eax
	movl 8(%esp), %ecx
	jmp *%eax
