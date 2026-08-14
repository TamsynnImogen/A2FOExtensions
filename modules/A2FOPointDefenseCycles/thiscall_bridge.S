	# ABI helpers for calling Armada's MSVC thiscall methods and for replacing
	# Fleet Operations' inline OrdnanceDefenseWeapon delay assignment.

	.text

	.globl _a2fo_point_defense_call_thiscall_0
_a2fo_point_defense_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_point_defense_call_thiscall_1
_a2fo_point_defense_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_point_defense_call_thiscall_3
_a2fo_point_defense_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_point_defense_call_thiscall_4
_a2fo_point_defense_call_thiscall_4:
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

	.globl _a2fo_point_defense_odw_delay_bridge
_a2fo_point_defense_odw_delay_bridge:
	# The replaced Fleet Ops continuation has the original Simulate frame in
	# EBP. Preserve its contract: EAX=weapon and EDX=candidate ordnance at the
	# mWillShotHit wrapper reached immediately afterwards.
	movl -4(%ebp), %eax
	pushl %eax
	call _a2fo_point_defense_select_odw_delay
	addl $4, %esp
	movl -4(%ebp), %ecx
	movl %eax, 0x28(%ecx)
	movl -0x18(%ebp), %edx
	movl %ecx, %eax
	jmp *_a2fo_point_defense_odw_continue

	.globl _a2fo_point_defense_pdl_timer_gate_bridge
_a2fo_point_defense_pdl_timer_gate_bridge:
	# Replace ArmadaL's late PointDefenseLaser timer code cave. The original
	# TEST result is still in AL; inactive weapons leave through the native
	# epilogue. Active weapons advance their timer before target search and only
	# enter the firing path once it is ready.
	testb %al, %al
	jz 1f
	pushl 8(%ebp)
	pushl -4(%ebp)
	call _a2fo_point_defense_advance_pdl_timer
	addl $8, %esp
	testl %eax, %eax
	jz 1f
	jmp *_a2fo_point_defense_pdl_timer_ready
1:
	jmp *_a2fo_point_defense_pdl_timer_blocked
