/*
 * Written by ZhongYao Luo <luozhongyao@gmail.com>
 *
 * Copyright 2020 ZhongYao Luo
 */

#ifndef __DEFS_H__
#define __DEFS_H__

#include <stddef.h>

#ifndef offsetof
# define offsetof(type, member) (uintptr_t)&(((type*)0)->member) 
#endif

#ifndef container_of
# define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#endif /* __DEFS_H__*/

