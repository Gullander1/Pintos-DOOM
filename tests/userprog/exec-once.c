/* Executes and waits for a single child process. */

#include "tests/lib.h"
#include "tests/main.h"

#include <syscall.h>

void test_main(void)
{
	wait(exec("child-simple"));
}
