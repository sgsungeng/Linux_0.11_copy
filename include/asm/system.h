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

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)
#define nop() __asm__ ("nop"::)

#define iret() __asm__ ("iret"::) // interrupt return

/*
typedef struct gate_t
{
    u16 offset0;    // 段内偏移 0 ~ 15 位
    u16 selector;   // 代码段选择子
    u8 reserved;    // 保留不用
    u8 type : 4;    // 任务门/中断门 14 /陷阱门 15
    u8 segment : 1; // segment = 0 表示系统段
    u8 DPL : 2;     // 使用 int 指令访问的最低权限
    u8 present : 1; // 是否有效
    u16 offset1;    // 段内偏移 16 ~ 31 位 (not used in linux 0.11, used in high level cpu)
} _packed gate_t;   // 64bit
*/
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \ // eax = 0x00080000 + addr
	"movw %0,%%dx\n\t" \
	"movl %%eax,%1\n\t" \ // code selecotr(8) offset0=addr
	"movl %%edx,%2" \ //set to 32-47bit(reserved,type,segment,dpl,present)
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \ // set type, dpl=0(ring0 cpu) present=1
	"o" (*((char *) (gate_addr))), \ // offset0
	"o" (*(4+(char *) (gate_addr))), \ // reserved
	"d" ((char *) (addr)),"a" (0x00080000))

#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

// idt: interrupt descriptor table, address is 0x54b8 in head.s
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)
#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) | \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) | \
		((limit) & 0x0ffff); }
/*
// 描述符
typedef struct descriptor_t //* 共 8 个字节
{
    unsigned short limit_low;      // 段界限 0 ~ 15 位
    unsigned int base_low : 24;    // 基地址 0 ~ 23 位 16M
    unsigned char type : 4;        // 段类型
    unsigned char segment : 1;     // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;         // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;     // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;  // 段界限 16 ~ 19;
    unsigned char available : 1;   // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;   // 64 位扩展标志
    unsigned char big : 1;         // 32 位 还是 16 位;
    unsigned char granularity : 1; // 粒度 4KB 或 1B
    unsigned char base_high;       // 基地址 24 ~ 31 位
} _packed descriptor_t;
*/
#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1\n\t" \ 		// set limit_low
	"movw %%ax,%2\n\t" \ 			// the adress of init_task.tss set to base_low（0-15）
	"rorl $16,%%eax\n\t" \			// 0-15 16-31-->>16-31 0-15
	"movb %%al,%3\n\t" \ 			// 16-23 set to base_low
	"movb $" type ",%4\n\t" \ 		// tss type=0x89(1000 1001),1001 set to type segment = 0, dpl = 0, present = `
	"movb $0x00,%5\n\t" \			// limit_high, avaialbel, long_mode,big
	"movb %%ah,%6\n\t" \			//	24-31 set to base_high
	"rorl $16,%%eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7)) \
	)

#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")
