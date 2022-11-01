void floppy_init(void)
{
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_trap_gate(0x26,&floppy_interrupt);
	outb(inb_p(0x21)&~0x40,0x21);
}
