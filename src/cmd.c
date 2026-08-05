#include "cmd.h"
#include "mid_alloc.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool cmd_args_inited = false;
static struct midcmd_Args cmd_args;

static void print_help_menu(void)
{
    printf("Mid C++ Compiler\n\n");
    printf("Usage: mcppc [options] file...\n\n");
    printf("Options:\n");
    printf("\t-h, --help\t\tPrint the help menu\n");
    printf("\t--ast <file>\t\tLog the AST of the compilation unit to a file\n");
    printf("\t--log-tokens\t\tLog a list of every token in the compilation "
           "unit\n");
    printf("\t--log-symbols\t\tLog a list of every symbol in the compilation "
           "unit\n");
}

void midcmd_init_args(int argc, char **argv)
{
    cmd_args = (struct midcmd_Args){};

    bool help = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ast"))
            cmd_args.ast_out = argv[++i];
        else if (!strcmp(argv[i], "--log-tokens"))
            cmd_args.log_tokens = true;
        else if (!strcmp(argv[i], "--log-symbols"))
            cmd_args.log_symbols = true;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
            help = true;
        else if (argv[i][0] == '-')
            fprintf(stderr, "error: unknown cmd line argument '%s'\n", argv[i]);
        else
            cmd_args.src = argv[i];
    }

    cmd_args_inited = true;

    if (help)
        print_help_menu();
}

const struct midcmd_Args *midcmd_get_args(void)
{
    assert(cmd_args_inited);
    return &cmd_args;
}

void midcmd_prt_line(const char *line)
{
    for (mid_isize i = 0; line[i] != '\n'; ++i)
        putchar(line[i]);
}

char *midcmd_fmt_to_str(const char *fmt, ...)
{
    char *str = NULL;

    va_list args;
    va_start(args, fmt);
    va_list argscpy;
    va_start(argscpy, fmt);

    int len = vsnprintf(str, 0, fmt, args);
    str = mid_malloc((len + 1) * sizeof(*str));
    vsprintf(str, fmt, argscpy);

    va_end(argscpy);
    va_end(args);

    return str;
}

void midcmd_prt_column_arrow(int32_t column)
{
    for (int32_t i = 0; i < column - 1; ++i)
        printf(" ");
    printf("^\n");
}
