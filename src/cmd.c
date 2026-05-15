#include "cmd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool cmd_args_inited = false;
static struct CMD_Args cmd_args;

void CMD_init_args(int argc, char **argv)
{
    cmd_args = (struct CMD_Args){};

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--ast"))
            cmd_args.ast_out = argv[++i];
        else if (!strcmp(argv[i], "--log-tokens"))
            cmd_args.log_tokens = true;
        else if (!strcmp(argv[i], "--log-symbols"))
            cmd_args.log_symbols = true;
        else if (argv[i][0] == '-')
            fprintf(stderr, "error: unknown cmd line argument '%s'\n", argv[i]);
        else
            cmd_args.src = argv[i];
    }

    cmd_args_inited = true;
}

const struct CMD_Args *CMD_get_args(void)
{
    assert(cmd_args_inited);
    return &cmd_args;
}
