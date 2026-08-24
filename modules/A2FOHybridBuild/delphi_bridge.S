	# ABI bridge for A2FOHybridBuild calls into Armada and FleetOpsHook. Generic
	# wrappers adapt cdecl C++ callers to MSVC thiscall or Delphi registers.

	.text

	# A2FOHybridBuild.dll ABI translations. Generic wrappers adapt C++ cdecl
	# calls to Armada's MSVC thiscall methods.
	.globl _a2fo_call_thiscall_0
_a2fo_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_thiscall_1
_a2fo_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_thiscall_2
_a2fo_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_thiscall_3
_a2fo_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_thiscall_4
_a2fo_call_thiscall_4:
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

	# Fleet Ops' Craft render callback calls Armada's actual base hull renderer
	# before adding shields, reflections, and special-effect overlays. Bracket
	# only that call with the mapped-lighting craft context. The former
	# whole-callback scope allowed those later passes to replay ship emissives
	# into the selective bloom mask at an alternate transform.
	.extern _a2fo_hybrid_nebula_base_render_begin
	.extern _a2fo_hybrid_nebula_base_render_end
	.extern _a2fo_hybrid_nebula_base_render_target
	.extern _a2fo_hybrid_nebula_base_render_return

	.globl _a2fo_hybrid_nebula_base_render_dispatch
_a2fo_hybrid_nebula_base_render_dispatch:
	pushfl
	pushal
	pushl 24(%esp)
	call _a2fo_hybrid_nebula_base_render_begin
	addl $4, %esp
	popal
	popfl
	# Displaced `push esi; call [Fleet Ops import]`.
	pushl %esi
	call *_a2fo_hybrid_nebula_base_render_target
	# End the narrow context while retaining the original call's complete
	# register/flag result. EAX and EBX are then overwritten by the two displaced
	# instructions which originally followed that call.
	pushfl
	pushal
	pushl -4(%ebp)
	call _a2fo_hybrid_nebula_base_render_end
	addl $4, %esp
	popal
	popfl
	movl -4(%ebp), %eax
	movl 0x1a4(%eax), %ebx
	jmp *_a2fo_hybrid_nebula_base_render_return

	# PopupPalette::UpdateButtons converges on one compaction/layout pass after
	# binding every menu control. Refit controls must be present before that
	# pass, otherwise they overlay stale root positions and migrate on the next
	# refresh. This mid-function boundary supplies the live popup/selection
	# frame to C++, then reproduces the two displaced register initializers.
	.extern _a2fo_hybrid_refit_pre_layout
	.extern _a2fo_hybrid_refit_pre_layout_continue
	.globl _a2fo_hybrid_refit_pre_layout_dispatch
_a2fo_hybrid_refit_pre_layout_dispatch:
	pushfl
	pushal
	pushl 8(%ebp)
	pushl -4(%ebp)
	call _a2fo_hybrid_refit_pre_layout
	addl $8, %esp
	popal
	popfl
	orl $-1, %edi
	xorl %esi, %esi
	jmp *_a2fo_hybrid_refit_pre_layout_continue

	# Armada's synchronized build-command receiver reads
	# ConstructionRig::mBuildPositionInterface directly from ESI+0x2a4. A
	# hybrid ResearchStation owns pod data at that address, so resolve the
	# interface through the module sidecar and resume after the replaced load.
	.extern _a2fo_hybrid_build_position_for_command
	.extern _a2fo_hybrid_build_position_load_continue
	.globl _a2fo_hybrid_build_position_load_dispatch
_a2fo_hybrid_build_position_load_dispatch:
	pushl %eax
	pushl %edx
	pushl %edi
	pushl %esi
	call _a2fo_hybrid_build_position_for_command
	addl $8, %esp
	movl %eax, %ecx
	popl %edx
	popl %eax
	jmp *_a2fo_hybrid_build_position_load_continue

	# Fleet Ops sprite helpers use Delphi's EAX/EDX/ECX convention.
	.globl _a2fo_fo_database_find_element
_a2fo_fo_database_find_element:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	xorl %ecx, %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_fo_sprite_set_colour
_a2fo_fo_sprite_set_colour:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_fo_sprite_draw_scaled_2d
_a2fo_fo_sprite_draw_scaled_2d:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	# Keep hybrid ResearchStations on Fleet Ops' stable single-object path.
	.extern _a2fo_hybrid_queue_post_dispatch
	.extern _a2fo_fo_single_object_display_target
	.extern _a2fo_fo_single_object_simulate_target

	.globl _a2fo_ship_display_single_object_display_dispatch
_a2fo_ship_display_single_object_display_dispatch:
	pushl %ecx
	call *_a2fo_fo_single_object_display_target
	pushal
	movl 32(%esp), %eax
	pushl $1
	pushl %eax
	call _a2fo_hybrid_queue_post_dispatch
	addl $8, %esp
	popal
	addl $4, %esp
	ret

	.globl _a2fo_ship_display_single_object_simulate_dispatch
_a2fo_ship_display_single_object_simulate_dispatch:
	pushl %ecx
	pushl 8(%esp)
	call *_a2fo_fo_single_object_simulate_target
	pushal
	movl 32(%esp), %eax
	pushl $0
	pushl %eax
	call _a2fo_hybrid_queue_post_dispatch
	addl $8, %esp
	popal
	addl $4, %esp
	ret $4
