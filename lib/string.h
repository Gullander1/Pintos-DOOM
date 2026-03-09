#ifndef __LIB_STRING_H
#define __LIB_STRING_H

#include <stddef.h>

/* Standard. */
void *memcpy(void *, void const *, size_t);
void *memmove(void *, void const *, size_t);
char *strncat(char *, char const *, size_t);
int memcmp(void const *, void const *, size_t);
int strcmp(char const *, char const *);
void *memchr(void const *, int, size_t);
char *strchr(char const *, int);
size_t strcspn(char const *, char const *);
char *strpbrk(char const *, char const *);
char *strrchr(char const *, int);
size_t strspn(char const *, char const *);
char *strstr(char const *, char const *);
void *memset(void *, int, size_t);
size_t strlen(char const *);

/* Extensions. */
size_t strlcpy(char *, char const *, size_t);
size_t strlcat(char *, char const *, size_t);
char *strtok_r(char *, char const *, char **);
size_t strnlen(char const *, size_t);

/* Try to be helpful. */
#define strcpy  dont_use_strcpy_use_strlcpy
#define strncpy dont_use_strncpy_use_strlcpy
#define strcat  dont_use_strcat_use_strlcat
#define strncat dont_use_strncat_use_strlcat
#define strtok  dont_use_strtok_use_strtok_r

#endif /* lib/string.h */
