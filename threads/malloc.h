#ifndef THREADS_MALLOC_H
#define THREADS_MALLOC_H

#include <debug.h>
#include <stddef.h>

void malloc_init(void);
void *malloc(size_t) __attribute__((malloc));
void *calloc(size_t, size_t) __attribute__((malloc));
void *realloc(void *, size_t);
void free(void *);

#ifdef LEAKCHECK
/* Use custom version of malloc that contains source information */
void *malloc_check(size_t, char const *) __attribute__((malloc));
void *realloc_check(void *, size_t, char const *);
#define malloc(size)         malloc_check(size, __func__)
#define realloc(alloc, size) realloc_check(alloc, size, __func__)

/* Enable leak checking, and check for leaks */
void malloc_enable_leak_check(void);
void malloc_check_leaks(void);
#endif

/* Tell the system that a particular allocation is not a memory leak */
void not_a_leak(void *);

#endif /* threads/malloc.h */
