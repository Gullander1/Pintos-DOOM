/* Wait for a subprocess to finish. */

#include "tests/lib.h"
#include "tests/main.h"

#include <syscall.h>

void test_main(void)
{
	msg("wait(exec()) = %d", wait(exec("child-simple")));
}
