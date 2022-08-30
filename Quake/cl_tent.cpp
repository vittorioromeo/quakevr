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
// cl_tent.c -- client side temporary entities

#include "quakedef.hpp"
#include "vr.hpp"
#include "util.hpp"
#include "console.hpp"
#include "glquake.hpp"
#include "msg.hpp"
#include "q_sound.hpp"
#include "client.hpp"
#include "sys.hpp"

struct qmodel_t;

int num_temp_entities;
entity_t cl_temp_entities[MAX_TEMP_ENTITIES];
beam_t cl_beams[MAX_BEAMS];

sfx_t* cl_sfx_wizhit;
sfx_t* cl_sfx_knighthit;
sfx_t* cl_sfx_tink1;
sfx_t* cl_sfx_ric1;
sfx_t* cl_sfx_ric2;
sfx_t* cl_sfx_ric3;
sfx_t* cl_sfx_r_exp3;

/*
=================
CL_ParseTEnt
=================
*/
void CL_InitTEnts()
{
    cl_sfx_wizhit = S_PrecacheSound("wizard/hit.wav");
    cl_sfx_knighthit = S_PrecacheSound("hknight/hit.wav");
    cl_sfx_tink1 = S_PrecacheSound("weapons/tink1.wav");
    cl_sfx_ric1 = S_PrecacheSound("weapons/ric1.wav");
    cl_sfx_ric2 = S_PrecacheSound("weapons/ric2.wav");
    cl_sfx_ric3 = S_PrecacheSound("weapons/ric3.wav");
    cl_sfx_r_exp3 = S_PrecacheSound("weapons/r_exp3.wav");
}

/*
=================
CL_ParseBeam
=================
*/
beam_t* CL_ParseBeam(qmodel_t* m)
{
    const int ent = MSG_ReadShort();
    const int disambiguator = MSG_ReadByte();
    const auto start = MSG_ReadVec3(cl.protocolflags);
    const auto end = MSG_ReadVec3(cl.protocolflags);

    const auto initBeam = [&](beam_t& b)
    {
        b.entity = ent;
        b.disambiguator = disambiguator;
        b.model = m;
        b.endtime = cl.time + 0.2;
        b.start = start;
        b.end = end;
        b.scaleRatioX = 1.f;
        b.spin = true;
    };

    // override any beam with the same entity and disambiguator
    for(int i = 0; i < MAX_BEAMS; ++i)
    {
        beam_t& b = cl_beams[i];

        if(b.entity == ent && b.disambiguator == disambiguator)
        {
            initBeam(b);
            return &b;
        }
    }

    // find a free beam
    for(int i = 0; i < MAX_BEAMS; ++i)
    {
        beam_t& b = cl_beams[i];

        if(!b.model || b.endtime < cl.time)
        {
            initBeam(b);
            return &b;
        }
    }

    // johnfitz -- less spammy overflow message
    if(!dev_overflows.beams ||
        dev_overflows.beams + CONSOLE_RESPAM_TIME < realtime)
    {
        Con_Printf("Beam list overflow!\n");
        dev_overflows.beams = realtime;
    }
    // johnfitz

    return nullptr;
}

void CL_SpawnSpriteEffect(
    vec3_t org /*, vec3_t dir, vec3_t orientationup*/, qmodel_t* model, int startframe, int framecount, float framerate /*, float alpha, float scale, float randspin, float gravity, int traileffect, unsigned int renderflags, int skinnum*/)
{
    if(startframe < 0)
    {
        startframe = framecount = 0;
    }
    if(!framecount)
    {
        framecount = model->numframes;
    }
    Con_DPrintf("CL_SpawnSpriteEffect: not implemented\n");
}

/*
=================
CL_ParseTEnt
=================
*/
void CL_ParseTEnt()
{
    const int type = MSG_ReadByte();
    switch(type)
    {
        case TE_WIZSPIKE: // spike hitting wall
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_RunParticleEffect_BulletPuff(pos, vec3_zero, 20, 30);
            S_StartSound(-1, 0, cl_sfx_wizhit, pos, 1, 1);
            break;
        }

        case TE_KNIGHTSPIKE: // spike hitting wall
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_RunParticleEffect_BulletPuff(pos, vec3_zero, 226, 20);
            S_StartSound(-1, 0, cl_sfx_knighthit, pos, 1, 1);
            break;
        }

        case TE_SPIKE: // spike hitting wall
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_RunParticleEffect_BulletPuff(pos, vec3_zero, 0, 10);
            if(rand() % 5)
            {
                S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
            }
            else
            {
                const int rnd = rand() & 3;
                if(rnd == 1)
                {
                    S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
                }
                else if(rnd == 2)
                {
                    S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
                }
                else
                {
                    S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
                }
            }
            break;
        }

        case TE_SUPERSPIKE: // super spike hitting wall
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_RunParticleEffect_BulletPuff(pos, vec3_zero, 0, 20);

            if(rand() % 5)
            {
                S_StartSound(-1, 0, cl_sfx_tink1, pos, 1, 1);
            }
            else
            {
                const int rnd = rand() & 3;
                if(rnd == 1)
                {
                    S_StartSound(-1, 0, cl_sfx_ric1, pos, 1, 1);
                }
                else if(rnd == 2)
                {
                    S_StartSound(-1, 0, cl_sfx_ric2, pos, 1, 1);
                }
                else
                {
                    S_StartSound(-1, 0, cl_sfx_ric3, pos, 1, 1);
                }
            }
            break;
        }

        case TE_GUNSHOT: // bullet hitting wall
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_RunParticleEffect_BulletPuff(pos, vec3_zero, 0, 10);
            break;
        }

        case TE_EXPLOSION: // rocket explosion
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_ParticleExplosion(pos);
            dlight_t* dl = CL_AllocDlight(0);
            dl->origin = pos;
            dl->radius = 350;
            dl->die = cl.time + 0.5;
            dl->decay = 300;
            S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
            break;
        }

        case TE_TAREXPLOSION: // tarbaby explosion
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_ParticleExplosion(pos);

            S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
            break;
        }

        case TE_LIGHTNING1: // lightning bolts (shambler)
        {
            static qmodel_t* model = Mod_ForName("progs/bolt.mdl", true);

            if(beam_t* b = CL_ParseBeam(model))
            {
                b->spin = true;
            }

            break;
        }

        case TE_LIGHTNING2: // lightning bolts (lighting gun, mjolnir, gremlin)
        {
            static qmodel_t* model = Mod_ForName("progs/bolt2.mdl", true);

            auto* hdr = (aliashdr_t*)Mod_Extradata(model);
            hdr->scale_origin = hdr->original_scale_origin * 0.5_qf;
            hdr->scale = hdr->original_scale * 0.5_qf;

            if(beam_t* b = CL_ParseBeam(model))
            {
                b->spin = true;

                const auto scaleRatioX = hdr->scale[0] / hdr->original_scale[0];
                b->scaleRatioX = scaleRatioX;
            }

            break;
        }

        case TE_LIGHTNING3: // lightning bolts (boss)
        {
            static qmodel_t* model = Mod_ForName("progs/bolt3.mdl", true);

            if(beam_t* b = CL_ParseBeam(model))
            {
                b->spin = true;
            }

            break;
        }

            // PGM 01/21/97
        case TE_BEAM: // grappling hook beam
        {
            static qmodel_t* model = Mod_ForName("progs/beam.mdl", true);
            static auto* hdr = []
            {
                auto* hdr = (aliashdr_t*)Mod_Extradata(model);
                hdr->scale_origin = hdr->original_scale_origin * 0.25_qf;
                hdr->scale = hdr->original_scale * 0.25_qf;

                hdr->scale[0] = hdr->original_scale[0] * 0.12_qf;
                hdr->scale_origin[0] = hdr->original_scale_origin[0] * 0.12_qf;
                return hdr;
            }();

            static const auto scaleRatioX =
                hdr->scale[0] / hdr->original_scale[0];

            if(beam_t* b = CL_ParseBeam(model))
            {
                b->spin = false;
                b->scaleRatioX = scaleRatioX;
            }

            break;
        }
            // PGM 01/21/97

        case TE_LAVASPLASH:
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_LavaSplash(pos);
            break;
        }

        case TE_TELEPORT:
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            R_TeleportSplash(pos);
            break;
        }

        case TE_EXPLOSION2: // color mapped explosion
        {
            const auto pos = MSG_ReadVec3(cl.protocolflags);
            const int colorStart = MSG_ReadByte();
            const int colorLength = MSG_ReadByte();

            // TODO VR: (P2): consider rewriting to use hue in fragment shader
            R_ParticleExplosion2(pos, colorStart, colorLength);

            dlight_t* dl = CL_AllocDlight(0);
            dl->origin = pos;
            dl->radius = 350;
            dl->die = cl.time + 0.5;
            dl->decay = 300;
            S_StartSound(-1, 0, cl_sfx_r_exp3, pos, 1, 1);
            break;
        }

        default: Sys_Error("CL_ParseTEnt: bad type");
    }
}

void CL_ParseEffect(bool big)
{
    vec3_t org;
    int modelindex;
    int startframe;
    int framecount;
    int framerate;
    qmodel_t* mod;

    org[0] = MSG_ReadCoord(cl.protocolflags);
    org[1] = MSG_ReadCoord(cl.protocolflags);
    org[2] = MSG_ReadCoord(cl.protocolflags);

    if(big)
    {
        modelindex = MSG_ReadShort();
    }
    else
    {
        modelindex = MSG_ReadByte();
    }

    if(big)
    {
        startframe = MSG_ReadShort();
    }
    else
    {
        startframe = MSG_ReadByte();
    }

    framecount = MSG_ReadByte();
    framerate = MSG_ReadByte();

    mod = cl.model_precache[modelindex];
    CL_SpawnSpriteEffect(org /*, nullptr, NULL*/, mod, startframe, framecount,
        framerate /*, mod->type==mod_sprite?-1:1, 1, 0, 0, P_INVALID, 0, 0*/);
}


/*
=================
CL_NewTempEntity
=================
*/
entity_t* CL_NewTempEntity()
{
    if(cl_numvisedicts == cl_maxvisedicts ||
        num_temp_entities == MAX_TEMP_ENTITIES)
    {
        return nullptr;
    }

    entity_t* const ent = &cl_temp_entities[num_temp_entities];
    memset(ent, 0, sizeof(*ent));

    num_temp_entities++;
    cl_visedicts[cl_numvisedicts] = ent;
    cl_numvisedicts++;

    ent->netstate.scale = 16;
    ent->netstate.colormod[0] = ent->netstate.colormod[1] =
        ent->netstate.colormod[2] = 32;
    ent->colormap = vid.colormap;
    return ent;
}


/*
=================
CL_UpdateTEnts
=================
*/
void CL_UpdateTEnts()
{
    num_temp_entities = 0;

    srand((int)(cl.time * 1000)); // johnfitz -- freeze beams when paused

    // update lightning
    for(int i = 0; i < MAX_BEAMS; ++i)
    {
        beam_t& b = cl_beams[i];

        if(!b.model || b.endtime < cl.time)
        {
            continue;
        }

        // calculate pitch and yaw
        auto dist = b.end - b.start;

        float pitch;
        float yaw;

        if(dist[1] == 0 && dist[0] == 0)
        {
            yaw = 0;
            if(dist[2] > 0)
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
            yaw = (int)(atan2(dist[1], dist[0]) * 180 / M_PI);
            if(yaw < 0)
            {
                yaw += 360;
            }

            const float forward = sqrt(dist[0] * dist[0] + dist[1] * dist[1]);
            pitch = (int)(atan2(dist[2], forward) * 180 / M_PI);
            if(pitch < 0)
            {
                pitch += 360;
            }
        }

        // add new entities for the lightning
        auto org = b.start;
        auto d = glm::length(dist);
        dist = safeNormalize(dist);

        const float incr = 30.f * b.scaleRatioX;

        while(d > 0)
        {
            entity_t* ent = CL_NewTempEntity();
            if(!ent)
            {
                return;
            }

            ent->origin = org;
            ent->model = b.model;
            ent->angles[PITCH] = pitch;
            ent->angles[YAW] = yaw;
            ent->angles[ROLL] = b.spin ? rand() % 360 : 0;

            // johnfitz -- use j instead of using i twice, so we don't corrupt
            // memory
            for(int j = 0; j < 3; j++)
            {
                org[j] += dist[j] * incr;
            }

            d -= incr;
        }
    }
}
