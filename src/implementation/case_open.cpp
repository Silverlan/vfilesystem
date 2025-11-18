/*
Copyright (c) 2009 Keith Bauer

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

module;

// Source: https://github.com/OneSadCookie/fcaseopen
#if !defined(_WIN32)

#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

module pragma.filesystem;

import :case_open;

// r must have strlen(path) + 3 bytes
int casepath(char const *path, char *r)
{
	size_t l = std::strlen(path);
	std::string pbuf;
	pbuf.resize(l + 1);
	char *p = pbuf.data();
	std::memcpy(p, path, l + 1);

	size_t rl = 0;

	DIR *d;
	if(p[0] == '/') {
		d = opendir("/");
		p = p + 1;
	}
	else {
		d = opendir(".");
		r[0] = '.';
		r[1] = 0;
		rl = 1;
	}

	int last = 0;
	char *c = strsep(&p, "/");
	while(c) {
		if(!d) {
			return 0;
		}

		if(last) {
			closedir(d);
			return 0;
		}

		r[rl] = '/';
		rl += 1;
		r[rl] = 0;

		struct dirent *e = readdir(d);
		while(e) {
			if(strcasecmp(c, e->d_name) == 0) {
				strcpy(r + rl, e->d_name);
				rl += strlen(e->d_name);

				closedir(d);
				d = opendir(r);

				break;
			}

			e = readdir(d);
		}

		if(!e) {
			strcpy(r + rl, c);
			rl += strlen(c);
			last = 1;
		}

		c = strsep(&p, "/");
	}

	if(d)
		closedir(d);
	return 1;
}
#else
#include <direct.h>
#include <stdio.h>

module pragma.filesystem;

import :case_open;
#endif

FILE *fcaseopen(char const *path, char const *mode)
{
#if !defined(_WIN32)
	FILE *f = fopen(path, mode);
	if(!f) {
		std::string rbuf;
		rbuf.resize(std::strlen(path) + 3);
		char *r = rbuf.data();
		if(casepath(path, r)) {
			f = fopen(r, mode);
		}
	}
#else
	FILE *f = NULL;
	fopen_s(&f, path, mode);
#endif
	return f;
}

FILE *fcasereopen(FILE **f, char const *path, char const *mode)
{
#if !defined(_WIN32)
	*f = freopen(path, mode, *f);
	if(!f) {
		std::string rbuf;
		rbuf.resize(std::strlen(path) + 3);
		char *r = rbuf.data();
		if(casepath(path, r)) {
			*f = freopen(r, mode, *f);
		}
	}
#else
	f = NULL;
	freopen_s(f, path, mode, *f);
#endif
	return *f;
}

void casechdir(char const *path)
{
#if !defined(_WIN32)
	std::string rbuf;
	rbuf.resize(std::strlen(path) + 3);
	char *r = rbuf.data();
	if(casepath(path, r)) {
		chdir(r);
	}
	else {
		errno = ENOENT;
	}
#else
	chdir(path);
#endif
}
