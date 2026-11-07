	.text
	# Qualified first workspace base constructor CALL. ECX=this, stack holds
	# two original ret8 arguments. CPU sources run before workspace publication.
	# Preserve all entry flags/registers/x87/SSE, then tail-dispatch exactly once.
	.globl _a2fo_refit_workspace_ctor_tap
_a2fo_refit_workspace_ctor_tap:
	pushfl
	pushal
	movl %esp,%ebx
	subl $528,%esp
	andl $-16,%esp
	fxsave (%esp)
	fninit
	movl %esp,%ebp
	pushl $0x1f80
	ldmxcsr (%esp)
	addl $4,%esp
	call _a2fo_refit_prepare_sources
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	jmp *_a2fo_refit_workspace_ctor_original
