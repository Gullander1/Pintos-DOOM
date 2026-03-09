#include "threads/malloc.h"

#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#include <debug.h>
#include <list.h>
#include <round.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* A simple implementation of malloc().

  The size of each request, in bytes, is rounded up to a power
  of 2 and assigned to the "descriptor" that manages blocks of
  that size.  The descriptor keeps a list of free blocks.  If
  the free list is nonempty, one of its blocks is used to
  satisfy the request.

  Otherwise, a new page of memory, called an "arena", is
  obtained from the page allocator (if none is available,
  malloc() returns a null pointer).  The new arena is divided
  into blocks, all of which are added to the descriptor's free
  list.  Then we return one of the new blocks.

  When we free a block, we add it to its descriptor's free list.
  But if the arena that the block was in now has no in-use
  blocks, we remove all of the arena's blocks from the free list
  and give the arena back to the page allocator.

  We can't handle blocks bigger than 2 kB using this scheme,
  because they're too big to fit in a single page with a
  descriptor.  We handle those by allocating contiguous pages
  with the page allocator and sticking the allocation size at
  the beginning of the allocated block's arena header. */

/* Remove leak check aliases. */
#ifdef malloc
#undef malloc
#endif

#ifdef realloc
#undef realloc
#endif

/* Descriptor. */
struct desc {
	size_t block_size;       /* Size of each element in bytes. */
	size_t blocks_per_arena; /* Number of blocks in an arena. */
	struct list free_list;   /* List of free blocks. */
	struct lock lock;        /* Lock. */
};

/* Magic number for detecting arena corruption. */
#define ARENA_MAGIC 0x9a548eed

/* Arena. */
struct arena {
	unsigned magic;    /* Always set to ARENA_MAGIC. */
	struct desc *desc; /* Owning descriptor, null for big block. */
	size_t free_cnt;   /* Free blocks; pages in big block. */
};

/* Block header for leak-checking. */
struct block_header {
	struct list_elem alloc_list; /* List element for all allocations. */
};

/* Free block. */
struct block {
#ifdef LEAKCHECK
	struct block_header header; /* Header of the block, for leak checking. */
#endif
	struct list_elem free_elem; /* Free list element, only used for free blocks.
	                             * Otherwise, the start of the actual allocation.
	                             * Must be the last member.*/
};

/* Size of the block header - we don't use sizeof to allow the size to be zero. */
static size_t const header_size = offsetof(struct block, free_elem);

/* Our set of descriptors. */
static struct desc descs[10]; /* Descriptors. */
static size_t desc_cnt;       /* Number of descriptors. */

#ifdef LEAKCHECK
/* Data structures for checking for memory leaks. */
static struct list alloc_list;
static struct lock alloc_list_lock;
#endif

static struct arena *block_to_arena(struct block *);
static struct block *arena_to_block(struct arena *, size_t idx);

static void *block_to_alloc(struct block *);
static struct block *alloc_to_block(void *);

static void init_block(struct block *);
static void free_block(struct block *);

/* Initializes the malloc() descriptors. */
void malloc_init(void)
{
	size_t block_size;

	for (block_size = 16; block_size < PGSIZE / 2; block_size *= 2) {
		struct desc *d = &descs[desc_cnt++];
		ASSERT(desc_cnt <= sizeof descs / sizeof *descs);
		d->block_size = block_size;
		d->blocks_per_arena = (PGSIZE - sizeof(struct arena)) / block_size;
		list_init(&d->free_list);
		lock_init(&d->lock);
	}

#ifdef LEAKCHECK
	list_init(&alloc_list);
	lock_init(&alloc_list_lock);
#endif
}

/* Obtains and returns a new block of at least SIZE bytes.
  Returns a null pointer if memory is not available. */
void *malloc(size_t size)
{
	struct desc *d;
	struct block *b;
	struct arena *a;

	/* A null pointer satisfies a request for 0 bytes. */
	if (size == 0) {
		return NULL;
	}

	/* Account for any additional memory that is required by the block itself. */
	size += header_size;

	/* Find the smallest descriptor that satisfies a SIZE-byte
	  request. */
	for (d = descs; d < descs + desc_cnt; d++) {
		if (d->block_size >= size) {
			break;
		}
	}
	if (d == descs + desc_cnt) {
		/* SIZE is too big for any descriptor.
		  Allocate enough pages to hold SIZE plus an arena. */
		size_t page_cnt = DIV_ROUND_UP(size + sizeof *a, PGSIZE);
		a = palloc_get_multiple(0, page_cnt);
		if (a == NULL) {
			return NULL;
		}

		/* Initialize the arena to indicate a big block of PAGE_CNT
		  pages, and return it. */
		a->magic = ARENA_MAGIC;
		a->desc = NULL;
		a->free_cnt = page_cnt;
		b = (struct block *) (a + 1);
		init_block(b);
		return block_to_alloc(b);
	}

	lock_acquire(&d->lock);

	/* If the free list is empty, create a new arena. */
	if (list_empty(&d->free_list)) {
		size_t i;

		/* Allocate a page. */
		a = palloc_get_page(0);
		if (a == NULL) {
			lock_release(&d->lock);
			return NULL;
		}

		/* Initialize arena and add its blocks to the free list. */
		a->magic = ARENA_MAGIC;
		a->desc = d;
		a->free_cnt = d->blocks_per_arena;
		for (i = 0; i < d->blocks_per_arena; i++) {
			struct block *b = arena_to_block(a, i);
			list_push_back(&d->free_list, &b->free_elem);
		}
	}

	/* Get a block from free list and return it. */
	b = list_entry(list_pop_front(&d->free_list), struct block, free_elem);
	a = block_to_arena(b);
	a->free_cnt--;
	lock_release(&d->lock);

	init_block(b);
	return block_to_alloc(b);
}

/* Allocates and return A times B bytes initialized to zeroes.
  Returns a null pointer if memory is not available. */
void *calloc(size_t a, size_t b)
{
	void *p;
	size_t size;

	/* Calculate block size and make sure it fits in size_t. */
	size = a * b;
	if (size < a || size < b) {
		return NULL;
	}

	/* Allocate and zero memory. */
	p = malloc(size);
	if (p != NULL) {
		memset(p, 0, size);
	}

	return p;
}

/* Returns the number of bytes allocated for BLOCK. */
static size_t block_size(struct block *block)
{
	struct arena *a = block_to_arena(block);
	struct desc *d = a->desc;

	return d != NULL ? d->block_size : PGSIZE * a->free_cnt - pg_ofs(block);
}

/* Attempts to resize OLD_BLOCK to NEW_SIZE bytes, possibly
  moving it in the process.
  If successful, returns the new block; on failure, returns a
  null pointer.
  A call with null OLD_BLOCK is equivalent to malloc(NEW_SIZE).
  A call with zero NEW_SIZE is equivalent to free(OLD_BLOCK). */
void *realloc(void *old_block, size_t new_size)
{
	if (new_size == 0) {
		free(old_block);
		return NULL;
	}
	else {
		void *new_block = malloc(new_size);
		if (old_block != NULL && new_block != NULL) {
			size_t old_size = block_size(alloc_to_block(old_block)) - header_size;
			size_t min_size = new_size < old_size ? new_size : old_size;
			memcpy(new_block, old_block, min_size);
			free(old_block);
		}
		return new_block;
	}
}

/* Frees block P, which must have been previously allocated with
  malloc(), calloc(), or realloc(). */
void free(void *p)
{
	if (p != NULL) {
		struct block *b = alloc_to_block(p);
		struct arena *a = block_to_arena(b);
		struct desc *d = a->desc;

		free_block(b);

		if (d != NULL) {
			/* It's a normal block.  We handle it here. */

#ifndef NDEBUG
			/* Clear the block to help detect use-after-free bugs. */
			memset(p, 0xcc, d->block_size - header_size);

#endif

			lock_acquire(&d->lock);

			/* Add block to free list. */
			list_push_front(&d->free_list, &b->free_elem);

			/* If the arena is now entirely unused, free it. */
			if (++a->free_cnt >= d->blocks_per_arena) {
				size_t i;

				ASSERT(a->free_cnt == d->blocks_per_arena);
				for (i = 0; i < d->blocks_per_arena; i++) {
					struct block *b = arena_to_block(a, i);
					list_remove(&b->free_elem);
				}
				palloc_free_page(a);
			}

			lock_release(&d->lock);
		}
		else {
			/* It's a big block.  Free its pages. */
			palloc_free_multiple(a, a->free_cnt);
			return;
		}
	}
}

/* Returns the arena that block B is inside. */
static struct arena *block_to_arena(struct block *b)
{
	struct arena *a = pg_round_down(b);

	/* Check that the arena is valid. */
	ASSERT(a != NULL);
	ASSERT(a->magic == ARENA_MAGIC);

	/* Check that the block is properly aligned for the arena. */
	ASSERT(a->desc == NULL || (pg_ofs(b) - sizeof *a) % a->desc->block_size == 0);
	ASSERT(a->desc != NULL || pg_ofs(b) == sizeof *a);

	return a;
}

/* Returns the (IDX - 1)'th block within arena A. */
static struct block *arena_to_block(struct arena *a, size_t idx)
{
	ASSERT(a != NULL);
	ASSERT(a->magic == ARENA_MAGIC);
	ASSERT(idx < a->desc->blocks_per_arena);
	return (struct block *) ((uint8_t *) a + sizeof *a + idx * a->desc->block_size);
}

/* Returns the offset into a block that should be returned from malloc. */
static void *block_to_alloc(struct block *b)
{
	return &b->free_elem;
}

/* Returns the block of an allocation. */
static struct block *alloc_to_block(void *alloc)
{
	return (struct block *) ((uint8_t *) alloc - header_size);
}

#ifdef LEAKCHECK

static bool record_leaks = false;

/* Version of malloc that adds some metadata. */
void *malloc_check(size_t size, char const *location)
{
	if (!record_leaks) {
		return malloc(size);
	}

	// Add space for "location".
	size_t location_size = strlen(location);
	void *allocated = malloc(size + location_size + 2);

	lock_acquire(&alloc_list_lock);

	// Add the block to our list of allocations.
	struct block *block = alloc_to_block(allocated);
	list_push_back(&alloc_list, &block->header.alloc_list);

	// Store the location at the end of the allocation.
	char *loc_start = (char *) allocated + (block_size(block) - header_size - location_size - 2);
	loc_start[0] = '\0';
	memcpy(loc_start + 1, location, location_size + 1);

	lock_release(&alloc_list_lock);

	return allocated;
}

/* Find metadata from "malloc_check" */
static char *find_location(struct block *block)
{
	char *alloc_start = block_to_alloc(block);
	size_t alloc_size = block_size(block) - header_size;

	// Loop from the end until we find a null byte:
	char *pos = alloc_start + alloc_size - 2;
	while (pos > alloc_start) {
		if (pos[-1] == '\0') {
			return pos;
		}
		pos--;
	}

	// Not found, nothing to print.
	return NULL;
}

void *realloc_check(void *old_block, size_t new_size, char const *location)
{
	if (new_size == 0) {
		free(old_block);
		return NULL;
	}
	else {
		void *new_block = malloc_check(new_size, location);
		if (old_block != NULL && new_block != NULL) {
			// Note: When growing blocks, this will copy the location from the old
			// allocation as well. This is fine, as we make sure to never overwrite
			// the one placed by malloc!
			size_t old_size = block_size(alloc_to_block(old_block)) - header_size;
			size_t min_size = new_size < old_size ? new_size : old_size;
			memcpy(new_block, old_block, min_size);
			free(old_block);
		}
		return new_block;
	}
}

static void init_block(struct block *b)
{
	b->header.alloc_list.prev = NULL;
	b->header.alloc_list.next = NULL;
}

static void free_block(struct block *b)
{
	lock_acquire(&alloc_list_lock);

	// Check if we tracked this allocation:
	if (b->header.alloc_list.next && b->header.alloc_list.prev) {
		// Allocations that we have free'd point to themselves, check for that and report:
		if (b->header.alloc_list.next == &b->header.alloc_list) {
			// Temporarily disable leak detection.
			bool old_record_leaks = record_leaks;
			record_leaks = false;

			printf("ERROR: Double free of address %p detected!\n", block_to_alloc(b));

			record_leaks = old_record_leaks;
		}
		else {
			list_remove(&b->header.alloc_list);
		}
	}

	lock_release(&alloc_list_lock);

	// Mark it so we detect double free in the future.
	b->header.alloc_list.next = &b->header.alloc_list;
	b->header.alloc_list.prev = &b->header.alloc_list;
}

void not_a_leak(void *alloc)
{
	struct block *b = alloc_to_block(alloc);

	lock_acquire(&alloc_list_lock);

	// Check if we tracked this allocation:
	if (b->header.alloc_list.next && b->header.alloc_list.prev) {
		// Allocations that we have free'd point to themselves, check for that and report:
		if (b->header.alloc_list.next == &b->header.alloc_list) {
			// Temporarily disable leak detection.
			bool old_record_leaks = record_leaks;
			record_leaks = false;

			printf("ERROR: Trying to mark deleted memory %p as not a leak!\n", alloc);

			record_leaks = old_record_leaks;
		}
		else {
			list_remove(&b->header.alloc_list);
		}
	}

	lock_release(&alloc_list_lock);

	// Mark it so that we don't try to unlink it again.
	b->header.alloc_list.next = NULL;
	b->header.alloc_list.prev = NULL;
}

void malloc_enable_leak_check(void)
{
	record_leaks = true;
}

void malloc_check_leaks(void)
{
	lock_acquire(&alloc_list_lock);

	// Disable leak checking, in case printf wishes to allocate memory.
	bool old_record_leaks = record_leaks;
	record_leaks = false;

	if (list_empty(&alloc_list)) {
		printf(
			"# ----------------\n"
			"# No memory leaks!\n"
			"# ----------------\n"
		);
	}
	else {
		printf("Memory leaks detected!\n------------------------------\n");

		int count = 0;

		for (struct list_elem *curr = list_begin(&alloc_list); curr != list_end(&alloc_list);
		     curr = list_next(curr)) {

			struct block *b = list_entry(curr, struct block, header.alloc_list);
			printf(
				"Memory leak detected:\n  Address: %p\n  Allocated in: %s\n",
				block_to_alloc(b),
				find_location(b)
			);
			count++;
		}

		printf("------------------------------\n%d leak%s found\n", count, count > 1 ? "s" : "");
	}

	record_leaks = old_record_leaks;

	lock_release(&alloc_list_lock);
}

#else

static void init_block(struct block *b)
{
	(void) b;
}

static void free_block(struct block *b)
{
	(void) b;
}

void not_a_leak(void *alloc)
{
	(void) alloc;
}

#endif
