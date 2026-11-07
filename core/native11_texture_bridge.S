	.text
	# Both checked CALLs use Delphi register ABI, with no stack arguments:
	# EAX=filesystem, EDX=request AnsiString, ECX=output AnsiString slot.
	# Native caller EBP is passed as telemetry; only C++ checked RPM reads it.
	.macro resolver_tap name,route
	.globl \name
\name:
	pushl %ebp
	movl %esp,%ebp
	pushl %ecx
	pushl %edx
	call *_a2fo_native11_texture_resolver_original
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
	pushl $\route
	pushl 0(%ebp)
	pushl -8(%ebp)
	pushl -4(%ebp)
	call _a2fo_native11_observe_texture_path
	addl $16,%esp
	fxrstor (%esp)
	movl %esi,%esp
	popal
	popfl
	movl %ebp,%esp
	popl %ebp
	ret
	.endm
	resolver_tap _a2fo_native11_texture_parameters_tap,1
	resolver_tap _a2fo_native11_texture_pixels_tap,2
