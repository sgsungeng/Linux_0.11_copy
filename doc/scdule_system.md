调度系统是当前多任务系统必备的模块，linux 0.11的调度比较简单，只是单纯的时间片，但是也提供了一种方案，本文将从代码的角度分析linux 0.11的调度系统
# task_struct

```c++
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	 // 运行状态 -1 unrunnable, 0 runnable, >0 stopped 
	long counter; // 时间片
	long priority; // 优先级
	long signal; // 信号，支持32个
	struct sigaction sigaction[32]; // 信号对应的欣慰
	long blocked;	// 屏蔽信号集，与signal一一对应
/* various fields */
	int exit_code; // 退出码
	unsigned long start_code,end_code,end_data,brk,start_stack;
	long pid,father,pgrp,session,leader;
	unsigned short uid,euid,suid;
	unsigned short gid,egid,sgid;
	long alarm;
	long utime,stime,cutime,cstime,start_time;
	unsigned short used_math;
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */
	unsigned short umask;
	struct m_inode * pwd;
	struct m_inode * root;
	struct m_inode * executable;
	unsigned long close_on_exec;
	struct file * filp[NR_OPEN];
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];
/* tss for this task */
	struct tss_struct tss;
};
```