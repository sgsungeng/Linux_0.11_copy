# 总述
init的代码位于init/main.c中
主要工作如下
- 初始化memory
- 初始化中断
- 初始化块设备
- 初始化字符设备
- 初始化调度系统
- 初始化文件缓冲区
- 初始话硬盘
- 初始话软盘
- 开启中断
- 进入系统模式
- 产生1号进程并执行init函数
这里主要讲跳入到
# 代码分析
这里对init代码进行简单分析，重要的子系统留到其他文档中分析
- 内存初始化
读取并配置buffer以及主内存大小，我们默认大于16M
```c++
// EXT_MEM_K =(*(unsigned short *)0x90002) 从0x90002读取内存大小
memory_end = (1<<20) + (EXT_MEM_K<<10); // 1M + 3B80(*(unsined short*)90002) << 10 = 16646144
memory_end &= 0xfffff000;
if (memory_end > 16*1024*1024)
    memory_end = 16*1024*1024;
if (memory_end > 12*1024*1024) 
    buffer_memory_end = 4*1024*1024;
else if (memory_end > 6*1024*1024)
    buffer_memory_end = 2*1024*1024;
else
    buffer_memory_end = 1*1024*1024;
main_memory_start = buffer_memory_end; // 4194304
#ifdef RAMDISK // 配置rmdisk 暂时不考虑 TOTO:
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
mem_init(main_memory_start,memory_end);
```
- 初始化中断
    ```c++
    trap_init();


    ```
- 初始化字符设备
    ```c++
	blk_dev_init();
    ```
- 初始化字符设备
    ```c++
	chr_dev_init();
    ```
- 初始化tty设备
    ```c++
	tty_init();
    ```
- 初始化系统时间
    ```c++
	time_init();
    ```
- 初始化调度系统
    ```c++
	sched_init();
    ```
- 初始化文件缓冲区
    ```c++
	buffer_init(buffer_memory_end);
    ```
- 初始化硬盘
    ```c++
	hd_init();
    ```
- 初始化软盘
    ```c++
	floppy_init();
    ```
- 开启中断
    ```c++
	sti();

    #define sti() __asm__ ("sti"::)
    ```
- 进入用户模式

    主要是 通过构造中断返回的栈空间，然后中断返回进入了用户模式
    ```c++
	move_to_user_mode();

    #define move_to_user_mode() \
    __asm__ ("movl %%esp,%%eax\n\t" \ // current infomation
	"pushl $0x17\n\t" \ //ss 0001 0111  00010(selector index)1(ldt)11(user mode) the indexreplace in {0,code,data} ldt in init_task
	"pushl %%eax\n\t" \ // esp
	"pushfl\n\t" \ // eflags
	"pushl $0x0f\n\t" \ // cs 1(orginal is 8)1(ldt)11 user state,
	"pushl $1f\n\t" \ // eip
	"iret\n" \ // before iret: eip cs eflags esp ss
	"1:\tmovl $0x17,%%eax\n\t" \
	"movw %%ax,%%ds\n\t" \
	"movw %%ax,%%es\n\t" \
	"movw %%ax,%%fs\n\t" \
	"movw %%ax,%%gs" \
	:::"ax")
    ```
- 打开1号进程
  
    建立1号进程， 0号进程进入暂停
    ```c++
    if (!fork()) {		/* we count on this going ok */
		init();
	}
    for(;;) pause();
    ```
# 1号进程

```c
ROOT_DEV = ORIG_ROOT_DEV; 
// #define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
drive_info = DRIVE_INFO;


static char * argv_rc[] = { "/bin/sh", NULL };
static char * envp_rc[] = { "HOME=/", NULL };

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

void init(void)
{
	int pid,i;
    // 根据根设备号读取根设备，同时挂着根文件系统
	setup((void *) &drive_info);
    // 打开tty0并作为复制作为标准输入，输出，错误
	(void) open("/dev/tty0",O_RDWR,0);
	(void) dup(0);
	(void) dup(0);
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {
		close(0);
		if (open("/etc/rc",O_RDONLY,0))
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);
		_exit(2);
	}
	if (pid>0)
		while (pid != wait(&i))
			/* nothing */;
	while (1) {
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);
			(void) dup(0);
			(void) dup(0);
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}

```