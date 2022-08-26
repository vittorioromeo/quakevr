#pragma once

#include "progs_types.hpp"
#include "areanode.hpp"
#include "edict.hpp"

struct qcvm_t
{
    dprograms_t* progs;
    dfunction_t* functions;
    dstatement_t* statements;
    float* globals;    /* same as pr_global_struct */
    ddef_t* fielddefs; // yay reflection.

    int edict_size; /* in bytes */

    builtin_t builtins[1024];
    int numbuiltins;

    int argc;

    bool trace;
    dfunction_t* xfunction;
    int xstatement;

    unsigned short crc;

    pr_extglobals_s extglobals;
    pr_extfuncs_s extfuncs;
    pr_extfields_s extfields;

    // was static inside pr_edict
    char* strings;
    int stringssize;
    const char** knownstrings;
    int maxknownstrings;
    int numknownstrings;
    int freeknownstrings;
    ddef_t* globaldefs;

    unsigned char* knownzone;
    size_t knownzonesize;

    // originally defined in pr_exec, but moved into the switchable qcvm struct
#define MAX_STACK_DEPTH 1024 /*was 64*/ /* was 32 */
    prstack_t stack[MAX_STACK_DEPTH];
    int depth;

#define LOCALSTACK_SIZE 16384 /* was 2048*/
    int localstack[LOCALSTACK_SIZE];
    int localstack_used;

    // originally part of the sv_state_t struct
    // FIXME: put worldmodel in here too.
    double time;
    int num_edicts;
    int reserved_edicts;
    int max_edicts;
    edict_t* edicts; // can NOT be array indexed, because edict_t is variable
                     // sized, but can be used to reference the world ent
    struct qmodel_t* worldmodel;
    struct qmodel_t* (*GetModel)(
        int modelindex); // returns the model for the given index, or null.

    // originally from world.c
    areanode_t areanodes[AREA_NODES];
    int numareanodes;
};

extern qcvm_t* qcvm;
void PR_SwitchQCVM(qcvm_t* nvm);
