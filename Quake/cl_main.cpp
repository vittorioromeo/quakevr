/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

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
// cl_main.c  -- client main loop

#include "quakedef.hpp"
#include "host.hpp"
#include "bgmusic.hpp"
#include "server.hpp"
#include "vr.hpp"
#include "util.hpp"
#include "cmd.hpp"
#include "cdaudio.hpp"
#include "console.hpp"
#include "net.hpp"
#include "glquake.hpp"
#include "keys.hpp"
#include "quakedef_macros.hpp"
#include "msg.hpp"
#include "client.hpp"
#include "screen.hpp"
#include "zone.hpp"
#include "input.hpp"
#include "q_sound.hpp"
#include "crc.hpp"

#include <string>
#include <vector>

// we need to declare some mouse variables here, because the menu system
// references them even when on a unix system.

// these two are not intended to be set directly
cvar_t cl_name = {"_cl_name", "player", CVAR_ARCHIVE};
cvar_t cl_color = {"_cl_color", "0", CVAR_ARCHIVE};

cvar_t cl_shownet = {"cl_shownet", "0", CVAR_NONE}; // can be 0, 1, or 2
cvar_t cl_nolerp = {"cl_nolerp", "0", CVAR_NONE};

cvar_t cfg_unbindall = {"cfg_unbindall", "1", CVAR_ARCHIVE};

cvar_t lookspring = {"lookspring", "0", CVAR_ARCHIVE};
cvar_t lookstrafe = {"lookstrafe", "0", CVAR_ARCHIVE};
cvar_t sensitivity = {"sensitivity", "3", CVAR_ARCHIVE};

cvar_t m_pitch = {"m_pitch", "0.022", CVAR_ARCHIVE};
cvar_t m_yaw = {"m_yaw", "0.022", CVAR_ARCHIVE};
cvar_t m_forward = {"m_forward", "1", CVAR_ARCHIVE};
cvar_t m_side = {"m_side", "0.8", CVAR_ARCHIVE};

cvar_t cl_maxpitch = {
    "cl_maxpitch", "90", CVAR_ARCHIVE}; // johnfitz -- variable pitch clamping
cvar_t cl_minpitch = {
    "cl_minpitch", "-90", CVAR_ARCHIVE}; // johnfitz -- variable pitch clamping

cvar_t cl_recordingdemo = {"cl_recordingdemo", "",
    CVAR_ROM}; // the name of the currently-recording demo.

client_static_t cls;
client_state_t cl;
// FIXME: put these on hunk?
lightstyle_t cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t cl_dlights[MAX_DLIGHTS];

int cl_numvisedicts;
int cl_maxvisedicts;
entity_t** cl_visedicts;

extern cvar_t r_lerpmodels, r_lerpmove; // johnfitz
extern float host_netinterval;          // Spike

void CL_ClearTrailStates()
{
    int i;
    for(i = 0; i < cl.num_statics; i++)
    {
        PScript_DelinkTrailstate(&(cl.static_entities[i]->trailstate));
        PScript_DelinkTrailstate(&(cl.static_entities[i]->emitstate));
    }
    for(i = 0; i < cl.max_edicts; i++)
    {
        PScript_DelinkTrailstate(&(cl.entities[i].trailstate));
        PScript_DelinkTrailstate(&(cl.entities[i].emitstate));
    }
    for(i = 0; i < MAX_BEAMS; i++)
    {
        PScript_DelinkTrailstate(&(cl_beams[i].trailstate));
    }
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState()
{
    if(!sv.active)
    {
        Host_ClearMemory();
    }

    CL_ClearTrailStates();

    PR_ClearProgs(&cl.qcvm);

    // wipe the entire cl structure
    cl = client_state_t{};

    SZ_Clear(&cls.message);

    // clear other arrays
    memset(cl_dlights, 0, sizeof(cl_dlights));
    memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
    memset(cl_temp_entities, 0, sizeof(cl_temp_entities));
    memset(cl_beams, 0, sizeof(cl_beams));

    // johnfitz -- cl.entities is now dynamically allocated
    cl.max_edicts = CLAMP(MIN_EDICTS, (int)max_edicts.value, MAX_EDICTS);
    cl.entities = (entity_t*)Hunk_AllocName(
        cl.max_edicts * sizeof(entity_t), "cl.entities");
    // johnfitz

    // Spike -- this stuff needs to get reset to defaults.
    cl.csqc_sensitivity = 1;

    cl.worldTexts.clear();

    forAllViewmodels(cl, [](entity_t& e) { e.netstate = nullentitystate; });

#ifdef PSET_SCRIPT
    PScript_Shutdown();
#endif
}

/*
=====================
CL_Disconnect

Sends a disconnect message to the server
This is also called on Host_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect()
{
    if(key_dest == key_message)
    {
        Key_EndChat(); // don't get stuck in chat mode
    }

    // stop sounds (especially looping!)
    S_StopAllSounds(true);
    BGM_Stop();
    CDAudio_Stop();

    // if running a local server, shut it down
    if(cls.demoplayback)
    {
        CL_StopPlayback();
    }
    else if(cls.state == ca_connected)
    {
        if(cls.demorecording)
        {
            CL_Stop_f();
        }

        Con_DPrintf("Sending clc_disconnect\n");
        SZ_Clear(&cls.message);
        MSG_WriteByte(&cls.message, clc_disconnect);
        NET_SendUnreliableMessage(cls.netcon, &cls.message);
        SZ_Clear(&cls.message);
        NET_Close(cls.netcon);
        cls.netcon = nullptr; // QSS

        cls.state = ca_disconnected;
        if(sv.active)
        {
            Host_ShutdownServer(false);
        }
    }

    cls.demoplayback = cls.timedemo = false;
    cls.demopaused = false;
    cls.signon = 0;

    // QSS
    cls.netcon = nullptr;
    if(cls.download.file)
    {
        fclose(cls.download.file);
    }
    memset(&cls.download, 0, sizeof(cls.download));
    cl.intermission = 0;
    cl.worldmodel = nullptr;
    cl.sendprespawn = false;
}

void CL_Disconnect_f()
{
    CL_Disconnect();
    if(sv.active)
    {
        Host_ShutdownServer(false);
    }
}


/*
=====================
CL_EstablishConnection

Host should be either "local" or a net address to be passed on
=====================
*/
void CL_EstablishConnection(const char* host)
{
    static char lasthost[NET_NAMELEN];

    if(cls.state == ca_dedicated)
    {
        return;
    }

    if(cls.demoplayback)
    {
        return;
    }

    if(!host)
    {
        host = lasthost;
        if(!*host)
        {
            return;
        }
    }
    else
    {
        q_strlcpy(lasthost, host, sizeof(lasthost));
    }

    CL_Disconnect();

    cls.netcon = NET_Connect(host);
    if(!cls.netcon)
    {
        Host_Error("CL_Connect: connect failed\n");
    }
    Con_DPrintf("CL_EstablishConnection: connected to %s\n", host);

    cls.demonum = -1; // not in the demo loop now
    cls.state = ca_connected;
    cls.signon = 0; // need all the signon messages before playing
    MSG_WriteByte(&cls.message, clc_nop); // NAT Fix from ProQuake
}

/*
=====================
CL_SignonReply

An svc_signonnum has been received, perform a client side setup
=====================
*/
void CL_SignonReply()
{
    char str[8192];

    Con_DPrintf("CL_SignonReply: %i\n", cls.signon);

    switch(cls.signon)
    {
        case 1:
            MSG_WriteByte(&cls.message, clc_stringcmd);
            MSG_WriteString(&cls.message, va("name \"%s\"\n", cl_name.string));

            cl.sendprespawn = true;
            break;

        case 2:
            MSG_WriteByte(&cls.message, clc_stringcmd);
            MSG_WriteString(
                &cls.message, va("color %i %i\n", ((int)cl_color.value) >> 4,
                                  ((int)cl_color.value) & 15));

            MSG_WriteByte(&cls.message, clc_stringcmd);
            sprintf(str, "spawn %s", cls.spawnparms);
            MSG_WriteString(&cls.message, str);
            break;

        case 3:
            MSG_WriteByte(&cls.message, clc_stringcmd);
            MSG_WriteString(&cls.message, "begin");
            Cache_Report(); // print remaining memory
            break;

        case 4:
            SCR_EndLoadingPlaque(); // allow normal screen updates
            break;
    }
}

/*
=====================
CL_NextDemo

Called to play the next demo in the demo loop
=====================
*/
void CL_NextDemo()
{
    char str[1024];

    if(cls.demonum == -1)
    {
        return; // don't play demos
    }

    if(!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS)
    {
        cls.demonum = 0;
        if(!cls.demos[cls.demonum][0])
        {
            Con_Printf("No demos listed with startdemos\n");
            cls.demonum = -1;
            CL_Disconnect();
            return;
        }
    }

    SCR_BeginLoadingPlaque();

    sprintf(str, "playdemo %s\n", cls.demos[cls.demonum]);
    Cbuf_InsertText(str);
    cls.demonum++;
}

/*
==============
CL_PrintEntities_f
==============
*/
void CL_PrintEntities_f()
{
    entity_t* ent;
    int i;

    if(cls.state != ca_connected)
    {
        return;
    }

    for(i = 0, ent = cl.entities; i < cl.num_entities; i++, ent++)
    {
        Con_Printf("%3i:", i);
        if(!ent->model)
        {
            Con_Printf("EMPTY\n");
            continue;
        }
        Con_Printf("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
            ent->model->name, ent->frame, ent->origin[0], ent->origin[1],
            ent->origin[2], ent->angles[0], ent->angles[1], ent->angles[2]);
    }
}

/*
===============
CL_AllocDlight

===============
*/
dlight_t* CL_AllocDlight(int key)
{
    int i;
    dlight_t* dl;

    // first look for an exact key match
    if(key)
    {
        dl = cl_dlights;
        for(i = 0; i < MAX_DLIGHTS; i++, dl++)
        {
            if(dl->key == key)
            {
                memset(dl, 0, sizeof(*dl));
                dl->key = key;
                dl->color[0] = dl->color[1] = dl->color[2] =
                    1; // johnfitz -- lit support via lordhavoc
                return dl;
            }
        }
    }

    // then look for anything else
    dl = cl_dlights;
    for(i = 0; i < MAX_DLIGHTS; i++, dl++)
    {
        if(dl->die < cl.time)
        {
            memset(dl, 0, sizeof(*dl));
            dl->key = key;
            dl->color[0] = dl->color[1] = dl->color[2] =
                1; // johnfitz -- lit support via lordhavoc
            return dl;
        }
    }

    dl = &cl_dlights[0];
    memset(dl, 0, sizeof(*dl));
    dl->key = key;
    dl->color[0] = dl->color[1] = dl->color[2] =
        1; // johnfitz -- lit support via lordhavoc
    return dl;
}


/*
===============
CL_DecayLights

===============
*/
void CL_DecayLights()
{
    int i;
    dlight_t* dl;
    float time;

    time = cl.time - cl.oldtime;

    dl = cl_dlights;
    for(i = 0; i < MAX_DLIGHTS; i++, dl++)
    {
        if(dl->die < cl.time || !dl->radius)
        {
            continue;
        }

        dl->radius -= time * dl->decay;
        if(dl->radius < 0)
        {
            dl->radius = 0;
        }
    }
}


/*
===============
CL_LerpPoint

Determines the fraction between the last two messages that the objects
should be put at.
===============
*/
float CL_LerpPoint()
{
    float f;
    float frac;

    f = cl.mtime[0] - cl.mtime[1];

    if(!f || cls.timedemo || (sv.active && !host_netinterval))
    {
        cl.time = cl.mtime[0];
        return 1;
    }

    if(f > 0.1) // dropped packet, or start of demo
    {
        cl.mtime[1] = cl.mtime[0] - 0.1;
        f = 0.1;
    }

    frac = (cl.time - cl.mtime[1]) / f;

    if(frac < 0)
    {
        if(frac < -0.01)
        {
            cl.time = cl.mtime[1];
        }
        frac = 0;
    }
    else if(frac > 1)
    {
        if(frac > 1.01)
        {
            cl.time = cl.mtime[0];
        }
        frac = 1;
    }

    // johnfitz -- better nolerp behavior
    if(cl_nolerp.value)
    {
        return 1;
    }
    // johnfitz

    return frac;
}

static bool CL_LerpEntity(entity_t* ent, qvec3& org, qvec3& ang, float frac)
{
    float f, d;
    int j;
    qvec3 delta;
    bool teleported = false;
    // figure out the pos+angles of the parent
    if(ent->forcelink)
    { // the entity was not updated in the last message
        // so move to the final spot
        org = ent->msg_origins[0];
        ang = ent->msg_angles[0];
    }
    else
    { // if the delta is large, assume a teleport and don't lerp
        f = frac;
        for(j = 0; j < 3; j++)
        {
            delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
            if(delta[j] > 100 || delta[j] < -100)
            {
                f = 1;             // assume a teleportation, not a motion
                teleported = true; // johnfitz -- don't lerp teleports
            }
        }

        // johnfitz -- don't cl_lerp entities that will be r_lerped
        if(r_lerpmove.value && (ent->lerpflags & LERP_MOVESTEP))
        {
            f = 1;
        }
        // johnfitz

        // interpolate the origin and angles
        for(j = 0; j < 3; j++)
        {
            org[j] = ent->msg_origins[1][j] + f * delta[j];

            d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
            if(d > 180)
            {
                d -= 360;
            }
            else if(d < -180)
            {
                d += 360;
            }
            ang[j] = ent->msg_angles[1][j] + f * d;
        }
    }
    return teleported;
}

static bool CL_AttachEntity(entity_t* ent, float frac)
{
    entity_t* parent;
    qvec3 porg, pang;
    qvec3 paxis[3];
    qvec3 tmp, fwd, up;
    unsigned int tagent = ent->netstate.tagentity;
    int runaway = 0;

    while(1)
    {
        if(!tagent)
        {
            return true; // nothing to do.
        }
        if(runaway++ == 10 || tagent >= (unsigned int)cl.num_entities)
        {
            return false; // parent isn't valid
        }
        parent = &cl.entities[tagent];

        if(tagent == cl.viewentity)
        {
            ent->eflags |= EFLAGS_EXTERIORMODEL;
        }

        if(!parent->model)
        {
            return false;
        }
        if(0) // tagent < ent-cl_entities)
        {
            tagent = parent->netstate.tagentity;
            porg = parent->origin;
            pang = parent->angles;
        }
        else
        {
            tagent = parent->netstate.tagentity;
            CL_LerpEntity(parent, porg, pang, frac);
        }

        // FIXME: this code needs to know the exact lerp info of the underlaying
        // model. however for some idiotic reason, someone decided to figure out
        // what should be displayed somewhere far removed from the code that
        // deals with timing so we have absolutely no way to get a reliable
        // origin in the meantime, r_lerpmove 0; r_lerpmodels 0 you might be
        // able to work around it by setting the attached entity to
        // movetype_step to match the attachee, and to avoid EF_MUZZLEFLASH.
        // personally I'm just going to call it a quakespasm bug that I cba to
        // fix.

        // FIXME: update porg+pang according to the tag index (we don't support
        // md3s/iqms, so we don't need to do anything here yet)

        if(parent->model && parent->model->type == mod_alias)
        {
            pang[0] *= -1;
        }

        std::tie(paxis[0], paxis[1], paxis[2]) =
            quake::util::getAngledVectors(pang);

        if(ent->model && ent->model->type == mod_alias)
        {
            ent->angles[0] *= -1;
        }

        std::tie(fwd, tmp, up) = quake::util::getAngledVectors(ent->angles);

        // transform the origin
        tmp = parent->origin + ent->origin[0] * paxis[0];
        tmp = tmp + -ent->origin[1] * paxis[1];
        ent->origin = tmp + ent->origin[2] * paxis[2];

        // transform the forward vector
        tmp = vec3_zero + fwd[0] * paxis[0];
        tmp = tmp + -fwd[1] * paxis[1];
        fwd = tmp + fwd[2] * paxis[2];

        // transform the up vector
        tmp = vec3_zero + up[0] * paxis[0];
        tmp = tmp + -up[1] * paxis[1];
        up = tmp + up[2] * paxis[2];

        // regenerate the new angles.
        // VectorAngles(fwd, up, ent->angles);
        ent->angles = VectorAngles(fwd);
        if(ent->model && ent->model->type == mod_alias)
        {
            ent->angles[0] *= -1;
        }

        ent->eflags |=
            parent->netstate.eflags & (EFLAGS_VIEWMODEL | EFLAGS_EXTERIORMODEL);
    }
}

/*
===============
CL_RelinkEntities
===============
*/
void CL_RelinkEntities()
{
    // determine partial update time
    const float frac = CL_LerpPoint();

    if(cl_numvisedicts + 64 > cl_maxvisedicts)
    {
        cl_maxvisedicts = cl_maxvisedicts + 64;
        cl_visedicts = (entity_t**)realloc(
            cl_visedicts, sizeof(*cl_visedicts) * cl_maxvisedicts);
    }
    cl_numvisedicts = 0;

    //
    // interpolate player info
    //
    for(int i = 0; i < 3; i++)
    {
        cl.velocity[i] = cl.mvelocity[1][i] +
                         frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);
    }

    float d;
    if(cls.demoplayback)
    {
        // interpolate the angles
        for(int j = 0; j < 3; j++)
        {
            d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
            if(d > 180)
            {
                d -= 360;
            }
            else if(d < -180)
            {
                d += 360;
            }
            cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
        }
    }

    const float bobjrotate = anglemod(100 * cl.time);

    // start on the entity after the world
    int i;
    entity_t* ent;
    for(i = 1, ent = cl.entities + 1; i < cl.num_entities; i++, ent++)
    {
        if(!ent->model)
        {
            // empty slot

            // ericw -- efrags are only used for static entities in GLQuake
            // ent can't be static, so this is a no-op.
            // if (ent->forcelink)
            //	R_RemoveEfrags (ent);	// just became empty
            continue;
        }
        ent->eflags = ent->netstate.eflags;

        // if the object wasn't included in the last packet, remove it
        if(ent->msgtime != cl.mtime[0])
        {
            ent->model = nullptr;
            ent->lerpflags |=
                LERP_RESETMOVE |
                LERP_RESETANIM; // johnfitz -- next time this entity slot is
                                // reused, the lerp will need to be reset
            continue;
        }

        const auto oldorg = ent->origin;

        if(ent->forcelink)
        {
            // the entity was not updated in the last message
            // so move to the final spot
            ent->origin = ent->msg_origins[0];
            ent->angles = ent->msg_angles[0];
            ent->model_scale = ent->msg_scales[0];
        }
        else
        {
            // if the delta is large, assume a teleport and don't lerp
            float f = frac;
            qvec3 delta;

            for(int j = 0; j < 3; j++)
            {
                delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
                if(delta[j] > 100 || delta[j] < -100)
                {
                    f = 1; // assume a teleportation, not a motion
                    ent->lerpflags |=
                        LERP_RESETMOVE; // johnfitz -- don't lerp teleports
                    if(ent == &cl.entities[cl.viewentity])
                    {
                        VR_PushYaw();
                    }
                }
            }

            // johnfitz -- don't cl_lerp entities that will be r_lerped
            if(r_lerpmove.value && (ent->lerpflags & LERP_MOVESTEP))
            {
                f = 1;
            }
            // johnfitz

            // interpolate the origin and angles and scales
            for(int j = 0; j < 3; j++)
            {
                ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

                d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
                if(d > 180)
                {
                    d -= 360;
                }
                else if(d < -180)
                {
                    d += 360;
                }
                ent->angles[j] = ent->msg_angles[1][j] + f * d;

                const float sd = ent->msg_scales[0][j] - ent->msg_scales[1][j];
                ent->model_scale[j] = ent->msg_scales[1][j] + f * sd;
            }
        }

        // rotate binary objects locally
        if(ent->model->flags & EF_ROTATE)
        {
            ent->angles[1] = bobjrotate;
        }

        if(ent->effects & EF_BRIGHTFIELD)
        {
            R_EntityParticles(ent);
        }

        dlight_t* dl;
        if(ent->effects & EF_MUZZLEFLASH)
        {
            dl = CL_AllocDlight(i);
            dl->origin = ent->origin;
            dl->origin[2] += 16;

            const auto fv = quake::util::getFwdVecFromPitchYawRoll(ent->angles);

            dl->origin += 18._qf * fv;
            dl->radius = 200 + (rand() & 31);
            dl->minlight = 32;
            dl->die = cl.time + 0.1;

            // johnfitz -- assume muzzle flash accompanied by muzzle flare,
            // which looks bad when lerped
            if(r_lerpmodels.value != 2)
            {
                if(ent == &cl.entities[cl.viewentity])
                {
                    cl.viewent.lerpflags |=
                        LERP_RESETANIM |
                        LERP_RESETANIM2; // no lerping for two frames

                    cl.offhand_viewent.lerpflags |=
                        LERP_RESETANIM |
                        LERP_RESETANIM2; // no lerping for two frames
                }
                else
                {
                    ent->lerpflags |=
                        LERP_RESETANIM |
                        LERP_RESETANIM2; // no lerping for two frames
                }
            }
            // johnfitz
        }
        if(ent->effects & EF_BRIGHTLIGHT)
        {
            dl = CL_AllocDlight(i);
            dl->origin = ent->origin;
            dl->origin[2] += 16;
            dl->radius = 400 + (rand() & 31);
            dl->die = cl.time + 0.001;
        }

        if(ent->effects & EF_DIMLIGHT)
        {
            dl = CL_AllocDlight(i);
            dl->origin = ent->origin;
            dl->radius = 200 + (rand() & 31);
            dl->die = cl.time + 0.001;
        }

        if(ent->effects & EF_VERYDIMLIGHT)
        {
            dl = CL_AllocDlight(i);
            dl->origin = ent->origin;
            dl->radius = 50 + (rand() & 31);
            dl->die = cl.time + 0.001;
        }

        if(ent->model->flags & EF_GIB)
        {
            R_RocketTrail(oldorg, ent->origin, 2 /* blood */);
        }
        else if(ent->model->flags & EF_ZOMGIB)
        {
            R_RocketTrail(oldorg, ent->origin, 4 /* slight blood */);
        }
        else if(ent->model->flags & EF_TRACER)
        {
            R_RocketTrail(oldorg, ent->origin, 3 /* tracer */);
        }
        else if(ent->model->flags & EF_TRACER2)
        {
            R_RocketTrail(oldorg, ent->origin, 5 /* tracer */);
        }
        else if(ent->model->flags & EF_ROCKET)
        {
            if(ent->effects & EF_MINIROCKET)
            {
                R_RocketTrail(oldorg, ent->origin, 7 /* mini rocket trail */);
                dl = CL_AllocDlight(i);
                dl->origin = ent->origin;
                dl->radius = 70;
                dl->die = cl.time + 0.01;
            }
            else
            {
                R_RocketTrail(oldorg, ent->origin, 0 /* rocket trail */);
                dl = CL_AllocDlight(i);
                dl->origin = ent->origin;
                dl->radius = 200;
                dl->die = cl.time + 0.01;
            }
        }
        else if(ent->model->flags & EF_GRENADE)
        {
            R_RocketTrail(oldorg, ent->origin, 1 /* smoke */);
        }
        else if(ent->model->flags & EF_TRACER3)
        {
            R_RocketTrail(oldorg, ent->origin, 6 /* voor trail */);
        }

        if(ent->effects & EF_LAVATRAIL)
        {
            R_RunParticleEffect_LavaSpike(ent->origin, vec3_zero, 4);
        }

        ent->forcelink = false;

#ifdef PSET_SCRIPT
        if(ent->netstate.emiteffectnum > 0)
        {
            vec3_t axis[3];
            AngleVectors(ent->angles, axis[0], axis[1], axis[2]);
            if(ent->model->type == mod_alias)
            {
                axis[0][2] *= -1; // stupid vanilla bug
            }
            PScript_RunParticleEffectState(ent->origin, axis[0], frametime,
                cl.particle_precache[ent->netstate.emiteffectnum].index,
                &ent->emitstate);
        }
        else if(ent->model->emiteffect >= 0)
        {
            vec3_t axis[3];
            AngleVectors(ent->angles, axis[0], axis[1], axis[2]);
            if(ent->model->flags & MOD_EMITFORWARDS)
            {
                if(ent->model->type == mod_alias)
                {
                    axis[0][2] *= -1; // stupid vanilla bug
                }
            }
            else
            {
                VectorScale(axis[2], -1, axis[0]);
            }
            PScript_RunParticleEffectState(ent->origin, axis[0], frametime,
                ent->model->emiteffect, &ent->emitstate);
            if(ent->model->flags & MOD_EMITREPLACE)
            {
                continue;
            }
        }
#endif

        // This hides the player model in first person view.
        if(i == cl.viewentity && !chase_active.value)
        {
            continue;
        }

        if(cl_numvisedicts < cl_maxvisedicts)
        {
            cl_visedicts[cl_numvisedicts] = ent;
            cl_numvisedicts++;
        }
    }
}

#ifdef PSET_SCRIPT
int CL_GenerateRandomParticlePrecache(const char* pname)
{ // for dpp7 compat
    size_t i;
    pname = va("%s", pname);
    for(i = 1; i < MAX_PARTICLETYPES; i++)
    {
        if(!cl.particle_precache[i].name)
        {
            cl.particle_precache[i].name =
                strcpy(Hunk_Alloc(strlen(pname) + 1), pname);
            cl.particle_precache[i].index =
                PScript_FindParticleType(cl.particle_precache[i].name);
            return i;
        }
        if(!strcmp(cl.particle_precache[i].name, pname))
        {
            return i;
        }
    }
    return 0;
}
#endif

// sent by the server to let us know that dp downloads can be used
void CL_ServerExtension_Download_f()
{
    if(Cmd_Argc() == 2)
    {
        cl.protocol_dpdownload = atoi(Cmd_Argv(1));
    }
}

// sent by the server to let us know when its finished sending the entire file
void CL_Download_Finished_f()
{
    if(cls.download.file)
    {
        char finalpath[MAX_OSPATH];
        unsigned int size = strtoul(Cmd_Argv(1), nullptr, 0);
        unsigned int hash = strtoul(Cmd_Argv(2), nullptr, 0);
        // const char *fname = Cmd_Argv(3);
        bool hashokay = false;
        if(size == cls.download.size)
        {
            byte* tmp = (byte*)malloc(size);
            if(tmp)
            {
                fseek(cls.download.file, 0, SEEK_SET);
                fread(tmp, 1, size, cls.download.file);
                hashokay = (hash == CRC_Block(tmp, size));
                free(tmp);

                if(!hashokay)
                {
                    Con_Warning("Download hash failure\n");
                }
            }
            else
            {
                Con_Warning("Download size too large\n");
            }
        }
        else
        {
            Con_Warning("Download size mismatch\n");
        }

        fclose(cls.download.file);
        cls.download.file = nullptr;
        if(hashokay)
        {
            q_snprintf(finalpath, sizeof(finalpath), "%s/%s", com_gamedir,
                cls.download.current);
            rename(cls.download.temp, finalpath);
            Con_SafePrintf("Downloaded %s: %u bytes\n", cls.download.current,
                cls.download.size);
        }
        else
        {
            Con_Warning("Download of %s failed\n", cls.download.current);
            unlink(cls.download.temp); // kill the temp
        }
    }

    cls.download.active = false;
}
// sent by the server (or issued by the user) to stop the current download for
// any reason.
void CL_StopDownload_f()
{
    if(cls.download.file)
    {
        fclose(cls.download.file);
        cls.download.file = nullptr;
        unlink(cls.download.temp);

        //		Con_SafePrintf("Download cancelled\n", cl.download_current,
        // cl.download_size);
    }
    cls.download.active = false;
}
// sent by the server to let us know that its going to start spamming us now.
void CL_Download_Begin_f()
{
    if(!cls.download.active)
    {
        return;
    }

    if(cls.download.file)
    {
        CL_StopDownload_f();
    }

    // cl_downloadbegin size "name"
    cls.download.size = strtoul(Cmd_Argv(1), nullptr, 0);

    COM_CreatePath(cls.download.temp);
    cls.download.file = fopen(cls.download.temp,
        "wb+"); //+ so we can read the data back to validate it

    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, "sv_startdownload\n");
}

void CL_Download_Data()
{
    byte* data;
    unsigned int start, size;
    start = MSG_ReadLong();
    size = (unsigned short)MSG_ReadShort();
    data = MSG_ReadData(size);
    if(msg_badread)
    {
        return;
    }
    if(!cls.download.file)
    {
        return; // demo started mid-record? something weird anyway
    }

    fseek(cls.download.file, start, SEEK_SET);
    fwrite(data, 1, size, cls.download.file);

    Con_SafePrintf("Downloading %s: %g%%\r", cls.download.current,
        100 * (start + size) / (double)cls.download.size);

    // should maybe use unreliables, but whatever, shouldn't matter too much,
    // it'll still complete
    MSG_WriteByte(&cls.message, clcdp_ackdownloaddata);
    MSG_WriteLong(&cls.message, start);
    MSG_WriteShort(&cls.message, size);
}

// returns true if we should block waiting for a download, false if there's no
// point.
bool CL_CheckDownload(const char* filename)
{
    if(sv.active)
    {
        return false; // no point downloading if we're the server...
    }
    if(*filename == '*')
    {
        return false; // don't download these...
    }
    if(cls.download.active)
    {
        return true; // block while we're already downloading something
    }
    if(!cl.protocol_dpdownload)
    {
        return false; // can't download anyway
    }
    if(*cls.download.current && !strcmp(cls.download.current, filename))
    {
        return false; // if the previous download failed, don't endlessly retry.
    }
    if(COM_FileExists(filename, nullptr))
    {
        return false; // no need to download anything.
    }
    if(!COM_DownloadNameOkay(filename))
    {
        return false; // diediedie
    }

    cls.download.active = true;
    q_strlcpy(cls.download.current, filename, sizeof(cls.download.current));
    q_snprintf(cls.download.temp, sizeof(cls.download.temp), "%s/%s.tmp",
        com_gamedir, filename);
    Con_Printf("Downloading %s...\r", filename);
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, va("download \"%s\"\n", filename));
    return true;
}

// download+load models and sounds as needed, once complete let the server know
// we're ready for the next stage. returning false will trigger nops.
bool CL_CheckDownloads()
{
    int i;
    if(cl.model_download == 0 && cl.model_count && cl.model_name[1])
    { // haxors, download the lit first, but only if we don't already have the
      // bsp
        // this ensures that we don't keep requesting the lit for maps that just
        // don't have one (although may be problematic if the first server we
        // find deleted them all, but oh well)
        char litname[MAX_QPATH];
        char* ext;
        q_strlcpy(litname, cl.model_name[1], sizeof(litname));
        ext = (char*)COM_FileGetExtension(litname);
        if(!q_strcasecmp(ext, "bsp"))
        {
            if(!COM_FileExists(litname, nullptr))
            {
                strcpy(ext, "lit");
                if(CL_CheckDownload(litname))
                {
                    return false;
                }
            }
        }
        cl.model_download++;
    }
    for(; cl.model_download < cl.model_count;)
    {
        if(*cl.model_name[cl.model_download])
        {
            if(CL_CheckDownload(cl.model_name[cl.model_download]))
            {
                return false;
            }
            cl.model_precache[cl.model_download] =
                Mod_ForName(cl.model_name[cl.model_download], false);
            if(cl.model_precache[cl.model_download] == nullptr)
            {
                Host_Error(
                    "Model %s not found", cl.model_name[cl.model_download]);
            }
        }
        cl.model_download++;
    }

    for(; cl.sound_download < cl.sound_count;)
    {
        if(*cl.sound_name[cl.sound_download])
        {
            if(CL_CheckDownload(
                   va("sound/%s", cl.sound_name[cl.sound_download])))
            {
                return false;
            }
            cl.sound_precache[cl.sound_download] =
                S_PrecacheSound(cl.sound_name[cl.sound_download]);
        }
        cl.sound_download++;
    }

    if(!cl.worldmodel && cl.model_count >= 2)
    {
        // local state
        cl.entities[0].model = cl.worldmodel = cl.model_precache[1];
        if(cl.worldmodel->type != mod_brush)
        {
            if(cl.worldmodel->type == mod_ext_invalid)
            {
                Host_Error("Worldmodel %s was not loaded", cl.model_name[1]);
            }
            else
            {
                Host_Error(
                    "Worldmodel %s is not a brushmodel", cl.model_name[1]);
            }
        }

        // fixme: deal with skybox somehow

        R_NewMap();

#ifdef PSET_SCRIPT
        // the protocol changing depending upon files found on the client's
        // computer is of course a really shit way to design things especially
        // when users have a nasty habit of changing config files.
        if(cl.protocol == PROTOCOL_VERSION_DP7)
        {
            PScript_FindParticleType(
                "effectinfo."); // make sure this is implicitly loaded.
            COM_Effectinfo_Enumerate(CL_GenerateRandomParticlePrecache);
            cl.protocol_particles = true;
        }
        else if(cl.protocol_pext2)
        {
            cl.protocol_particles =
                true; // doesn't have a pext flag of its own, but at least we
        }
        // know what it is.
#endif
    }

    // make sure ents have the correct models, now that they're actually loaded.
    for(i = 0; i < cl.num_statics; i++)
    {
        if(cl.static_entities[i]->model)
        {
            continue;
        }
        cl.static_entities[i]->model =
            cl.model_precache[cl.static_entities[i]->netstate.modelindex];
        R_AddEfrags(cl.static_entities[i]);
    }
    return true;
}


/*
===============
CL_ReadFromServer

Read all incoming data from the server
===============
*/
int CL_ReadFromServer()
{
    int ret;
    extern int num_temp_entities; // johnfitz
    int num_beams = 0;            // johnfitz
    int num_dlights = 0;          // johnfitz
    beam_t* b;                    // johnfitz
    dlight_t* l;                  // johnfitz
    int i;                        // johnfitz


    cl.oldtime = cl.time;
    cl.time += host_frametime;

    do
    {
        ret = CL_GetMessage();
        if(ret == -1)
        {
            Host_Error("CL_ReadFromServer: lost server connection");
        }
        if(!ret)
        {
            break;
        }

        cl.last_received_message = realtime;
        CL_ParseServerMessage();
    } while(ret && cls.state == ca_connected);

    if(cl_shownet.value)
    {
        Con_Printf("\n");
    }

    CL_RelinkEntities();
    CL_UpdateTEnts();

    // johnfitz -- devstats

    // visedicts
    if(cl_numvisedicts > 256 && dev_peakstats.visedicts <= 256)
    {
        Con_DWarning(
            "%i visedicts exceeds standard limit of 256.\n", cl_numvisedicts);
    }
    dev_stats.visedicts = cl_numvisedicts;
    dev_peakstats.visedicts = q_max(cl_numvisedicts, dev_peakstats.visedicts);

    // temp entities
    if(num_temp_entities > 64 && dev_peakstats.tempents <= 64)
    {
        Con_DWarning(
            "%i tempentities exceeds standard limit of 64 (max = %d).\n",
            num_temp_entities, MAX_TEMP_ENTITIES);
    }
    dev_stats.tempents = num_temp_entities;
    dev_peakstats.tempents = q_max(num_temp_entities, dev_peakstats.tempents);

    // beams
    for(i = 0, b = cl_beams; i < MAX_BEAMS; i++, b++)
    {
        if(b->model && b->endtime >= cl.time)
        {
            num_beams++;
        }
    }

    if(num_beams > 24 && dev_peakstats.beams <= 24)
    {
        Con_DWarning("%i beams exceeded standard limit of 24 (max = %d).\n",
            num_beams, MAX_BEAMS);
    }

    dev_stats.beams = num_beams;
    dev_peakstats.beams = q_max(num_beams, dev_peakstats.beams);

    // dlights
    for(i = 0, l = cl_dlights; i < MAX_DLIGHTS; i++, l++)
    {
        if(l->die >= cl.time && l->radius)
        {
            num_dlights++;
        }
    }
    if(num_dlights > 32 && dev_peakstats.dlights <= 32)
    {
        Con_DWarning("%i dlights exceeded standard limit of 32 (max = %d).\n",
            num_dlights, MAX_DLIGHTS);
    }
    dev_stats.dlights = num_dlights;
    dev_peakstats.dlights = q_max(num_dlights, dev_peakstats.dlights);

    // johnfitz

    //
    // bring the links up to date
    //
    return 0;
}

/*
=================
CL_UpdateViewAngles

Spike: split from CL_SendCmd, to do clientside viewangle changes separately from
outgoing packets.
=================
*/
void CL_AccumulateCmd()
{
    if(cls.signon == SIGNONS)
    {
        // basic keyboard looking
        CL_AdjustAngles();

        // accumulate movement from other devices
        IN_Move(&cl.pendingcmd);
    }
}

/*
=================
CL_SendCmd
=================
*/
void CL_SendCmd()
{
    usercmd_t cmd;

    if(cls.state != ca_connected)
    {
        return;
    }

    if(cls.signon == SIGNONS)
    {
        // get basic movement from keyboard
        CL_BaseMove(&cmd);

        // allow mice or other external controllers to add to the move
        IN_Move(&cmd);
        VR_Move(&cmd);

        // send the unreliable message
        CL_SendMove(&cmd);
    }
    else
    {
        CL_SendMove(nullptr);
    }
    memset(&cl.pendingcmd, 0, sizeof(cl.pendingcmd));

    if(cls.demoplayback)
    {
        SZ_Clear(&cls.message);
        return;
    }

    // send the reliable message
    if(!cls.message.cursize)
    {
        return; // no message at all
    }

    if(!NET_CanSendMessage(cls.netcon))
    {
        Con_DPrintf("CL_SendCmd: can't send\n");
        return;
    }

    if(NET_SendMessage(cls.netcon, &cls.message) == -1)
    {
        Host_Error("CL_SendCmd: lost server connection");
    }

    SZ_Clear(&cls.message);
}

/*
=============
CL_Tracepos_f -- johnfitz

display impact point of trace along VPN
=============
*/
void CL_Tracepos_f(refdef_t& refdef)
{
    if(cls.state != ca_connected)
    {
        return;
    }

    const auto v = refdef.vieworg + 8192._qf * vpn;
    const auto trace = TraceLine(refdef.vieworg, v);

    if(!quake::util::hitSomething(trace))
    {
        Con_Printf("Tracepos: trace didn't hit anything\n");
    }
    else
    {
        const auto& w = trace.endpos;
        Con_Printf("Tracepos: (%i %i %i)\n", (int)w[0], (int)w[1], (int)w[2]);
    }
}

/*
=============
CL_Viewpos_f -- johnfitz

display client's position and angles
=============
*/
void CL_Viewpos_f([[maybe_unused]] refdef_t& refdef)
{
    if(cls.state != ca_connected)
    {
        return;
    }
#if 0
	//camera position
	Con_Printf ("Viewpos: (%i %i %i) %i %i %i\n",
		(int)refdef.vieworg[0],
		(int)refdef.vieworg[1],
		(int)refdef.vieworg[2],
		(int)refdef.viewangles[PITCH],
		(int)refdef.viewangles[YAW],
		(int)refdef.viewangles[ROLL]);
#else
    // player position
    Con_Printf("Viewpos: (%i %i %i) %i %i %i\n",
        (int)cl.entities[cl.viewentity].origin[0],
        (int)cl.entities[cl.viewentity].origin[1],
        (int)cl.entities[cl.viewentity].origin[2], (int)cl.viewangles[PITCH],
        (int)cl.viewangles[YAW], (int)cl.viewangles[ROLL]);
#endif
}

static void CL_ServerExtension_FullServerinfo_f()
{
    //	const char *newserverinfo = Cmd_Argv(1);
}
static void CL_ServerExtension_ServerinfoUpdate_f()
{
    //	const char *newserverkey = Cmd_Argv(1);
    //	const char *newservervalue = Cmd_Argv(2);
}

static void CL_ServerExtension_Ignore_f()
{
    Con_DPrintf2("Ignoring stufftext: %s\n", Cmd_Argv(0));
}

/*
=================
CL_Init
=================
*/
void CL_Init()
{
    SZ_Alloc(&cls.message, 1024);

    CL_InitInput();
    CL_InitTEnts();

    Cvar_RegisterVariable(&cl_name);
    Cvar_RegisterVariable(&cl_color);
    Cvar_RegisterVariable(&cl_upspeed);
    Cvar_RegisterVariable(&cl_forwardspeed);
    Cvar_RegisterVariable(&cl_backspeed);
    Cvar_RegisterVariable(&cl_sidespeed);
    Cvar_RegisterVariable(&cl_movespeedkey);
    Cvar_RegisterVariable(&cl_yawspeed);
    Cvar_RegisterVariable(&cl_pitchspeed);
    Cvar_RegisterVariable(&cl_anglespeedkey);
    Cvar_RegisterVariable(&cl_shownet);
    Cvar_RegisterVariable(&cl_nolerp);
    Cvar_RegisterVariable(&lookspring);
    Cvar_RegisterVariable(&lookstrafe);
    Cvar_RegisterVariable(&sensitivity);

    Cvar_RegisterVariable(&cl_alwaysrun);

    Cvar_RegisterVariable(&m_pitch);
    Cvar_RegisterVariable(&m_yaw);
    Cvar_RegisterVariable(&m_forward);
    Cvar_RegisterVariable(&m_side);

    Cvar_RegisterVariable(&cfg_unbindall);

    Cvar_RegisterVariable(&cl_maxpitch); // johnfitz -- variable pitch clamping
    Cvar_RegisterVariable(&cl_minpitch); // johnfitz -- variable pitch clamping
    Cvar_RegisterVariable(&cl_recordingdemo); // spike -- for mod hacks. combine
                                              // with cvar_string or something

    Cmd_AddCommand("entities", CL_PrintEntities_f);
    Cmd_AddCommand("disconnect", CL_Disconnect_f);
    Cmd_AddCommand("record", CL_Record_f);
    Cmd_AddCommand("stop", CL_Stop_f);
    Cmd_AddCommand("playdemo", CL_PlayDemo_f);
    Cmd_AddCommand("timedemo", CL_TimeDemo_f);

    Cmd_AddCommand("tracepos", [] { CL_Tracepos_f(r_refdef); }); // johnfitz
    Cmd_AddCommand("viewpos", [] { CL_Viewpos_f(r_refdef); });   // johnfitz

    // spike -- add stubs to mute various invalid stuffcmds
    Cmd_AddCommand_ServerCommand(
        "fullserverinfo", CL_ServerExtension_FullServerinfo_f); // spike
    Cmd_AddCommand_ServerCommand(
        "svi", CL_ServerExtension_ServerinfoUpdate_f); // spike
    Cmd_AddCommand_ServerCommand("paknames",
        CL_ServerExtension_Ignore_f); // package names in use by the server
                                      // (including gamedir+extension)
    Cmd_AddCommand_ServerCommand(
        "paks", CL_ServerExtension_Ignore_f); // provides hashes to go with the
                                              // paknames list
    // Cmd_AddCommand_ServerCommand ("vwep", CL_ServerExtension_Ignore_f);
    // //invalid for nq, provides an alternative list of model precaches for
    // vweps. Cmd_AddCommand_ServerCommand ("at", CL_ServerExtension_Ignore_f);
    // //invalid for nq, autotrack info for mvds
    Cmd_AddCommand_ServerCommand(
        "wps", CL_ServerExtension_Ignore_f); // ktx/cspree weapon stats
    Cmd_AddCommand_ServerCommand(
        "it", CL_ServerExtension_Ignore_f); // cspree item timers
    Cmd_AddCommand_ServerCommand(
        "tinfo", CL_ServerExtension_Ignore_f); // ktx team info
    Cmd_AddCommand_ServerCommand(
        "exectrigger", CL_ServerExtension_Ignore_f); // spike
    Cmd_AddCommand_ServerCommand(
        "csqc_progname", CL_ServerExtension_Ignore_f); // spike
    Cmd_AddCommand_ServerCommand(
        "csqc_progsize", CL_ServerExtension_Ignore_f); // spike
    Cmd_AddCommand_ServerCommand(
        "csqc_progcrc", CL_ServerExtension_Ignore_f); // spike
    Cmd_AddCommand_ServerCommand(
        "cl_fullpitch", CL_ServerExtension_Ignore_f); // spike
    Cmd_AddCommand_ServerCommand(
        "pq_fullpitch", CL_ServerExtension_Ignore_f); // spike

    Cmd_AddCommand_ServerCommand(
        "cl_serverextension_download", CL_ServerExtension_Download_f); // spike
    Cmd_AddCommand_ServerCommand(
        "cl_downloadbegin", CL_Download_Begin_f); // spike
    Cmd_AddCommand_ServerCommand(
        "cl_downloadfinished", CL_Download_Finished_f); // spike
    Cmd_AddCommand("stopdownload", CL_StopDownload_f);  // spike
}
