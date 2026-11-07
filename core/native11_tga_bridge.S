	.text
	.globl _a2fo_native11_tga_open_tap
_a2fo_native11_tga_open_tap:
	pushl %ebp
	movl %esp,%ebp
	pushl %ecx
	pushl 12(%ebp)
	pushl 8(%ebp)
	call *_a2fo_native11_tga_open_original
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
	# Six callback arguments plus padding preserve the C stack alignment.
	subl $8,%esp
	pushl 16(%esi)
	pushl 4(%esi)
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl -4(%ebp)
	pushl 28(%esi)
	call _a2fo_native11_observe_tga_open
	addl $32,%esp
	fxrstor (%esp)
	movl %esi,%esp
	popal
	popfl
	movl %ebp,%esp
	popl %ebp
	ret $8
