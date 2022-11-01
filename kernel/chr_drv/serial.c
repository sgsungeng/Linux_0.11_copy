/*
 *  linux/kernel/serial.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	serial.c
 *
 * This module implements the rs232 io functions
 *	void rs_write(struct tty_struct * queue);
 *	void rs_init(void);
 * and all interrupts pertaining to serial IO.
 */

#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

#define WAKEUP_CHARS (TTY_BUF_SIZE/4) // 256

extern void rs1_interrupt(void);
extern void rs2_interrupt(void);

static void init(int port)
{
	outb_p(0x80,port+3);	/* set DLAB of line control reg */
	outb_p(0x30,port);	/* LS of divisor (48 -> 2400 bps */
	outb_p(0x00,port+1);	/* MS of divisor */
	outb_p(0x03,port+3);	/* reset DLAB */
	outb_p(0x0b,port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d,port+1);	/* enable all intrs but writes */
	(void)inb(port);	/* read data port to reset things (?) */
}

void rs_init(void)
{
	set_intr_gate(0x24,rs1_interrupt); // set serial1 interrupt
	set_intr_gate(0x23,rs2_interrupt); // set serial2 interrupt
	init(tty_table[1].read_q.data); // init serial1 0x3f8
	init(tty_table[2].read_q.data); // ... 0x2f8
	outb(inb_p(0x21)&0xE7,0x21); // allow interrupt of main chip(8259a) IRQ3(serial2) IRQ4(serial1)
}
