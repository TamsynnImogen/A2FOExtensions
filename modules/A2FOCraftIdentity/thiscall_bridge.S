	# ABI bridge for A2FOCraftIdentity calls into Armada's MSVC thiscall UI and
	# class methods. Each wrapper is intentionally policy-free.

	.text

	# Craft::Save's first FileOutFloat call retains its Craft in EBX. Add it
	# as a fourth cdecl argument without taking ownership of Save's entry.
	.extern _a2fo_editor_write_first_field
	.globl _a2fo_editor_save_prefix_bridge
_a2fo_editor_save_prefix_bridge:
	pushl %ebx
	pushl 16(%esp)
	pushl 16(%esp)
	pushl 16(%esp)
	call _a2fo_editor_write_first_field
	addl $16, %esp
	ret

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

	# FleetOpsHook's technology-tree evaluator uses Delphi's register ABI:
	# EAX = team tree and EDX = weapon project ID.
	.globl _a2fo_identity_call_delphi_2
_a2fo_identity_call_delphi_2:
	pushl %ebp
	movl %esp, %ebp
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# The object inspector also uses Delphi's EAX/EDX register convention.
	.extern _a2fo_fo_editor_build_cpp
	.globl _a2fo_fo_editor_build_bridge
_a2fo_fo_editor_build_bridge:
	pushl %edx
	pushl %eax
	call _a2fo_fo_editor_build_cpp
	addl $8, %esp
	ret

	.extern _a2fo_fo_editor_ok_cpp
	.globl _a2fo_fo_editor_ok_bridge
_a2fo_fo_editor_ok_bridge:
	pushl %edx
	pushl %eax
	call _a2fo_fo_editor_ok_cpp
	addl $8, %esp
	ret

	.extern _a2fo_fo_editor_destroy_cpp
	.globl _a2fo_fo_editor_destroy_bridge
_a2fo_fo_editor_destroy_bridge:
	pushl %edx
	pushl %eax
	call _a2fo_fo_editor_destroy_cpp
	addl $8, %esp
	ret

	# Native AddProperty(form=EAX, type=DL, caption=ECX), with eight
	# callee-cleaned stack arguments. Descriptors own their inspector rows;
	# backing values remain in the extension's current dialog draft.
	.globl _a2fo_fo_editor_add_property
_a2fo_fo_editor_add_property:
	pushl %ebp
	movl %esp, %ebp
	pushl 32(%ebp)            # writable
	pushl $0                 # advanced-only
	pushl 28(%ebp)            # backing value
	pushl 24(%ebp)            # parent Craft node
	pushl $0                 # custom read callback
	pushl $0                 # custom write callback
	pushl $0                 # optional item class
	pushl $0                 # native text option
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	movl 20(%ebp), %ecx
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

	# Fleet Operations' Delphi ShipSystemIcon renderer passes its sprite in EAX
	# and colour in EDX. Its caller frame keeps the live ShipSystemIcon pointer
	# at [EBP-4], allowing weaponXiconpos to share the native system-state palette
	# without detouring the render entry used by A2FOFireArcs.
	.extern _a2fo_identity_weapon_icon_set_colour_hook_cpp
	.globl _a2fo_identity_weapon_icon_set_colour_bridge
_a2fo_identity_weapon_icon_set_colour_bridge:
	pushl %edx
	pushl -4(%ebp)
	pushl %eax
	call _a2fo_identity_weapon_icon_set_colour_hook_cpp
	addl $12, %esp
	ret

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
