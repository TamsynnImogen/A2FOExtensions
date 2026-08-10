	# ABI bridge for the normal-weapon technology filter. It adapts C++ calls to
	# Armada's thiscall owner lookup and FleetOpsHook's Delphi tree evaluator.

	.text

	# Generic wrapper for Weapon::GetOwner(), an MSVC thiscall function with
	# no stack arguments.
	.globl _a2fo_normal_weapon_tech_call_thiscall_0
_a2fo_normal_weapon_tech_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# FleetOpsHook's ordinary technology-tree evaluator uses Delphi's EAX/EDX
	# register ABI for its team-tree and project-ID arguments.
	.globl _a2fo_normal_weapon_tech_call_delphi_2
_a2fo_normal_weapon_tech_call_delphi_2:
	pushl %ebp
	movl %esp, %ebp
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret
