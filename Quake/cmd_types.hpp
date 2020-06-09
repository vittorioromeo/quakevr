#pragma once

enum cmd_source_t
{
    src_client,  // came in over a net connection as a clc_stringcmd
                 // host_client will be valid during this state.
    src_command, // from the command buffer

    // QSS
    src_server // from a svc_stufftext
};

extern cmd_source_t cmd_source;

// QSS
using xcommand_t = void (*)(void);

struct cmd_function_t
{
    struct cmd_function_t* next;
    const char* name;
    xcommand_t function;
    cmd_source_t srctype;
};
