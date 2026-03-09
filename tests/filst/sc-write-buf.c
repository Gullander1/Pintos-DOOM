#include "tests/lib.h"
#include "tests/main.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>

/**
 * Thanks to Herman Appelgren for suggesting this test case.
 */

/**
 * From threads/vaddr.h:
 */
#define BITMASK(SHIFT, CNT) (((1ul << (CNT)) - 1) << (SHIFT))

#define PGSHIFT 0                        /* Index of first offset bit. */
#define PGBITS  12                       /* Number of offset bits. */
#define PGSIZE  (1 << PGBITS)            /* Bytes in a page. */
#define PGMASK  BITMASK(PGSHIFT, PGBITS) /* Page offset bits (0:12). */

static inline void *pg_round_up(void const *va)
{
	return (void *) (((uintptr_t) va + PGSIZE - 1) & ~PGMASK);
}

/**
 * External symbol which address is the first address after all data in the BSS segment.
 */
extern int _end_bss;

void test_main(void)
{
	// Create a file to write to.
	create("test", 50);

	// Open the file.
	int fd = open("test");
	if (fd < 0) {
		fail("could not open file.");
	}

	// Get the address of the first unmapped page in the process. Addresses from
	// here and onwards are not mapped to any physical address.
	char *message = pg_round_up(&_end_bss);

	// Go down a few bytes so that we can store our message.
	message -= 10;

	// Copy the string into the end of the last page. It will fit the end of the
	// page with some room to spare.
	strlcpy(message, "Hello", 6);

	// Call write with a too large address. This should fail as the last few
	// addresses are beyond the mapped region in memory.
	write(fd, message, 20);

	fail("should have died.");
}
