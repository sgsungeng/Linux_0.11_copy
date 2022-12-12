#ifndef _HEAD_H
#define _HEAD_H

typedef struct desc_struct { // 描述符
	unsigned long a,b;
} desc_table[256];

extern unsigned long pg_dir[1024]; // 内存页目录数组
extern desc_table idt,gdt; // 中断描述符 全局描述符

#define GDT_NUL 0
#define GDT_CODE 1
#define GDT_DATA 2
#define GDT_TMP 3

#define LDT_NUL 0
#define LDT_CODE 1
#define LDT_DATA 2

#endif
