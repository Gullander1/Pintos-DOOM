#include "tests/lib.h"
#include "tests/main.h"
#include <stdio.h>

/**
 * This program starts two processes as children as illustrated below:
 *
 * wait-wrong (parent)
 * |- wait-child (child A)
 * |- wait-child <pid> (child B)
 *
 * The PID of child A is passed to child B. Child B then tries to wait for child
 * A. This should not work (i.e. "wait" should return -1) since child A is not a
 * child to child B (both are started by us).
 *
 * Depending on the implementation, this might cause the system to deadlock
 * and/or crash.
 */
void test_main(void)
{
	// Start child A
	pid_t child_a = exec("wait-child");
	if (child_a < 0) {
		fail("exec(\"wait-child\")");
	}

	// Pass the PID of child A to a new child:
	char cmdline_b[32];
	snprintf(cmdline_b, 32, "wait-child %d", child_a);

	pid_t child_b = exec(cmdline_b);
	if (child_b < 0) {
		fail("exec(\"%s\")", cmdline_b);
	}

	// Wait for child_b first, in case 'wait' actually worked. This ensures that
	// we don't accidentally make 'wait' in child A to work by "stealing" the row
	// in the plist.
	int result_b = wait(child_b);
	if (result_b != 100) {
		fail("wait for child B");
	}

	// Then, wait for child_a. This could deadlock or return -1 depending on the
	// details of the implementation. We should always get the proper exit status.
	int result_a = wait(child_a);
	if (result_a != 200) {
		fail("wait for child A");
	}
}
