	.text
	# CALL replaces the whole six-byte indirect call; its trailing NOP remains
	# on the return path. Original args/return stay on the native caller stack.
	.macro save_sprite_state
	pushfl
	pushal
	movl %esp,%esi
	leal -544(%esp),%esp
	andl $-16,%esp
	fxsave (%esp)
	fninit
	cld
	movl $0x1f80,512(%esp)
	ldmxcsr 512(%esp)
	.endm
	.macro restore_sprite_state
	fxrstor (%esp)
	movl %esi,%esp
	popal
	popfl
	.endm
	.globl _a2fo_native11_sprite_dispatch_tap
_a2fo_native11_sprite_dispatch_tap:
	save_sprite_state
	movzwl (%esp),%eax
	orl $0x10000,%eax
	leal 40(%esi),%edx
	pushl 20(%esi)
	pushl %eax
	pushl %edx
	pushl 24(%esi)
	call _a2fo_native11_observe_sprite
	addl $16,%esp
	restore_sprite_state
	jmp *0xc4(%edx)
	.globl _a2fo_native11_sprite_end_scene_tap
_a2fo_native11_sprite_end_scene_tap:
	save_sprite_state
	pushl $0
	pushl 24(%esi)
	pushl 40(%esi)
	pushl 4(%esi)
	call _a2fo_native11_end_sprite_ui
	addl $16,%esp
	restore_sprite_state
	jmp *0x8c(%ecx)
	# The qualified FleetOpsHook path is a direct stdcall callback. Chain it
	# instead of treating the COM table as the original CALL target.
	.globl _a2fo_native11_sprite_fleet_end_scene_tap
_a2fo_native11_sprite_fleet_end_scene_tap:
	save_sprite_state
	pushl $0
	pushl 24(%esi)
	pushl 40(%esi)
	pushl 4(%esi)
	call _a2fo_native11_end_sprite_ui
	addl $16,%esp
	restore_sprite_state
	jmp *_a2fo_native11_sprite_end_scene_original
