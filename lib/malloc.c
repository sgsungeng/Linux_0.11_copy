/*
 * malloc.c --- a general purpose kernel memory allocator for Linux.
 * 
 * Written by Theodore Ts'o (tytso@mit.edu), 11/29/91
 *
 * This routine is written to be as fast as possible, so that it
 * can be called from the interrupt level.
 *
 * Limitations: maximum size of memory we can allocate using this routine
 *	is 4k, the size of a page in Linux.
 *
 * The general game plan is that each page (called a bucket) will only hold
 * objects of a given size.  When all of the object on a page are released,
 * the page can be returned to the general free pool.  When malloc() is
 * called, it looks for the smallest bucket size which will fulfill its
 * request, and allocate a piece of memory from that bucket pool.
 *
 * Each bucket has as its control block a bucket descriptor which keeps 
 * track of how many objects are in use on that page, and the free list
 * for that page.  Like the buckets themselves, bucket descriptors are
 * stored on pages requested from get_free_page().  However, unlike buckets,
 * pages devoted to bucket descriptor pages are never released back to the
 * system.  Fortunately, a system should probably only need 1 or 2 bucket
 * descriptor pages, since a page can hold 256 bucket descriptors (which
 * corresponds to 1 megabyte worth of bucket pages.)  If the kernel is using 
 * that much allocated memory, it's probably doing something wrong.  :-)
 *
 * Note: malloc() and free() both call get_free_page() and free_page()
 *	in sections of code where interrupts are turned off, to allow
 *	malloc() and free() to be safely called from an interrupt routine.
 *	(We will probably need this functionality when networking code,
 *	particularily things like NFS, is added to Linux.)  However, this
 *	presumes that get_free_page() and free_page() are interrupt-level
 *	safe, which they may not be once paging is added.  If this is the
 *	case, we will need to modify malloc() to keep a few unused pages
 *	"pre-allocated" so that it can safely draw upon those pages if
 * 	it is called from an interrupt routine.
 *
 * 	Another concern is that get_free_page() should not sleep; if it 
 *	does, the code is carefully ordered so as to avoid any race 
 *	conditions.  The catch is that if malloc() is called re-entrantly, 
 *	there is a chance that unecessary pages will be grabbed from the 
 *	system.  Except for the pages for the bucket descriptor page, the 
 *	extra pages will eventually get released back to the system, though,
 *	so it isn't all that bad.
 */

#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

struct bucket_desc {	/* 16 bytes */
	void			*page; // 页面
	struct bucket_desc	*next; // 下一个描述符
	void			*freeptr;// 空闲指针
	unsigned short		refcnt; // 引用计数
	unsigned short		bucket_size; // 本描述符的桶大小
};

// 桶描述符目录结构
struct _bucket_dir {	/* 8 bytes */
	int			size;
	struct bucket_desc	*chain;
};

/*
 * The following is the where we store a pointer to the first bucket
 * descriptor for a given size.  
 *
 * If it turns out that the Linux kernel allocates a lot of objects of a
 * specific size, then we may want to add that specific size to this list,
 * since that will allow the memory to be allocated more efficiently.
 * However, since an entire page must be dedicated to each specific size
 * on this list, some amount of temperance must be exercised here.
 *
 * Note that this list *must* be kept in order.
 */
struct _bucket_dir bucket_dir[] = { // 桶
	{ 16,	(struct bucket_desc *) 0}, // 桶描述符目录
	{ 32,	(struct bucket_desc *) 0},
	{ 64,	(struct bucket_desc *) 0},
	{ 128,	(struct bucket_desc *) 0},
	{ 256,	(struct bucket_desc *) 0},
	{ 512,	(struct bucket_desc *) 0},
	{ 1024,	(struct bucket_desc *) 0},
	{ 2048, (struct bucket_desc *) 0},
	{ 4096, (struct bucket_desc *) 0},
	{ 0,    (struct bucket_desc *) 0}};   /* End of list marker */

/*
 * This contains a linked list of free bucket descriptor blocks
 */
struct bucket_desc *free_bucket_desc = (struct bucket_desc *) 0;

/*
 * This routine initializes a bucket description page.
 */
static inline void init_bucket_desc()
{
	struct bucket_desc *bdesc, *first;
	int	i;
	
	first = bdesc = (struct bucket_desc *) get_free_page(); //申请一页来存放描述符
	if (!bdesc)
		panic("Out of memory in init_bucket_desc()");
	for (i = PAGE_SIZE/sizeof(struct bucket_desc); i > 1; i--) {
		bdesc->next = bdesc+1;
		bdesc++;
	}
	/*
	 * This is done last, to avoid race conditions in case 
	 * get_free_page() sleeps and this routine gets called again....
	 */
	bdesc->next = free_bucket_desc;
	free_bucket_desc = first;
}

void *malloc(unsigned int len)
{
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc;
	void			*retval;

	/*
	 * First we search the bucket_dir to find the right bucket change
	 * for this request.
	 */
	for (bdir = bucket_dir; bdir->size; bdir++)
		if (bdir->size >= len) // 找到一个大于等于的桶
			break;
	if (!bdir->size) { // 没有找到
		printk("malloc called with impossibly large argument (%d)\n",
			len);
		panic("malloc: bad arg");
	}
	/*
	 * Now we search for a bucket descriptor which has free space
	 */
	cli();	/* Avoid race conditions 关闭中断 */
	for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) 
		if (bdesc->freeptr) // 找到可用的的页面，如果没有找到bdesc = null
			break;
	/*
	 * If we didn't find a bucket with free space, then we'll 
	 * allocate a new one.
	 */
	if (!bdesc) { // 如果没有找到，
		char		*cp;
		int		i;

		if (!free_bucket_desc)	
			init_bucket_desc();
		bdesc = free_bucket_desc; //查找一个可用的描述符表
		free_bucket_desc = bdesc->next; // 将之前的描述符从空闲表中移除
		bdesc->refcnt = 0; // 配置描述符表
		bdesc->bucket_size = bdir->size;
		bdesc->page = bdesc->freeptr = (void *) (cp = (char *) get_free_page()); // 申请一页内存
		if (!cp)
			panic("Out of memory in kernel malloc()");
		/* Set up the chain of free objects */
		for (i=PAGE_SIZE/bdir->size; i > 1; i--) {
			*((char **) cp) = cp + bdir->size; // 设置空闲链表
			cp += bdir->size;
		}
		*((char **) cp) = 0; //最后一个清空
		bdesc->next = bdir->chain; /* OK, link it in! */ // 连接到桶中
		bdir->chain = bdesc;
	}
	retval = (void *) bdesc->freeptr; //分配一个块
	bdesc->freeptr = *((void **) retval); // 空闲链表指向下一个
	bdesc->refcnt++;
	sti();	/* OK, we're safe again */
	return(retval);
}

/*
 * Here is the free routine.  If you know the size of the object that you
 * are freeing, then free_s() will use that information to speed up the
 * search for the bucket descriptor.
 * 
 * We will #define a macro so that "free(x)" is becomes "free_s(x, 0)"
 */
void free_s(void *obj, int size) // 释放一个块
{
	void		*page;
	struct _bucket_dir	*bdir;
	struct bucket_desc	*bdesc, *prev;
	bdesc = prev = 0;
	/* Calculate what page this object lives in */
	page = (void *)  ((unsigned long) obj & 0xfffff000); //找到这个地址对应的page
	/* Now search the buckets looking for that page */
	for (bdir = bucket_dir; bdir->size; bdir++) {
		prev = 0;
		/* If size is zero then this conditional is always false */
		if (bdir->size < size)
			continue;
		for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
			if (bdesc->page == page)  // 找到page
				goto found;
			prev = bdesc;
		}
	}
	panic("Bad address passed to kernel free_s()");
found:
	cli(); /* To avoid race conditions */
	*((void **)obj) = bdesc->freeptr;
	bdesc->freeptr = obj; // 将其放入到链表头中
	bdesc->refcnt--;
	if (bdesc->refcnt == 0) { // 如果这一页没有人使用就释放
		/*
		 * We need to make sure that prev is still accurate.  It
		 * may not be, if someone rudely interrupted us....
		 */
		if ((prev && (prev->next != bdesc)) ||
		    (!prev && (bdir->chain != bdesc)))
			for (prev = bdir->chain; prev; prev = prev->next)
				if (prev->next == bdesc) // 找到prev
					break;
		if (prev) 
			prev->next = bdesc->next; // 从链表中移除
		else {
			if (bdir->chain != bdesc)
				panic("malloc bucket chains corrupted");
			bdir->chain = bdesc->next; 
		}
		free_page((unsigned long) bdesc->page); // 释放这个页面
		bdesc->next = free_bucket_desc;
		free_bucket_desc = bdesc; // 将描述符表释放回去
	}
	sti();
	return;
}

