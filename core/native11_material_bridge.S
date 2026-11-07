	.text
	# Checked CALL in CraftInstance::Render, after native and extension material
	# selection. ECX=instance, [ESP+4]=camera. Preserve the full pre-call state
	# around observation and cleanup, executing the original ret4 function once.
	.globl _a2fo_native11_material_draw_tap
_a2fo_native11_material_draw_tap:
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
	subl $8,%esp
	pushl 40(%ebx)
	pushl 24(%ebx)
	call _a2fo_native11_observe_materials
	addl $16,%esp
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	# Duplicate only the camera argument; the original consumes the copy.
	# PUSH/CALL do not alter the restored entry flags or registers.
	pushl 4(%esp)
	call *_a2fo_native11_material_draw_original
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
	call _a2fo_native11_material_render_end
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	ret $4
