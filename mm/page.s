/*
 *  linux/mm/page.s
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * page.s contains the low-level page-exception code.
 * the real work is done in mm.c
 */

.globl page_fault

page_fault: // index = 14
	xchgl %eax,(%esp) // set the top of stack to eax, error code
	pushl %ecx
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%edx
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	movl %cr2,%edx // set edx as vitural address
	pushl %edx
	pushl %eax
	testl $1,%eax // error code == 1 indecats page missing， or write protect for other
	jne 1f
	call do_no_page
	jmp 2f
1:	call do_wp_page
2:	addl $8,%esp
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
