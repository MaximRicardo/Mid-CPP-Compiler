#include "cmd.h"
#include "apfloat.h"
#include "ints.h"
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
    printf("\t-fset-rounding-mode=<mode>\t\tTells the compiler to assume a "
           "specific rounding mode. Valid modes are nearest-ties-even, "
           "nearest-ties-away, down, up and towards-zero. By default the "
           "compiler assumes nearest-ties-even is selected\n");
}

// finds the equals sign in the argument. returns -1 if it couldn't be found
// example: "-std=c++11"
//               ^
//          find_eq(arg)
static mid_isize find_eq(const char *arg)
{
    for (mid_isize i = 0; arg[i] != '\0'; ++i) {
        if (arg[i] == '=')
            return i;
    }

    return -1;
}

static char *get_arg_with_eq_name(const char *arg, mid_isize eq)
{
    char *name = mid_malloc((eq + 1) * sizeof(*name));
    memcpy(name, arg, eq * sizeof(*name));
    name[eq] = '\0';

    return name;
}

static char *get_arg_with_eq_value(const char *arg, mid_isize eq)
{
    mid_isize arg_len = strlen(arg);
    mid_isize val_len = arg_len - eq;

    char *val = mid_malloc((val_len + 1) * sizeof(*val));
    strcpy(val, &arg[eq + 1]);

    return val;
}

static void print_err(const char *restrict fmt, ...)
{
    va_list args;
    va_start(args);

    fprintf(stderr, "%serror%s: ", midcmd_ansi_red, midcmd_ansi_reset);
    vfprintf(stderr, fmt, args);

    va_end(args);
}

static void fset_rounding_mode(struct midcmd_Args *args, const char *val)
{
    if (!strcmp(val, "nearest-ties-even"))
        args->fpu.rmode = MIDFLT_ROUND_NEAREST_TIES_EVEN;
    else if (!strcmp(val, "nearest-ties-away"))
        args->fpu.rmode = MIDFLT_ROUND_NEAREST_TIES_AWAY;
    else if (!strcmp(val, "down"))
        args->fpu.rmode = MIDFLT_ROUND_DOWN;
    else if (!strcmp(val, "up"))
        args->fpu.rmode = MIDFLT_ROUND_UP;
    else if (!strcmp(val, "towards-zero"))
        args->fpu.rmode = MIDFLT_ROUND_TOWARDS_ZERO;
    else
        print_err("unknown rounding mode '%s'\n", val);
}

// returns true if the argument was successfully read, false otherwise
static bool read_arg_with_eq(struct midcmd_Args *args, const char *arg)
{
    mid_isize eq = find_eq(arg);
    if (eq == -1)
        return false;

    char *name = get_arg_with_eq_name(arg, eq);
    char *val = get_arg_with_eq_value(arg, eq);

    bool found = true;
    if (!strcmp(name, "-fset-rounding-mode"))
        fset_rounding_mode(args, val);
    else
        found = false;

    free(name);
    free(val);
    return found;
}

static void set_default_args(struct midcmd_Args *args)
{
    args->fpu.rmode = MIDFLT_ROUND_NEAREST_TIES_EVEN;
}

void midcmd_init_args(int argc, const char *const *argv)
{
    cmd_args = (struct midcmd_Args){};
    set_default_args(&cmd_args);

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
        else if (read_arg_with_eq(&cmd_args, argv[i]))
            continue;
        else if (argv[i][0] == '-')
            print_err("unknown cmd line argument '%s'\n", argv[i]);
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

const struct midcmd_FPUArgs *midcmd_get_fpu()
{
    return &midcmd_get_args()->fpu;
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
