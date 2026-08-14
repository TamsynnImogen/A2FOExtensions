	# ABI bridge for the core DLL's calls into 32-bit Armada and FleetOpsHook.
	# No gameplay policy belongs here; each wrapper only translates registers,
	# stack arguments, and callee-cleanup rules that MinGW C++ cannot express.

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

	# Delphi constructor entry: EAX=class, DL=allocation flag, ECX=owner.
	.globl _a2fo_call_delphi_constructor
_a2fo_call_delphi_constructor:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl $1, %edx
	movl 16(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# TControl.SetBounds uses EAX=self, EDX=left, ECX=top and two
	# callee-cleaned stack arguments (width, height).
	.globl _a2fo_call_delphi_set_bounds
_a2fo_call_delphi_set_bounds:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 28(%ebp)
	movl 20(%ebp), %ecx
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# Fleet Operations' Mods form uses Delphi event handlers. These entry
	# shims retain the EAX/EDX arguments before calling ordinary cdecl C++.
	.globl _a2fo_mod_settings_form_show_bridge
_a2fo_mod_settings_form_show_bridge:
	pushl %eax
	call _a2fo_mod_settings_form_show_hook_cpp
	addl $4, %esp
	ret

	.globl _a2fo_mod_settings_launch_bridge
_a2fo_mod_settings_launch_bridge:
	pushl %edx
	pushl %eax
	call _a2fo_mod_settings_launch_hook_cpp
	addl $8, %esp
	ret

	.globl _a2fo_module_menu_jet_button_click_bridge
_a2fo_module_menu_jet_button_click_bridge:
	pushl %eax
	call _a2fo_module_menu_jet_button_click_hook_cpp
	addl $4, %esp
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

	# ParameterDB's int, float, and bool getters share the same thiscall
	# shape: key, output pointer, and a four-byte default value. Keep the last
	# argument bit-exact so this bridge also carries IEEE-754 float defaults.
	.globl _a2fo_parameter_db_get_scalar
_a2fo_parameter_db_get_scalar:
	pushl %ebp
	movl %esp, %ebp
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
