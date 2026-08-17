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
