#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

/**
 * Child process to "sc-wait-wrong".
 *
 * This process simply waits for the process specified on the command-line. If
 * none is provided, does nothing.
 */
int main(int argc, char const **argv)
{
	if (argc >= 2) {
		// Try to wait for the process.
		pid_t pid = atoi(argv[1]);
		int result = wait(pid);
		printf("Child B's wait returned: %d\n", result);
		if (result != -1) {
			printf("should have returned -1: FAILED\n");
		}

		return 100;
	}
	else {
		return 200;
	}
}
