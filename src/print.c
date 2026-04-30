#include "print.h"
#include "ints.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void Print_line(const char *line)
{
    for (isize_t i = 0; line[i] != '\n'; ++i)
        putchar(line[i]);
}

char *Print_fmt_to_str(const char *fmt, ...)
{
    char *str = NULL;

    va_list args;
    va_start(args, fmt);
    va_list argscpy;
    va_start(argscpy, fmt);

    int len = vsnprintf(str, 0, fmt, args);
    str = malloc((len + 1) * sizeof(*str));
    vsprintf(str, fmt, argscpy);

    va_end(argscpy);
    va_end(args);

    return str;
}

void Print_column_arrow(i32 column)
{
    for (i32 i = 0; i < column - 1; ++i)
        printf(" ");
    printf("^\n");
}
