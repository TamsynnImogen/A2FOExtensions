	.text

	# ABI translation helpers for 32-bit code only.
	#
	# C++ calls every symbol below with ordinary cdecl stack arguments. The
	# wrapper moves them into whichever nonstandard ABI the target expects:
	#   Delphi register convention: EAX, EDX, ECX, then stack arguments.
	#   MSVC thiscall:              ECX=this, remaining arguments on stack.
	# The first C argument is always the target function address.

	.globl _a2fo_call_get_file_from_hash
_a2fo_call_get_file_from_hash:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	movl 20(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Delphi routines whose complete ABI is EAX and no stack arguments.
	.globl _a2fo_call_delphi_one_register
_a2fo_call_delphi_one_register:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# Delphi routines whose complete ABI is EAX, EDX and no stack arguments.
	# Used for TModificationSupport.getModUserDirectory and System.@LStrAsg.
	.globl _a2fo_call_delphi_two_registers
_a2fo_call_delphi_two_registers:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	# Armada gameplay methods use MSVC thiscall (ECX=self).
	.globl _a2fo_call_evolver_build_class
_a2fo_call_evolver_build_class:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_evolver_dtor
_a2fo_call_evolver_dtor:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_craft_explode
_a2fo_call_craft_explode:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_game_object_method
_a2fo_call_game_object_method:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_game_object_construct
_a2fo_call_game_object_construct:
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

	.globl _a2fo_call_game_object_argument
_a2fo_call_game_object_argument:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_parameter_db_get_string
_a2fo_parameter_db_get_string:
	pushl %ebp
	movl %esp, %ebp
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	movzbl %al, %eax
	leave
	ret

	.globl _a2fo_load_sod
_a2fo_load_sod:
	pushl %ebp
	movl %esp, %ebp
	pushl $0x3f800000
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Mid-function cocoon splice. Preserve Armada's live volatile registers,
	# ask C++ for the geometry, place the result in the EDI register expected by
	# the original continuation, then jump back immediately after the patch.
	.globl _a2fo_cocoon_selector_hook
_a2fo_cocoon_selector_hook:
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	call _a2fo_select_cocoon
	addl $4, %esp
	movl %eax, %edi
	popl %edx
	popl %ecx
	popl %eax
	jmp *_a2fo_cocoon_resume

	# Fleet Ops' second cocoon selector runs inside
	# Evolver::mUpdateConstructionEffect and expects the selected geometry in
	# EAX. Reuse the same policy/cache as construction start so a custom cocoon
	# never falls back to an unloaded default geometry during animation.
	.globl _a2fo_cocoon_update_selector_hook
_a2fo_cocoon_update_selector_hook:
	pushl %ecx
	pushl %edx
	pushl %ebx
	call _a2fo_select_cocoon
	addl $4, %esp
	popl %edx
	popl %ecx
	jmp *_a2fo_cocoon_update_resume
