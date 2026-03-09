#ifndef __LIB_STDLIB_H
#define __LIB_STDLIB_H

#include <stddef.h>

/* Standard functions. */
int atoi(char const *);
void qsort(void *array, size_t cnt, size_t size, int (*compare)(void const *, void const *));
void *bsearch(
	void const *key,
	void const *array,
	size_t cnt,
	size_t size,
	int (*compare)(void const *, void const *)
);

/* Nonstandard functions. */
void sort(
	void *array,
	size_t cnt,
	size_t size,
	int (*compare)(void const *, void const *, void *aux),
	void *aux
);
void *binary_search(
	void const *key,
	void const *array,
	size_t cnt,
	size_t size,
	int (*compare)(void const *, void const *, void *aux),
	void *aux
);

#endif /* lib/stdlib.h */
