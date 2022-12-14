/*
 * This file has definitions for some important file table
 * structures etc.
 */

#ifndef _FS_H
#define _FS_H

#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 * // 系统中所含有的设备 的主设备号
 * 0 - unused (nodev)
 * 1 - /dev/mem  内存设备
 * 2 - /dev/fd 软盘设备
 * 3 - /dev/hd 硬盘设备
 * 4 - /dev/ttyx tty 串行终端
 * 5 - /dev/tty tty终端
 * 6 - /dev/lp // 打印设备
 * 7 - unnamed pipes // 匿名管道
 */

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3) // 设备号 为1，2，3 的可seek

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */ // 预读
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8) // high 8 bit
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1

#define I_MAP_SLOTS 8 // i节点位图 块数量
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20 // 进程可打开文件数量
#define NR_INODE 32 // 系统同时最多使用i节点数量
#define NR_FILE 64 // 系统最多打开文件个数
#define NR_SUPER 8 // 系统最多打开super数量
#define NR_HASH 307 // hash素数
#define NR_BUFFERS nr_buffers // 系统中含有缓冲块个数，由内存大小决定，初始化后不再改变
#define BLOCK_SIZE 1024 // block拥有的字节大小
#define BLOCK_SIZE_BITS 10 // 数据块长度所占用bit数量
#ifndef NULL
#define NULL ((void *) 0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode))) // 一个block 可以放多少个inode
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry))) // 一个block 可以有多少个目录项

// 循环队列操作
#define PIPE_HEAD(inode) ((inode).i_zone[0]) // 管道头，保存在inode的第0个直接块地址内
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))
#define PIPE_EMPTY(inode) (PIPE_HEAD(inode)==PIPE_TAIL(inode))
#define PIPE_FULL(inode) (PIPE_SIZE(inode)==(PAGE_SIZE-1))
#define INC_PIPE(head) \ 
__asm__("incl %0\n\tandl $4095,%0"::"m" (head)) // ((*head)++)&(pagesize-1)

typedef char buffer_block[BLOCK_SIZE]; // 块缓冲区

// 数据块缓冲区
struct buffer_head {
	char * b_data;			/* pointer to data block (1024 bytes) */
	unsigned long b_blocknr;	/* block number 块号 ，在dev中的*/
	unsigned short b_dev;		/* device (0 = free) */
	unsigned char b_uptodate;    // isValid
	unsigned char b_dirt;		/* 0-clean,1-dirty */
	unsigned char b_count;		/* users using this block */
	unsigned char b_lock;		/* 0 - ok, 1 -locked */
	struct task_struct * b_wait; // 等待队列
	struct buffer_head * b_prev; // hash链表
	struct buffer_head * b_next;
	struct buffer_head * b_prev_free; // 空闲队列
	struct buffer_head * b_next_free;
};

// inode in disk
struct d_inode {
	unsigned short i_mode;	// rwx
	unsigned short i_uid;	// user id
	unsigned long i_size;	// file size
	unsigned long i_time;	//change time from 1970 .1.1 seonds
	unsigned char i_gid;	// group id 
	unsigned char i_nlinks; // link 指向这个inode的 dir_entry数量
	unsigned short i_zone[9]; // zones
};
// inode in memory
struct m_inode {
	unsigned short i_mode;
	unsigned short i_uid;
	unsigned long i_size;
	unsigned long i_mtime;
	unsigned char i_gid;
	unsigned char i_nlinks;
	unsigned short i_zone[9];
	// 内存中的inode信息
	struct task_struct * i_wait; // 等待队列
	unsigned long i_atime;	// view time
	unsigned long i_ctime;	// create time
	unsigned short i_dev;	// dev of inode
	unsigned short i_num;	// inode block number
	unsigned short i_count; // 
	unsigned char i_lock;
	unsigned char i_dirt;
	unsigned char i_pipe; // 是否是管道
	unsigned char i_mount; // 是否mount
	unsigned char i_seek;
	unsigned char i_update;
};

// 文件
struct file {
	unsigned short f_mode; // RW
	unsigned short f_flags; // 打开标志
	unsigned short f_count; // 引用计数
	struct m_inode * f_inode; // 文件的inode
	off_t f_pos; // 文件位置
};

// minix文件系统超级
struct super_block {
	unsigned short s_ninodes; 	// inodes number
	unsigned short s_nzones;	// block number
	unsigned short s_imap_blocks; // bit map blocks of inode
	unsigned short s_zmap_blocks; // bit map blocks of zone
	unsigned short s_firstdatazone; // block number of first data zone
	unsigned short s_log_zone_size; // log(data block/logic number)
	unsigned long s_max_size;	// max size of file
	unsigned short s_magic;	//magic number of file system
/* These are only in memory */
	struct buffer_head * s_imap[8]; //inode 的buffer head
	struct buffer_head * s_zmap[8];
	unsigned short s_dev;
	struct m_inode * s_isup; // 被安装的根节点inode
	struct m_inode * s_imount; // 被安装到的inode
	unsigned long s_time;
	struct task_struct * s_wait;
	unsigned char s_lock;
	unsigned char s_rd_only; // readonly
	unsigned char s_dirt;
};
// super in disk
struct d_super_block { 
	unsigned short s_ninodes;
	unsigned short s_nzones;
	unsigned short s_imap_blocks;
	unsigned short s_zmap_blocks;
	unsigned short s_firstdatazone;
	unsigned short s_log_zone_size;
	unsigned long s_max_size;
	unsigned short s_magic;
};

struct dir_entry {
	unsigned short inode;
	char name[NAME_LEN];
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern struct buffer_head * start_buffer;
extern int nr_buffers;

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern void truncate(struct m_inode * inode); // remove all zone
extern void sync_inodes(void);
extern void wait_on(struct m_inode * inode);
extern int bmap(struct m_inode * inode,int block); // get logic number of data block in file
extern int create_block(struct m_inode * inode,int block);
extern struct m_inode * namei(const char * pathname);
extern int open_namei(const char * pathname, int flag, int mode,
	struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);
extern struct m_inode * iget(int dev,int nr);
extern struct m_inode * get_empty_inode(void);
extern struct m_inode * get_pipe_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern void bread_page(unsigned long addr,int dev,int b[4]);
extern struct buffer_head * breada(int dev,int block,...);
extern int new_block(int dev);
extern void free_block(int dev, int block);
extern struct m_inode * new_inode(int dev);
extern void free_inode(struct m_inode * inode);
extern int sync_dev(int dev);
extern struct super_block * get_super(int dev);
extern int ROOT_DEV;

extern void mount_root(void);

#endif
