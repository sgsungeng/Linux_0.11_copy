/*
 * 'tty.h' defines some structures used by tty_io.c and some defines.
 *
 * NOTE! Don't touch this without checking that nothing in rs_io.s or
 * con_io.s breaks. Some constants are hardwired into the system (mainly
 * offsets into 'tty_queue'
 */

#ifndef _TTY_H
#define _TTY_H

#include <termios.h>

#define TTY_BUF_SIZE 1024

struct tty_queue {
	unsigned long data;
	unsigned long head;
	unsigned long tail;
	struct task_struct * proc_list;
	char buf[TTY_BUF_SIZE];
};
struct tty_struct {
	struct termios termios;
	int pgrp;
	int stopped;
	void (*write)(struct tty_struct * tty);
	struct tty_queue read_q;
	struct tty_queue write_q;
	struct tty_queue secondary;
	};
