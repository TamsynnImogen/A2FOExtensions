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
