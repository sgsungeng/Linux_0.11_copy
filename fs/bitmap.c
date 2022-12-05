/*
 *  linux/fs/bitmap.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* bitmap.c contains the code that handles the inode and block bitmaps */
#include <string.h>

#include <linux/sched.h>
#include <linux/kernel.h>
// cld set direction 
// stosl mov eax to es:edi
// 将addr开始的1024（Block_size）个字节块清理为0
#define clear_block(addr) \
__asm__ __volatile__ ("cld\n\t" \
	"rep\n\t" \
	"stosl" \
	::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)))

// bts： bit test and set， and return the old value
#define set_bit(nr,addr) ({\
register int res ; \
__asm__ __volatile__("btsl %2,%3\n\tsetb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

// 复位指定地址开始的第nr位偏移处的bit位。返回原bit位值的反码
#define clear_bit(nr,addr) ({\
register int res ; \
__asm__ __volatile__("btrl %2,%3\n\tsetnb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})
// 从一个块 1024字节中找到第一个为0的位置
#define find_first_zero(addr) ({ \
int __res; \
__asm__ __volatile__ ("cld\n" \  // si,di++
	"1:\tlodsl\n\t" \ // load to eax
	"notl %%eax\n\t" \ // not eax
	"bsfl %%eax,%%edx\n\t" \ // get the first 1 and set to edx
	"je 2f\n\t" \ // if zero == eax
	"addl %%edx,%%ecx\n\t" \ // ecx += edx
	"jmp 3f\n" \
	"2:\taddl $32,%%ecx\n\t" \ // ecx += 32
	"cmpl $8192,%%ecx\n\t" \ 
	"jl 1b\n" \
	"3:" \
	:"=c" (__res):"c" (0),"S" (addr)); \ // esi = addr
__res;})

// 释放dev设备的第block
void free_block(int dev, int block)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!(sb = get_super(dev))) // 获取superblock
		panic("trying to free block on nonexistent device");
	if (block < sb->s_firstdatazone || block >= sb->s_nzones)
		panic("trying to free block not in datazone");
	bh = get_hash_table(dev,block); // 从hashtable中获取 buffer head 
	if (bh) { 
		if (bh->b_count != 1) {
			printk("trying to free block (%04x:%d), count=%d\n",
				dev,block,bh->b_count);
			return;
		}
		bh->b_dirt=0; // 直接释放，本操作之针对下层操作，块之间的联系由其他块修改
		bh->b_uptodate=0;
		brelse(bh); // 释放缓冲区
	}
	block -= sb->s_firstdatazone - 1 ;
	if (clear_bit(block&8191,sb->s_zmap[block/8192]->b_data)) { // 清理对应的bit位
		printk("block (%04x:%d) ",dev,block+sb->s_firstdatazone-1);
		panic("free_block: bit already cleared");
	}
	sb->s_zmap[block/8192]->b_dirt = 1; // 写回 block zmap ，，将该bit map block 设置为脏
}
// 申请新的块
int new_block(int dev)
{
	struct buffer_head * bh;
	struct super_block * sb;
	int i,j;

	if (!(sb = get_super(dev)))
		panic("trying to get new block from nonexistant device");
	j = 8192;
	for (i=0 ; i<8 ; i++) // 找到可用的块
		if ((bh=sb->s_zmap[i]))
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	if (i>=8 || !bh || j>=8192)
		return 0;
	if (set_bit(j,bh->b_data)) // 设置bit map 为1
		panic("new_block: bit already set");
	bh->b_dirt = 1; // 该bitmap已经被修改过了
	j += i*8192 + sb->s_firstdatazone-1; // 得到整体的块号
	if (j >= sb->s_nzones)
		return 0;
	if (!(bh=getblk(dev,j))) // 依据块号从设备中获取buffer_head
		panic("new_block: cannot get block");
	if (bh->b_count != 1)
		panic("new block: count is != 1");
	clear_block(bh->b_data); // 清空对应的块
	bh->b_uptodate = 1; // 设置为有效
	bh->b_dirt = 1; // 需要会写
	brelse(bh); // 将这个buffer_head 释放回去。只是释放到了缓冲区
	return j;
}

// 释放inode
void free_inode(struct m_inode * inode)
{
	struct super_block * sb;
	struct buffer_head * bh;

	if (!inode)
		return;
	if (!inode->i_dev) {
		memset(inode,0,sizeof(*inode));
		return;
	}
	if (inode->i_count>1) {
		printk("trying to free inode with count=%d\n",inode->i_count);
		panic("free_inode");
	}
	if (inode->i_nlinks)
		panic("trying to free inode with links");
	if (!(sb = get_super(inode->i_dev)))
		panic("trying to free inode on nonexistent device");
	if (inode->i_num < 1 || inode->i_num > sb->s_ninodes)
		panic("trying to free inode 0 or nonexistant inode");
	if (!(bh=sb->s_imap[inode->i_num>>13])) //8192     
		panic("nonexistent imap in superblock");
	if (clear_bit(inode->i_num&8191,bh->b_data))
		printk("free_inode: bit already cleared.\n\r");
	bh->b_dirt = 1;
	memset(inode,0,sizeof(*inode));
}

// 申请inode
struct m_inode * new_inode(int dev)
{
	struct m_inode * inode;
	struct super_block * sb;
	struct buffer_head * bh;
	int i,j;

	if (!(inode=get_empty_inode())) // 从系统中的inode表中获取一个inode
		return NULL;
	if (!(sb = get_super(dev)))
		panic("new_inode with unknown device");
	j = 8192;
	for (i=0 ; i<8 ; i++) // bit map中查找可用的inode
		if ((bh=sb->s_imap[i]))
			if ((j=find_first_zero(bh->b_data))<8192)
				break;
	if (!bh || j >= 8192 || j+i*8192 > sb->s_ninodes) {
		iput(inode);
		return NULL;
	}
	if (set_bit(j,bh->b_data)) // 将这个bit位置为1
		panic("new_inode: bit already set");
	bh->b_dirt = 1; // 配置好相关参数
	inode->i_count=1;
	inode->i_nlinks=1;
	inode->i_dev=dev;
	inode->i_uid=current->euid;
	inode->i_gid=current->egid;
	inode->i_dirt=1;
	inode->i_num = j + i*8192;
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	return inode;
}
