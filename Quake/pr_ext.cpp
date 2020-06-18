/* vim: set tabstop=4: */
/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// provides a few convienience extensions, primarily builtins, but also
// autocvars. Also note the set+seta features.

#include "progs.hpp"
#include "qcvm.hpp"
#include "cvar.hpp"
#include "client.hpp"
#include "server.hpp"
#include "console.hpp"

#include <cstddef>
#include <cstdint>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

struct qpic_t;

// there's a few different aproaches to tempstrings...
// the lame way is to just have a single one (vanilla).
// the only slightly less lame way is to just cycle between 16 or so (most
// engines). one funky way is to allocate a single large buffer and just
// concatenate it for more tempstring space. don't forget to resize (dp).
// alternatively, just allocate them persistently and purge them only when there
// appear to be no more references to it (fte). makes strzone redundant.

struct
{
    char name[MAX_QPATH];
    int type;
    qpic_t* pic;
} * qcpics;

size_t numqcpics;
size_t maxqcpics;

void PR_ReloadPics(bool purge)
{
    numqcpics = 0;

    free(qcpics);
    qcpics = nullptr;
    maxqcpics = 0;
}

int PR_MakeTempString(const char* val)
{
    char* tmp = PR_GetTempString();
    q_strlcpy(tmp, val, STRINGTEMP_LENGTH);
    return PR_SetEngineString(tmp);
}

void PR_AutoCvarChanged(cvar_t* var)
{
    (void)var;

    char* n;
    ddef_t* glob;
    qcvm_t* oldqcvm = qcvm;
    PR_SwitchQCVM(nullptr);

    if(sv.active)
    {
        PR_SwitchQCVM(&sv.qcvm);
        n = va("autocvar_%s", var->name);
        glob = ED_FindGlobal(n);
        if(glob)
        {
            if(!ED_ParseEpair((void*)qcvm->globals, glob, var->string))
            {
                Con_Warning("EXT: Unable to configure %s\n", n);
            }
        }
        PR_SwitchQCVM(nullptr);
    }

    if(cl.qcvm.globals)
    {
        PR_SwitchQCVM(nullptr);
        PR_SwitchQCVM(&cl.qcvm);
        n = va("autocvar_%s", var->name);
        glob = ED_FindGlobal(n);
        if(glob)
        {
            if(!ED_ParseEpair((void*)qcvm->globals, glob, var->string))
            {
                Con_Warning("EXT: Unable to configure %s\n", n);
            }
        }
        PR_SwitchQCVM(nullptr);
    }

    PR_SwitchQCVM(oldqcvm);
}
