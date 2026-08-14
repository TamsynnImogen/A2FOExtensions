	# ABI bridges for Armada's MSVC thiscall methods.

	.text

	.globl _a2fo_weapon_damage_call_thiscall_0
_a2fo_weapon_damage_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_weapon_damage_call_thiscall_2
_a2fo_weapon_damage_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_weapon_damage_call_thiscall_3
_a2fo_weapon_damage_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_weapon_damage_call_thiscall_4
_a2fo_weapon_damage_call_thiscall_4:
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

	# Craft::Damage internal bridge at Armada RVA 0x000c5f08. EBP is the
	# original Craft::Damage frame; [EBP+0x14] is its resolved hull/system
	# amount. Preserve caller-saved registers around the C helper, replay the
	# two overwritten instructions, then continue at RVA 0x000c5f12.
	.globl _a2fo_weapon_damage_hull_amount_bridge
_a2fo_weapon_damage_hull_amount_bridge:
	pushl %eax
	pushl %ecx
	pushl %edx
	leal 20(%ebp), %eax
	pushl %eax
	call _a2fo_weapon_damage_scale_hull_amount
	addl $4, %esp
	popl %edx
	popl %ecx
	popl %eax
	movl 4(%ebx), %eax
	movl $0, -4(%ebp)
	jmp *_a2fo_weapon_damage_hull_amount_continue
