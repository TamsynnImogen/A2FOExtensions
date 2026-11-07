	.text
	.macro lifecycle_tap name,kind,original
	.globl \name
\name:
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
	# Two cdecl arguments plus two padding words; saved ECX is this.
	pushl $0
	pushl $0
	pushl 24(%esi)
	pushl $\kind
	call _a2fo_native11_observe_destruction
	addl $16,%esp
	fxrstor (%esp)
	movl %esi,%esp
	popal
	popfl
	# Original return address and args remain exactly where the native entry
	# expects them. The trampoline recreates its five displaced prolog bytes.
	jmp *\original
	.endm
	lifecycle_tap _a2fo_native11_model_destroy_tap,1,_a2fo_native11_model_destroy_original
	lifecycle_tap _a2fo_native11_texture_destroy_tap,2,_a2fo_native11_texture_destroy_original
