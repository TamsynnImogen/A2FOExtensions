	.text

	# ABI translation helpers for 32-bit code only.
	#
	# C++ calls every symbol below with ordinary cdecl stack arguments. The
	# wrapper moves them into whichever nonstandard ABI the target expects:
	#   Delphi register convention: EAX, EDX, ECX, then stack arguments.
	#   MSVC thiscall:              ECX=this, remaining arguments on stack.
	# The first C argument is always the target function address.

	# Delphi long-string conversion: EAX=&destination, EDX=source PChar.
	.globl _a2fo_lstr_from_pchar
_a2fo_lstr_from_pchar:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_lstr_clear
_a2fo_lstr_clear:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	# Construct a TFOFSVirtualDirectory. EAX=class/self, EDX=constructor flag,
	# ECX=Delphi path, followed by the remaining constructor fields.
	.globl _a2fo_create_virtual_directory
_a2fo_create_virtual_directory:
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

	# Delphi container and filesystem helpers used while extending the ODF VFS.
	.globl _a2fo_tlist_add
_a2fo_tlist_add:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_hash_string
_a2fo_hash_string:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	xorl %edx, %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_add_file_to_hash
_a2fo_add_file_to_hash:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	movl 20(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_add_items_from_disk
_a2fo_add_items_from_disk:
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

	.globl _a2fo_add_items_from_pack
_a2fo_add_items_from_pack:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	movl 20(%ebp), %ecx
	movl 16(%ebp), %edx
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_renew_overrides
_a2fo_renew_overrides:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_file_system_get_file
_a2fo_call_file_system_get_file:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_get_file_from_hash
_a2fo_call_get_file_from_hash:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %eax
	movl 16(%ebp), %edx
	movl 20(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Armada gameplay methods use MSVC thiscall (ECX=self).
	.globl _a2fo_call_evolver_build_class
_a2fo_call_evolver_build_class:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_call_evolver_dtor
_a2fo_call_evolver_dtor:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_parameter_db_get_string
_a2fo_parameter_db_get_string:
	pushl %ebp
	movl %esp, %ebp
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	movzbl %al, %eax
	leave
	ret

	.globl _a2fo_load_sod
_a2fo_load_sod:
	pushl %ebp
	movl %esp, %ebp
	pushl $0x3f800000
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Mid-function cocoon splice. Preserve Armada's live volatile registers,
	# ask C++ for the geometry, place the result in the EDI register expected by
	# the original continuation, then jump back immediately after the patch.
	.globl _a2fo_cocoon_selector_hook
_a2fo_cocoon_selector_hook:
	pushl %eax
	pushl %ecx
	pushl %edx
	pushl %ebx
	call _a2fo_select_cocoon
	addl $4, %esp
	movl %eax, %edi
	popl %edx
	popl %ecx
	popl %eax
	jmp *_a2fo_cocoon_resume
