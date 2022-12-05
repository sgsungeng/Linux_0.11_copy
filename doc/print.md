# vsprintf 不定参数解析
## 关键宏定义
- va_list
  
  为`char *`的指针
    ```c++
    typedef char *va_list;
    ```
- __va_rounded_size
    
    4字节对齐

    (1 + 4 -1)/4 *4 = 4

    (2 + 4 -1)/4 *4 = 4

    (3 + 4 -1)/4 *4 = 4

    (5 + 4 -1)/4 *4 = 8

    ...
    ```c++
    #define __va_rounded_size(TYPE)  \
    (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))
    ```

- va_start
    
    获取下一个参数
    ```c++
        #define va_start(AP, LASTARG) 						\
        (AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
    ```

- va_end
    
    一种实现是直接赋值为结束
    ```c++
        #define va_end(ap) (ap = (va_list)0)
    ```
- va_arg
  
    return *((TYPE *)AP);
    AP++;
    ```c++
        #define va_arg(AP, TYPE)						\
        (AP += __va_rounded_size (TYPE),					\
        * ((TYPE *) (AP - __va_rounded_size (TYPE))))
    ```
## 重要函数
### `skip_atoi`
往下扫描，如果是数字，返回数字，并将指针指向第一个不是数字的地方
```c++
static int skip_atoi(const char **s)
{
	int i=0;

	while (is_digit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}
```
### do_div
除操作。输入：n 为被除数，base 为除数；结果：n 为商，函数返回值为余数
```c++
#define do_div(n,base) ({ \
int __res; \
__asm__("divl %4":"=a" (n),"=d" (__res):"0" (n),"1" (0),"r" (base)); \
__res; })
```

### vsprintf
主要功能是将fmt格式的输出到buf中
```c++
int vsprintf(char *buf, const char *fmt, va_list args)
{
	int len;
	int i;
	char * str;
	char *s;
	int *ip;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */

	for (str=buf ; *fmt ; ++fmt) {
		if (*fmt != '%') {// 普通字符，直接拷贝
			*str++ = *fmt;
			continue;
		}
		// 处理%开头的字符	
		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) { // check the flag after %
				case '-': flags |= LEFT; goto repeat;
				case '+': flags |= PLUS; goto repeat;
				case ' ': flags |= SPACE; goto repeat;
				case '#': flags |= SPECIAL; goto repeat;
				case '0': flags |= ZEROPAD; goto repeat;
				}
		
		/* get field width */
		field_width = -1;
		if (is_digit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			// printf("%*d", 3, 4): space space 4 
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;	
			if (is_digit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier  没有用到*/
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
			qualifier = *fmt;
			++fmt;
		}

		switch (*fmt) {
		case 'c': // 字符
			if (!(flags & LEFT))
				while (--field_width > 0)
					*str++ = ' ';
			*str++ = (unsigned char) va_arg(args, int);
			while (--field_width > 0)
				*str++ = ' ';
			break;

		case 's': //字符串
			s = va_arg(args, char *);
			len = strlen(s);
			if (precision < 0)
				precision = len;
			else if (len > precision)
				len = precision;

			if (!(flags & LEFT))
				while (len < field_width--)
					*str++ = ' ';
			for (i = 0; i < len; ++i)
				*str++ = *s++;
			while (len < field_width--)
				*str++ = ' ';
			break;

		case 'o': // 8进制数出
			str = number(str, va_arg(args, unsigned long), 8,
				field_width, precision, flags);
			break;

		case 'p': // 处理指针
			if (field_width == -1) {
				field_width = 8;
				flags |= ZEROPAD;
			}
			str = number(str,
				(unsigned long) va_arg(args, void *), 16,
				field_width, precision, flags);
			break;

		case 'x': // 处理16进制数字
			flags |= SMALL;
		case 'X':
			str = number(str, va_arg(args, unsigned long), 16,
				field_width, precision, flags);
			break;

		case 'd': // 处理数字
		case 'i':
			flags |= SIGN;
		case 'u':
			str = number(str, va_arg(args, unsigned long), 10,
				field_width, precision, flags);
			break;

		case 'n': // 将已经打印的字符输出到*ip中
			ip = va_arg(args, int *);
			*ip = (str - buf);
			break;

		default: // 处理转译字符
			if (*fmt != '%')
				*str++ = '%';
			if (*fmt)
				*str++ = *fmt;
			else
				--fmt;
			break;
		}
	}
	*str = '\0';
	return str-buf;
}

```
### printf
```c++
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt); // 获取fmt 后一个参数
    // 首先将printf传入的数据格式化输出到printbuf中
    // 然后通过系统调用写入到1号句柄中
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));
	va_end(args);
	
	return i;
}

```
### printk

```c++
int printk(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i=vsprintf(buf,fmt,args);
	va_end(args);
	__asm__("push %%fs\n\t"
		"push %%ds\n\t"
		"pop %%fs\n\t"
		"pushl %0\n\t"
		"pushl $buf\n\t"
		"pushl $0\n\t"
		"call tty_write\n\t"//直接调用 tty_write 0
		"addl $8,%%esp\n\t"
		"popl %0\n\t"
		"pop %%fs"
		::"r" (i):"ax","cx","dx");
	return i;
}
```