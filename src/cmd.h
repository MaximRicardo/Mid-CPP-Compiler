#pragma once

struct MidCMD_Args {
    const char *src;
    const char *ast_out;
    const char *asm_out;
    bool log_tokens;
    bool log_symbols;
};

void MidCMD_init_args(int argc, char **argv);
const struct MidCMD_Args *MidCMD_get_args(void);
