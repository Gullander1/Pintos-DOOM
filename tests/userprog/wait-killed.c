/* Wait for a process that will be killed for bad behavior. */

#include "tests/lib.h"
#include "tests/main.h"

#include <syscall.h>

void test_main(void)
{
	msg("wait(exec()) = %d", wait(exec("child-bad")));
}
