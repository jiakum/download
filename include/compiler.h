
#ifndef _DOWNLOAD_COMPILER_H__
#define _DOWNLOAD_COMPILER_H__

#ifdef offsetof
#undef offsetof
#endif
#define __compiler_offsetof(a,b) __builtin_offsetof(a,b)
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({          \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})


#endif
