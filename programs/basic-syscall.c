/* klaar@ida

  Compile it, and run it from userprog/ with the following command
pintos -v -k --filesys-size=2 -p ../examples/basic-syscall -a sysc -- -f -q run sysc

  Test program for basic system calls. Inspired by previous versions
  by various IDA-employees / lab-assistants.

  Emergency exit QEMU bu pressing C-a x (release Ctrl before pressing x)

  DONE: Warn when '\r' instead of '\n'
  DONE: Check result for JUNK
  DONE: Compare/verify content of buffer after reads.
  DONE: Make sure content of buffer is known/reset before filling it.
  DONE: Fix verify seek past filesize

 */

#include <stdio.h>
#include <string.h>
#include <syscall.h>

#define SIZE 1024
#define JUNK 0xCCCC /* 52428 */
#define CRAP 0xDDDD

#define msg(comment) write(STDOUT_FILENO, "\n" comment "\n", strlen(comment) + 2)

#define end(comment)                                              \
	write(STDOUT_FILENO, "\n" comment "\n", strlen(comment) + 2); \
	skip_line()

static void skip_line(void);
static int get_line(char *buf, int size);
static void verify(int test);
static void init_buffer(char *, int);

int main(int argc, char *argv[])
{
	int id = JUNK;
	int i, j;

	msg("* ------------------ write screen test ------------------ *");
	{
		char *msg = "Now displaying the arguments to main\n";
		int length = strlen(msg);
		int result = JUNK;

		result = write(STDOUT_FILENO, msg, length);

		for (i = 0; i < argc; ++i) {
			write(STDOUT_FILENO, argv[i], strlen(argv[i]));
			write(STDOUT_FILENO, "\n", 1);
		}

		verify(length == result);
	}
	end("* -------------------- press enter ---------------------- *");

	printf("To emergency exit QEMU at any time:\n");
	printf("Hold 'Control' and press 'a' and then \n");
	printf("release 'Control' and press 'x'\n");

	end("* -------------------- press enter ---------------------- *");

	msg("* ----------------- read keyboard test ------------------ *");
	{
		char *input = "qwerty1234";
		char buffer[10];
		int length = CRAP;
		int result = JUNK;
		char yes;

		init_buffer(buffer, 10);

		printf("Will now try to read 10 characters.\n");
		printf("Please write \"%s\": ", input);
		result = read(STDIN_FILENO, buffer, 10);
		length = strlen(buffer);
		printf("\nThe following characters was read: '%s'\n", buffer);

		verify(length == result && memcmp(buffer, input, 10) == 0);

		printf("\nDid you see each character as you typed it? (y/n) ");
		result = read(STDIN_FILENO, &yes, 1);
		printf("\n");

		verify(result == 1 && yes == 'y');
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ create file test ------------------- *");
	{
		int success = JUNK;

		printf("Will try to create 'test.txt'\n");
		success = create("test.txt", SIZE);
		verify(success != JUNK && success);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ open file test --------------------- *");
	{
		printf("Will try to open 'non_existent_file'\n");
		id = open("non_existent_file");
		verify(id == -1);

		printf("Will try to open 'test.txt'\n");
		id = open("test.txt");
		verify(id > 1);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ write file test -------------------- *");
	{
		char buffer[8];
		int result = JUNK;
		bool success = true;

		init_buffer(buffer, 8);

		printf("Will try to write a sequence to '%d'\n", id);
		for (i = 0; i < 16; ++i) {
			for (j = 0; j < 16; ++j) {
				snprintf(buffer, 8, "%4d", i * 16 + j);
				result = write(STDOUT_FILENO, buffer, 4);
				result = write(id, buffer, 4);
				success = success && (result == 4);
			}
			result = write(STDOUT_FILENO, "\n", 1);
		}
		verify(success);

		printf("Will try to write 8 characters to '%d'\n", JUNK);
		result = write(JUNK, buffer, 8);
		verify(result == -1);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ read file test --------------------- *");
	{
		char buffer[8];
		char verify_buffer[8];
		int result = JUNK;
		bool success = true;

		init_buffer(buffer, 8);
		init_buffer(verify_buffer, 8);

		printf("Will try to reopen 'test.txt'\n");
		close(id);
		id = open("test.txt");

		printf("Will try to read a sequence from '%d'\n", id);
		for (i = 0; i < 16; ++i) {
			for (j = 0; j < 16; ++j) {
				snprintf(verify_buffer, 8, "%4d", i * 16 + j);
				result = read(id, buffer, 4);
				success = success && (result == 4);
				result = write(STDOUT_FILENO, buffer, 4);

				success = success && (memcmp(buffer, verify_buffer, 4) == 0);
			}
			result = write(STDOUT_FILENO, "\n", 1);
		}
		verify(success);

		printf("Will try to read 8 characters from '%d'\n", JUNK);
		result = read(JUNK, buffer, 8);
		verify(result == -1);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ close file test -------------------- *");
	{
		char buffer[128];
		int result = JUNK;

		init_buffer(buffer, 128);

		printf("Will try to close 'STDIN_FILENO' and then read from it.\n");
		printf("Write some input please: ");
		close(STDIN_FILENO);
		result = get_line(buffer, 128);
		verify(result < 128 && result == (int) strlen(buffer));

		printf("Will try to close 'STDOUT_FILENO' and then write to it.\n");
		close(STDOUT_FILENO);
		result = write(STDOUT_FILENO, buffer, strlen(buffer));
		printf("\n");
		verify(result == (int) strlen(buffer));

		printf("Will try to close id '%d' and then read from it\n", id);
		close(id);
		result = read(id, buffer, 128);
		verify(result == -1);

		printf("Will try to close id '%d' and then read from it\n", id);
		close(JUNK);
		result = read(JUNK, buffer, 128);
		verify(result == -1);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ remove file test ------------------- *");
	{
		int success = JUNK;

		printf("Will try to remove 'test.txt' and then open it\n");
		success = remove("test.txt");
		id = open("test.txt");
		verify(success && id == -1);

		printf("Will try to remove 'non_existent_file'\n");
		success = remove("non_existent_file");
		verify(!success);
	}
	end("* -------------------- press enter ---------------------- *");

	printf("To emergency exit QEMU at any time:\n");
	printf("Hold 'Control' and press 'a' and then \n");
	printf("release 'Control' and press 'x'\n");

	end("* -------------------- press enter ---------------------- *");

	msg("* ---------------- seek/tell file test ------------------ *");
	{
		char buffer[8];
		char verify_buffer[8];
		int result = JUNK;
		int success = CRAP;

		init_buffer(buffer, 8);
		init_buffer(verify_buffer, 8);

		printf("Will try to create and open 'test.txt'\n");
		success = create("test.txt", SIZE);
		id = open("test.txt");
		verify(success != CRAP && success && id > 1);

		printf("Will try to write a sequence to '%d'\n", id);
		for (i = 0; i < 16; ++i) {
			for (j = 0; j < 16; ++j) {
				snprintf(buffer, 8, "%4d", j * 16 + i);
				seek(id, (j * 16 + i) * 4);
				result = write(STDOUT_FILENO, buffer, 4);
				result = write(id, buffer, 4);
				success = success && (result == 4);
				result = tell(id);
				success = success && (result == (j * 16 + i + 1) * 4);
			}
			result = write(STDOUT_FILENO, "\n", 1);
		}
		verify(success);

		printf("Will try to read the sequence from '%d'\n", id);
		seek(id, 0);
		for (i = 0; i < 16; ++i) {
			for (j = 0; j < 16; ++j) {
				snprintf(verify_buffer, 8, "%4d", i * 16 + j);
				result = read(id, buffer, 4);
				success = success && (result == 4);
				result = write(STDOUT_FILENO, buffer, 4);

				success = success && (memcmp(buffer, verify_buffer, 4) == 0);
			}
			result = write(STDOUT_FILENO, "\n", 1);
		}
		result = tell(id);
		verify(success && result == 256 * 4);

		printf("Will try to determine filesize and seek past it\n");
		result = filesize(id);
		/* calling seek past filesize is defined to return the position of the end of file
		 */
		seek(id, result * 2);
		result = tell(id);
		verify(result == SIZE);
	}
	end("* -------------------- press enter ---------------------- *");

	msg("* ------------------ sleep test ------------------- *");
	{
		int ms = 3000;

		printf("Going to sleep for %d ms\n", ms);

		sleep(ms);

		printf("Now i am awake :)\n");
		printf("\033[1;32;40m RESULT OK \033[1;0m \n");
	}
	end("* -------------------- press enter ---------------------- *");

	printf("If you got this far, you've passed all the tests this file offers!\n");
	printf(
		"Pintos should now seeminly hang, because you haven't implemented "
		"process_wait() yet.\n"
	);
	printf("Press CTRL+C to terminate Pintos.\n");

	return 0;
}

char const *crlf_warning = ("\033[1;31;40mWARNING\033[1;0m: "
                            "You should convert '\\r' to '\\n'.\n");

static void skip_line(void)
{
	char c = '\0';

	while (c != '\n' && c != '\r') {
		read(STDIN_FILENO, &c, 1);
	}

	if (c == '\r') {
		write(STDOUT_FILENO, crlf_warning, strlen(crlf_warning));
	}
}

static int get_line(char *buf, int size)
{
	int i;

	for (i = 0; i < (size - 1); ++i) {
		buf[i] = '\0';

		if (read(STDIN_FILENO, buf + i, 1) != 1) {
			return -1;
		}

		if (buf[i] == '\r') {
			write(STDOUT_FILENO, crlf_warning, strlen(crlf_warning));
		}

		if (buf[i] == '\n') {
			break;
		}
	}
	buf[i] = '\0';
	return i;
}

static void verify(int test)
{
	/* will only print nice in vt100 terminal */
	/* assuming black background */
	char const *result[3] = { "\033[1;31;40m WRONG RESULT \033[1;0m \n",
		                        "\033[1;32;40m RESULT OK \033[1;0m \n",
		                        "\033[1;31;40m BAD BOOLEAN \033[1;0m \n" };

	if (test < 0 || test > 1) {
		test = 2;
	}

	write(STDOUT_FILENO, result[test], strlen(result[test]));
}

static void init_buffer(char *buffer, int size)
{
	char const *data = "ERROR";
	int const S = strlen(data);
	int i;

	for (i = 0; i < (size - 1); ++i) {
		buffer[i] = data[i % S];
	}
	buffer[i] = '\0';
}
