#ifndef __DEBUG_H__
#define __DEBUG_H__
#include <unistd.h>

#define BMB() __asm__("xchgw %bx, %bx")

#endif
