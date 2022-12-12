#ifndef _SYS_UTSNAME_H
#define _SYS_UTSNAME_H

#include <sys/types.h>
// 系统名字
struct utsname {
	char sysname[9]; // 系统名称
	char nodename[9]; // 主机名称
	char release[9]; // 发行版本
	char version[9]; 
	char machine[9]; // 硬件名称
};

extern int uname(struct utsname * utsbuf);

#endif
