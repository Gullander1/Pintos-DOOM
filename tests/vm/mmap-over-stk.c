/* Verifies that mapping over the stack segment is disallowed. */

#include "tests/lib.h"
#include "tests/main.h"

#include <round.h>
#include <stdint.h>
#include <syscall.h>

void test_main(void)
{
	int handle;
	uintptr_t handle_page = ROUND_DOWN((uintptr_t) &handle, 4096);

	CHECK((handle = open("sample.txt")) > 1, "open \"sample.txt\"");
	CHECK(mmap(handle, (void *) handle_page) == MAP_FAILED, "try to mmap over stack segment");
}
