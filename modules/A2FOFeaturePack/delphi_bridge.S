	# ABI bridge for FeaturePack calls into FleetOpsHook's Delphi-register
	# routines. C++ policy and signature checks remain in the owning sources.

	.text

	# The confirmed BuildYard provider (FO +13d4a4) is thiscall/ret 4,
	# but overwrites ESI at +13d4d0 without saving it. Shield the C++ caller
	# from that native ABI violation; do not change the generic wrappers.
	.globl _a2fo_buildyard_call_tooltip_provider
_a2fo_buildyard_call_tooltip_provider:
	pushl %ebp
	movl %esp, %ebp
	pushl %esi
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	popl %esi
	leave
	ret

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

	.globl _a2fo_call_thiscall_7
_a2fo_call_thiscall_7:
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

	# Call a Delphi register-convention routine with EAX/EDX/ECX plus one
	# callee-cleaned stack argument. BuildYard's private GetProjectId wrapper
	# uses this shape.
	.globl _a2fo_buildyard_call_delphi_reg3_stack1
_a2fo_buildyard_call_delphi_reg3_stack1:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	movl 20(%ebp), %ecx
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# Call a Delphi register-convention routine with EAX/EDX.  Used by the
	# BuildYard technology-entry policy to invoke Fleet Ops' recursive team
	# technology-tree evaluator.
	.globl _a2fo_buildyard_call_delphi_reg2
_a2fo_buildyard_call_delphi_reg2:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	# Replacement for BuildYard's one RequiredTechnology GetProjectId call.
	# The native caller supplies EAX=ParameterDB, EDX=Delphi command string,
	# ECX=output cPrjID, and a callee-cleaned default value on the stack.
	.globl _a2fo_buildyard_project_id_call_bridge
_a2fo_buildyard_project_id_call_bridge:
	pushl 4(%esp)
	pushl %ecx
	pushl %edx
	pushl %eax
	call _a2fo_buildyard_project_id_call_handler
	addl $16, %esp
	ret $4

	# Wrapper for FleetOpsHook calls to Armada's localization manager lookup.
	# Native shape is MSVC thiscall: ECX=manager, one callee-cleaned key pointer
	# on the stack. The C++ handler is ordinary cdecl(manager, key).
	.extern _a2fo_buildyard_localization_lookup_handler
	.globl _a2fo_buildyard_localization_lookup_bridge
_a2fo_buildyard_localization_lookup_bridge:
	pushl %ebp
	movl %esp, %ebp
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_buildyard_localization_lookup_handler
	addl $8, %esp
	leave
	ret $4

	# Patched call at FO+11fc20. EAX is the native file object; EBP still
	# addresses the verified XML reader frame. Handler returns the SAME
	# borrowed text pointer after copying it for MODULE parsing.
	.globl _a2fo_buildyard_tree_text_bridge
_a2fo_buildyard_tree_text_bridge:
	pushl %ecx
	pushl %edx
	pushl %ebp
	pushl %eax
	call _a2fo_buildyard_tree_text_handler
	addl $8, %esp
	popl %edx
	popl %ecx
	ret

	# The root loader must have no MinGW C++ frame across native Delphi SEH.
	# Begin bookkeeping, call native with only this assembly frame live, then
	# commit on normal return. Native SEH unwinding skips the commit path.
	.globl _a2fo_buildyard_tree_root_bridge
_a2fo_buildyard_tree_root_bridge:
	pushl %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %eax
	call _a2fo_buildyard_tree_begin
	addl $12, %esp
	popl %eax
	popl %edx
	popl %ecx
	pushl %eax                         # tree for post-load commit
	call *_g_a2fo_buildyard_tree_dispatch
	pushl %eax                         # preserve native volatile outputs
	pushl %edx
	pushl %ecx
	pushl 12(%esp)                     # tree
	call _a2fo_buildyard_tree_commit
	addl $4, %esp
	popl %ecx
	popl %edx
	popl %eax
	addl $4, %esp                      # saved tree
	ret
