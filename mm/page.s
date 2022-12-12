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

page_fault: // 缺页异常 index = 14
	xchgl %eax,(%esp) // set the top of stack to eax, error code
	pushl %ecx// 将其他参数压入到栈中
	pushl %edx
	push %ds
	push %es
	push %fs
	movl $0x10,%edx // 将ds ，es ，fs 设置为内核态
	mov %dx,%ds
	mov %dx,%es
	mov %dx,%fs
	movl %cr2,%edx // set edx as vitural address 读取出问题的虚拟地址
	pushl %edx // 将虚拟地址放入到栈中
	pushl %eax // 将之前的栈顶放入到栈中
	testl $1,%eax // error code == 1 indecats page missing， or write protect for other 如果error code == 1 则是缺页异常否则是写保护了
	jne 1f
	call do_no_page
	jmp 2f
1:	call do_wp_page
2:	addl $8,%esp // 恢复栈
	pop %fs
	pop %es
	pop %ds
	popl %edx
	popl %ecx
	popl %eax
	iret
