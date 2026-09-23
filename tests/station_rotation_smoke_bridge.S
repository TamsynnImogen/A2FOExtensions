	.text
	.globl _station_test_position
_station_test_position:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl $0x13579bdf, %ebx
	movl $0x2468ace0, %esi
	movl 24(%ebp), %edi
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	xorl %eax, %eax
	leal -12(%ebp), %edx
	cmpl %edx, %esp
	jne 1f
	cmpl $0x13579bdf, %ebx
	jne 1f
	cmpl $0x2468ace0, %esi
	jne 1f
	cmpl 24(%ebp), %edi
	jne 1f
	incl %eax
1:
	leal -12(%ebp), %esp
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret

	.globl _station_test_footprint
_station_test_footprint:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	movl 28(%ebp), %ebx
	movl %ebx, %esi
	movl %ebx, %edi
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	xorl %eax, %eax
	leal -12(%ebp), %edx
	cmpl %edx, %esp
	jne 2f
	cmpl 28(%ebp), %ebx
	jne 2f
	cmpl %ebx, %esi
	jne 2f
	cmpl %ebx, %edi
	jne 2f
	incl %eax
2:
	leal -12(%ebp), %esp
	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	ret
