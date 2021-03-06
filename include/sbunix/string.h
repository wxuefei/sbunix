#ifndef _SBUNIX_STRING_H
#define _SBUNIX_STRING_H

#include <sys/types.h>

#define NULL ((void*)0)

int strncasecmp(const char *s1, const char *s2, size_t len);
int strnicmp(const char *s1, const char *s2, size_t len);
int strcasecmp(const char *s1, const char *s2);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t count);
size_t strlcpy(char *dest, const char *src, size_t size);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t count);
int strcmp(const char *cs, const char *ct);
int strncmp(const char *cs, const char *ct, size_t count);
char *strchr(const char *s, int c);
char *strchrnul(const char *s, int c);
char *strrchr(const char *s, int c);
char *strnchr(const char *s, size_t count, int c);
char *skip_spaces(const char *str);
char *strim(char *s);
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t count);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
char *strpbrk(const char *cs, const char *ct);
char *strsep(char **s, const char *ct);
void *memset(void *s, int c, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *cs, const void *ct, size_t count);
void *memscan(void *addr, int c, size_t size);
char *strstr(const char *s1, const char *s2);
char *strnstr(const char *s1, const char *s2, size_t len);
void *memchr(const void *s, int c, size_t n);
char *strrev(char *str);
char *itoa(long long val, int base, char *str, size_t len);
char *uitoa(unsigned long long val, int base, char *str, size_t len);
char *strerror(int err);

char *strsignal(int sig);

extern const char * const sys_siglist[];

#endif /* _SBUNIX_STRING_H */
