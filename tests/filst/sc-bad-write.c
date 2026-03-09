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

/**
 * String constants used in the asm blocks.
 */
char const *WORKS = "WORKS\n";
char const *FAIL = "FAIL\n";

void test_main(void)
{
	// Get the address of the first unmapped page in the process.
	unsigned page = (unsigned) pg_round_up(&_end_bss);

	// Reserve space for 4 parameters.
	unsigned base = page - sizeof(int) * 4;

	// Call write() with space for 4 parameters (should be fine).
	asm volatile("movl %%esp, %%edi;"
	             "movl %0, %%esp;"     // Set stack pointer to right below page boundary.
	             "movl %1, (%%esp);"   // Try to call SYS_WRITE
	             "movl %2, 4(%%esp);"  // Write to STDOUT
	             "movl %3, 8(%%esp);"  // Load buffer.
	             "movl $6, 12(%%esp);" // Write length of string
	             "int $0x30;"
	             "movl %%edi, %%esp;" // Restore esp.
	             :
	             : "r"(base), "i"(SYS_WRITE), "i"(STDOUT_FILENO), "r"(WORKS)
	             : "%eax", "%edi");

	// Reserve space for 3 parameters (write requires 4).
	base = page - sizeof(int) * 3;

	// Call write() with space for 3 parameters (the kernel should kill us for doing
	// this).
	asm volatile("movl %%esp, %%edi;"
	             "movl %0, %%esp;"    // Set stack pointer to right below page boundary.
	             "movl %1, (%%esp);"  // Try to call SYS_WRITE
	             "movl %2, 4(%%esp);" // Write to STDOUT
	             "movl %3, 8(%%esp);" // Load buffer.
	             //"movl $5, 12(%%esp);" // Can not write the last parameter as we would get a
	             // pagefault.
	             "int $0x30;"
	             "movl %%edi, %%esp;" // Restore esp in case we do not crash (as we should).
	             :
	             : "r"(base), "i"(SYS_WRITE), "i"(STDOUT_FILENO), "r"(FAIL)
	             : "%eax", "%edi");

	fail("should have died.");
}
