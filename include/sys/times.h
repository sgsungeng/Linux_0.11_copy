#ifndef _TIMES_H
#define _TIMES_H

#include <sys/types.h>

struct tms {
	time_t tms_utime; // 用户使用cpu时间
	time_t tms_stime; // 内核是有cpu时间
	time_t tms_cutime; // 已经终止的子进程使用的cpu时间
	time_t tms_cstime;
};

extern time_t times(struct tms * tp);

#endif
