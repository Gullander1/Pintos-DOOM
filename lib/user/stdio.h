#ifndef __LIB_USER_STDIO_H
#define __LIB_USER_STDIO_H

int hprintf(int, char const *, ...) PRINTF_FORMAT(2, 3);
int vhprintf(int, char const *, va_list) PRINTF_FORMAT(2, 0);

#endif /* lib/user/stdio.h */
