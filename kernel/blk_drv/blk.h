#ifndef _BLK_H
#define _BLK_H

#define NR_BLK_DEV	7
/*
 * NR_REQUEST is the number of entries in the request-queue.
 * NOTE that writes may use only the low 2/3 of these: reads
 * take precedence.
 *
 * 32 seems to be a reasonable number: enough to get some benefit
 * from the elevator-mechanism, but not so much as to lock a lot of
 * buffers when they are in the queue. 64 seems to be too many (easily
 * long pauses in reading when heavy writing/syncing is going on)
 */
#define NR_REQUEST	32

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests when that is implemented. In
 * paging, 'bh' is NULL, and 'waiting' is used to wait for
 * read/write completion.
 */
struct request {
	int dev;		/* -1 if no request */
	int cmd;		/* READ or WRITE */
	int errors;
	unsigned long sector;
	unsigned long nr_sectors;
	char * buffer;
	struct task_struct * waiting;
	struct buffer_head * bh;
	struct request * next;
};
