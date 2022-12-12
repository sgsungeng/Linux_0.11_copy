/*
 *  linux/fs/inode.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <string.h> 
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

struct m_inode inode_table[NR_INODE]={{0,},};

static void read_inode(struct m_inode * inode);
static void write_inode(struct m_inode * inode);

// 不可中断睡眠
static inline void wait_on_inode(struct m_inode * inode)
{
	cli();
	while (inode->i_lock)
		sleep_on(&inode->i_wait);
	sti();
}

static inline void lock_inode(struct m_inode * inode)
{
	cli();
	while (inode->i_lock)
		sleep_on(&inode->i_wait);
	inode->i_lock=1;
	sti();
}

static inline void unlock_inode(struct m_inode * inode)
{
	inode->i_lock=0;
	wake_up(&inode->i_wait);
}
// 让dev的inode失效
void invalidate_inodes(int dev)
{
	int i;
	struct m_inode * inode;

	inode = 0+inode_table;
	for(i=0 ; i<NR_INODE ; i++,inode++) {
		wait_on_inode(inode);
		if (inode->i_dev == dev) {
			if (inode->i_count)
				printk("inode in use on removed disk\n\r");
			inode->i_dev = inode->i_dirt = 0;
		}
	}
}
//将inode_table中的inode写入到缓冲区中
void sync_inodes(void)
{
	int i;
	struct m_inode * inode;

	inode = 0+inode_table;
	for(i=0 ; i<NR_INODE ; i++,inode++) {
		wait_on_inode(inode);
		if (inode->i_dirt && !inode->i_pipe)
			write_inode(inode);
	}
}
// 通过文件的块号获取在实际设备中的块号
static int _bmap(struct m_inode * inode,int block,int create)
{
	struct buffer_head * bh;
	int i;
	// 判断块号是否超过文件范围
	if (block<0)
		panic("_bmap: block<0");
	if (block >= 7+512+512*512)
		panic("_bmap: block>big");
	if (block<7) { // 如果小于7，那么就在直接块中
		if (create && !inode->i_zone[block]) // 如果为空且需要创建
			if ((inode->i_zone[block]=new_block(inode->i_dev))) { // 申请一个块
				inode->i_ctime=CURRENT_TIME;
				inode->i_dirt=1;
			}
		return inode->i_zone[block];
	}
	block -= 7; // 一级间接块
	if (block<512) {
		if (create && !inode->i_zone[7]) // 需要创建且 第8块为空
			if ((inode->i_zone[7]=new_block(inode->i_dev))) { // 申请一个块来存放间接块号
				inode->i_dirt=1;
				inode->i_ctime=CURRENT_TIME;
			}
		if (!inode->i_zone[7])
			return 0;
		if (!(bh = bread(inode->i_dev,inode->i_zone[7]))) // 读取这个块到缓冲区
			return 0;
		i = ((unsigned short *) (bh->b_data))[block]; // 获取这个块在一级间接块中的位置 一个块指向占用两个字节
		if (create && !i) // 如果 为空且需要创建
			if ((i=new_block(inode->i_dev))) { // i指向新的块
				((unsigned short *) (bh->b_data))[block]=i;
				bh->b_dirt=1;
			}
		brelse(bh);
		return i;
	}
	block -= 512; // 如果在二级间接块中
	if (create && !inode->i_zone[8]) // 判断第9块是否为空且需要创建
		if ((inode->i_zone[8]=new_block(inode->i_dev))) {
			inode->i_dirt=1;
			inode->i_ctime=CURRENT_TIME;
		}
	if (!inode->i_zone[8])
		return 0;
	if (!(bh=bread(inode->i_dev,inode->i_zone[8]))) // 读取直接块到buffer中
		return 0;
	i = ((unsigned short *)bh->b_data)[block>>9]; // 获取这个块对应的一级间接块在直接块中的位置 一个块指向占用两个字节
	if (create && !i) // 创建对应的一级间接块
		if ((i=new_block(inode->i_dev))) {
			((unsigned short *) (bh->b_data))[block>>9]=i;
			bh->b_dirt=1;
		}
	brelse(bh);
	if (!i)
		return 0;
	if (!(bh=bread(inode->i_dev,i))) //读取对应的一级间接块
		return 0;
	i = ((unsigned short *)bh->b_data)[block&511]; // block&511 获取块内偏移 ，i= 块内地址
	if (create && !i) // 创建对应的二级间接块
		if ((i=new_block(inode->i_dev))) {
			((unsigned short *) (bh->b_data))[block&511]=i;
			bh->b_dirt=1;
		}
	brelse(bh);
	return i;
}

int bmap(struct m_inode * inode,int block)
{
	return _bmap(inode,block,0);
}

int create_block(struct m_inode * inode, int block)
{
	return _bmap(inode,block,1);
}
// 放回inode
void iput(struct m_inode * inode)
{
	if (!inode)
		return;
	wait_on_inode(inode);
	if (!inode->i_count)
		panic("iput: trying to free free inode");
	if (inode->i_pipe) { // 如果是pipe文件
		wake_up(&inode->i_wait);
		if (--inode->i_count) // 如果--之后还有占用，直接返回
			return;
		free_page(inode->i_size); // 否值直接释放inode占用的内存页
		inode->i_count=0;
		inode->i_dirt=0;
		inode->i_pipe=0;
		return;
	}
	if (!inode->i_dev) { // 如果没有设备
		inode->i_count--; // --后返回
		return;
	}
	if (S_ISBLK(inode->i_mode)) { // 如果是块设备
		sync_dev(inode->i_zone[0]);// 设备文件所定义设备的设备号是保存在其i节点的i_zone[0]中
		wait_on_inode(inode);
	}
repeat:
	if (inode->i_count>1) { // 如果还有占用--后直接返回
		inode->i_count--;
		return;
	}
	if (!inode->i_nlinks) { // 如果没有人链接这个文件 直接清空并释放这个inode
		truncate(inode); // 清空文件内容
		free_inode(inode); //释放 对应的inode
		return;
	}
	if (inode->i_dirt) { // 如果文件被写过了
		write_inode(inode);	/* 需要写入到缓冲区 */
		wait_on_inode(inode);
		goto repeat; //重新检查
	}
	inode->i_count--; // 其他情况--
	return;
}
// 从inodetable中获取inode
struct m_inode * get_empty_inode(void)
{
	struct m_inode * inode;
	static struct m_inode * last_inode = inode_table;
	int i;

	do {
		inode = NULL;
		for (i = NR_INODE; i ; i--) {
			if (++last_inode >= inode_table + NR_INODE)
				last_inode = inode_table;
			if (!last_inode->i_count) { // 这个inode没有人使用
				inode = last_inode;
				if (!inode->i_dirt && !inode->i_lock) // 尽量找一个赶紧的inode
					break;
			}
		}
		if (!inode) { // 没有找到可用的inode
			for (i=0 ; i<NR_INODE ; i++)
				printk("%04x: %6d\t",inode_table[i].i_dev,
					inode_table[i].i_num);
			panic("No free inodes in mem");
		}
		wait_on_inode(inode); // 等待看看有没有人用
		while (inode->i_dirt) { // 如果是dirt的需要写回到cache中
			write_inode(inode);
			wait_on_inode(inode);
		}
	} while (inode->i_count);
	memset(inode,0,sizeof(*inode)); 
	inode->i_count = 1;
	return inode;
}

struct m_inode * get_pipe_inode(void)
{
	struct m_inode * inode;

	if (!(inode = get_empty_inode())) // 获取一个可用的inode
		return NULL;
	if (!(inode->i_size=get_free_page())) { // 申请一页内存并设置到i_size中
		inode->i_count = 0;
		return NULL;
	}
	inode->i_count = 2;	/* sum of readers/writers */
	PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;
	inode->i_pipe = 1;
	return inode;
}

// 获取指定设备dev的指定i节点号nr
struct m_inode * iget(int dev,int nr)
{
	struct m_inode * inode, * empty;

	if (!dev)
		panic("iget with dev==0");
	empty = get_empty_inode();
	inode = inode_table;
	while (inode < NR_INODE+inode_table) {
		if (inode->i_dev != dev || inode->i_num != nr) { // 查找属于dev设备的nr节点
			inode++;
			continue;
		}
		// 找到了
		wait_on_inode(inode); // 等待可用。等待之后再次检查这个inode是否被其他占用
		if (inode->i_dev != dev || inode->i_num != nr) {
			inode = inode_table;
			continue;
		}
		inode->i_count++; // 将inode使用次数加一
		if (inode->i_mount) { // 如果是mount
			int i;

			for (i = 0 ; i<NR_SUPER ; i++)
				if (super_block[i].s_imount==inode) // 如果sb 被安装到 inode
					break;
			if (i >= NR_SUPER) {// 没有找到直接返回
				printk("Mounted inode hasn't got sb\n");
				if (empty)
					iput(empty);
				return inode;
			}
			iput(inode);
			dev = super_block[i].s_dev; // 找到这个sb的dev
			nr = ROOT_INO; // 找到这个sb的 第一个inode
			inode = inode_table; // 重新开始查找

			continue;
		}
		//否则直接返回
		if (empty)
			iput(empty);
		return inode;
	}
	// 没有找到就在读取dev的inode
	if (!empty)
		return (NULL);
	inode=empty;
	inode->i_dev = dev;
	inode->i_num = nr;
	read_inode(inode);
	return inode;
}

static void read_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

	lock_inode(inode);
	if (!(sb=get_super(inode->i_dev)))
		panic("trying to read inode without dev");
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
		(inode->i_num-1)/INODES_PER_BLOCK; // 找到对应的块号（在设备中的）
	if (!(bh=bread(inode->i_dev,block))) // 读取到bufuer中
		panic("unable to read i-node block");
	*(struct d_inode *)inode =
		((struct d_inode *)bh->b_data)
			[(inode->i_num-1)%INODES_PER_BLOCK]; // 赋值
	brelse(bh);
	unlock_inode(inode);
}
// 写入到buffer中
static void write_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;
	int block;

	lock_inode(inode);
	if (!inode->i_dirt || !inode->i_dev) { // if this inode is not change or i_dev == 0 ignore
		unlock_inode(inode);
		return;
	}
	if (!(sb=get_super(inode->i_dev)))
		panic("trying to write inode without device");
	block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
		(inode->i_num-1)/INODES_PER_BLOCK;
	if (!(bh=bread(inode->i_dev,block)))
		panic("unable to read i-node block");
	((struct d_inode *)bh->b_data)
		[(inode->i_num-1)%INODES_PER_BLOCK] =
			*(struct d_inode *)inode;
	bh->b_dirt=1;
	inode->i_dirt=0;
	brelse(bh);
	unlock_inode(inode);
}
