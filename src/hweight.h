#ifndef _HWEIGHT_H__
#define _HWEIGHT_H__

#include <stdint.h>

#if defined(__x86_64__) || defined(__x86_32__)
#define ARCH_HAS_FAST_MULTIPLIER 1
#endif // __x86_64__

extern unsigned long __sw_hweight64(uint64_t w);
extern unsigned int __sw_hweight32(unsigned int w);
extern unsigned int __sw_hweight16(unsigned int w);
extern unsigned int __sw_hweight8(unsigned int w);

static inline unsigned int __arch_hweight32(unsigned int w)
{
    return __sw_hweight32(w);
}

static inline unsigned int __arch_hweight16(unsigned int w)
{
    return __sw_hweight16(w);
}

static inline unsigned int __arch_hweight8(unsigned int w)
{
    return __sw_hweight8(w);
}

static inline unsigned long __arch_hweight64(uint64_t w)
{   
    return __sw_hweight64(w);
}

#endif
