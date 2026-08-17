	# ABI bridge for calls into Armada's no-argument MSVC thiscall methods.
	# C++ supplies ordinary cdecl arguments; the wrapper places self in ECX.

	.text

	.globl _a2fo_instant_action_settings_call_thiscall_0
_a2fo_instant_action_settings_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Call an MSVC thiscall method with two stack arguments. The callee owns
	# those eight argument bytes.
	.globl _a2fo_instant_action_settings_call_thiscall_2
_a2fo_instant_action_settings_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Inline-hook entry shim for FileReader(reader, path_string, options).
	# Preserve the original thiscall stack cleanup while adapting ECX.
	.extern _a2fo_instant_action_settings_file_reader_hook_cpp
	.globl _a2fo_instant_action_settings_file_reader_bridge
_a2fo_instant_action_settings_file_reader_bridge:
	movl %esp, %eax
	pushl 8(%eax)
	pushl 4(%eax)
	pushl %ecx
	call _a2fo_instant_action_settings_file_reader_hook_cpp
	addl $12, %esp
	ret $8

	# Fleet Operations' Advanced Settings reader uses Delphi's register ABI:
	# EAX=form and DL=whether default values should be shown.
	.globl _a2fo_instant_action_settings_call_delphi_2
_a2fo_instant_action_settings_call_delphi_2:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	# Inline-hook entry shim. Preserve Delphi's EAX/EDX arguments while
	# adapting them to the ordinary cdecl C++ diagnostics handler.
	.extern _a2fo_instant_action_settings_read_advanced_hook_cpp
	.globl _a2fo_instant_action_settings_read_advanced_bridge
_a2fo_instant_action_settings_read_advanced_bridge:
	pushl %edx
	pushl %eax
	call _a2fo_instant_action_settings_read_advanced_hook_cpp
	addl $8, %esp
	ret
