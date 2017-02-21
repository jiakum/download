#ifndef _BITS_H__
#define _BITS_H__

#include <stdint.h>
#define BIT(nr)             (1UL << (nr))
#define BIT_MASK(nr)        (1UL << ((nr) % __BITS_PER_LONG))
#define BIT_WORD(nr)        ((nr) / __BITS_PER_LONG)

/*
 *  * On ARMv5 and above those functions can be implemented around
 *   * the clz instruction for much better code efficiency.
 *    */
#if defined(__ARM_EABI__) || defined(__ARMEL__)

static inline int constant_fls(int x)
{
    int r = 32;

    if (!x)
        return 0;
    if (!(x & 0xffff0000u)) {
        x <<= 16;
        r -= 16;
    }
    if (!(x & 0xff000000u)) {
        x <<= 8;
        r -= 8;
    }
    if (!(x & 0xf0000000u)) {
        x <<= 4;
        r -= 4;
    }
    if (!(x & 0xc0000000u)) {
        x <<= 2;
        r -= 2;
    }
    if (!(x & 0x80000000u)) {
        x <<= 1;
        r -= 1;
    }
    return r;
}

static inline int fls(int x)
{
    int ret;

    if (__builtin_constant_p(x))
        return constant_fls(x);

    asm("clz\t%0, %1" : "=r" (ret) : "r" (x));
    ret = 32 - ret;
    return ret;
}

#define __fls(x) (fls(x) - 1)
#define ffs(x) ({ unsigned long __t = (x); fls(__t & -__t); })
#define __ffs(x) (ffs(x) - 1)
#define ffz(x) __ffs( ~(x) )

#include <asm/types.h>

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void __set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static inline void __clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void __change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p ^= mask;
}

/**
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static inline int __test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static inline int __test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

/* WARNING: non atomic and it can be reordered! */
static inline int __test_and_change_bit(int nr,
					    volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (__BITS_PER_LONG-1)));
}

#endif // defined(__ARM_EABI__) || defined(__ARMEL__)

#if defined(__x86_64__) || defined(_X86_32__)

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
/* Technically wrong, but this avoids compilation errors on some gcc
 *    versions. */
#define BITOP_ADDR(x) "=m" (*(volatile long *) (x))
#else
#define BITOP_ADDR(x) "+m" (*(volatile long *) (x))
#endif

#define ADDR                BITOP_ADDR(addr)

/**
 *  * __ffs - find first set bit in word
 *   * @word: The word to search
 *    *
 *     * Undefined if no bit exists, so code should check against 0 first.
 *      */
static inline unsigned long __ffs(unsigned long word)
{
    asm("bsf %1,%0"
        : "=r" (word)
        : "rm" (word));
    return word;
}

/**
 *  * ffz - find first zero bit in word
 *   * @word: The word to search
 *    *
 *     * Undefined if no zero exists, so code should check against ~0UL first.
 *      */
static inline unsigned long ffz(unsigned long word)
{
    asm("bsf %1,%0"
        : "=r" (word)
        : "r" (~word));
    return word;
}

static inline int fls(int x)
{
    int r;

#ifdef __x86_64__
    long tmp = -1;
    asm("bsrl %1,%0"
        : "=r" (r)
        : "rm" (x), "0" (tmp));
#else
    asm("bsrl %1,%0\n\t"
        "jnz 1f\n\t"
        "movl $-1,%0\n"
        "1:" : "=r" (r) : "rm" (x));
#endif
    return r + 1;
}

static inline int constant_test_bit(int nr, const void *addr)
{
    const uint32_t *p = (const uint32_t *)addr;
    return ((1UL << (nr & 31)) & (p[nr >> 5])) != 0;
}
static inline int variable_test_bit(int nr, const void *addr)
{
    uint8_t v;
    const uint32_t *p = (const uint32_t *)addr;

    asm("btl %2,%1; setc %0" : "=qm" (v) : "m" (*p), "Ir" (nr));
    return v;
}

#define test_bit(nr,addr) \
    (__builtin_constant_p(nr) ? \
     constant_test_bit((nr),(addr)) : \
      variable_test_bit((nr),(addr)))

static inline void set_bit(int nr, void *addr)
{   
    asm("btsl %1,%0" : "+m" (*(uint32_t *)addr) : "Ir" (nr));
}

static inline void __set_bit(int nr, volatile unsigned long *addr)
{
    asm volatile("bts %1,%0" : ADDR : "Ir" (nr) : "memory");
}

static inline void __clear_bit(int nr, volatile unsigned long *addr)
{
    asm volatile("btr %1,%0" : ADDR : "Ir" (nr));
}

static inline void __change_bit(int nr, volatile unsigned long *addr)
{
    asm volatile("btc %1,%0" : ADDR : "Ir" (nr));
}

static inline int __test_and_set_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;

    asm("bts %2,%1\n\t"
        "sbb %0,%0"
        : "=r" (oldbit), ADDR
        : "Ir" (nr));
    return oldbit;
}

static inline int __test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;

    asm volatile("btr %2,%1\n\t"
        "sbb %0,%0"
        : "=r" (oldbit), ADDR
        : "Ir" (nr));
    return oldbit;
}

static inline int __test_and_change_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;

    asm volatile("btc %2,%1\n\t"
        "sbb %0,%0"
        : "=r" (oldbit), ADDR
        : "Ir" (nr) : "memory");

    return oldbit;
}

#endif //__x86_64__


#define set_bit              __set_bit
#define clear_bit            __clear_bit
#define chage_bit            __change_bit
#define test_and_set_bit     __test_and_set_bit
#define test_and_clear_bit   __test_and_clear_bit
#define test_and_change_bit  __test_and_change_bit

#define for_each_set_bit(bit, addr, size) \
    for ((bit) = find_first_bit((addr), (size));        \
             (bit) < (size);                    \
                  (bit) = find_next_bit((addr), (size), (bit) + 1))
                
                /* same as for_each_set_bit() but use bit as value to start with */
#define for_each_set_bit_from(bit, addr, size) \
    for ((bit) = find_next_bit((addr), (size), (bit));  \
             (bit) < (size);                    \
                  (bit) = find_next_bit((addr), (size), (bit) + 1))

#define for_each_clear_bit(bit, addr, size) \
    for ((bit) = find_first_zero_bit((addr), (size));   \
                 (bit) < (size);                    \
                          (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

        /* same as for_each_clear_bit() but use bit as value to start with */
#define for_each_clear_bit_from(bit, addr, size) \
        for ((bit) = find_next_zero_bit((addr), (size), (bit)); \
                 (bit) < (size);                    \
                  (bit) = find_next_zero_bit((addr), (size), (bit) + 1))


#endif
