/*
 *  linux/fs/block_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>
// 块读取函数      设备      位置        写入源位置     数量
int block_write(int dev, long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS; //块间偏移
	int offset = *pos & (BLOCK_SIZE-1); //块内偏移
	int chars;
	int written = 0;
	struct buffer_head * bh;
	register char * p;

	while (count>0) {
		chars = BLOCK_SIZE - offset; // 块内开始位置
		if (chars > count) // 如果当前block剩余 数量大于剩余需要写入的量就复制为剩下的量
			chars=count;
		if (chars == BLOCK_SIZE) // 获取缓冲区中的块，没有就找一个
			bh = getblk(dev,block);
		else
			bh = breada(dev,block,block+1,block+2,-1); // 预读块
		block++;
		if (!bh)
			return written?written:-EIO;
		p = offset + bh->b_data; // 第一次offset 不为0，
		offset = 0;
		*pos += chars; // 更新文件中 pos
		written += chars; // 写入量
		count -= chars; // 剩余量
		while (chars-->0)
			*(p++) = get_fs_byte(buf++);  // 写入
		bh->b_dirt = 1; // 这个块已经脏了
		brelse(bh); // 释放buffer_head
	}
	return written;
}
// 同上
int block_read(int dev, unsigned long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS;
	int offset = *pos & (BLOCK_SIZE-1);
	int chars;
	int read = 0;
	struct buffer_head * bh;
	register char * p;

	while (count>0) {
		chars = BLOCK_SIZE-offset;
		if (chars > count)
			chars = count;
		if (!(bh = breada(dev,block,block+1,block+2,-1)))
			return read?read:-EIO;
		block++;
		p = offset + bh->b_data;
		offset = 0;
		*pos += chars;
		read += chars;
		count -= chars;
		while (chars-->0)
			put_fs_byte(*(p++),buf++);
		brelse(bh);
	}
	return read;
}
