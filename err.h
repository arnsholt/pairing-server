#ifndef _ERR_H
#define _ERR_H

#include <stdarg.h>
#include <stdio.h>

static int err(const char *fmt, ...) {
    va_list varargs;
    int ret;
    va_start(varargs, fmt);
    ret = vfprintf(stderr, fmt, varargs);
    va_end(varargs);
    return ret;
}

static int trace(const char *fmt, ...) {
    va_list varargs;
    int ret;
    va_start(varargs, fmt);
    fprintf(stderr, "TRACE ");
    ret = vfprintf(stderr, fmt, varargs);
    va_end(varargs);
    return ret;
}

#endif
