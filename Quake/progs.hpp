/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

#include "edict.hpp"
#include "progs_types.hpp"
#include "pr_comp.hpp"  /* defs shared with qcc */
#include "progdefs.hpp" /* generated by program cdefs */
#include "common.hpp"
#include "sizebuf.hpp"
#include "protocol.hpp"

union eval_t
{
    string_t string;
    float _float;
    float vector[3];
    func_t function;
    int _int;
    int edict;
};

struct qcvm_t;

void PR_Init();

void PR_ExecuteProgram(func_t fnum);
void PR_ClearProgs(qcvm_t* vm);
bool PR_LoadProgs(
    const char* filename, bool fatal, builtin_t* builtins, size_t numbuiltins);

// from pr_ext.cpp
void PR_InitExtensions();
void PR_EnableExtensions(
    ddef_t* pr_globaldefs); // adds in the extra builtins etc
void PR_AutoCvarChanged(
    cvar_t* var); // updates the autocvar_ globals when their cvar is changed
void PR_ShutdownExtensions();   // nooooes!
void PR_ReloadPics(bool purge); // for gamedir or video changes
func_t PR_FindExtFunction(const char* entryname);
void PR_DumpPlatform_f(
    void); // console command: writes out a qsextensions.qc file
// special hacks...
int PF_SV_ForceParticlePrecache(const char* s);
int SV_Precache_Model(const char* s);
int SV_Precache_Sound(const char* s);
void PR_spawnfunc_misc_model(edict_t* self);

// from pr_edict, for pr_ext. reflection is messy.
bool ED_ParseEpair(void* base, ddef_t* key, const char* s);
const char* PR_UglyValueString(int type, eval_t* val);
ddef_t* ED_FindField(const char* name);
ddef_t* ED_FindGlobal(const char* name);
dfunction_t* ED_FindFunction(const char* fn_name);

const char* PR_GetString(int num);
int PR_SetEngineString(const char* s);
int PR_AllocString(int bufferlength, char** ptr);
void PR_ClearEngineString(int num);

void PR_Profile_f();

edict_t* ED_Alloc();
void ED_Free(edict_t* ed);

void ED_Print(edict_t* ed);
void ED_Write(FILE* f, edict_t* ed);
const char* ED_ParseEdict(const char* data, edict_t* ent);

void ED_WriteGlobals(FILE* f);
const char* ED_ParseGlobals(const char* data);

void ED_LoadFromFile(const char* data);

edict_t* EDICT_NUM(int n);
int NUM_FOR_EDICT(edict_t* e);

#define NEXT_EDICT(e) ((edict_t*)((byte*)e + qcvm->edict_size))

#define EDICT_TO_PROG(e) ((byte*)e - (byte*)qcvm->edicts)
#define PROG_TO_EDICT(e) ((edict_t*)((byte*)qcvm->edicts + e))

#define G_FLOAT(o) (qcvm->globals[o])
#define G_INT(o) (*(int*)&qcvm->globals[o])
#define G_EDICT(o) ((edict_t*)((byte*)qcvm->edicts + *(int*)&qcvm->globals[o]))
#define G_EDICTNUM(o) NUM_FOR_EDICT(G_EDICT(o))
#define G_VECTOR(o) (&qcvm->globals[o])
#define G_STRING(o) (PR_GetString(*(string_t*)&qcvm->globals[o]))
#define G_FUNCTION(o) (*(func_t*)&qcvm->globals[o])

#define G_VECTORSET(r, x, y, z) \
    do                          \
    {                           \
        G_FLOAT((r) + 0) = x;   \
        G_FLOAT((r) + 1) = y;   \
        G_FLOAT((r) + 2) = z;   \
    } while(0)

#define E_FLOAT(e, o) (((float*)&e->v)[o])
#define E_INT(e, o) (*(int*)&((float*)&e->v)[o])
#define E_VECTOR(e, o) (&((float*)&e->v)[o])
#define E_STRING(e, o) (PR_GetString(*(string_t*)&((float*)&e->v)[o]))

extern int type_size[8];

[[noreturn]] void PR_RunError(const char* error, ...) FUNC_PRINTF(1, 2);
#ifdef __WATCOMC__
#pragma aux PR_RunError aborts;
#endif

void ED_PrintEdicts();
void ED_PrintNum(int ent);

eval_t* GetEdictFieldValue(
    edict_t* ed, int fldofs); // handles invalid offsets with a null
int ED_FindFieldOffset(const char* name);

// from pr_cmds, no longer static so that pr_ext can use them.
sizebuf_t* WriteDest();
char* PR_GetTempString();
int PR_MakeTempString(const char* val);
char* PF_VarString(int first);
#define STRINGTEMP_BUFFERS 1024
#define STRINGTEMP_LENGTH 1024
void PF_Fixme(); // the 'unimplemented' builtin. woot.

struct areanode_t;
struct qmodel_t;
struct edict_t;

// extern cvar_t pr_checkextension; // if 0, extensions are disabled (unless they'd
                                    // be fatal, but they're still spammy)

#define CSIE_KEYDOWN 0
#define CSIE_KEYUP 1
#define CSIE_MOUSEDELTA 2
#define CSIE_MOUSEABS 3
//#define CSIE_ACCELEROMETER	4
//#define CSIE_FOCUS			5
#define CSIE_JOYAXIS 6
//#define CSIE_GYROSCOPE		7

extern globalvars_t* pr_global_struct;

extern builtin_t pr_ssqcbuiltins[];
extern int pr_ssqcnumbuiltins;
extern builtin_t pr_csqcbuiltins[];
extern int pr_csqcnumbuiltins;
