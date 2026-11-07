	.text
	# ECX=mesh; original stack arguments are pass, texture material (ret8).
	# Observation never claims a draw. Restore complete entry state, call the
	# checked native body once, then preserve its return state through cleanup.
	.macro mesh_tap name,route,original
	.globl \name
\name:
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
	pushl $\route
	pushl 44(%ebx)
	pushl 40(%ebx)
	pushl 24(%ebx)
	call _a2fo_refit_observe_mesh
	addl $16,%esp
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	# Duplicate the two native arguments; the original consumes their copies.
	pushl 8(%esp)
	pushl 8(%esp)
	call *\original
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
	subl $12,%esp
	pushl $\route
	call _a2fo_refit_mesh_finished
	addl $16,%esp
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	ret $8
	.endm
	mesh_tap _a2fo_refit_mesh_fast_tap,0,_a2fo_refit_mesh_fast_original
	mesh_tap _a2fo_refit_mesh_legacy_tap,1,_a2fo_refit_mesh_legacy_original
