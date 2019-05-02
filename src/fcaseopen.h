#ifndef fcaseopen_h
#define fcaseopen_h

// Source: https://github.com/OneSadCookie/fcaseopen
#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern FILE *fcaseopen(char const *path, char const *mode);
extern FILE *fcasereopen(FILE **f,char const *path, char const *mode);
extern int casepath(char const *path, char *r);

extern void casechdir(char const *path);

#if defined(__cplusplus)
}
#endif

#endif
