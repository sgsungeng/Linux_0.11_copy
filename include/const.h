#ifndef _CONST_H
#define _CONST_H

#define BUFFER_END 0x200000 // 没有使用

// i节点数据结构中i_mode字段的标志位
#define I_TYPE          0170000
#define I_DIRECTORY	0040000 // 目录文件
#define I_REGULAR       0100000
#define I_BLOCK_SPECIAL 0060000
#define I_CHAR_SPECIAL  0020000
#define I_NAMED_PIPE	0010000 
#define I_SET_UID_BIT   0004000
#define I_SET_GID_BIT   0002000

#endif
