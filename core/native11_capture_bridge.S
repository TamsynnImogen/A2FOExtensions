	.text
	# Replaces precisely mov edx,[ecx]; call [edx+14h] in Entity::Render,
	# after its successful instance frustum test and on-screen flag write.
	# Entry: ECX=instance, ESI=entity, EDI=camera, [esp+4]=camera.
	# Preserve integer/flags and x87/SSE state around the observer, then tail
	# dispatch the original virtual call once. Its ret 4 consumes the argument.
	.globl _a2fo_native11_entity_render_tap
_a2fo_native11_entity_render_tap:
	pushfl
	pushal
	movl %esp, %ebx
	subl $528, %esp
	andl $-16, %esp
	fxsave (%esp)
	# C++ expects an empty x87 stack. The intercepted instruction may have
	# live x87 values; saving without resetting lets callback temporaries
	# overflow that stack. Restore the engine's full state after observation.
	fninit
	movl %esp, %ebp
	pushl $0x1f80
	ldmxcsr (%esp)
	addl $4, %esp
	subl $8, %esp
	pushl 0(%ebx)
	pushl 4(%ebx)
	call _a2fo_native11_observe_entity
	addl $16, %esp
	fxrstor (%ebp)
	movl %ebx, %esp
	popal
	popfl
	movl (%ecx), %edx
	jmp *0x14(%edx)
