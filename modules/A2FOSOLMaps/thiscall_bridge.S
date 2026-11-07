	.text

	# cdecl helper for zero-argument MSVC thiscall methods.
	# args: function, this
	.globl _a2fo_sol_call_thiscall_0
_a2fo_sol_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# cdecl helper for one-argument MSVC thiscall methods.
	# args: function, this, argument1
	.globl _a2fo_sol_call_thiscall_1
_a2fo_sol_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# cdecl helper for four-argument MSVC thiscall methods.
	# args: function, this, argument1, argument2, argument3, argument4
	.globl _a2fo_sol_call_thiscall_4
_a2fo_sol_call_thiscall_4:
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

	# The two KnownMaps call sites expect AddMapsWithMask(thiscall) to pop its
	# one mask argument. Adapt that ABI to a normal C++ dispatcher while keeping
	# ECX (KnownMaps*) intact as an explicit argument.
	.globl _a2fo_sol_add_maps_with_mask_hook
_a2fo_sol_add_maps_with_mask_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_sol_add_maps_with_mask_dispatch
	addl $8, %esp
	leave
	ret $4


	# GameObject::IsAIControlled replacement entry.  Native thiscall supplies
	# the GameObject in ECX and no stack arguments.  Forward ECX to a cdecl
	# dispatcher which keeps native behaviour when AI exists and returns false
	# during SOL object-only bring-up while the AI singleton is still absent.
	.globl _a2fo_sol_is_ai_controlled_hook
_a2fo_sol_is_ai_controlled_hook:
	pushl %ecx
	call _A2FOSOLMaps_IsAiControlledSafe
	addl $4, %esp
	ret
