	# ABI bridge for A2FOCraftIdentity calls into Armada's MSVC thiscall UI and
	# class methods. Each wrapper is intentionally policy-free.

	.text

	.globl _a2fo_identity_call_thiscall_0
_a2fo_identity_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_identity_call_thiscall_1
_a2fo_identity_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_identity_call_thiscall_2
_a2fo_identity_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_identity_call_thiscall_3
_a2fo_identity_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_identity_call_thiscall_4
_a2fo_identity_call_thiscall_4:
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

	.globl _a2fo_identity_call_thiscall_7
_a2fo_identity_call_thiscall_7:
	pushl %ebp
	movl %esp, %ebp
	pushl 40(%ebp)
	pushl 36(%ebp)
	pushl 32(%ebp)
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Fleet Ops sprite helpers use Delphi's EAX/EDX/ECX convention and then
	# forward to Armada's ST3D sprite implementation.
	.globl _a2fo_identity_fo_sprite_set_colour
_a2fo_identity_fo_sprite_set_colour:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_identity_fo_sprite_draw_scaled_2d
_a2fo_identity_fo_sprite_draw_scaled_2d:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 24(%ebp)
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	# The six native SystemIcon colour-call sites retain the current icon in
	# ESI. Forward that context to C++ without detouring SystemIcon::Render,
	# whose entry Fleet Operations patches before A2FO modules are loaded.
	.extern _a2fo_identity_system_icon_set_colour_hook_cpp
	.globl _a2fo_identity_system_icon_set_colour_bridge
_a2fo_identity_system_icon_set_colour_bridge:
	pushl 4(%esp)
	pushl %esi
	pushl %ecx
	call _a2fo_identity_system_icon_set_colour_hook_cpp
	addl $12, %esp
	ret $4

	# SystemValue::Render likewise retains its component in ESI while issuing
	# the checked sprite-colour call for the adjacent numeric value.
	.extern _a2fo_identity_system_text_set_colour_hook_cpp
	.globl _a2fo_identity_system_text_set_colour_bridge
_a2fo_identity_system_text_set_colour_bridge:
	pushl 4(%esp)
	pushl %esi
	pushl %ecx
	call _a2fo_identity_system_text_set_colour_hook_cpp
	addl $12, %esp
	ret $4

	# GUIText::Render receives a temporary {colour, text} record as its fourth
	# DrawText argument.  The supported native value components retain their
	# live object in ESI at this call site.  Let C++ replace only the record's RGB
	# fields, restore the native renderer in ECX, then tail-chain the original
	# thiscall so its seven arguments and callee cleanup remain untouched.
	.extern _a2fo_identity_value_text_colour_hook_cpp
	.extern _a2fo_identity_value_text_draw_original
	.globl _a2fo_identity_value_text_draw_bridge
_a2fo_identity_value_text_draw_bridge:
	pushl %ecx
	pushl 20(%esp)
	pushl %esi
	call _a2fo_identity_value_text_colour_hook_cpp
	addl $8, %esp
	popl %ecx
	jmp *_a2fo_identity_value_text_draw_original
