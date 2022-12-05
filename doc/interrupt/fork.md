fork是系统中第一个被使用的系统调用

```c++
static inline _syscall0(int,fork)

// 展开后如下
int fork(void)
{
	long __res;
	__asm__ volatile (
		"int $0x80"
		: "=a" (__res)
		: "0" (__NR_fork));
	if (__res >= 0)
			return (int) __res;
	errno = -__res;
	return -1; 
}


// 实际处理函数
sys_fork:
	call find_empty_process //  在task数组中查找空的task位置
	testl %eax,%eax
	js 1f
	push %gs // 配置段，用于线程拷贝
	pushl %esi
	pushl %edi
	pushl %ebp
	pushl %eax
	call copy_process
	addl $20,%esp
1:	ret


int find_empty_process(void)
{
	int i;
	BMB;
	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
		//查找空的last_task（）
		// 没有使用的pid
			if (task[i] && task[i]->pid == last_pid) goto repeat; 
	for(i=1 ; i<NR_TASKS ; i++) // 查找空的task
		if (!task[i])
			return i;
	return -EAGAIN;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
		long ebx,long ecx,long edx,
		long fs,long es,long ds,
		long eip,long cs,long eflags,long esp,long ss)
{
	struct task_struct *p;
	int i;
	struct file *f;
	// 获取一页内存
	p = (struct task_struct *) get_free_page(); // get a free page for task ... stack
	if (!p)
		return -EAGAIN;
	task[nr] = p;
	// 拷贝task信息
	// NOTE!: the following statement now work with gcc 4.3.2 now, and you
	// must compile _THIS_ memcpy without no -O of gcc.#ifndef GCC4_3
	*p = *current;	/* NOTE! this doesn't copy the supervisor stack */
	p->state = TASK_UNINTERRUPTIBLE;
	p->pid = last_pid;
	p->father = current->pid;
	p->counter = p->priority;
	p->signal = 0;
	p->alarm = 0;
	p->leader = 0;		/* process leadership doesn't inherit */
	p->utime = p->stime = 0;
	p->cutime = p->cstime = 0;
	p->start_time = jiffies;
	p->tss.back_link = 0;
	p->tss.esp0 = PAGE_SIZE + (long) p;
	p->tss.ss0 = 0x10;
	p->tss.eip = eip;
	p->tss.eflags = eflags;
	p->tss.eax = 0;  //这个是子函数的返回值
	p->tss.ecx = ecx;
	p->tss.edx = edx;
	p->tss.ebx = ebx;
	p->tss.esp = esp;
	p->tss.ebp = ebp;
	p->tss.esi = esi;
	p->tss.edi = edi;
	p->tss.es = es & 0xffff;
	p->tss.cs = cs & 0xffff;
	p->tss.ss = ss & 0xffff;
	p->tss.ds = ds & 0xffff;
	p->tss.fs = fs & 0xffff;
	p->tss.gs = gs & 0xffff;
	p->tss.ldt = _LDT(nr);
	p->tss.trace_bitmap = 0x80000000;
	if (last_task_used_math == current) // 如果当前父进程有使用协处理器，保存协处理器状态
		__asm__("clts ; fnsave %0"::"m" (p->tss.i387));
	if (copy_mem(nr,p)) { // 拷贝ldt以及页表
		task[nr] = NULL;
		free_page((long) p);
		return -EAGAIN;
	}
	for (i=0; i<NR_OPEN;i++) // file
		if ((f=p->filp[i]))
			f->f_count++;
	if (current->pwd) 
		current->pwd->i_count++;
	if (current->root)
		current->root->i_count++;
	if (current->executable)
		current->executable->i_count++;
	set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss)); 
	set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
	p->state = TASK_RUNNING;	/* do this last, just in case */
	return last_pid; // pid for parent progress return value
}



int copy_mem(int nr,struct task_struct * p) // set the ldt info to task p
{
	unsigned long old_data_base,new_data_base,data_limit;
	unsigned long old_code_base,new_code_base,code_limit;
	// get current's limit and base
	code_limit=get_limit(0x0f); // 1 index 1ï¼ 1 local descriptorï¼ 11 user mode ï¼ code segment
	data_limit=get_limit(0x17); // 10 index 2ï¼1 11 data segment
	old_code_base = get_base(current->ldt[1]);
	old_data_base = get_base(current->ldt[2]);
	if (old_data_base != old_code_base)
		panic("We don't support separate I&D");
	if (data_limit < code_limit)
		panic("Bad data_limit");
	new_data_base = new_code_base = nr * 0x4000000; // 64MB for one task
	p->start_code = new_code_base;
	set_base(p->ldt[1],new_code_base);
	set_base(p->ldt[2],new_data_base);
	if (copy_page_tables(old_data_base,new_data_base,data_limit)) {// 拷贝页表
		printk("free_page_tables: from copy_mem\n");
		free_page_tables(new_data_base,data_limit);
		return -ENOMEM;
	}
	return 0;
}

```