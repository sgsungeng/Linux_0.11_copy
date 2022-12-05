#ifndef _A_OUT_H
#define _A_OUT_H

#define __GNU_EXEC_MACROS__

struct exec {
  unsigned long a_magic;	/* Use macros N_MAGIC, etc for access 执行文件魔数*/
  unsigned a_text;		/* length of text, in bytes 代码段长度 */
  unsigned a_data;		/* length of data, in bytes 数据段长度*/
  unsigned a_bss;		/* length of uninitialized data area for file, in bytes 未初始化数据段长度 */
  unsigned a_syms;		/* length of symbol table data in file, in bytes 符号表长度 */
  unsigned a_entry;		/* start address  执行开始位置*/
  unsigned a_trsize;		/* length of relocation info for text, in bytes 代码重定为信息长度 */
  unsigned a_drsize;		/* length of relocation info for data, in bytes 数据重定位信息长度 */
};

#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

#ifndef OMAGIC
/* Code indicating object file or impure executable.  */
#define OMAGIC 0407
/* Code indicating pure executable.  */
#define NMAGIC 0410
/* Code indicating demand-paged executable.  */
#define ZMAGIC 0413
#endif /* not OMAGIC */

#ifndef N_BADMAG
#define N_BADMAG(x)					\ // 如果不是上面三种魔数就不是可执行文件
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)
#endif

#define _N_BADMAG(x)					\
 (N_MAGIC(x) != OMAGIC && N_MAGIC(x) != NMAGIC		\
  && N_MAGIC(x) != ZMAGIC)

#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec)) //1024 -文件头末端

#ifndef N_TXTOFF
#define N_TXTOFF(x) \ // 代码部分偏移值 如果是zmagic文件执行位置就从1024开始执行，否则从文件头末端开始执行
 (N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof (struct exec) : sizeof (struct exec))
#endif

#ifndef N_DATOFF // 数据段开始偏移值
#define N_DATOFF(x) (N_TXTOFF(x) + (x).a_text)
#endif

#ifndef N_TRELOFF // 代码重定向信息开始偏移值
#define N_TRELOFF(x) (N_DATOFF(x) + (x).a_data)
#endif

#ifndef N_DRELOFF // 数据重定向信息开始偏移值
#define N_DRELOFF(x) (N_TRELOFF(x) + (x).a_trsize)
#endif

#ifndef N_SYMOFF// 符号表信息开始偏移值
#define N_SYMOFF(x) (N_DRELOFF(x) + (x).a_drsize)
#endif

#ifndef N_STROFF // 字符串信息开始偏移值
#define N_STROFF(x) (N_SYMOFF(x) + (x).a_syms)
#endif

/* Address of text segment in memory after it is loaded.  */
#ifndef N_TXTADDR // 代码加载到内存中的线性地址位置
#define N_TXTADDR(x) 0
#endif

/* Address of data segment in memory after it is loaded.
   Note that it is up to you to define SEGMENT_SIZE
   on machines not listed here.  */
#if defined(vax) || defined(hp300) || defined(pyr)
#define SEGMENT_SIZE PAGE_SIZE
#endif
#ifdef	hp300
#define	PAGE_SIZE	4096
#endif
#ifdef	sony
#define	SEGMENT_SIZE	0x2000
#endif	/* Sony.  */
#ifdef is68k
#define SEGMENT_SIZE 0x20000
#endif
#if defined(m68k) && defined(PORTAR)
#define PAGE_SIZE 0x400
#define SEGMENT_SIZE PAGE_SIZE
#endif

#define PAGE_SIZE 4096
#define SEGMENT_SIZE 1024

#define _N_SEGMENT_ROUND(x) (((x) + SEGMENT_SIZE - 1) & ~(SEGMENT_SIZE - 1)) // 按照段对齐后的最后地址

#define _N_TXTENDADDR(x) (N_TXTADDR(x)+(x).a_text) //代码段段尾地址

#ifndef N_DATADDR
#define N_DATADDR(x) \
    (N_MAGIC(x)==OMAGIC? (_N_TXTENDADDR(x)) \ // 数据段开始位置 如果是OMAGIC则代码段后就是数据段
     : (_N_SEGMENT_ROUND (_N_TXTENDADDR(x)))) // 否则要段对齐
#endif

/* Address of bss segment in memory after it is loaded.  */
#ifndef N_BSSADDR
#define N_BSSADDR(x) (N_DATADDR(x) + (x).a_data) // bss地址
#endif

#ifndef N_NLIST_DECLARED // 符号表记录结构体
struct nlist {
  union {
    char *n_name;
    struct nlist *n_next;
    long n_strx;
  } n_un;
  unsigned char n_type;
  char n_other;
  short n_desc;
  unsigned long n_value;
};
#endif
// nlist中的ntype字段值
#ifndef N_UNDF
#define N_UNDF 0
#endif
#ifndef N_ABS
#define N_ABS 2
#endif
#ifndef N_TEXT
#define N_TEXT 4
#endif
#ifndef N_DATA
#define N_DATA 6
#endif
#ifndef N_BSS
#define N_BSS 8
#endif
#ifndef N_COMM
#define N_COMM 18
#endif
#ifndef N_FN
#define N_FN 15
#endif
// nlist中的ntype屏蔽码（8进制）
#ifndef N_EXT
#define N_EXT 1 // 0x1 符号是否是外部的
#endif
#ifndef N_TYPE
#define N_TYPE 036 // = 0x1e = 1，1110 0x1e文件类型
#endif
#ifndef N_STAB
#define N_STAB 0340 // = 0xe0 文件类型
#endif

/* The following type indicates the definition of a symbol as being
   an indirect reference to another symbol.  The other symbol
   appears as an undefined reference, immediately following this symbol.

   Indirection is asymmetrical.  The other symbol's value will be used
   to satisfy requests for the indirect symbol, but not vice versa.
   If the other symbol does not have a definition, libraries will
   be searched to find a definition.  */
#define N_INDR 0xa 

/* The following symbols refer to set elements.
   All the N_SET[ATDB] symbols with the same name form one set.
   Space is allocated for the set in the text section, and each set
   element's value is stored into one word of the space.
   The first word of the space is the length of the set (number of elements).

   The address of the set is made into an N_SETV symbol
   whose name is the same as the name of the set.
   This symbol acts like a N_DATA global symbol
   in that it can satisfy undefined external references.  */

/* These appear as input to LD, in a .o file.  */
#define	N_SETA	0x14		/* Absolute set element symbol */ 
#define	N_SETT	0x16		/* Text set element symbol */ 
#define	N_SETD	0x18		/* Data set element symbol */ 
#define	N_SETB	0x1A		/* Bss set element symbol */

/* This is output from LD.  */
#define N_SETV	0x1C		/* Pointer to set vector in data area.  */

#ifndef N_RELOCATION_INFO_DECLARED

/* This structure describes a single relocation to be performed.
   The text-relocation section of the file is a vector of these structures,
   all of which apply to the text section.
   Likewise, the data-relocation section applies to the data section.  */

struct relocation_info
{
  /* Address (within segment) to be relocated.  */
  int r_address;
  /* The meaning of r_symbolnum depends on r_extern.  */
  unsigned int r_symbolnum:24;
  /* Nonzero means value is a pc-relative offset
     and it should be relocated for changes in its own address
     as well as for changes in the symbol or section specified.  */
  unsigned int r_pcrel:1;
  /* Length (as exponent of 2) of the field to be relocated.
     Thus, a value of 2 indicates 1<<2 bytes.  */
  unsigned int r_length:2;
  /* 1 => relocate with value of symbol.
          r_symbolnum is the index of the symbol
	  in file's the symbol table.
     0 => relocate with the address of a segment.
          r_symbolnum is N_TEXT, N_DATA, N_BSS or N_ABS
	  (the N_EXT bit may be set also, but signifies nothing).  */
  unsigned int r_extern:1;
  /* Four bits that aren't used, but when writing an object file
     it is desirable to clear them.  */
  unsigned int r_pad:4;
};
#endif /* no N_RELOCATION_INFO_DECLARED.  */


#endif /* __A_OUT_GNU_H__ */
