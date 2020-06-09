#pragma once

typedef enum
{
    src_client,  // came in over a net connection as a clc_stringcmd
                 // host_client will be valid during this state.
    src_command, // from the command buffer

    // QSS
    src_server // from a svc_stufftext
} cmd_source_t;

extern cmd_source_t cmd_source;

// QSS
typedef void (*xcommand_t)(void);
typedef struct cmd_function_s
{
    struct cmd_function_s* next;
    const char* name;
    xcommand_t function;
    cmd_source_t srctype;
} cmd_function_t;
