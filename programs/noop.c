#include <stdio.h>
#include <syscall.h>

/*  A program that does nothing, a classic no-op.
 *
 * Compile it, and run it from userprog/ with the following command:
pintos -T 5 --filesys-size=2 -p ../examples/noop -a binary -- -f -q run 'binary -s 17'
*/

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	return EXIT_SUCCESS;
}
