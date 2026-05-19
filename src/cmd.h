#pragma once

struct CMD_Args {
    const char *src;
    const char *ast_out;
    const char *asm_out;
    bool log_tokens;
    bool log_symbols;
};

void CMD_init_args(int argc, char **argv);
const struct CMD_Args *CMD_get_args(void);
