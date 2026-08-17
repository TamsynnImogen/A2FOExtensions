	# ABI bridges for Armada's MSVC thiscall functions and the two native
	# call-site wrappers owned by A2FOTextureVariants.

	.text

	.globl _a2fo_texture_variants_call_thiscall_0
_a2fo_texture_variants_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_texture_variants_call_thiscall_1
_a2fo_texture_variants_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_texture_variants_call_thiscall_2
_a2fo_texture_variants_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_texture_variants_call_thiscall_3
_a2fo_texture_variants_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_texture_variants_call_thiscall_4
_a2fo_texture_variants_call_thiscall_4:
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

	# Replaces CraftInstance::RenderInternal's call to
	# CraftClass::SetBorgMeshTextures. ESI is the CraftInstance, ECX is its
	# CraftClass, and the one stack argument is the native Borg-selection flag.
	.globl _a2fo_texture_variants_render_swap_hook
_a2fo_texture_variants_render_swap_hook:
	pushl %ebp
	movl %esp, %ebp
	subl $8, %esp
	movl %ecx, -4(%ebp)
	movl %esi, -8(%ebp)
	pushfl
	pushal
	pushl 8(%ebp)
	pushl -4(%ebp)
	pushl -8(%ebp)
	call _a2fo_texture_variants_apply_render_textures
	addl $12, %esp
	popal
	popfl
	leave
	ret $4
