#include "cmd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool cmd_args_inited = false;
static struct CMD_Args cmd_args;

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

void CMD_init_args(int argc, char **argv)
{
    cmd_args = (struct CMD_Args){};

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

const struct CMD_Args *CMD_get_args(void)
{
    assert(cmd_args_inited);
    return &cmd_args;
}
