#pragma once

struct midcmd_Args {
    const char *src;
    const char *ast_out;
    const char *asm_out;
    bool log_tokens;
    bool log_symbols;
};

void midcmd_init_args(int argc, char **argv);
const struct midcmd_Args *midcmd_get_args(void);
