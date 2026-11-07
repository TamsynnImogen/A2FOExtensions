	.text
	# Exact LoadDatabase CALL -> ReadDatabase. ECX=memory stream,
	# [esp+4]=parser scalar bits, [esp+8]=database name. Both thiscall
	# arguments are callee-cleaned. Forward the native call exactly once;
	# observe its completed DB while the input stream is still alive.
	.globl _a2fo_native11_database_read_tap
_a2fo_native11_database_read_tap:
	pushl %ebp
	movl %esp, %ebp
	pushl %ecx
	# Record loader-entry control without changing flags or arithmetic state.
	pushl $0
	fnstcw -8(%ebp)
	# PUSH/MOV/CALL leave incoming flags and FP state untouched.
	pushl 12(%ebp)
	pushl 8(%ebp)
	call *_a2fo_native11_read_database_original
	pushfl
	pushal
	movl %esp, %esi
	leal -544(%esp), %esp
	andl $-16, %esp
	fxsave (%esp)
	fninit
	cld
	movl $0x1f80, 512(%esp)
	ldmxcsr 512(%esp)
	# A changed control word cannot certify loader arithmetic. The callback
	# receives unknown (0) rather than silently substituting draw-time state.
	movzwl (%esp), %eax
	cmpw -8(%ebp), %ax
	jne 1f
	orl $0x10000, %eax
	jmp 2f
1:	xorl %eax, %eax
2:	# Five cdecl arguments plus 12 bytes padding preserve stack alignment.
	leal -12(%esp), %esp
	pushl %eax
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl -4(%ebp)
	pushl 28(%esi)
	call _a2fo_native11_observe_database
	addl $32, %esp
	fxrstor (%esp)
	movl %esi, %esp
	popal
	popfl
	movl %ebp, %esp
	popl %ebp
	ret $8
