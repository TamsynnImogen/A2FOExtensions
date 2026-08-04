	.text

	# A2FOFeaturePack.dll ABI translations. C++ uses cdecl; FleetOpsHook.dll uses
	# Delphi's EAX/EDX/ECX register convention before stack arguments.

	.globl _a2fo_odf_lstr_from_pchar
_a2fo_odf_lstr_from_pchar:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_lstr_clear
_a2fo_odf_lstr_clear:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_create_virtual_directory
_a2fo_odf_create_virtual_directory:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	movl 16(%ebp), %ecx
	movl $1, %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_tlist_add
_a2fo_odf_tlist_add:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_hash_string
_a2fo_odf_hash_string:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	xorl %edx, %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_add_file_to_hash
_a2fo_odf_add_file_to_hash:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	movl 20(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_add_items_from_disk
_a2fo_odf_add_items_from_disk:
	pushl %ebp
	movl %esp, %ebp
	pushl 28(%ebp)
	pushl 24(%ebp)
	movl 20(%ebp), %ecx
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_add_items_from_pack
_a2fo_odf_add_items_from_pack:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	movl 20(%ebp), %ecx
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_odf_renew_overrides
_a2fo_odf_renew_overrides:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# Generic wrappers for MSVC thiscall functions. The target cleans its stack
	# arguments; the wrapper remains ordinary cdecl for its C++ caller.
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

	# Armada's synchronized build-command receiver reads
	# ConstructionRig::mBuildPositionInterface directly from ESI+0x2a4. A
	# hybrid ResearchStation owns pod data at that address, so resolve the
	# interface through the feature sidecar and resume immediately after the
	# replaced six-byte load. The original instruction preserves EAX/EDX; this
	# bridge does the same while returning the selected interface in ECX.
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

	# Fleet Ops sprite helpers use Delphi's EAX/EDX/ECX convention. These
	# adapters let the hybrid hover preview use the same database lookup,
	# colour, and scaled-draw path as SelectionDisplay.
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
	# These shims call the original callback first, then run the queue post-pass
	# with the callback's result registers preserved. The display callback has
	# no stack argument; the simulate callback receives one argument and returns
	# with `ret 4`, so its shim duplicates that argument for the inner call and
	# performs the original cleanup when returning to Armada.
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
