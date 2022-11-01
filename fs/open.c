/*
 *  linux/fs/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* #include <string.h> */
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>

int sys_ustat(int dev, struct ustat * ubuf)
{
	return -ENOSYS;
}
