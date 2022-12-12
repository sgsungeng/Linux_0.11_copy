/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p) // 打印task
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void) // 打印所有的task
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

#define LATCH (1193180/HZ) // 10ms
extern int timer_interrupt(void);
extern int system_call(void);
union task_union {// 一页的大小，底部是task 顶部是栈
	struct task_struct task;
	char stack[PAGE_SIZE];
};

static union task_union init_task = {INIT_TASK,}; // 初始化1号进程

long volatile jiffies=0;
long startup_time=0;
struct task_struct *current = &(init_task.task);
struct task_struct *last_task_used_math = NULL;

struct task_struct * task[NR_TASKS] = {&(init_task.task), };
/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
void schedule(void) // 调度
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */

	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {
			if ((*p)->alarm && (*p)->alarm < jiffies) { // check alarm， 当前jiffies 已经大于 之前的jiffies + time了，需要发送信号
					(*p)->signal |= (1<<(SIGALRM-1)); // send alarm signal
					(*p)->alarm = 0;
				}
			// 过滤屏蔽信号集
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) && // sigalkill and sigstop canot be block
			(*p)->state==TASK_INTERRUPTIBLE) //get signal in interruptible  sleep with without bolock
				(*p)->state=TASK_RUNNING; // 将可中断睡眠进程唤醒
		}

/* this is the scheduler proper: */
	// 找到next
	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p) // 如果task [i] == null continue
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c) // get the stask of max counter 
				c = (*p)->counter, next = i; // 如果task[i]是运行状态，且 剩余时间片最多
		}
		if (c) break; // if c > 0 get the next else set the all task's counter as priority // 如果找到了就退出
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)// 否者设置所有时间片为其priority
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

int sys_pause(void) // 暂停系统调用
{
	current->state = TASK_INTERRUPTIBLE; //将自己设置为可中断睡眠，然后重新 调用
	schedule();
	return 0;
}

void sleep_on(struct task_struct **p) //将current睡眠到p中，不可中断睡眠
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task)) //初始进程不允许睡眠
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();//重新调度，如果即本进程睡眠了
	if (tmp) //如果本进程被唤醒了，就唤醒上一个
		tmp->state=0;
}

void interruptible_sleep_on(struct task_struct **p) // 可以中断睡眠
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) { // 如果等待队列前面还有其他的，就直接唤醒前面的
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;
}

void wake_up(struct task_struct **p) //唤醒p队列中的task
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.软盘相关的字程序，不管
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}
// 最多有64个定时器
#define TIME_REQUESTS 64

static struct timer_list {// 定时器链表，定时器数组
	long jiffies;
	void (*fn)();
	struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;
// 添加定时器，在jiffies 执行fn
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0) // 时间小于0直接执行
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)
				break; // 找到对应空位置
		if (p >= timer_list + TIME_REQUESTS)
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p; // 添加到队列头
		while (p->next && p->next->jiffies < p->jiffies) { // 升序 插入到对应位置
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

void do_timer(long cpl)// cpl 特权级别
{
	extern int beepcount;
	extern void sysbeepstop(void);

	if (beepcount) // 如果beep在响就是减减并判断是否要关闭
		if (!--beepcount)
			sysbeepstop();

	if (cpl)//添加执行时间
		current->utime++;
	else
		current->stime++;

	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)(); // 执行，且遍历，单个减少，不是所有的
		}
	}
	if (current_DOR & 0xf0)// 内核态
		do_floppy_timer();
	if ((--current->counter)>0) return;
	current->counter=0;
	if (!cpl) return;// 用户态重新调度
	schedule();
}

int sys_alarm(long seconds) // 设置alarm
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

int sys_getpid(void)
{
	return current->pid;
}

int sys_getppid(void)
{
	return current->father;
}

int sys_getuid(void)
{
	return current->uid;
}

int sys_geteuid(void)
{
	return current->euid;
}

int sys_getgid(void)
{
	return current->gid;
}

int sys_getegid(void)
{
	return current->egid;
}

int sys_nice(long increment) // 设置优先级
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}
// 调度初始化
void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes"); 
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss)); // 将0号进程的信息设置到cpu中
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	for(i=1;i<NR_TASKS;i++) { //clear the other 
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl"); // clear nt, return with normal mode in interrupt 
	ltr(0); // load tss to  TR register
	lldt(0); // load ldt 
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 set to 8253 controller */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */ // set timer interrupt is 100HZ/10 ms
	outb(LATCH >> 8 , 0x40);	/* MSB */ // 0x40 system timer 0x41 dma 0x42 beep
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21); // open timer interrupt
	set_system_gate(0x80,&system_call);
}
