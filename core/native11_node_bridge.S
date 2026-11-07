	.text
	# Replaces add esi,44h; mov edx/eax,[ecx] after animation evaluation.
	# Entry ESI=instance, ECX=DB; the native EBP frame has camera at +8.
	# Observe without advancing animation; replay both displaced instructions.
	.macro observe_node
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
	# Reuse the former alignment word for the original x87 control word.
	# The callback runs with a clean FP stack, but can reproduce native math.
	movzwl (%ebp),%eax
	orl $0x10000,%eax
	pushl %eax
	pushl 24(%ebx)
	movl 8(%ebx),%eax
	pushl 8(%eax)
	pushl 4(%ebx)
	call _a2fo_native11_observe_nodes
	addl $16,%esp
	fxrstor (%ebp)
	movl %ebx,%esp
	popal
	popfl
	addl $0x44,%esi
	.endm
	.globl _a2fo_native11_node_branch_a
_a2fo_native11_node_branch_a:
	observe_node
	movl (%ecx),%edx
	ret
	.globl _a2fo_native11_node_branch_b
_a2fo_native11_node_branch_b:
	observe_node
	movl (%ecx),%eax
	ret
