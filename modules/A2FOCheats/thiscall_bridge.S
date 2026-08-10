	.text

	# Armada's Team::AddCrewCapacity uses MSVC thiscall: Team is in ECX and
	# the four-byte float is callee-cleaned from the stack. Keep that ABI
	# boundary out of C++ so MinGW cannot lower it as an ordinary cdecl call.
	.globl _a2fo_cheats_call_thiscall_float
_a2fo_cheats_call_thiscall_float:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# GameTypeGeneral::Eliminate uses MSVC thiscall with one callee-cleaned
	# integer argument.
	.globl _a2fo_cheats_call_thiscall_int
_a2fo_cheats_call_thiscall_int:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# GameObject::QueueCommand(AiCommand, const Vector3&, long) uses ECX for
	# the object and removes its three stack arguments itself.
	.globl _a2fo_cheats_call_thiscall_command_vector_int
_a2fo_cheats_call_thiscall_command_vector_int:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Entity::GetTransform has no stack arguments and returns a pointer in EAX.
	.globl _a2fo_cheats_call_thiscall_pointer
_a2fo_cheats_call_thiscall_pointer:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret
