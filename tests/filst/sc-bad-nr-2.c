#include "tests/lib.h"
#include "tests/main.h"

#include <syscall-nr.h>

// From "lib/user/syscall.c", but modified to push some zeros on the stack
// first, to get a better output for the tests if the stack error message
// remains.
#define syscall0(NUMBER)                                                                           \
	({                                                                                               \
		int retval;                                                                                    \
		asm volatile("pushl $0; pushl $0; pushl %[number]; int $0x30; addl $4, %%esp"                  \
		             : "=a"(retval)                                                                    \
		             : [number] "i"(NUMBER)                                                            \
		             : "memory");                                                                      \
		retval;                                                                                        \
	})

void test_main(void)
{
	// Call a syscall that is quite a bit larger than the maximum syscall number.
	syscall0(1024 * 1024);
	fail("Should have failed.");
}
