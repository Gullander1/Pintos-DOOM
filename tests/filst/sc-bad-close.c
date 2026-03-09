#include "tests/lib.h"
#include "tests/main.h"

#include <stdint.h>
#include <stdio.h>
#include <syscall-nr.h>

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
	// Get the addres of the first unmapped page in the system.
	unsigned page = (unsigned) pg_round_up(&_end_bss);

	// Reserve space for 2 parameters.
	unsigned base = page - sizeof(int) * 2;

	// Call close() with space for 2 parameters (syscall number + parameter, should be
	// fine).
	asm volatile("movl %%esp, %%edi;"
	             "movl %0, %%esp;"    // Set stack pointer to right below page boundary.
	             "movl %1, (%%esp);"  // Try to call SYS_CLOSE
	             "movl $8, 4(%%esp);" // Close fileno #8
	             "int $0x30;"
	             "movl %%edi, %%esp;" // Restore esp.
	             :
	             : "r"(base), "i"(SYS_CLOSE)
	             : "%eax", "%edi");

	write(STDOUT_FILENO, "OK\n", 3);

	// Reserve space for only syscall number (close requires an additional parameter).
	base = page - sizeof(int) * 1;

	// Call close() with space for 1 parameter (the kernel should kill us for doing
	// this).
	asm volatile("movl %%esp, %%edi;"
	             "movl %0, %%esp;"   // Set stack pointer to right below page boundary.
	             "movl %1, (%%esp);" // Try to call SYS_CLOSE
	             // "movl $8, 4(%%esp);"    // Close fileno #8
	             "int $0x30;"
	             "movl %%edi, %%esp;" // Restore esp in case we do not crash (as we should).
	             :
	             : "r"(base), "i"(SYS_CLOSE)
	             : "%eax", "%edi");

	fail("should have died.");
}
