/* Tests argument passing to child processes. */

#include "tests/main.h"

#include <syscall.h>

void test_main(void)
{
	wait(exec("child-args childarg"));
}
