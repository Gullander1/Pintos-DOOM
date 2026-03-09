#ifndef __DOOMTYPE__
#define __DOOMTYPE__

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>
#include <limits.h>
#include <syscall.h>

void *malloc(size_t size);
void free(void *ptr);

#define fopen(n, m)        ((void*)(size_t)open(n))
#define fclose(f)          close((int)(size_t)f)
#define fread(p, s, n, f)  read((int)(size_t)f, p, (s)*(n))
#define fwrite(p, s, n, f) write((int)(size_t)f, p, (s)*(n))
#define ftell(f)           tell((int)(size_t)f)
#define fseek(f, o, w)     (0)
#define system(cmd)        (-1)
#define fflush(stream)     (0)

#define rename(o, n)       (-1)
#define sscanf(s, f, ...)  (0)
#define atof(s)            (0)

void exit(int status);

#ifndef _SKIP_MKDIR_FIX
  bool (mkdir)(const char *dir); 
  #define mkdir(path, mode) ((void)(mode), (mkdir)(path))
#endif

#include <stdarg.h>
int vprintf(const char *format, va_list args);
#define vfprintf(stream, fmt, args) vprintf(fmt, args)

#ifndef errno
  #define errno 0
  #define EISDIR 21
#endif

#ifndef SEEK_SET
  #define SEEK_SET 0
  #define SEEK_END 2
#endif

#ifndef FILE
  #define FILE int
#endif

#define stderr 2
#define stdout 1
#define fprintf(file, fmt, ...) printf(fmt, ##__VA_ARGS__)

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);

typedef bool boolean;
typedef uint8_t byte;

#ifdef __GNUC__
  #define PACKEDATTR __attribute__((packed))
#else
  #define PACKEDATTR
#endif

static inline void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

static inline char* strdup(const char* s) {
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char* new_str = (char*)malloc(len);
    if (new_str) memcpy(new_str, s, len);
    return new_str;
}

#ifndef abs
  static inline int abs(int x) { return (x < 0) ? -x : x; }
#endif

#define fabs(x) ((x) < 0 ? -(x) : (x))

#undef strncpy
static inline char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}

#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_S "/"
#define PATH_SEPARATOR ':'
#define arrlen(array) (sizeof(array) / sizeof(*array))

#endif /* __DOOMTYPE__ */