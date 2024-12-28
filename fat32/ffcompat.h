#ifndef _FFCOMPAT_H_
#define _FFCOMPAT_H_

// Include only types.h, not defs.h (which is causing the pagetable_t errors)
#include "kernel/types.h"

// Basic type definitions needed by FAT32
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned int   UINT;
typedef uint32        DWORD;

// Function declarations (normally from string.h)
int strncmp(const char*, const char*, uint);
void* memmove(void*, const void*, uint);

// Function mappings with correct type signatures
#define memset(dst, val, cnt)  memset_custom(dst, val, cnt)
#define memcpy(dst, src, cnt)  memmove(dst, src, cnt)
#define memcmp(ptr1, ptr2, cnt) strncmp((const char*)ptr1, (const char*)ptr2, cnt)
#define strchr(str, chr)       strchr_custom(str, chr)

// Custom implementations
static inline void* memset_custom(void* dst, int val, uint cnt) {
    unsigned char* ptr = (unsigned char*)dst;
    while (cnt-- > 0) *ptr++ = val;
    return dst;
}

static inline char* strchr_custom(const char* str, int chr) {
    while (*str && *str != chr) str++;
    return (*str == chr) ? (char*)str : 0;
}

#endif // _FFCOMPAT_H_
