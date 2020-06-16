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

#include "host.hpp"
#include "console.hpp"
#include "cvar.hpp"
#include "pr_comp.hpp"
#include "progs.hpp"
#include "protocol.hpp"
#include "quakedef.hpp"
#include "quakeglm.hpp"
#include "util.hpp"
#include "worldtext.hpp"
#include "console.hpp"
#include "glquake.hpp"
#include "msg.hpp"
#include "sys.hpp"
#include "cmd.hpp"
#include "developer.hpp"

#include <cmath>
#include <glm/gtx/rotate_vector.hpp>
#include "quakeglm_qquat.hpp"

static char pr_string_temp[STRINGTEMP_BUFFERS][STRINGTEMP_LENGTH];
static byte pr_string_tempindex = 0;

char* PR_GetTempString()
{
    return pr_string_temp[(STRINGTEMP_BUFFERS - 1) & ++pr_string_tempindex];
}

#define RETURN_EDICT(e) (((int*)qcvm->globals)[OFS_RETURN] = EDICT_TO_PROG(e))

#define MSG_BROADCAST 0 // unreliable to all
#define MSG_ONE 1       // reliable to one (msg_entity)
#define MSG_ALL 2       // reliable to all
#define MSG_INIT 3      // write to the init string
#define MSG_EXT_MULTICAST \
    4 // temporary buffer that can be splurged more reliably / with more
      // control.
#define MSG_EXT_ENTITY \
    5 // for csqc networking. we don't actually support this. I'm just defining
      // it for completeness.

[[nodiscard]] QUAKE_FORCEINLINE static qvec3 extractVector(
    const int parm) noexcept
{
    float* const ptr = G_VECTOR(parm);
    return {ptr[0], ptr[1], ptr[2]};
}

QUAKE_FORCEINLINE static void returnVector(const qvec3& v) noexcept
{
    G_VECTOR(OFS_RETURN)[0] = v[0];
    G_VECTOR(OFS_RETURN)[1] = v[1];
    G_VECTOR(OFS_RETURN)[2] = v[2];
}



/*
===============================================================================

    BUILT-IN FUNCTIONS

===============================================================================
*/

char* PF_VarString(int first)
{
    int i;
    static char out[1024];
    size_t s;

    out[0] = 0;
    s = 0;
    for(i = first; i < qcvm->argc; i++)
    {
        s = q_strlcat(out, G_STRING((OFS_PARM0 + i * 3)), sizeof(out));
        if(s >= sizeof(out))
        {
            Con_Warning("PF_VarString: overflow (string truncated)\n");
            return out;
        }
    }
    if(s > 255)
    {
        if(!dev_overflows.varstring ||
            dev_overflows.varstring + CONSOLE_RESPAM_TIME < realtime)
        {
            Con_DWarning(
                "PF_VarString: %i characters exceeds standard limit of 255 "
                "(max = %d).\n",
                (int)s, (int)(sizeof(out) - 1));
            dev_overflows.varstring = realtime;
        }
    }
    return out;
}


/*
=================
PF_error

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
static void PF_error()
{
    char* s;
    edict_t* ed;

    s = PF_VarString(0);
    Con_Printf("======SERVER ERROR in %s:\n%s\n",
        PR_GetString(qcvm->xfunction->s_name), s);
    ed = PROG_TO_EDICT(pr_global_struct->self);
    ED_Print(ed);

    Host_Error("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
static void PF_objerror()
{
    char* s;
    edict_t* ed;

    s = PF_VarString(0);
    Con_Printf("======OBJECT ERROR in %s:\n%s\n",
        PR_GetString(qcvm->xfunction->s_name), s);
    ed = PROG_TO_EDICT(pr_global_struct->self);
    ED_Print(ed);
    ED_Free(ed);

    // Host_Error ("Program error"); //johnfitz -- by design, this should not be
    // fatal
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
static void PF_makevectors()
{
    const auto org = extractVector(OFS_PARM0);

    std::tie(pr_global_struct->v_forward, pr_global_struct->v_right,
        pr_global_struct->v_up) = quake::util::getAngledVectors(org);
}


/*
==============
PF_makeforward

Writes new value for v_forward based on angles
makeforward(vector)
==============
*/
static void PF_makeforward()
{
    const auto org = extractVector(OFS_PARM0);

    pr_global_struct->v_forward = quake::util::getFwdVecFromPitchYawRoll(org);
}


/*
==============
PF_maprange

TODO VR: (P2) docs
maprange(vector)
==============
*/
static void PF_maprange()
{
    const auto input = G_FLOAT(OFS_PARM0);
    const auto inputMin = G_FLOAT(OFS_PARM1);
    const auto inputMax = G_FLOAT(OFS_PARM2);
    const auto outputMin = G_FLOAT(OFS_PARM3);
    const auto outputMax = G_FLOAT(OFS_PARM4);

    G_FLOAT(OFS_RETURN) = quake::util::mapRange<float>(
        input, inputMin, inputMax, outputMin, outputMax);
}


/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics
of the world (setting velocity and waiting).  Directly changing origin
will not set internal links correctly, so clipping would be messed up.

This should be called when an object is spawned, and then only if it is
teleported.

setorigin(entity, origin)
=================
*/
static void PF_setorigin()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    e->v.origin = extractVector(OFS_PARM1);
    SV_LinkEdict(e, false);
}

static void SetMinMaxSize(
    edict_t* e, const qvec3& minvec, const qvec3& maxvec, bool rotate)
{
    qvec3 rmin;
    qvec3 rmax;
    qvec3 bounds[2];
    float xvector[2];
    float yvector[2];
    float a;
    qvec3 base;
    qvec3 transformed;
    int i;
    int j;
    int k;
    int l;

    for(i = 0; i < 3; i++)
    {
        if(minvec[i] > maxvec[i])
        {
            PR_RunError("backwards mins/maxs");
        }
    }

    rotate = false; // FIXME: implement rotation properly again

    if(!rotate)
    {
        rmin = minvec;
        rmax = maxvec;
    }
    else
    {
        // find min / max for rotations
        a = e->v.angles[1] / 180 * M_PI;

        xvector[0] = cos(a);
        xvector[1] = sin(a);
        yvector[0] = -sin(a);
        yvector[1] = cos(a);

        bounds[0] = minvec;
        bounds[1] = maxvec;

        rmin[0] = rmin[1] = rmin[2] = FLT_MAX;
        rmax[0] = rmax[1] = rmax[2] = -FLT_MAX;

        for(i = 0; i <= 1; i++)
        {
            base[0] = bounds[i][0];
            for(j = 0; j <= 1; j++)
            {
                base[1] = bounds[j][1];
                for(k = 0; k <= 1; k++)
                {
                    base[2] = bounds[k][2];

                    // transform the point
                    transformed[0] =
                        xvector[0] * base[0] + yvector[0] * base[1];
                    transformed[1] =
                        xvector[1] * base[0] + yvector[1] * base[1];
                    transformed[2] = base[2];

                    for(l = 0; l < 3; l++)
                    {
                        if(transformed[l] < rmin[l])
                        {
                            rmin[l] = transformed[l];
                        }
                        if(transformed[l] > rmax[l])
                        {
                            rmax[l] = transformed[l];
                        }
                    }
                }
            }
        }
    }

    // set derived values
    e->v.mins = rmin;
    e->v.maxs = rmax;
    e->v.size = maxvec - minvec;

    SV_LinkEdict(e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize(entity, minvector, maxvector)
=================
*/
static void PF_setsize()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const auto minvec = extractVector(OFS_PARM1);
    const auto maxvec = extractVector(OFS_PARM2);
    SetMinMaxSize(e, minvec, maxvec, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
static void PF_sv_setmodel()
{
    edict_t* e = G_EDICT(OFS_PARM0);
    const char* m = G_STRING(OFS_PARM1);

    // check to see if model was properly precached
    int i;
    const char** check;
    for(i = 0, check = sv.model_precache; *check; i++, check++)
    {
        if(!strcmp(*check, m))
        {
            break;
        }
    }

    if(!*check)
    {
        PR_RunError("no precache: %s", m);
    }
    e->v.model = PR_SetEngineString(*check);
    e->v.modelindex = i; // SV_ModelIndex (m);

    qmodel_t* mod = sv.models[(int)e->v.modelindex]; // Mod_ForName (m, true);

    if(mod)
    // johnfitz -- correct physics cullboxes for bmodels
    {
        if(mod->type == mod_brush)
        {
            SetMinMaxSize(e, mod->clipmins, mod->clipmaxs, true);
        }
        else
        {
            SetMinMaxSize(e, mod->mins, mod->maxs, true);
        }
    }
    // johnfitz
    else
    {
        SetMinMaxSize(e, vec3_zero, vec3_zero, true);
    }
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
static void PF_bprint()
{
    char* s;

    s = PF_VarString(0);
    SV_BroadcastPrintf("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
static void PF_sprint()
{
    char* s;
    client_t* client;
    int entnum;

    entnum = G_EDICTNUM(OFS_PARM0);
    s = PF_VarString(1);

    if(entnum < 1 || entnum > svs.maxclients)
    {
        Con_Printf("tried to sprint to a non-client\n");
        return;
    }

    client = &svs.clients[entnum - 1];

    MSG_WriteChar(&client->message, svc_print);
    MSG_WriteString(&client->message, s);
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
static void PF_centerprint()
{
    char* s;
    client_t* client;
    int entnum;

    entnum = G_EDICTNUM(OFS_PARM0);
    s = PF_VarString(1);

    if(entnum < 1 || entnum > svs.maxclients)
    {
        Con_Printf("tried to sprint to a non-client\n");
        return;
    }

    client = &svs.clients[entnum - 1];

    MSG_WriteChar(&client->message, svc_centerprint);
    MSG_WriteString(&client->message, s);
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
static void PF_normalize()
{
    const auto v = extractVector(OFS_PARM0);

    double new_temp = std::sqrt(
        (double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);

    if(new_temp == 0)
    {
        returnVector(vec3_zero);
    }
    else
    {
        new_temp = 1 / new_temp;
        const auto res = v * static_cast<qfloat>(new_temp);
        returnVector(res);
    }
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
static void PF_vlen()
{
    const auto v = extractVector(OFS_PARM0);

    const double new_temp = std::sqrt(
        (double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);

    G_FLOAT(OFS_RETURN) = new_temp;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
static void PF_vectoyaw()
{
    const auto v = extractVector(OFS_PARM0);

    float yaw;
    if(v[1] == 0 && v[0] == 0)
    {
        yaw = 0;
    }
    else
    {
        yaw = (int)(atan2(v[1], v[0]) * 180 / M_PI);
        if(yaw < 0)
        {
            yaw += 360;
        }
    }

    G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
static void PF_vectoangles()
{
    const auto v = extractVector(OFS_PARM0);

    float forward;
    float yaw;
    float pitch;

    if(v[1] == 0 && v[0] == 0)
    {
        yaw = 0;
        if(v[2] > 0)
        {
            pitch = 90;
        }
        else
        {
            pitch = 270;
        }
    }
    else
    {
        yaw = (int)(atan2(v[1], v[0]) * 180 / M_PI);
        if(yaw < 0)
        {
            yaw += 360;
        }

        forward = sqrt(v[0] * v[0] + v[1] * v[1]);
        pitch = (int)(atan2(v[2], forward) * 180 / M_PI);
        if(pitch < 0)
        {
            pitch += 360;
        }
    }

    G_FLOAT(OFS_RETURN + 0) = pitch;
    G_FLOAT(OFS_RETURN + 1) = yaw;
    G_FLOAT(OFS_RETURN + 2) = 0;
}

/*
=================
PF_Random

Returns a number from 0 <= num < 1

random()
=================
*/
static void PF_random()
{
    const float num = (rand() & 0x7fff) / ((float)0x7fff);
    G_FLOAT(OFS_RETURN) = num;
}

/*
=================
PF_Pow

power function
=================
*/
static void PF_pow()
{
    const float base = G_FLOAT(OFS_PARM0);
    const float exp = G_FLOAT(OFS_PARM1);

    const float res = std::pow(base, exp);
    G_FLOAT(OFS_RETURN) = res;
}

/*
=================
PF_particle

particle(origin, dir, color, count)
=================
*/
static void PF_particle()
{
    const auto org = extractVector(OFS_PARM0);
    const auto dir = extractVector(OFS_PARM1);
    const float color = G_FLOAT(OFS_PARM2);
    const float count = G_FLOAT(OFS_PARM3);
    SV_StartParticle(org, dir, color, count);
}

/*
=================
PF_particle2

particle2(origin, dir, preset, count)
=================
*/
static void PF_particle2()
{
    const auto org = extractVector(OFS_PARM0);
    const auto dir = extractVector(OFS_PARM1);
    const int preset = G_FLOAT(OFS_PARM2);
    const int count = G_FLOAT(OFS_PARM3);
    SV_StartParticle2(org, dir, preset, count);
}

/*
=================
PF_Haptic

VR haptics function
=================
*/
static void PF_haptic()
{
    // {0 = off hand, 1 = main hand}
    const int hand = G_FLOAT(OFS_PARM0);

    const float delay = G_FLOAT(OFS_PARM1);
    const float duration = G_FLOAT(OFS_PARM2);
    const float frequency = G_FLOAT(OFS_PARM3);
    const float amplitude = G_FLOAT(OFS_PARM4);

    extern void VR_DoHaptic(const int hand, const float delay,
        const float duration, const float frequency, const float amplitude);
    VR_DoHaptic(hand, delay, duration, frequency, amplitude);
}

/*
=================
PF_min

min function
=================
*/
static void PF_min()
{
    G_FLOAT(OFS_RETURN) = std::min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

/*
=================
PF_max

max function
=================
*/
static void PF_max()
{
    G_FLOAT(OFS_RETURN) = std::max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}


/*
=================
PF_ambientsound

=================
*/
static void PF_sv_ambientsound()
{
    const char* samp;
    const char** check;
    float vol;
    float attenuation;
    int i;

    int soundnum;
    int large = false; // johnfitz -- PROTOCOL_QUAKEVR

    const auto pos = extractVector(OFS_PARM0);
    samp = G_STRING(OFS_PARM1);
    vol = G_FLOAT(OFS_PARM2);
    attenuation = G_FLOAT(OFS_PARM3);

    // check to see if samp was properly precached
    for(soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
    {
        if(!strcmp(*check, samp))
        {
            break;
        }
    }

    if(!*check)
    {
        Con_Printf("no precache: %s\n", samp);
        return;
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(soundnum > 255)
    {
        large = true;
    }
    // johnfitz

    // add an svc_spawnambient command to the level signon packet

    // johnfitz -- PROTOCOL_QUAKEVR
    if(large)
    {
        MSG_WriteByte(&sv.signon, svc_spawnstaticsound2);
    }
    else
    {
        MSG_WriteByte(&sv.signon, svc_spawnstaticsound);
    }
    // johnfitz

    for(i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.signon, pos[i], sv.protocolflags);
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(large)
    {
        MSG_WriteShort(&sv.signon, soundnum);
    }
    else
    {
        MSG_WriteByte(&sv.signon, soundnum);
    }
    // johnfitz

    MSG_WriteByte(&sv.signon, vol * 255);
    MSG_WriteByte(&sv.signon, attenuation * 64);
}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
static void PF_sound()
{
    edict_t* entity = G_EDICT(OFS_PARM0);
    const int channel = G_FLOAT(OFS_PARM1);
    const char* sample = G_STRING(OFS_PARM2);
    const int volume = G_FLOAT(OFS_PARM3) * 255;
    const float attenuation = G_FLOAT(OFS_PARM4);

    if(volume < 0 || volume > 255)
    {
        Host_Error("SV_StartSound: volume = %i", volume);
    }

    if(attenuation < 0 || attenuation > 4)
    {
        Host_Error("SV_StartSound: attenuation = %f", attenuation);
    }

    if(channel < 0 || channel > 7)
    {
        Host_Error("SV_StartSound: channel = %i", channel);
    }

    SV_StartSound(entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
static void PF_break()
{
    Con_Printf("break statement\n");
    *(int*)-4 = 0; // dump to debugger
    //	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entities, and also slide box
entities if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
static void PF_traceline()
{
    auto v1 = extractVector(OFS_PARM0);
    auto v2 = extractVector(OFS_PARM1);
    int nomonsters = G_FLOAT(OFS_PARM2);
    edict_t* ent = G_EDICT(OFS_PARM3);

    /* FIXME FIXME FIXME: Why do we hit this with certain progs.dat ?? */
    if(quake::vr::developerMode())
    {
        if(IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) || IS_NAN(v2[0]) ||
            IS_NAN(v2[1]) || IS_NAN(v2[2]))
        {
            Con_Warning(
                "NAN in traceline:\nv1(%f %f %f) v2(%f %f %f)\nentity %d\n",
                v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], NUM_FOR_EDICT(ent));
        }
    }

    if(IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]))
    {
        v1[0] = v1[1] = v1[2] = 0;
    }

    if(IS_NAN(v2[0]) || IS_NAN(v2[1]) || IS_NAN(v2[2]))
    {
        v2[0] = v2[1] = v2[2] = 0;
    }

    const trace_t trace = SV_MoveTrace(v1, v2, nomonsters, ent);

    pr_global_struct->trace_allsolid = trace.allsolid;
    pr_global_struct->trace_startsolid = trace.startsolid;
    pr_global_struct->trace_fraction = trace.fraction;
    pr_global_struct->trace_inwater = trace.inwater;
    pr_global_struct->trace_inopen = trace.inopen;
    pr_global_struct->trace_endpos = trace.endpos;
    pr_global_struct->trace_plane_normal = trace.plane.normal;
    pr_global_struct->trace_plane_dist = trace.plane.dist;

    if(trace.ent)
    {
        pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
    }
    else
    {
        pr_global_struct->trace_ent = EDICT_TO_PROG(qcvm->edicts);
    }
}

/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
#if 0
static void PF_checkpos()
{
}
#endif

//============================================================================

static byte* checkpvs; // ericw -- changed to malloc
static int checkpvs_capacity;

static int PF_newcheckclient(int check)
{
    int i;
    edict_t* ent;
    int pvsbytes;

    // cycle to the next one

    if(check < 1)
    {
        check = 1;
    }
    if(check > svs.maxclients)
    {
        check = svs.maxclients;
    }

    if(check == svs.maxclients)
    {
        i = 1;
    }
    else
    {
        i = check + 1;
    }

    for(;; i++)
    {
        if(i == svs.maxclients + 1)
        {
            i = 1;
        }

        ent = EDICT_NUM(i);

        if(i == check)
        {
            break; // didn't find anything else
        }

        if(ent->free)
        {
            continue;
        }
        if(ent->v.health <= 0)
        {
            continue;
        }
        if(quake::util::hasFlag(ent, FL_NOTARGET))
        {
            continue;
        }

        // anything that is a client, or has a client as an enemy
        break;
    }

    // get the PVS for the entity
    qvec3 org = ent->v.origin + ent->v.view_ofs;

    mleaf_t* leaf = Mod_PointInLeaf(org, qcvm->worldmodel);
    byte* pvs = Mod_LeafPVS(leaf, qcvm->worldmodel);

    pvsbytes = (qcvm->worldmodel->numleafs + 7) >> 3;
    if(checkpvs == nullptr || pvsbytes > checkpvs_capacity)
    {
        checkpvs_capacity = pvsbytes;
        checkpvs = (byte*)realloc(checkpvs, checkpvs_capacity);
        if(!checkpvs)
        {
            Sys_Error("PF_newcheckclient: realloc() failed on %d bytes",
                checkpvs_capacity);
        }
    }
    memcpy(checkpvs, pvs, pvsbytes);

    return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define MAX_CHECK 16
static int c_invis, c_notvis;
static void PF_sv_checkclient()
{
    edict_t* ent;
    edict_t* self;
    mleaf_t* leaf;
    int l;

    // find a new check if on a new frame
    if(qcvm->time - sv.lastchecktime >= 0.1)
    {
        sv.lastcheck = PF_newcheckclient(sv.lastcheck);
        sv.lastchecktime = qcvm->time;
    }

    // return check if it might be visible
    ent = EDICT_NUM(sv.lastcheck);
    if(ent->free || ent->v.health <= 0)
    {
        RETURN_EDICT(qcvm->edicts);
        return;
    }

    // if current entity can't possibly see the check entity, return 0
    self = PROG_TO_EDICT(pr_global_struct->self);
    const qvec3 view = self->v.origin + self->v.view_ofs;
    leaf = Mod_PointInLeaf(view, qcvm->worldmodel);
    l = (leaf - qcvm->worldmodel->leafs) - 1;
    if((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7))))
    {
        c_notvis++;
        RETURN_EDICT(qcvm->edicts);
        return;
    }

    // might be able to see it
    c_invis++;
    RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
static void PF_stuffcmd()
{
    int entnum;
    const char* str;
    client_t* old;

    entnum = G_EDICTNUM(OFS_PARM0);
    if(entnum < 1 || entnum > svs.maxclients)
    {
        PR_RunError("Parm 0 not a client");
    }
    str = G_STRING(OFS_PARM1);

    old = host_client;
    host_client = &svs.clients[entnum - 1];
    Host_ClientCommands("%s", str);
    host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
static void PF_localcmd()
{
    const char* str;

    str = G_STRING(OFS_PARM0);
    Cbuf_AddText(str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void PF_cvar()
{
    G_FLOAT(OFS_RETURN) = Cvar_VariableValue(G_STRING(OFS_PARM0));
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
static void PF_cvar_set()
{
    const char* var = G_STRING(OFS_PARM0);
    const char* val = G_STRING(OFS_PARM1);

    Cvar_Set(var, val);
}

/*
=================
PF_cvar_hmake

float cvar_hmake (string)
=================
*/
static void PF_cvar_hmake()
{
    G_INT(OFS_RETURN) = Cvar_MakeHandle(G_STRING(OFS_PARM0));
}

/*
=================
PF_cvar_hget

float cvar_hget (float)
=================
*/
static void PF_cvar_hget()
{
    G_FLOAT(OFS_RETURN) = Cvar_GetValueFromHandle(G_INT(OFS_PARM0));
}

/*
=================
PF_cvar_hget

float cvar_hget (float)
=================
*/
static void PF_cvar_hset()
{
    Cvar_SetValueFromHandle(G_INT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

template <typename F>
static void forAllActiveOrSpawnedClients(F&& f)
{
    for(int i = 0; i < svs.maxclients; i++)
    {
        client_t& client = svs.clients[i];

        if(client.active || client.spawned)
        {
            f(client);
        }
    }
}

static void PF_worldtext_hmake()
{
    if(!sv.hasAnyFreeWorldTextHandle())
    {
        Host_Error("No free world text handles available");
        return;
    }

    const WorldTextHandle wth = sv.makeWorldTextHandle();

    forAllActiveOrSpawnedClients(
        [&](client_t& client) { sv.SendMsg_WorldTextHMake(client, wth); });

    G_INT(OFS_RETURN) = wth;
}

static void PF_worldtext_hsettext()
{
    const WorldTextHandle wth = G_INT(OFS_PARM0);
    const char* text = G_STRING(OFS_PARM1);

    if(!sv.isValidWorldTextHandle(wth))
    {
        Host_Error("Invalid world text handle '%d'", wth);
        return;
    }

    sv.getWorldText(wth)._text = text;

    forAllActiveOrSpawnedClients([&](client_t& client) {
        sv.SendMsg_WorldTextHSetText(client, wth, text);
    });
}

static void PF_worldtext_hsetpos()
{
    const WorldTextHandle wth = G_INT(OFS_PARM0);
    const qvec3 pos = extractVector(OFS_PARM1);

    if(!sv.isValidWorldTextHandle(wth))
    {
        Host_Error("Invalid world text handle '%d'", wth);
        return;
    }

    sv.getWorldText(wth)._pos = pos;

    forAllActiveOrSpawnedClients([&](client_t& client) {
        sv.SendMsg_WorldTextHSetPos(client, wth, pos);
    });
}

static void PF_worldtext_hsetangles()
{
    const WorldTextHandle wth = G_INT(OFS_PARM0);
    const qvec3 angles = extractVector(OFS_PARM1);

    if(!sv.isValidWorldTextHandle(wth))
    {
        Host_Error("Invalid world text handle '%d'", wth);
        return;
    }

    sv.getWorldText(wth)._angles = angles;

    forAllActiveOrSpawnedClients([&](client_t& client) {
        sv.SendMsg_WorldTextHSetAngles(client, wth, angles);
    });
}

static void PF_worldtext_hsethalign()
{
    const WorldTextHandle wth = G_INT(OFS_PARM0);
    const float hAlignFloat = G_FLOAT(OFS_PARM1);

    if(!sv.isValidWorldTextHandle(wth))
    {
        Host_Error("Invalid world text handle '%d'", wth);
        return;
    }

    const auto hAlign = static_cast<WorldText::HAlign>(hAlignFloat);

    sv.getWorldText(wth)._hAlign = hAlign;

    forAllActiveOrSpawnedClients([&](client_t& client) {
        sv.SendMsg_WorldTextHSetHAlign(client, wth, hAlign);
    });
}

static void PF_strlen()
{
    G_FLOAT(OFS_RETURN) = std::strlen(G_STRING(OFS_PARM0));
}

static void PF_nthchar()
{
    G_FLOAT(OFS_RETURN) =
        (G_STRING(OFS_PARM0))[static_cast<int>(G_FLOAT(OFS_PARM1))];
}

static void PF_substr()
{
    const char* s = G_STRING(OFS_PARM0);
    const int b = static_cast<int>(G_FLOAT(OFS_PARM1));
    const int e = static_cast<int>(G_FLOAT(OFS_PARM2));

    char* buf = PR_GetTempString();
    int i = 0;
    for(; i < e - b; ++i)
    {
        buf[i] = s[b + i];
    }

    buf[i] = '\0';

    // TODO VR: (P1): does this continuously allocate new strings?
    G_INT(OFS_RETURN) = PR_SetEngineString(buf);
}

static void PF_calcthrowangle()
{
    // TODO VR: (P2): repetition with `SV_AddGravityImpl`
    const auto getGravity = [&](const float entGravity) {
        extern cvar_t sv_gravity;
        return (double)entGravity * (double)sv_gravity.value * host_frametime;
    };

    const float inEntGravity = G_FLOAT(OFS_PARM0);

    const float entGravity = inEntGravity == 0 ? 1.f : inEntGravity;
    const float throwSpeed = G_FLOAT(OFS_PARM1);
    const qvec3 fromPos = extractVector(OFS_PARM2);
    const qvec3 toPos = extractVector(OFS_PARM3);

    float xx = toPos.x - fromPos.x;
    float xy = toPos.y - fromPos.y;
    float x = std::sqrt(xx * xx + xy * xy);
    float z = fromPos.z - toPos.z;

    float v = throwSpeed;
    float g = -getGravity(entGravity);

    float xSqrt = (v * v * v * v) - (g * (g * (x * x) + 2.f * z * (v * v)));

    // Not enough range
    if(xSqrt < 0)
    {
        G_FLOAT(OFS_RETURN) = 0.0f;
        return;
    }

    G_FLOAT(OFS_RETURN) =
        glm::degrees(std::atan2(((v * v) - std::sqrt(xSqrt)), (g * x)));
}

static void PF_rotatevec()
{
    const qvec3 vec = extractVector(OFS_PARM0);
    const qvec3 upx = extractVector(OFS_PARM1);
    const float angle = glm::radians(G_FLOAT(OFS_PARM2));

    auto rers =
        glm::normalize(vec + glm::vec3{0.0, 0.0, std::abs(std::tan(angle))});
    returnVector(rers);
    return;

    // up direction:
    glm::vec3 up(0.0, 0.0, 1.0);
    // find right vector:
    auto right = glm::cross(glm::normalize(vec), glm::normalize(up));

    qquat m;
    m = glm::rotate(m, angle, right);
    m = glm::normalize(m);

    qvec3 rr = m * vec;
    returnVector(rr);
    return;

    // TODO VR: (P1): fix this. consider checking QSS source code for rotation
    // code



    glm::mat4 rotationMat(1); // Creates a identity matrix
    rotationMat = glm::rotate(rotationMat, angle, right);
    auto res = glm::vec3(rotationMat * glm::vec4(vec, 1.0));

    returnVector(res);

    //    returnVector(glm::rotate(vec, angle, up));
}

static void PF_sin()
{
    G_FLOAT(OFS_RETURN) = std::sin(glm::radians(G_FLOAT(OFS_PARM0)));
}

static void PF_cos()
{
    G_FLOAT(OFS_RETURN) = std::cos(glm::radians(G_FLOAT(OFS_PARM0)));
}

static void PF_tan()
{
    G_FLOAT(OFS_RETURN) = std::tan(glm::radians(G_FLOAT(OFS_PARM0)));
}

static void PF_asin()
{
    G_FLOAT(OFS_RETURN) = glm::degrees(std::asin(G_FLOAT(OFS_PARM0)));
}

static void PF_acos()
{
    G_FLOAT(OFS_RETURN) = glm::degrees(std::acos(G_FLOAT(OFS_PARM0)));
}

static void PF_atan()
{
    G_FLOAT(OFS_RETURN) = glm::degrees(std::atan(G_FLOAT(OFS_PARM0)));
}

static void PF_sqrt()
{
    G_FLOAT(OFS_RETURN) = std::sqrt(G_FLOAT(OFS_PARM0));
}


static void PF_atan2()
{
    G_FLOAT(OFS_RETURN) =
        glm::degrees(std::atan2(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1)));
}


/*
=================
PF_cvar_hclear

void cvar_hclear ()
=================
*/
static void PF_cvar_hclear()
{
    Cvar_ClearAllHandles();
}

static void PF_redirectvector()
{
    const auto input = extractVector(OFS_PARM0);
    const auto exemplar = extractVector(OFS_PARM1);
    returnVector(quake::util::redirectVector(input, exemplar));
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
static void PF_findradius()
{
    edict_t* chain = (edict_t*)qcvm->edicts;

    const auto org = extractVector(OFS_PARM0);
    float rad = G_FLOAT(OFS_PARM1);

    edict_t* ent = NEXT_EDICT(qcvm->edicts);
    for(int i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
    {
        if(ent->free)
        {
            continue;
        }
        if(ent->v.solid == SOLID_NOT)
        {
            continue;
        }

        qvec3 eorg;
        for(int j = 0; j < 3; j++)
        {
            eorg[j] = org[j] - (ent->v.origin[j] +
                                   (ent->v.mins[j] + ent->v.maxs[j]) * 0.5);
        }

        if(glm::length(eorg) > rad)
        {
            continue;
        }

        ent->v.chain = EDICT_TO_PROG(chain);
        chain = ent;
    }

    RETURN_EDICT(chain);
}

/*
=========
PF_dprint
=========
*/
static void PF_dprint()
{
    Con_DPrintf("%s", PF_VarString(0));
}

static void PF_ftos()
{
    float v = G_FLOAT(OFS_PARM0);
    char* s = PR_GetTempString();

    if(v == (int)v)
    {
        sprintf(s, "%d", (int)v);
    }
    else
    {
        sprintf(s, "%5.1f", v);
    }

    G_INT(OFS_RETURN) = PR_SetEngineString(s);
}

static void PF_fabs()
{
    float v = G_FLOAT(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = fabs(v);
}

static void PF_vtos()
{
    char* s = PR_GetTempString();
    sprintf(s, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0],
        G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
    G_INT(OFS_RETURN) = PR_SetEngineString(s);
}

static void PF_Spawn()
{
    edict_t* ed = ED_Alloc();
    RETURN_EDICT(ed);
}

static void PF_Remove()
{
    edict_t* ed = G_EDICT(OFS_PARM0);
    ED_Free(ed);
}


// entity (entity start, .string field, string match) find = #5;
static void PF_Find()
{
    int e = G_EDICTNUM(OFS_PARM0);
    int f = G_INT(OFS_PARM1);
    const char* s = G_STRING(OFS_PARM2);

    if(!s)
    {
        PR_RunError("PF_Find: bad search string");
    }

    const char* t;
    edict_t* ed;
    for(e++; e < qcvm->num_edicts; e++)
    {
        ed = EDICT_NUM(e);
        if(ed->free)
        {
            continue;
        }
        t = E_STRING(ed, f);
        if(!t)
        {
            continue;
        }
        if(!strcmp(t, s))
        {
            RETURN_EDICT(ed);
            return;
        }
    }

    RETURN_EDICT(qcvm->edicts);
}

static void PR_CheckEmptyString(const char* s)
{
    if(s[0] <= ' ')
    {
        PR_RunError("Bad string");
    }
}

static void PF_precache_file()
{
    // precache_file is only used to copy files with qcc, it does nothing
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

static void PF_sv_precache_sound()
{
    const char* s;
    int i;

    if(sv.state != ss_loading)
    {
        PR_RunError(
            "PF_Precache_*: Precache can only be done in spawn functions");
    }

    s = G_STRING(OFS_PARM0);
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
    PR_CheckEmptyString(s);

    for(i = 0; i < MAX_SOUNDS; i++)
    {
        if(!sv.sound_precache[i])
        {
            sv.sound_precache[i] = s;
            return;
        }
        if(!strcmp(sv.sound_precache[i], s))
        {
            return;
        }
    }
    PR_RunError("PF_precache_sound: overflow");
}

static void PF_sv_precache_model()
{
    const char* s;
    int i;

    if(sv.state != ss_loading)
    {
        PR_RunError(
            "PF_Precache_*: Precache can only be done in spawn functions");
    }

    s = G_STRING(OFS_PARM0);
    G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
    PR_CheckEmptyString(s);

    for(i = 0; i < MAX_MODELS; i++)
    {
        if(!sv.model_precache[i])
        {
            sv.model_precache[i] = s;

            // VR: Load with a fallback in case a model is missing. Useful for
            // mission-pack specific models that are missing for some reason.
            sv.models[i] = Mod_ForName_WithFallback(s, "progs/player.mdl");
            return;
        }
        if(!strcmp(sv.model_precache[i], s))
        {
            return;
        }
    }
    PR_RunError("PF_precache_model: overflow");
}


static void PF_coredump()
{
    ED_PrintEdicts();
}

static void PF_traceon()
{
    qcvm->trace = true;
}

static void PF_traceoff()
{
    qcvm->trace = false;
}

static void PF_eprint()
{
    ED_PrintNum(G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
static void PF_walkmove()
{
    edict_t* ent;
    float yaw;
    float dist;
    qvec3 move;
    dfunction_t* oldf;
    int oldself;

    ent = PROG_TO_EDICT(pr_global_struct->self);
    yaw = G_FLOAT(OFS_PARM0);
    dist = G_FLOAT(OFS_PARM1);

    if(!quake::util::hasAnyFlag(ent, FL_ONGROUND, FL_FLY, FL_SWIM))
    {
        G_FLOAT(OFS_RETURN) = 0;
        return;
    }

    yaw *= M_PI * 2 / 360;

    move[0] = cos(yaw) * dist;
    move[1] = sin(yaw) * dist;
    move[2] = 0;

    // save program state, because SV_movestep may call other progs
    oldf = qcvm->xfunction;
    oldself = pr_global_struct->self;

    G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


    // restore program state
    qcvm->xfunction = oldf;
    pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
static void PF_droptofloor()
{
    edict_t& ent = *PROG_TO_EDICT(pr_global_struct->self);

    qfloat highestZ = std::numeric_limits<qfloat>::lowest();
    edict_t* groundEnt = nullptr;

    const auto processHit = [&](const qvec3& xyOffset) {
        const qvec3 corner =
            ent.v.origin + xyOffset + qvec3{0, 0, ent.v.mins[2]};

        const trace_t trace = SV_MoveTrace(
            corner, corner + qvec3{0, 0, -256._qf}, MOVE_NOMONSTERS, &ent);

        if(!quake::util::hitSomething(trace) || trace.allsolid)
        {
            return false;
        }

        if(highestZ < trace.endpos[2])
        {
            highestZ = trace.endpos[2];
            groundEnt = trace.ent;
        }

        return true;
    };

    const bool anyFloorHit =
        processHit(vec3_zero) || quake::util::anyXYCorner(ent, processHit);

    if(!anyFloorHit)
    {
        G_FLOAT(OFS_RETURN) = 0; // FALSE
        return;
    }

    ent.v.origin[2] = highestZ - ent.v.mins[2];
    SV_LinkEdict(&ent, false);
    quake::util::addFlag(&ent, FL_ONGROUND);
    ent.v.groundentity = EDICT_TO_PROG(groundEnt);
    G_FLOAT(OFS_RETURN) = 1; // TRUE
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
static void PF_sv_lightstyle()
{
    int style;
    const char* val;
    client_t* client;
    int j;

    style = G_FLOAT(OFS_PARM0);
    val = G_STRING(OFS_PARM1);

    // bounds check to avoid clobbering sv struct
    if(style < 0 || style >= MAX_LIGHTSTYLES)
    {
        Con_DWarning("PF_lightstyle: invalid style %d\n", style);
        return;
    }

    // change the string in sv
    sv.lightstyles[style] = val;

    // send message to all clients on this server
    if(sv.state != ss_active)
    {
        return;
    }

    for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
    {
        if(client->active || client->spawned)
        {
            MSG_WriteChar(&client->message, svc_lightstyle);
            MSG_WriteChar(&client->message, style);
            MSG_WriteString(&client->message, val);
        }
    }
}

static void PF_rint()
{
    float f;
    f = G_FLOAT(OFS_PARM0);
    if(f > 0)
    {
        G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
    }
    else
    {
        G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
    }
}

static void PF_floor()
{
    G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}

static void PF_ceil()
{
    G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
static void PF_checkbottom()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = SV_CheckBottom(ent);
}

/*
=============
PF_pointcontents
=============
*/
static void PF_pointcontents()
{
    const auto v = extractVector(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = SV_PointContents(v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
static void PF_nextent()
{
    int i;
    edict_t* ent;

    i = G_EDICTNUM(OFS_PARM0);
    while(true)
    {
        i++;
        if(i == qcvm->num_edicts)
        {
            RETURN_EDICT(qcvm->edicts);
            return;
        }
        ent = EDICT_NUM(i);
        if(!ent->free)
        {
            RETURN_EDICT(ent);
            return;
        }
    }
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t sv_aim = {
    "sv_aim", "1", CVAR_NONE}; // ericw -- turn autoaim off by default. was 0.93
static void PF_aim()
{
    edict_t* ent;
    edict_t* check;
    edict_t* bestent;
    qvec3 start;
    qvec3 dir;
    qvec3 end;
    int i;
    int j;
    trace_t tr;
    qfloat dist;
    float bestdist;
    float speed;

    ent = G_EDICT(OFS_PARM0);
    speed = G_FLOAT(OFS_PARM1);
    (void)speed; /* variable set but not used */

    start = ent->v.origin;
    start[2] += 20;

    // try sending a trace straight
    dir = pr_global_struct->v_forward;
    end = start + 2048._qf * dir;
    tr = SV_MoveTrace(start, end, false, ent);
    if(tr.ent && tr.ent->v.takedamage == DAMAGE_AIM &&
        (!teamplay.value || ent->v.team <= 0 || ent->v.team != tr.ent->v.team))
    {
        returnVector(pr_global_struct->v_forward);
        return;
    }

    // try all possible entities
    qvec3 bestdir = dir;
    bestdist = sv_aim.value;
    bestent = nullptr;

    check = NEXT_EDICT(qcvm->edicts);
    for(i = 1; i < qcvm->num_edicts; i++, check = NEXT_EDICT(check))
    {
        if(check->v.takedamage != DAMAGE_AIM)
        {
            continue;
        }
        if(check == ent)
        {
            continue;
        }
        if(teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
        {
            continue; // don't aim at teammate
        }
        for(j = 0; j < 3; j++)
        {
            end[j] = check->v.origin[j] +
                     0.5 * (check->v.mins[j] + check->v.maxs[j]);
        }
        dir = end - start;
        dir = safeNormalize(dir);
        dist = DotProduct(dir, pr_global_struct->v_forward);
        if(dist < bestdist)
        {
            continue; // to far to turn
        }
        tr = SV_MoveTrace(start, end, false, ent);
        if(tr.ent == check)
        {
            // can shoot at this one
            bestdist = dist;
            bestent = check;
        }
    }

    if(bestent)
    {
        dir = bestent->v.origin - ent->v.origin;
        dist = DotProduct(dir, pr_global_struct->v_forward);
        end = pr_global_struct->v_forward * float(dist);
        end[2] = dir[2];
        end = safeNormalize(end);
        returnVector(end);
    }
    else
    {
        returnVector(bestdir);
    }
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw()
{
    edict_t* ent = PROG_TO_EDICT(pr_global_struct->self);
    float current = anglemod(ent->v.angles[1]);
    float ideal = ent->v.ideal_yaw;
    float speed = ent->v.yaw_speed;

    if(current == ideal)
    {
        return;
    }

    float move = ideal - current;

    if(ideal > current)
    {
        if(move >= 180)
        {
            move -= 360;
        }
    }
    else
    {
        if(move <= -180)
        {
            move += 360;
        }
    }

    if(move > 0)
    {
        if(move > speed)
        {
            move = speed;
        }
    }
    else
    {
        if(move < -speed)
        {
            move = -speed;
        }
    }

    ent->v.angles[1] = anglemod(current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

sizebuf_t* WriteDest()
{
    int entnum;
    int dest;
    edict_t* ent;

    dest = G_FLOAT(OFS_PARM0);
    switch(dest)
    {
        case MSG_BROADCAST: return &sv.datagram;

        case MSG_ONE:
            ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
            entnum = NUM_FOR_EDICT(ent);
            if(entnum < 1 || entnum > svs.maxclients)
            {
                PR_RunError("WriteDest: not a client");
            }
            return &svs.clients[entnum - 1].message;

        case MSG_ALL: return &sv.reliable_datagram;

        case MSG_INIT: return &sv.signon;

        case MSG_EXT_MULTICAST: return &sv.multicast;

        default: PR_RunError("WriteDest: bad destination"); break;
    }

    return nullptr;
}

static void PF_sv_WriteByte()
{
    MSG_WriteByte(WriteDest(), G_FLOAT(OFS_PARM1));
}

static void PF_sv_WriteChar()
{
    MSG_WriteChar(WriteDest(), G_FLOAT(OFS_PARM1));
}

static void PF_sv_WriteShort()
{
    MSG_WriteShort(WriteDest(), G_FLOAT(OFS_PARM1));
}

static void PF_sv_WriteLong()
{
    MSG_WriteLong(WriteDest(), G_FLOAT(OFS_PARM1));
}

static void PF_sv_WriteAngle()
{
    MSG_WriteAngle(WriteDest(), G_FLOAT(OFS_PARM1), sv.protocolflags);
}

static void PF_sv_WriteCoord()
{
    MSG_WriteCoord(WriteDest(), G_FLOAT(OFS_PARM1), sv.protocolflags);
}

static void PF_sv_WriteString()
{
    MSG_WriteString(WriteDest(), G_STRING(OFS_PARM1));
}

static void PF_sv_WriteEntity()
{
    MSG_WriteShort(WriteDest(), G_EDICTNUM(OFS_PARM1));
}

static void PF_sv_WriteVec3()
{
    MSG_WriteVec3(WriteDest(), extractVector(OFS_PARM1), sv.protocolflags);
}

//=============================================================================

static void PF_sv_makestatic()
{
    int i;
    int bits = 0; // johnfitz -- PROTOCOL_QUAKEVR

    edict_t* ent = G_EDICT(OFS_PARM0);

    // johnfitz -- don't send invisible static entities
    if(ent->alpha == ENTALPHA_ZERO)
    {
        ED_Free(ent);
        return;
    }
    // johnfitz

    // johnfitz -- PROTOCOL_QUAKEVR
    if(SV_ModelIndex(PR_GetString(ent->v.model)) & 0xFF00)
    {
        bits |= B_LARGEMODEL;
    }
    if((int)(ent->v.frame) & 0xFF00)
    {
        bits |= B_LARGEFRAME;
    }
    if(ent->alpha != ENTALPHA_DEFAULT)
    {
        bits |= B_ALPHA;
    }

    if(bits)
    {
        MSG_WriteByte(&sv.signon, svc_spawnstatic2);
        MSG_WriteByte(&sv.signon, bits);
    }
    else
    {
        MSG_WriteByte(&sv.signon, svc_spawnstatic);
    }

    if(bits & B_LARGEMODEL)
    {
        MSG_WriteShort(&sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));
    }
    else
    {
        MSG_WriteByte(&sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));
    }

    if(bits & B_LARGEFRAME)
    {
        MSG_WriteShort(&sv.signon, ent->v.frame);
    }
    else
    {
        MSG_WriteByte(&sv.signon, ent->v.frame);
    }
    // johnfitz

    MSG_WriteByte(&sv.signon, ent->v.colormap);
    MSG_WriteByte(&sv.signon, ent->v.skin);
    for(i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.signon, ent->v.origin[i], sv.protocolflags);
        MSG_WriteAngle(&sv.signon, ent->v.angles[i], sv.protocolflags);
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(bits & B_ALPHA)
    {
        MSG_WriteByte(&sv.signon, ent->alpha);
    }
    // johnfitz

    // throw the entity away now
    ED_Free(ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
static void PF_sv_setspawnparms()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    int i = NUM_FOR_EDICT(ent);
    if(i < 1 || i > svs.maxclients)
    {
        PR_RunError("Entity is not a client");
    }

    // copy spawn parms out of the client_t
    client_t* client = svs.clients + (i - 1);

    // VR: Parms like `parm8` are handled specially:
    for(i = 0; i < NUM_SPAWN_PARMS; i++)
    {
        (&pr_global_struct->parm1)[i] = client->spawn_parms[i];
    }
}

/*
==============
PF_changelevel
==============
*/
static void PF_sv_changelevel()
{
    // make sure we don't issue two changelevels
    if(svs.changelevel_issued)
    {
        return;
    }

    svs.changelevel_issued = true;

    const char* s = G_STRING(OFS_PARM0);
    Cbuf_AddText(va("changelevel %s\n", s));
}

void PF_Fixme()
{
    PR_RunError("unimplemented builtin");
}


void PR_spawnfunc_misc_model(edict_t* self)
{
    eval_t* val;
    if(!self->v.model &&
        (val = GetEdictFieldValue(self, ED_FindFieldOffset("mdl"))))
    {
        self->v.model = val->string;
    }
    if(!*PR_GetString(self->v.model))
    { // must have a model, because otherwise various
        // things will assume its not valid at all.
        self->v.model = PR_SetEngineString("*null");
    }

    if(self->v.angles[1] < 0)
    { // mimic AD. shame there's no avelocity
        // clientside.
        self->v.angles[1] = (rand() * (360.0f / RAND_MAX));
    }

    // make sure the model is precached, to avoid errors.
    G_INT(OFS_PARM0) = self->v.model;
    PF_sv_precache_model();

    // and lets just call makestatic instead of worrying if it'll interfere with
    // the rest of the qc.
    G_INT(OFS_PARM0) = EDICT_TO_PROG(self);
    PF_sv_makestatic();
}

static builtin_t pr_builtin[] = {
    PF_Fixme,
    PF_makevectors,    // void(entity e) makevectors		= #1
    PF_setorigin,      // void(entity e, vector o) setorigin	= #2
    PF_sv_setmodel,    // void(entity e, string m) setmodel	= #3
    PF_setsize,        // void(entity e, vector min, vector max) setsize	= #4
    PF_Fixme,          // void(entity e, vector min, vector max) setabssize	= #5
    PF_break,          // void() break				= #6
    PF_random,         // float() random			= #7
    PF_sound,          // void(entity e, float chan, string samp) sound	= #8
    PF_normalize,      // vector(vector v) normalize		= #9
    PF_error,          // void(string e) error			= #10
    PF_objerror,       // void(string e) objerror		= #11
    PF_vlen,           // float(vector v) vlen			= #12
    PF_vectoyaw,       // float(vector v) vectoyaw		= #13
    PF_Spawn,          // entity() spawn			= #14
    PF_Remove,         // void(entity e) remove		= #15
    PF_traceline,      // float(vector v1, vector v2, float tryents) traceline	=
                       // #16
    PF_sv_checkclient, // entity() clientlist			= #17
    PF_Find, // entity(entity start, .string fld, string match) find	= #18
    PF_sv_precache_sound, // void(string s) precache_sound	= #19
    PF_sv_precache_model, // void(string s) precache_model	= #20
    PF_stuffcmd,       // void(entity client, string s)stuffcmd	= #21
    PF_findradius,     // entity(vector org, float rad) findradius	= #22
    PF_bprint,         // void(string s) bprint		= #23
    PF_sprint,         // void(entity client, string s) sprint	= #24
    PF_dprint,         // void(string s) dprint		= #25
    PF_ftos,           // void(string s) ftos			= #26
    PF_vtos,           // void(string s) vtos			= #27
    PF_coredump, PF_traceon, PF_traceoff,
    PF_eprint,   // void(entity e) debug print an entire entity
    PF_walkmove, // float(float yaw, float dist) walkmove
    PF_Fixme,    // float(float yaw, float dist) walkmove
    PF_droptofloor, PF_sv_lightstyle, PF_rint, PF_floor, PF_ceil, PF_Fixme,
    PF_checkbottom, PF_pointcontents, PF_Fixme, PF_fabs, PF_aim, PF_cvar,
    PF_localcmd, PF_nextent, PF_particle, PF_changeyaw, PF_Fixme,
    PF_vectoangles,

    PF_sv_WriteByte, PF_sv_WriteChar, PF_sv_WriteShort, PF_sv_WriteLong,
    PF_sv_WriteCoord, PF_sv_WriteAngle, PF_sv_WriteString, PF_sv_WriteEntity,

    PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme, PF_Fixme,

    SV_MoveToGoal, PF_precache_file, PF_sv_makestatic,

    PF_sv_changelevel, PF_Fixme,

    PF_cvar_set, PF_centerprint,

    PF_sv_ambientsound,

    PF_sv_precache_model,
    PF_sv_precache_sound, // precache_sound2 is different only for qcc
    PF_precache_file,

    PF_sv_setspawnparms,
    PF_particle2,   // #79
    PF_pow,         // #80
    PF_haptic,      // #81
    PF_min,         // #82
    PF_max,         // #83
    PF_makeforward, // #84
    PF_maprange,    // #85

    PF_cvar_hmake,  // #86
    PF_cvar_hget,   // #87
    PF_cvar_hclear, // #88

    PF_redirectvector, // #89

    PF_cvar_hset, // #90

    PF_worldtext_hmake,      // #91
    PF_worldtext_hsettext,   // #92
    PF_worldtext_hsetpos,    // #93
    PF_worldtext_hsetangles, // #94

    PF_sv_WriteVec3, // #95

    PF_worldtext_hsethalign, // #96

    PF_strlen,  // #97
    PF_nthchar, // #98
    PF_substr,  // #99

    PF_calcthrowangle, // #100
    PF_rotatevec,      // #101

    PF_sin, // #102
    PF_cos, // #103

    PF_asin, // #104
    PF_acos, // #105

    PF_tan,  // #106
    PF_atan, // #107

    PF_sqrt,  // #108
    PF_atan2, // #109
};

builtin_t* pr_builtins = pr_builtin;
const int pr_numbuiltins = sizeof(pr_builtin) / sizeof(pr_builtin[0]);
