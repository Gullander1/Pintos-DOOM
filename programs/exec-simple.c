#include <stdio.h>
#include <syscall.h>

/* Compile it, and run it from userprog/ with the following command:
pintos -v -k -T 240 --filesys-size=2 -p ../examples/exec-simple -a execs -p ../examples/printf -a printf -- -f -q run execs

  If exec is correctly implemented this program should call printf 5 times,
  and spawn a child process for each of these calls. In order to see if your
  implementation works check if the string "You got it, use your debugging
  skills during the labs!" is printed 5 times and if the PID of each new
  child process is incremented as it should.
*/

int main(void)
{
	int pid = -1;
	for (int i = 0; i < 5; i++) {
		pid = exec("printf");
		printf("Child %d process ID: %d\n", i, pid);
	}

	return EXIT_SUCCESS;
}
