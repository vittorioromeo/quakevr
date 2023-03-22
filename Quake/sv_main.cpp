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
// sv_main.c -- server main program

#include "host.hpp"
#include "menu.hpp"
#include "quakedef.hpp"
#include "server.hpp"
#include "vr.hpp"
#include "util.hpp"
#include "cmd.hpp"
#include "common.hpp"
#include "console.hpp"
#include "net.hpp"
#include "glquake.hpp"
#include "protocol.hpp"
#include "worldtext.hpp"
#include "msg.hpp"
#include "sys.hpp"
#include "snd_voip.hpp"
#include "qcvm.hpp"
#include "client.hpp"

#include <algorithm>

server_t sv;
server_static_t svs;

static char localmodels[MAX_MODELS][8]; // inline model names for precache

int sv_protocol = PROTOCOL_QUAKEVR;                      // johnfitz
unsigned int sv_protocol_pext2 = PEXT2_SUPPORTED_SERVER; // spike

//============================================================================

void SV_CalcStats(client_t* client, int* statsi, float* statsf)
{
    size_t i;
    edict_t* ent = client->edict;
    // FIXME: string stats!
    int items;
    eval_t* val = GetEdictFieldValue(ent, qcvm->extfields.items2);
    if(val)
    {
        items = (int)ent->v.items | ((int)val->_float << 23);
    }
    else
    {
        items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);
    }

    memset(statsi, 0, sizeof(*statsi) * MAX_CL_STATS);
    memset(statsf, 0, sizeof(*statsf) * MAX_CL_STATS);
    statsf[STAT_HEALTH] = ent->v.health;
    //	statsf[STAT_FRAGS] = ent->v.frags;	//obsolete
    statsi[STAT_WEAPON] = SV_ModelIndex(PR_GetString(ent->v.weaponmodel));
    if((unsigned int)statsi[STAT_WEAPON] >= client->limit_models)
    {
        statsi[STAT_WEAPON] = 0;
    }
    statsf[STAT_AMMO] = ent->v.currentammo;
    statsf[STAT_ARMOR] = ent->v.armorvalue;
    statsf[STAT_WEAPONFRAME] = ent->v.weaponframe;
    statsf[STAT_SHELLS] = ent->v.ammo_shells;
    statsf[STAT_NAILS] = ent->v.ammo_nails;
    statsf[STAT_ROCKETS] = ent->v.ammo_rockets;
    statsf[STAT_CELLS] = ent->v.ammo_cells;
    statsf[STAT_ACTIVEWEAPON] = ent->v.weapon; // sent in a way that does NOT
                                               // depend upon the current mod...
    //	statsf[STAT_TOTALSECRETS] = pr_global_struct->total_secrets;	//don't
    // bother with these, the qc sends extra updates and we don't want to end up
    // with a race condition. 	statsf[STAT_TOTALMONSTERS] =
    // pr_global_struct->total_monsters; 	statsf[STAT_SECRETS] =
    // pr_global_struct->found_secrets; 	statsf[STAT_MONSTERS] =
    // pr_global_struct->killed_monsters; 	statsi[STAT_TIME] = sv.time*1000;
    ////in ms, this was a hack to work around vanilla QW clients not having any
    // concept of serverside time. 	statsi[STAT_MATCHSTARTTIME] = 0*1000;
    ////in ms, set by the mod to when the current match actually starts (so
    // pregame stuff doesn't interfere with game clocks). 	stats[STAT_VIEW2] =
    // NUM_FOR_EDICT(PROG_TO_EDICT(ent->v.view2));
    if((val = GetEdictFieldValue(ent, qcvm->extfields.viewzoom)) && val->_float)
    {
        statsf[STAT_VIEWZOOM] = val->_float * 255;
        if(statsf[STAT_VIEWZOOM] < 1)
        {
            statsf[STAT_VIEWZOOM] = 1;
        }
    }

    // FIXME: add support for clientstat/globalstat qc builtins.

    if(client->protocol_pext2 & PEXT2_PREDINFO)
    { // predinfo also kills clc_clientdata
        statsi[STAT_ITEMS] = items;
        statsf[STAT_VIEWHEIGHT] = ent->v.view_ofs[2];
        statsf[STAT_IDEALPITCH] = ent->v.idealpitch;
        statsf[STAT_PUNCHANGLE_X] = ent->v.punchangle[0];
        statsf[STAT_PUNCHANGLE_Y] = ent->v.punchangle[1];
        statsf[STAT_PUNCHANGLE_Z] = ent->v.punchangle[2];
    }
    /*
        if (client->protocol_pext2 & PEXT2_PREDINFO)
        {	//prediction needs some info on the server's rules
            statsf[STAT_MOVEVARS_FRICTION] = sv_friction.value;
            statsf[STAT_MOVEVARS_WATERFRICTION] = sv_waterfriction.value;
            statsf[STAT_MOVEVARS_TICRATE] = host_maxfps.value;
            statsf[STAT_MOVEVARS_TIMESCALE] = sv_gamespeed.value;
            statsf[STAT_MOVEVARS_GRAVITY] = sv_gravity.value;
            statsf[STAT_MOVEVARS_STOPSPEED] = sv_stopspeed.value;
            statsf[STAT_MOVEVARS_MAXSPEED] = client->maxspeed;
            statsf[STAT_MOVEVARS_SPECTATORMAXSPEED] =
    sv_spectatormaxspeed.value; statsf[STAT_MOVEVARS_ACCELERATE] =
    sv_accelerate.value; statsf[STAT_MOVEVARS_AIRACCELERATE] =
    sv_airaccelerate.value; statsf[STAT_MOVEVARS_WATERACCELERATE] =
    sv_wateraccelerate.value; statsf[STAT_MOVEVARS_ENTGRAVITY] =
    client->entgravity/sv_gravity.value; statsf[STAT_MOVEVARS_JUMPVELOCITY] =
    sv_jumpvelocity.value;	//bah statsf[STAT_MOVEVARS_EDGEFRICTION] =
    sv_edgefriction.value; statsf[STAT_MOVEVARS_MAXAIRSPEED] = client->maxspeed;
            statsf[STAT_MOVEVARS_STEPHEIGHT] = 18;
    //		statsf[STAT_MOVEVARS_AIRACCEL_QW] = 0;
    //		statsf[STAT_MOVEVARS_AIRACCEL_SIDEWAYS_FRICTION] = sv_gravity.value;
        }
    */

    for(i = 0; i < sv.numcustomstats; i++)
    {
        eval_t* eval = sv.customstats[i].ptr;
        if(!eval)
        {
            eval = GetEdictFieldValue(ent, sv.customstats[i].fld);
        }

        switch(sv.customstats[i].type)
        {
            case ev_ext_integer:
                statsi[sv.customstats[i].idx] = eval->_int;
                break;
            case ev_entity:
                statsi[sv.customstats[i].idx] =
                    NUM_FOR_EDICT(PROG_TO_EDICT(eval->edict));
                break;
            case ev_float: statsf[sv.customstats[i].idx] = eval->_float; break;
            case ev_vector:
                statsf[sv.customstats[i].idx + 0] = eval->vector[0];
                statsf[sv.customstats[i].idx + 1] = eval->vector[1];
                statsf[sv.customstats[i].idx + 2] = eval->vector[2];
                break;
            case ev_string:   // not supported in this build... send with
                              // svcfte_updatestatstring on change, which is
                              // annoying.
            case ev_void:     // nothing...
            case ev_field:    // panic! everyone panic!
            case ev_function: // doesn't make much sense
            case ev_pointer:  // doesn't make sense
            default: break;
        }
    }
}


/*server-side-only flags that re-use encoding bits*/
#define UF_REMOVE UF_16BIT /*says we removed the entity in this frame*/
#define UF_MOVETYPE                                     \
    UF_EFFECTS2 /*this flag isn't present in the header \
                   itself*/
#define UF_RESET2                                                    \
    UF_EXTEND1 /*so new ents are reset multiple times to avoid weird \
                  baselines*/
// #define UF_UNUSED		UF_EXTEND2	/**/
#define UF_WEAPONFRAME_OLD UF_EXTEND2
#define UF_VIEWANGLES UF_EXTEND3 /**/



static unsigned int SVFTE_DeltaPredCalcBits(
    entity_state_t* from, entity_state_t* to)
{
    unsigned int bits = 0;
    //	if (from && from->pmovetype != to->pmovetype)
    //		bits |= UFP_MOVETYPE;

    //	if (to->movement[0])
    //		bits |= UFP_FORWARD;
    //	if (to->movement[1])
    //		bits |= UFP_SIDE;
    //	if (to->movement[2])
    //		bits |= UFP_UP;
    if(to->velocity[0])
    {
        bits |= UFP_VELOCITYXY;
    }
    if(to->velocity[1])
    {
        bits |= UFP_VELOCITYXY;
    }
    if(to->velocity[2])
    {
        bits |= UFP_VELOCITYZ;
    }
    //	if (to->msec)
    //		bits |= UFP_MSEC;

    return bits;
}

static unsigned int MSGFTE_DeltaCalcBits(
    entity_state_t* from, entity_state_t* to)
{
    unsigned int bits = 0;

    if(from->pmovetype != to->pmovetype)
    {
        bits |= UF_PREDINFO | UF_MOVETYPE;
    }
    /*	if (from->weaponframe != to->weaponframe)
            bits |= UF_PREDINFO|UF_WEAPONFRAME_OLD;
    *///	if (to->pmovetype)	//we don't support prediction, but we still need to network the player's velocity for bob etc.
    {
        if(SVFTE_DeltaPredCalcBits(from, to))
        {
            bits |= UF_PREDINFO;
        }

        // moving players get extra data forced upon them which is not
        // deltatracked
        if((bits & UF_PREDINFO) &&
            (from->velocity[0] || from->velocity[1] || from->velocity[2]))
        {
            // if we've got player movement then write the origin anyway, to
            // cover packetloss
            bits |= UF_ORIGINXY | UF_ORIGINZ;
            // and force angles too, if its not us
            //			if (host_client != svs.clients + to->number-1)
            //				bits |= UF_ANGLESXZ | UF_ANGLESY;
        }
    }



    if(to->origin[0] != from->origin[0])
    {
        bits |= UF_ORIGINXY;
    }
    if(to->origin[1] != from->origin[1])
    {
        bits |= UF_ORIGINXY;
    }
    if(to->origin[2] != from->origin[2])
    {
        bits |= UF_ORIGINZ;
    }

    if(to->angles[0] != from->angles[0])
    {
        bits |= UF_ANGLESXZ;
    }
    if(to->angles[1] != from->angles[1])
    {
        bits |= UF_ANGLESY;
    }
    if(to->angles[2] != from->angles[2])
    {
        bits |= UF_ANGLESXZ;
    }


    if(to->modelindex != from->modelindex)
    {
        bits |= UF_MODEL;
    }
    if(to->frame != from->frame)
    {
        bits |= UF_FRAME;
    }
    if(to->skin != from->skin)
    {
        bits |= UF_SKIN;
    }
    if(to->colormap != from->colormap)
    {
        bits |= UF_COLORMAP;
    }
    if(to->effects != from->effects)
    {
        bits |= UF_EFFECTS;
    }
    if(to->eflags != from->eflags)
    {
        bits |= UF_FLAGS;
    }
    //	if (to->solidsize != from->solidsize)
    //		bits |= UF_SOLID;

    if(to->scale != from->scale)
    {
        bits |= UF_SCALE;
    }
    if(to->alpha != from->alpha)
    {
        bits |= UF_ALPHA;
    }
    //	if (to->fatness != from->fatness)
    //		bits |= UF_FATNESS;

    //	if (to->hexen2flags != from->hexen2flags || to->abslight !=
    // from->abslight) 		bits |= UF_DRAWFLAGS;

    //	if (to->bonecount != from->bonecount || (to->bonecount &&
    // memcmp(frombonedata+from->boneoffset, tobonedata+to->boneoffset,
    // to->bonecount*sizeof(short)*7))) 		bits |= UF_BONEDATA; 	if
    // (!to->bonecount
    //&& (to->basebone != from->basebone || to->baseframe != from->baseframe))
    //		bits |= UF_BONEDATA;

    if(to->colormod[0] != from->colormod[0] ||
        to->colormod[1] != from->colormod[1] ||
        to->colormod[2] != from->colormod[2])
    {
        bits |= UF_COLORMOD;
    }

    //	if
    //(to->glowsize!=from->glowsize||to->glowcolour!=from->glowcolour||to->glowmod[0]!=from->glowmod[0]||to->glowmod[1]!=from->glowmod[1]||to->glowmod[2]!=from->glowmod[2])
    //		bits |= UF_GLOW;

    if(to->tagentity != from->tagentity || to->tagindex != from->tagindex)
    {
        bits |= UF_TAGINFO;
    }

    //	if (to->light[0] != from->light[0] || to->light[1] != from->light[1] ||
    // to->light[2] != from->light[2] || to->light[3] != from->light[3] ||
    // to->lightstyle != from->lightstyle || to->lightpflags !=
    // from->lightpflags) 		bits |= UF_LIGHT;

    if(to->traileffectnum != from->traileffectnum ||
        to->emiteffectnum != from->emiteffectnum)
    {
        bits |= UF_TRAILEFFECT;
    }

    //	if (to->modelindex2 != from->modelindex2)
    //		bits |= UF_MODELINDEX2;

    //	if (to->gravitydir[0] != from->gravitydir[0] || to->gravitydir[1] !=
    // from->gravitydir[1]) 		bits |= UF_GRAVITYDIR;

    return bits;
}

// #undef MSG_WriteEntity
void MSG_WriteEntity(sizebuf_t* sb, int c, unsigned int pext2)
{
    // high short, low byte
    if(c > 0x7fff && (pext2 & PEXT2_REPLACEMENTDELTAS))
    {
        MSG_WriteShort(sb, 0x8000 | (c >> 8));
        MSG_WriteByte(sb, c & 0xff);
    }
    else
    {
        MSG_WriteShort(sb, c);
    }
}
static void MSGFTE_WriteEntityUpdate(unsigned int bits, entity_state_t* state,
    sizebuf_t* msg, unsigned int pext2, unsigned int protocolflags)
{
    unsigned int predbits = 0;
    if(bits & UF_MOVETYPE)
    {
        bits &= ~UF_MOVETYPE;
        predbits |= UFP_MOVETYPE;
    }
    if(pext2 & PEXT2_PREDINFO)
    {
        if(bits & UF_VIEWANGLES)
        {
            bits &= ~UF_VIEWANGLES;
            bits |= UF_PREDINFO;
            predbits |= UFP_VIEWANGLE;
        }
    }
    else
    {
        if(bits & UF_VIEWANGLES)
        {
            bits &= ~UF_VIEWANGLES;
            bits |= UF_PREDINFO;
        }
        if(bits & UF_WEAPONFRAME_OLD)
        {
            bits &= ~UF_WEAPONFRAME_OLD;
            predbits |= UFP_WEAPONFRAME_OLD;
        }
    }

    //	if (!(pext2 & PEXT2_NEWSIZEENCODING))	//was added at the same time
    bits &= ~UF_BONEDATA;

    /*check if we need more precision for some things*/
    if((bits & UF_MODEL) && state->modelindex > 255)
    {
        bits |= UF_16BIT;
    }
    //	if ((bits & UF_SKIN) && state->skin > 255)
    //		bits |= UF_16BIT;
    if((bits & UF_FRAME) && state->frame > 255)
    {
        bits |= UF_16BIT;
    }

    /*convert effects bits to higher lengths if needed*/
    if(bits & UF_EFFECTS)
    {
        if(state->effects & 0xffff0000)
        { /*both*/
            bits |= UF_EFFECTS | UF_EFFECTS2;
        }
        else if(state->effects & 0x0000ff00)
        { /*2 only*/
            bits = (bits & ~UF_EFFECTS) | UF_EFFECTS2;
        }
    }
    if(bits & 0xff000000)
    {
        bits |= UF_EXTEND3;
    }
    if(bits & 0x00ff0000)
    {
        bits |= UF_EXTEND2;
    }
    if(bits & 0x0000ff00)
    {
        bits |= UF_EXTEND1;
    }

    MSG_WriteByte(msg, (bits >> 0) & 0xff);
    if(bits & UF_EXTEND1)
    {
        MSG_WriteByte(msg, (bits >> 8) & 0xff);
    }
    if(bits & UF_EXTEND2)
    {
        MSG_WriteByte(msg, (bits >> 16) & 0xff);
    }
    if(bits & UF_EXTEND3)
    {
        MSG_WriteByte(msg, (bits >> 24) & 0xff);
    }

    if(bits & UF_FRAME)
    {
        if(bits & UF_16BIT)
        {
            MSG_WriteShort(msg, state->frame);
        }
        else
        {
            MSG_WriteByte(msg, state->frame);
        }
    }
    if(bits & UF_ORIGINXY)
    {
        MSG_WriteCoord(msg, state->origin[0], protocolflags);
        MSG_WriteCoord(msg, state->origin[1], protocolflags);
    }
    if(bits & UF_ORIGINZ)
    {
        MSG_WriteCoord(msg, state->origin[2], protocolflags);
    }

    if((bits & UF_PREDINFO) && !(pext2 & PEXT2_PREDINFO))
    { /*if we have pred info, (always) use more precise angles*/
        if(bits & UF_ANGLESXZ)
        {
            MSG_WriteAngle16(msg, state->angles[0], protocolflags);
            MSG_WriteAngle16(msg, state->angles[2], protocolflags);
        }
        if(bits & UF_ANGLESY)
        {
            MSG_WriteAngle16(msg, state->angles[1], protocolflags);
        }
    }
    else
    {
        if(bits & UF_ANGLESXZ)
        {
            MSG_WriteAngle(msg, state->angles[0], protocolflags);
            MSG_WriteAngle(msg, state->angles[2], protocolflags);
        }
        if(bits & UF_ANGLESY)
        {
            MSG_WriteAngle(msg, state->angles[1], protocolflags);
        }
    }

    if((bits & (UF_EFFECTS | UF_EFFECTS2)) == (UF_EFFECTS | UF_EFFECTS2))
    {
        MSG_WriteLong(msg, state->effects);
    }
    else if(bits & UF_EFFECTS2)
    {
        MSG_WriteShort(msg, state->effects);
    }
    else if(bits & UF_EFFECTS)
    {
        MSG_WriteByte(msg, state->effects);
    }

    if(bits & UF_PREDINFO)
    {
        /*movetype is set above somewhere*/
        predbits |= SVFTE_DeltaPredCalcBits(nullptr, state);

        MSG_WriteByte(msg, predbits);
        /*
                if (predbits & UFP_FORWARD)
                    MSG_WriteShort(msg, state->movement[0]);
                if (predbits & UFP_SIDE)
                    MSG_WriteShort(msg, state->movement[1]);
                if (predbits & UFP_UP)
                    MSG_WriteShort(msg, state->movement[2]);
        */
        if(predbits & UFP_MOVETYPE)
        {
            MSG_WriteByte(msg, state->pmovetype);
        }
        if(predbits & UFP_VELOCITYXY)
        {
            MSG_WriteShort(msg, state->velocity[0]);
            MSG_WriteShort(msg, state->velocity[1]);
        }
        if(predbits & UFP_VELOCITYZ)
        {
            MSG_WriteShort(msg, state->velocity[2]);
        }
        /*		if (predbits & UFP_MSEC)
                    MSG_WriteByte(msg, state->msec);
                if (pext2 & PEXT2_PREDINFO)
                {
                    if (predbits & UFP_VIEWANGLE)
                    {	//if we have pred info, use more precise angles
                        if (bits & UF_ANGLESXZ)
                        {
                            MSG_WriteShort(msg, state->vangle[0]);
                            MSG_WriteShort(msg, state->vangle[2]);
                        }
                        if (bits & UF_ANGLESY)
                            MSG_WriteShort(msg, state->vangle[1]);
                    }
                }
                else
                {
                    if (predbits & UFP_WEAPONFRAME_OLD)
                    {
                        if (state->weaponframe > 127)
                        {
                            MSG_WriteByte(msg, 128 | (state->weaponframe &
           127)); MSG_WriteByte(msg, state->weaponframe>>7);
                        }
                        else
                            MSG_WriteByte(msg, state->weaponframe);
                    }
                }
        */
    }

    if(bits & UF_MODEL)
    {
        if(bits & UF_16BIT)
        {
            MSG_WriteShort(msg, state->modelindex);
        }
        else
        {
            MSG_WriteByte(msg, state->modelindex);
        }
    }
    if(bits & UF_SKIN)
    {
        if(bits & UF_16BIT)
        {
            MSG_WriteShort(msg, state->skin);
        }
        else
        {
            MSG_WriteByte(msg, state->skin);
        }
    }
    if(bits & UF_COLORMAP)
    {
        MSG_WriteByte(msg, state->colormap & 0xff);
    }

    if(bits & UF_SOLID)
    {
        /*		if (pext2 & PEXT2_NEWSIZEENCODING)
                {
                    if (!state->solidsize)
                        MSG_WriteByte(msg, 0);
                    else if (state->solidsize == ES_SOLID_BSP)
                        MSG_WriteByte(msg, 1);
                    else if (state->solidsize == ES_SOLID_HULL1)
                        MSG_WriteByte(msg, 2);
                    else if (state->solidsize == ES_SOLID_HULL2)
                        MSG_WriteByte(msg, 3);
                    else if (!ES_SOLID_HAS_EXTRA_BITS(state->solidsize))
                    {
                        MSG_WriteByte(msg, 16);
                        MSG_WriteSize16(msg, state->solidsize);
                    }
                    else
                    {
                        MSG_WriteByte(msg, 32);
                        MSG_WriteLong(msg, state->solidsize);
                    }
                }
                else
                    MSG_WriteSize16(msg, state->solidsize);
        */
    }

    if(bits & UF_FLAGS)
    {
        MSG_WriteByte(msg, state->eflags);
    }

    if(bits & UF_ALPHA)
    {
        MSG_WriteByte(msg, (state->alpha - 1) & 0xff);
    }
    if(bits & UF_SCALE)
    {
        MSG_WriteByte(msg, state->scale);
    }
    /*	if (bits & UF_BONEDATA)
        {
            short *bonedata;
            int i;
            byte bfl = 0;
            if (state->bonecount && boneptr)
                bfl |= 0x80;
            if (state->basebone || state->baseframe)
                bfl |= 0x40;
            MSG_WriteByte(msg, bfl);
            if (bfl & 0x80)
            {
                //this is NOT finalized
                MSG_WriteByte(msg, state->bonecount);
                bonedata = (short*)(boneptr + state->boneoffset);
                for (i = 0; i < state->bonecount*7; i++)
                    MSG_WriteShort(msg, bonedata[i]);
            }
            if (bfl & 0x40)
            {
                MSG_WriteByte(msg, state->basebone);
                MSG_WriteShort(msg, state->baseframe);
            }
        }
        if (bits & UF_DRAWFLAGS)
        {
            MSG_WriteByte(msg, state->drawflags);
            if ((state->drawflags & MLS_MASK) == MLS_ABSLIGHT)
                MSG_WriteByte(msg, state->abslight);
        }
    */
    if(bits & UF_TAGINFO)
    {
        MSG_WriteEntity(msg, state->tagentity, pext2);
        MSG_WriteByte(msg, state->tagindex);
    }
    /*	if (bits & UF_LIGHT)
        {
            MSG_WriteShort (msg, state->light[0]);
            MSG_WriteShort (msg, state->light[1]);
            MSG_WriteShort (msg, state->light[2]);
            MSG_WriteShort (msg, state->light[3]);
            MSG_WriteByte (msg, state->lightstyle);
            MSG_WriteByte (msg, state->lightpflags);
        }
    */
    if(bits & UF_TRAILEFFECT)
    {
        if(state->emiteffectnum)
        { // 3 spare bits. so that's nice (this is guarenteed to be 14 bits max
          // due to precaches using the upper two bits).
            MSG_WriteShort(msg, (state->traileffectnum & 0x3fff) | 0x8000);
            MSG_WriteShort(msg, state->emiteffectnum & 0x3fff);
        }
        else
        {
            MSG_WriteShort(msg, state->traileffectnum & 0x3fff);
        }
    }

    if(bits & UF_COLORMOD)
    {
        MSG_WriteByte(msg, state->colormod[0]);
        MSG_WriteByte(msg, state->colormod[1]);
        MSG_WriteByte(msg, state->colormod[2]);
    }
    /*	if (bits & UF_GLOW)
        {
            MSG_WriteByte(msg, state->glowsize);
            MSG_WriteByte(msg, state->glowcolour);
            MSG_WriteByte(msg, state->glowmod[0]);
            MSG_WriteByte(msg, state->glowmod[1]);
            MSG_WriteByte(msg, state->glowmod[2]);
        }
        if (bits & UF_FATNESS)
            MSG_WriteByte(msg, state->fatness);
        if (bits & UF_MODELINDEX2)
        {
            if (bits & UF_16BIT)
                MSG_WriteShort(msg, state->modelindex2);
            else
                MSG_WriteByte(msg, state->modelindex2);
        }

        if (bits & UF_GRAVITYDIR)
        {
            MSG_WriteByte(msg, state->gravitydir[0]);
            MSG_WriteByte(msg, state->gravitydir[1]);
        }
    */
}

static client_t::entity_num_state_s* snapshot_entstate;
static size_t snapshot_numents;
static size_t snapshot_maxents;

void SVFTE_DestroyFrames(client_t* client)
{
    if(client->previousentities)
    {
        free(client->previousentities);
    }
    client->previousentities = nullptr;
    client->numpreviousentities = 0;
    client->maxpreviousentities = 0;


    if(client->pendingentities_bits)
    {
        free(client->pendingentities_bits);
    }
    client->pendingentities_bits = nullptr;
    client->numpendingentities = 0;

    while(client->numframes > 0)
    {
        client->numframes--;
        free(client->frames[client->numframes].ents);
    }
    if(client->frames)
    {
        free(client->frames);
    }
    client->frames = nullptr;

    client->lastacksequence = 0;
}

static void SVFTE_SetupFrames(client_t* client)
{
    size_t fr;
    // the client will clear out their stats on receipt of the svc_serverinfo
    // packet. we won't send any reliables until they receive it so it should be
    // enough to just clear these here, and they'll get their new stats with the
    // first entity update once they're spawned
    memset(client->oldstats_i, 0, sizeof(client->oldstats_i));
    memset(client->oldstats_f, 0, sizeof(client->oldstats_f));
    client->lastmovemessage = 0; // it'll clear this too

    if(!client->protocol_pext2)
    {
        SVFTE_DestroyFrames(client);
        return;
    }

    client->numframes = 64; // must be power-of-two
    client->frames = (client_t::deltaframe_s*)malloc(
        sizeof(*client->frames) * client->numframes);
    client->lastacksequence = (int)0x80000000;
    memset(client->frames, 0, sizeof(*client->frames) * client->numframes);
    for(fr = 0; fr < client->numframes; fr++)
    {
        client->frames[fr].sequence = client->lastacksequence;
    }

    client->numpendingentities = qcvm->num_edicts;
    client->pendingentities_bits = (unsigned int*)calloc(
        client->numpendingentities, sizeof(*client->pendingentities_bits));

    client->pendingentities_bits[0] = UF_REMOVE;
}

static void SVFTE_DroppedFrame(client_t* client, int sequence)
{
    int i;
    client_t::deltaframe_s* frame =
        &client->frames[sequence & (client->numframes - 1)];
    if(frame->sequence != sequence)
        return; // this frame was stale... client is running too far behind.
                // we'll probably be spamming resends as a result.
    frame->sequence = -1;
    // flag their stats for resend
    for(i = 0; i < MAX_CL_STATS / 32; i++)
        client->resendstats[i] |= frame->resendstats[i];
    // flag their various entities as needing a resend too.
    for(i = 0; i < frame->numents; i++)
        client->pendingentities_bits[frame->ents[i].num] |= frame->ents[i].bits;
}

void SVFTE_Ack(client_t* client, int sequence)
{ // any gaps in the sequence need to considered dropped
    client_t::deltaframe_s* frame;
    int dropseq = client->lastacksequence + 1;
    if(!client->numframes)
    {
        return; // client shouldn't be using this.
    }
    if(sequence == -1)
    {
        client->pendingentities_bits[0] |=
            UF_REMOVE; // client wants a full resend. which might happen from it
    }
    // just starting to record a demo, saving it from writing
    // all the deltas out.
    if(sequence < client->lastacksequence)
    {
        //		else Con_SafePrintf("dupe or stale ack (%s, %i->%i)\n",
        // client->name, client->lastacksequence, sequence);
        return; // panic
    }
    if((unsigned)(dropseq - sequence) >= client->numframes)
    {
        dropseq = sequence - client->numframes;
    }
    while(dropseq < sequence)
    {
        SVFTE_DroppedFrame(client, dropseq++);
    }
    client->lastacksequence = sequence;

    frame = &client->frames[sequence & (client->numframes - 1)];
    if(frame->sequence >= 0)
    {
        frame->sequence = -1;
        host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] =
            qcvm->time - frame->timestamp;
        host_client->num_pings++;
    }
}
static void SVFTE_WriteStats(client_t* client, sizebuf_t* msg)
{
    int statsi[MAX_CL_STATS];
    float statsf[MAX_CL_STATS];
    int i;
    client_t::deltaframe_s* frame;
    int sequence = NET_QSocketGetSequenceOut(client->netconnection);

    frame = &client->frames[sequence & (client->numframes - 1)];

    if(frame->sequence ==
        sequence -
            (int)client
                ->numframes) // client is getting behind... this may get really
                             // spammy, lets hope it clears up at some point
        SVFTE_DroppedFrame(client, frame->sequence);

    // figure out the current values in a nice easy way (yay for copying to make
    // arrays easier!)
    SV_CalcStats(client, statsi, statsf);

    for(i = 0; i < MAX_CL_STATS; i++)
    {
        // small cleanup
        if(!statsi[i])
        {
            statsi[i] = statsf[i];
        }
        else
        {
            statsf[i] = 0; // statsi[i];
        }

        // if it changed flag for sending
        if(statsi[i] != client->oldstats_i[i] ||
            statsf[i] != client->oldstats_f[i])
        {
            client->oldstats_i[i] = statsi[i];
            client->oldstats_f[i] = statsf[i];
            client->resendstats[i / 32] |= 1u << (i & 31);
        }

        // if its flagged then unflag it, log it, and send it
        if(client->resendstats[i / 32] & (1u << (i & 31)))
        {
            client->resendstats[i / 32] &= ~(1u << (i & 31));
            frame->resendstats[i / 32] |= 1u << (i & 31);

            if((double)statsi[i] != statsf[i] && statsf[i])
            { // didn't round nicely, so send as a float
                MSG_WriteByte(msg, svcfte_updatestatfloat);
                MSG_WriteByte(msg, i);
                MSG_WriteFloat(msg, statsf[i]);
            }
            else
            {
                if(statsi[i] < 0 || statsi[i] > 255)
                { // needs to be big
                    MSG_WriteByte(msg, svc_updatestat);
                    MSG_WriteByte(msg, i);
                    MSG_WriteLong(msg, statsi[i]);
                }
                else
                { // can be fairly small
                    MSG_WriteByte(msg, svcdp_updatestatbyte);
                    MSG_WriteByte(msg, i);
                    MSG_WriteByte(msg, statsi[i]);
                }
            }
        }
    }
}
static void SVFTE_CalcEntityDeltas(client_t* client)
{
    client_t::entity_num_state_s *olds, *news, *oldstop, *newstop;

    if((int)client->numpendingentities < qcvm->num_edicts)
    {
        int newmax = qcvm->num_edicts + 64;
        client->pendingentities_bits =
            (unsigned int*)realloc(client->pendingentities_bits,
                sizeof(*client->pendingentities_bits) * newmax);
        memset(client->pendingentities_bits + client->numpendingentities, 0,
            sizeof(*client->pendingentities_bits) *
                (newmax - client->numpendingentities));
        client->numpendingentities = newmax;
    }

    // if we're clearing the list and starting from scratch, just wipe all
    // lingering state
    if(client->pendingentities_bits[0] & UF_REMOVE)
    {
        client->numpreviousentities = 0;
        client->pendingentities_bits[0] = UF_REMOVE;
    }

    news = snapshot_entstate;
    newstop = news + snapshot_numents;
    olds = client->previousentities;
    oldstop = olds + client->numpreviousentities;

    // we have two sets of entity state, pvs culled etc already.
    // figure out which flags changed,
    for(;;)
    {
        if(olds == oldstop && news == newstop)
        {
            break;
        }
        if(news == newstop || (olds != oldstop && olds->num < news->num))
        {
            // old ent is no longer visible, so flag for removal.
            client->pendingentities_bits[olds->num] = UF_REMOVE;
            olds++;
        }
        else if(olds == oldstop || (news != newstop && news->num < olds->num))
        {
            // new ent is new this frame, so reset everything.
            client->pendingentities_bits[news->num] = UF_RESET | UF_RESET2;
            // don't need to calc the other bits here, resets are enough
            news++;
        }
        else
        { // simple entity delta
            // its flagged for removing, that's weird... must be some killer
            // packetloss. turn that back into a reset or something
            if(client->pendingentities_bits[news->num] & UF_REMOVE)
                client->pendingentities_bits[news->num] =
                    (client->pendingentities_bits[news->num] & ~UF_REMOVE) |
                    UF_RESET2;
            client->pendingentities_bits[news->num] |=
                MSGFTE_DeltaCalcBits(&olds->state, &news->state);
            news++;
            olds++;
        }
    }

    // now we know what flags to apply, the client needs a copy of that state
    // for the next frame too. outgoing data can just read off these states too,
    // instead of needing to hit the edicts memory (which may be spread over
    // multiple allocations, yay cache). to avoid a potentially large memcopy,
    // I'm just going to swap these buffers.
    olds = client->previousentities;
    oldstop = olds + client->maxpreviousentities;

    client->previousentities = snapshot_entstate;
    client->numpreviousentities = snapshot_numents;
    client->maxpreviousentities = snapshot_maxents;

    snapshot_entstate = olds;
    snapshot_numents = 0;
    snapshot_maxents = oldstop - olds;
}

static void SVFTE_WriteEntitiesToClient(
    client_t* client, sizebuf_t* msg, size_t overflowsize)
{
    client_t::entity_num_state_s *state, *stateend;
    unsigned int bits, logbits;
    size_t entnum;
    int sequence = NET_QSocketGetSequenceOut(client->netconnection);
    size_t origmaxsize = msg->maxsize;
    size_t rollbacksize; // I'm too lazy to figure out sizes (especially if
                         // someone updates this for bone states or whatever)
    client_t::deltaframe_s* frame =
        &client->frames[sequence & (client->numframes - 1)];
    frame->sequence = sequence; // so we know that it wasn't stale later.
    frame->timestamp = qcvm->time;

    msg->maxsize = overflowsize;

    state = client->previousentities;
    stateend = state + client->numpreviousentities;

    MSG_WriteByte(msg, svcfte_updateentities);

    frame->numents = 0;
    if(client->protocol_pext2 & PEXT2_PREDINFO)
    {
        MSG_WriteShort(msg, (client->lastmovemessage & 0xffff));
    }
    MSG_WriteFloat(
        msg, qcvm->time); // should be the time the last physics frame was run.
    for(entnum = client->snapshotresume; entnum < client->numpendingentities;
        entnum++)
    {
        bits = client->pendingentities_bits[entnum];
        if(!(bits & ~UF_RESET2))
        {
            continue; // nothing to send (if reset2 is still set, then leave it
        }
        // pending until there's more data

        rollbacksize = msg->cursize;
        client->pendingentities_bits[entnum] = 0;
        logbits = 0;
        if(bits & UF_REMOVE)
        {
            if(entnum > 0x3fff)
            {
                MSG_WriteShort(msg, 0xc000 | (entnum & 0x3fff));
                MSG_WriteShort(msg, entnum >> 14);
            }
            else
            {
                MSG_WriteShort(msg, 0x8000 | entnum);
            }
            logbits = UF_REMOVE;
        }
        else
        {
            while(state < stateend && state->num < entnum) state++;
            if(state < stateend && state->num == entnum)
            {
                if(bits & UF_RESET2)
                {
                    /*if reset2, then this is the second packet sent to the
                     * client and should have a forced reset (but which isn't
                     * tracked)*/
                    logbits = bits & ~(UF_RESET | UF_RESET2);
                    bits = UF_RESET |
                           MSGFTE_DeltaCalcBits(
                               &EDICT_NUM(entnum)->baseline, &state->state);
                    //					Con_Printf("RESET2 %i @ %i\n", j,
                    // sequence);
                }
                else if(bits & UF_RESET)
                {
                    /*flag the entity for the next packet, so we always get two
                     * resets when it appears, to reduce the effects of
                     * packetloss on seeing rockets etc*/
                    client->pendingentities_bits[entnum] = UF_RESET2;
                    bits = UF_RESET |
                           MSGFTE_DeltaCalcBits(
                               &EDICT_NUM(entnum)->baseline, &state->state);
                    logbits = UF_RESET;
                    //					Con_Printf("RESET %i @ %i\n", j,
                    // sequence);
                }
                else
                {
                    logbits = bits;
                }

                if(entnum >= 0x4000)
                {
                    MSG_WriteShort(msg, 0x4000 | (entnum & 0x3fff));
                    MSG_WriteShort(msg, entnum >> 14);
                }
                else
                {
                    MSG_WriteShort(msg, entnum);
                }
                //				SV_EmitDeltaEntIndex(msg, j, false, true);
                MSGFTE_WriteEntityUpdate(bits, &state->state, msg,
                    client->protocol_pext2, sv.protocolflags);
            }
        }

        if((size_t)msg->cursize + 2 > origmaxsize)
        {
            msg->cursize = rollbacksize; // roll back
            client->pendingentities_bits[entnum] |=
                logbits; // make sure those bits get re-applied later.
            break;
        }
        if(frame->numents == frame->maxents)
        {
            frame->maxents += 64;
            frame->ents = (client_t::deltaframe_s::frameent_t*)realloc(
                frame->ents, sizeof(*frame->ents) * frame->maxents);
        }
        frame->ents[frame->numents].num = entnum;
        frame->ents[frame->numents].bits = logbits;
        frame->numents++;
    }
    msg->maxsize = origmaxsize;
    MSG_WriteShort(msg, 0); // eom

    // remember how far we got, so we can keep things flushed, instead of only
    // updating the first N entities.
    client->snapshotresume = (entnum < client->numpendingentities ? entnum : 0);


    if(msg->cursize > 1024 && dev_peakstats.packetsize <= 1024)
    {
        Con_DWarning(
            "%i byte packet exceeds standard limit of 1024.\n", msg->cursize);
    }
    dev_stats.packetsize = msg->cursize;
    dev_peakstats.packetsize = q_max(msg->cursize, dev_peakstats.packetsize);
}

/*
SV_BuildEntityState
copies edict state into a more compact entity_state_t with all the extension
fields etc sorted out and neatened up for network precision. note: ignores
viewmodelforclient and other client-specific stuff.
*/
void SV_BuildEntityState(edict_t* ent, entity_state_t* state)
{
    eval_t* val;
    state->eflags = 0;
    state->origin = ent->v.origin;
    state->angles = ent->v.angles;
    state->modelindex = ent->v.modelindex;
    state->frame = ent->v.frame;
    state->colormap = ent->v.colormap;
    state->skin = ent->v.skin;

    if((val = GetEdictFieldValue(ent, qcvm->extfields.scale)) && val->_float)
    {
        state->scale = val->_float * 16;
    }
    else
    {
        state->scale = 16;
    }

    if((val = GetEdictFieldValue(ent, qcvm->extfields.alpha)))
    {
        state->alpha = ENTALPHA_ENCODE(val->_float);
    }
    else
    {
        state->alpha = ent->alpha;
    }
    if((val = GetEdictFieldValue(ent, qcvm->extfields.colormod)) &&
        (val->vector[0] || val->vector[1] || val->vector[2]))
    {
        state->colormod[0] = val->vector[0] * 32;
        state->colormod[1] = val->vector[1] * 32;
        state->colormod[2] = val->vector[2] * 32;
    }
    else
    {
        state->colormod[0] = state->colormod[1] = state->colormod[2] = 32;
    }
    state->traileffectnum =
        GetEdictFieldValue(ent, qcvm->extfields.traileffectnum)->_float;
    state->emiteffectnum =
        GetEdictFieldValue(ent, qcvm->extfields.emiteffectnum)->_float;
    if((val = GetEdictFieldValue(ent, qcvm->extfields.tag_entity)) &&
        val->edict)
    {
        state->tagentity = NUM_FOR_EDICT(PROG_TO_EDICT(val->edict));
    }
    else
    {
        state->tagentity = 0;
    }
    if((val = GetEdictFieldValue(ent, qcvm->extfields.tag_index)))
    {
        state->tagindex = val->_float;
    }
    else
    {
        state->tagindex = 0;
    }
    state->effects = ent->v.effects;
    if((val = GetEdictFieldValue(ent, qcvm->extfields.modelflags)))
    {
        state->effects |= ((unsigned int)val->_float) << 24;
    }
    if(!ent->v.movetype || ent->v.movetype == MOVETYPE_STEP)
    {
        state->eflags |= EFLAGS_STEP;
    }

    state->pmovetype = 0;
    state->velocity[0] = state->velocity[1] = state->velocity[2] = 0;
}

byte* SV_FatPVS(const qvec3& org, qmodel_t* worldmodel);

static void SVFTE_BuildSnapshotForClient(client_t* client)
{
    unsigned int e, i;
    byte* pvs;
    qvec3 org;
    edict_t *ent, *parent;
    unsigned int maxentities = client->limit_entities;
    edict_t* clent = client->edict;
    eval_t* val;
    unsigned char eflags;
    int proged = EDICT_TO_PROG(clent);

    client_t::entity_num_state_s* ents = snapshot_entstate;
    size_t numents = 0;
    size_t maxents = snapshot_maxents;
    int emiteffect;

    // find the client's PVS
    org = clent->v.origin + clent->v.view_ofs;
    pvs = SV_FatPVS(org, qcvm->worldmodel);

    if(maxentities > (unsigned int)qcvm->num_edicts)
    {
        maxentities = (unsigned int)qcvm->num_edicts;
    }

    // send over all entities (excpet the client) that touch the pvs
    ent = NEXT_EDICT(qcvm->edicts);
    for(e = 1; e < maxentities; e++, ent = NEXT_EDICT(ent))
    {
        eflags = 0;
        emiteffect =
            GetEdictFieldValue(ent, qcvm->extfields.emiteffectnum)->_float;
        if(ent != clent) // clent is ALLWAYS sent
        {
            // ignore ents without visible models
            if((!ent->v.modelindex || !PR_GetString(ent->v.model)[0]) &&
                !emiteffect)
            {
                continue;
            }

            val = GetEdictFieldValue(ent, qcvm->extfields.viewmodelforclient);
            if(val && val->edict == proged)
            {
                eflags |= EFLAGS_VIEWMODEL;
            }
            else if(val && val->edict)
            {
                continue;
            }
            else
            {
                // attached entities should use the pvs of the parent rather
                // than the child (because the child will typically be bugging
                // out around '0 0 0', so won't be useful)
                parent = ent;
                while((val = GetEdictFieldValue(
                           parent, qcvm->extfields.tag_entity)) &&
                      val->edict)
                {
                    parent = PROG_TO_EDICT(val->edict);
                }
                if(parent->num_leafs)
                {
                    // ignore if not touching a PV leaf
                    for(i = 0; i < parent->num_leafs; i++)
                    {
                        if(pvs[parent->leafnums[i] >> 3] &
                            (1 << (parent->leafnums[i] & 7)))
                        {
                            break;
                        }
                    }

                    // ericw -- added ent->num_leafs < MAX_ENT_LEAFS condition.
                    //
                    // if ent->num_leafs == MAX_ENT_LEAFS, the ent is visible
                    // from too many leafs for us to say whether it's in the
                    // PVS, so don't try to vis cull it. this commonly happens
                    // with rotators, because they often have huge bboxes
                    // spanning the entire map, or really tall lifts, etc.
                    if(i == parent->num_leafs &&
                        parent->num_leafs < MAX_ENT_LEAFS)
                    {
                        continue; // not visible
                    }
                }
            }
        }

        // okay, we care about this entity.

        if(numents == maxents)
        {
            maxents += 64;
            ents = (client_t::entity_num_state_s*)realloc(
                ents, maxents * sizeof(*ents));
        }

        ents[numents].num = e;
        SV_BuildEntityState(ent, &ents[numents].state);
        if((unsigned int)ents[numents].state.modelindex >= client->limit_models)
            ents[numents].state.modelindex = 0;
        if(ent ==
            clent) // add velocity, but we only care for the local player
                   // (should add prediction for other entities some time too).
        {
            ents[numents].state.pmovetype =
                0; // ent->v.movetype;	//fixme: we don't do prediction, so
                   // don't tell the client that it can try
            if((int)ent->v.flags & FL_ONGROUND)
            {
                eflags |= EFLAGS_ONGROUND;
            }
            ents[numents].state.velocity[0] = ent->v.velocity[0] * 8;
            ents[numents].state.velocity[1] = ent->v.velocity[1] * 8;
            ents[numents].state.velocity[2] = ent->v.velocity[2] * 8;
        }
        else if(ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
        { // don't send invisible entities unless they
            // have effects
            continue;
        }
        val = GetEdictFieldValue(ent, qcvm->extfields.exteriormodeltoclient);
        if(val && val->edict == proged)
        {
            eflags |= EFLAGS_EXTERIORMODEL;
        }
        // EFLAGS_VIEWMODEL was handled above
        ents[numents].state.eflags |= eflags;

        numents++;
    }

    snapshot_entstate = ents;
    snapshot_numents = numents;
    snapshot_maxents = maxents;
}

void MSG_WriteStaticOrBaseLine(sizebuf_t* buf, int idx, entity_state_t* state,
    unsigned int protocol_pext2, unsigned int protocol,
    unsigned int protocolflags)
{
    int i;
    if(protocol_pext2 & PEXT2_REPLACEMENTDELTAS)
    {
        if(idx >= 0)
        {
            MSG_WriteByte(buf, svcfte_spawnbaseline2);
            MSG_WriteShort(buf, idx);
        }
        else
        {
            MSG_WriteByte(buf, svcfte_spawnstatic2);
        }

        MSGFTE_WriteEntityUpdate(MSGFTE_DeltaCalcBits(&nullentitystate, state),
            state, buf, protocol_pext2, protocolflags);
    }
    else
    {
        int bits = 0;
        if(protocol == PROTOCOL_VERSION_DP7)
        {
            if((state->modelindex & 0xFF00) || (state->frame & 0xFF00))
            {
                bits = B_LARGEMODEL | B_LARGEFRAME;
            }
            // no alpha etc, no idea how that's meant to work with static ents.
            if(idx >= 0)
            {
                MSG_WriteByte(
                    buf, bits ? svcdp_spawnbaseline2 : svc_spawnbaseline);
                MSG_WriteEntity(buf, idx, protocol_pext2);
            }
            else
            {
                MSG_WriteByte(buf, bits ? svcdp_spawnstatic2 : svc_spawnstatic);
            }
        }
        else
        {
            if(protocol == PROTOCOL_FITZQUAKE ||
                protocol ==
                    PROTOCOL_RMQ) // still want to send baseline in
                                  // PROTOCOL_NETQUAKE, so reset these values
            {
                if(state->modelindex & 0xFF00)
                {
                    bits |= B_LARGEMODEL;
                }
                if(state->frame & 0xFF00)
                {
                    bits |= B_LARGEFRAME;
                }
                if(state->alpha != ENTALPHA_DEFAULT)
                {
                    bits |= B_ALPHA;
                }
            }
            if(idx >= 0)
            {
                MSG_WriteByte(
                    buf, bits ? svc_spawnbaseline2 : svc_spawnbaseline);
                MSG_WriteEntity(buf, idx, protocol_pext2);
            }
            else
            {
                MSG_WriteByte(buf, bits ? svc_spawnstatic2 : svc_spawnstatic);
            }


            if(bits)
            {
                MSG_WriteByte(buf, bits);
            }
        }

        if((bits & B_LARGEMODEL) || protocol == PROTOCOL_VERSION_BJP3)
        {
            MSG_WriteShort(buf, state->modelindex);
        }
        else
        {
            MSG_WriteByte(buf, state->modelindex);
        }

        if(bits & B_LARGEFRAME)
        {
            MSG_WriteShort(buf, state->frame);
        }
        else
        {
            MSG_WriteByte(buf, state->frame);
        }

        MSG_WriteByte(buf, state->colormap);
        MSG_WriteByte(buf, state->skin);
        for(i = 0; i < 3; i++)
        {
            MSG_WriteCoord(buf, state->origin[i], protocolflags);
            MSG_WriteAngle(buf, state->angles[i], protocolflags);
        }
        if(bits & B_ALPHA)
        {
            MSG_WriteByte(buf, state->alpha);
        }
    }
}


static void SV_Pext_f();

/*
===============
SV_Protocol_f
===============
*/
static void SV_Protocol_f()
{
    int i;
    const char* s;
    int prot, pext2;

    prot = sv_protocol;
    pext2 = sv_protocol_pext2;

    switch(Cmd_Argc())
    {
        case 1:
            //"FTE+15" or "15", just to be explicit about it
            Con_Printf("\"sv_protocol\" is \"%s%i\"\n",
                sv_protocol_pext2 ? "fte" : "", sv_protocol);
            break;
        case 2:
            s = Cmd_Argv(1);
            if(!q_strncasecmp(s, "FTE", 3))
            {
                s += 3;
                if(*s == '+' || *s == '-')
                {
                    s++;
                }
                pext2 = PEXT2_SUPPORTED_SERVER;
            }
            else if(!q_strncasecmp(s, "+", 3))
            {
                s += 1;
                pext2 = PEXT2_SUPPORTED_SERVER;
            }
            else if(!q_strncasecmp(s, "Base", 4))
            {
                s += 4;
                if(*s == '+' || *s == '-')
                {
                    s++;
                }
                pext2 = 0;
            }
            else if(*s == '-')
            {
                s++;
                pext2 = 0;
            }

            i = strtol(s, (char**)&s, 0);
            if(*s == '-')
            {
                pext2 = 0;
            }
            else if(*s == '+')
            {
                pext2 = PEXT2_SUPPORTED_SERVER;
            }

            if(i != PROTOCOL_QUAKEVR)
            {
                Con_Printf("sv_protocol must be %i", PROTOCOL_QUAKEVR);
            }
            else
            {
                sv_protocol = i;
                sv_protocol_pext2 = pext2;
                if(sv.active)
                {
                    if(prot == sv_protocol && pext2 == sv_protocol_pext2)
                    {
                        Con_Printf("specified protocol already active.\n");
                    }
                    else
                    {
                        Con_Printf(
                            "changes will not take effect until the next level "
                            "load.\n");
                    }
                }
            }
            break;
        default: Con_SafePrintf("usage: sv_protocol <protocol>\n"); break;
    }
}

/*
===============
SV_Init
===============
*/
void SV_Init()
{
    int i;
    const char* p;
    extern cvar_t sv_maxvelocity;
    extern cvar_t sv_gravity;
    extern cvar_t sv_nostep;
    extern cvar_t sv_freezenonclients;
    extern cvar_t sv_gameplayfix_spawnbeforethinks;
    extern cvar_t
        sv_gameplayfix_setmodelrealbox; // spike: 1 to replicate a quakespasm
                                        // bug, 0 for actual vanilla compat.
    extern cvar_t sv_friction;
    extern cvar_t sv_edgefriction;
    extern cvar_t sv_stopspeed;
    extern cvar_t sv_maxspeed;
    extern cvar_t sv_accelerate;
    extern cvar_t sv_idealpitchscale;
    extern cvar_t sv_aim;
    extern cvar_t sv_altnoclip;        // johnfitz
    extern cvar_t sv_public;           // spike
    extern cvar_t sv_reportheartbeats; // spike
    extern cvar_t com_protocolname;    // spike
    extern cvar_t net_masters[];       // spike
    extern cvar_t rcon_password;       // spike, proquake-compatible rcon
    extern cvar_t
        sv_sound_watersplash;    // spike - making these changable is handy...
    extern cvar_t sv_sound_land; // spike - and also mutable...


    Cvar_RegisterVariable(&sv_maxvelocity);
    Cvar_RegisterVariable(&sv_gravity);
    Cvar_RegisterVariable(&sv_friction);
    Cvar_SetCallback(&sv_gravity, Host_Callback_Notify);
    Cvar_SetCallback(&sv_friction, Host_Callback_Notify);
    Cvar_RegisterVariable(&sv_edgefriction);
    Cvar_RegisterVariable(&sv_stopspeed);
    Cvar_RegisterVariable(&sv_maxspeed);
    Cvar_SetCallback(&sv_maxspeed, Host_Callback_Notify);
    Cvar_RegisterVariable(&sv_accelerate);
    Cvar_RegisterVariable(&sv_idealpitchscale);
    Cvar_RegisterVariable(&sv_aim);
    Cvar_RegisterVariable(&sv_nostep);
    Cvar_RegisterVariable(&sv_freezenonclients);
    Cvar_RegisterVariable(&sv_gameplayfix_spawnbeforethinks);
    Cvar_RegisterVariable(&sv_gameplayfix_setmodelrealbox);
    Cvar_RegisterVariable(&pr_checkextension);
    Cvar_RegisterVariable(&sv_altnoclip); // johnfitz

    Cvar_RegisterVariable(&sv_sound_watersplash); // spike
    Cvar_RegisterVariable(&sv_sound_land);        // spike

    if(isDedicated)
    {
        sv_public.string = "1";
    }
    else
    {
        sv_public.string = "0";
    }
    Cvar_RegisterVariable(&sv_public);
    Cvar_RegisterVariable(&sv_reportheartbeats);
    Cvar_RegisterVariable(&com_protocolname);
    for(i = 0; net_masters[i].name; i++)
    {
        Cvar_RegisterVariable(&net_masters[i]);
    }

    Cvar_RegisterVariable(&rcon_password);

    Cmd_AddCommand_ClientCommand("pext", SV_Pext_f);
    Cmd_AddCommand("sv_protocol", &SV_Protocol_f); // johnfitz

    for(i = 0; i < MAX_MODELS; i++)
    {
        sprintf(localmodels[i], "*%i", i);
    }

    i = COM_CheckParm("-protocol");
    if(i && i < com_argc - 1)
    {
        sv_protocol = atoi(com_argv[i + 1]);
    }
    switch(sv_protocol)
    {
        case PROTOCOL_QUAKEVR: p = "Quake VR"; break;
        default:
            Sys_Error("Bad protocol version request %i. Accepted values: %i.",
                sv_protocol, PROTOCOL_QUAKEVR);
            return; /* silence compiler */
    }
    Sys_Printf("Server using protocol %i (%s)\n", sv_protocol, p);

    SV_VoiceInit();
}

/*
=============================================================================

EVENT MESSAGES

=============================================================================
*/

static void writeCommonParticleData(const qvec3& org, const qvec3& dir)
{
    MSG_WriteVec3(&sv.datagram, org, sv.protocolflags);
    for(int i = 0; i < 3; i++)
    {
        int v = dir[i] * 16;
        if(v > 127)
        {
            v = 127;
        }
        else if(v < -128)
        {
            v = -128;
        }
        MSG_WriteChar(&sv.datagram, v);
    }
}

/*
==================
SV_StartParticle

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle(
    const qvec3& org, const qvec3& dir, const int color, const int count)
{
    if(sv.datagram.cursize > MAX_DATAGRAM - 16)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, svc_particle);
    writeCommonParticleData(org, dir);
    MSG_WriteByte(&sv.datagram, count);
    MSG_WriteByte(&sv.datagram, color);
}

/*
==================
SV_StartParticle2

Make sure the event gets sent to all clients
==================
*/
void SV_StartParticle2(
    const qvec3& org, const qvec3& dir, const int preset, const int count)
{
    if(sv.datagram.cursize > MAX_DATAGRAM - 16)
    {
        return;
    }

    MSG_WriteByte(&sv.datagram, svc_particle2);
    writeCommonParticleData(org, dir);
    MSG_WriteByte(&sv.datagram, preset);
    MSG_WriteShort(&sv.datagram, count);
}

/*
==================
SV_StartSound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
allready running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)

==================
*/
void SV_StartSound(edict_t* entity, const qvec3* origin, int channel,
    const char* sample, int volume, float attenuation)
{
    unsigned int sound_num, ent;
    int i, field_mask;
    int p;
    client_t* cl;

    if(volume < 0)
    {
        Host_Error("SV_StartSound: volume = %i", volume);
    }
    else if(volume > 255)
    {
        volume = 255;
        Con_Printf("SV_StartSound: volume = %i\n", volume);
    }

    if(attenuation < 0 || attenuation > 4)
    {
        Host_Error("SV_StartSound: attenuation = %f", attenuation);
    }

    if(channel < 0 || channel > 255)
    {
        Host_Error("SV_StartSound: channel = %i", channel);
    }
    else if(channel > 7)
    {
        Con_DPrintf("SV_StartSound: channel = %i\n", channel);
    }

    if(sv.datagram.cursize > MAX_DATAGRAM - 16)
    {
        return;
    }

    // find precache number for sound
    for(sound_num = 1; sound_num < MAX_SOUNDS && sv.sound_precache[sound_num];
        sound_num++)
    {
        if(!strcmp(sample, sv.sound_precache[sound_num]))
        {
            break;
        }
    }

    if(sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num])
    {
        Con_Printf("SV_StartSound: %s not precacheed\n", sample);
        return;
    }

    ent = NUM_FOR_EDICT(entity);

    field_mask = 0;
    if(volume != DEFAULT_SOUND_PACKET_VOLUME)
    {
        field_mask |= SND_VOLUME;
    }
    if(attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
    {
        field_mask |= SND_ATTENUATION;
    }

    // johnfitz -- PROTOCOL_FITZQUAKE
    if(ent >= 8192 || channel >= 8)
    {
        field_mask |= SND_LARGEENTITY;
    }
    if(sound_num >= 256)
    {
        field_mask |= SND_LARGESOUND;
    }
    // johnfitz

    for(p = 0; p < svs.maxclients; p++)
    {
        cl = &svs.clients[p];
        if(!cl->active || !cl->spawned)
        {
            continue;
        }

        if(ent >= cl->limit_entities)
        {
            continue;
        }

        if(sound_num >= cl->limit_sounds)
        {
            continue;
        }

        /*
        if((field_mask & (SND_LARGEENTITY | SND_LARGESOUND)) &&
            (!cl->protocol_pext2 || sv.protocol == PROTOCOL_NETQUAKE))
        {
            continue;
        }
        */

        // directed messages go only to the entity the are targeted on
        MSG_WriteByte(&cl->datagram, svc_sound);
        MSG_WriteByte(&cl->datagram, field_mask);
        if(field_mask & SND_VOLUME)
        {
            MSG_WriteByte(&cl->datagram, volume);
        }
        if(field_mask & SND_ATTENUATION)
        {
            MSG_WriteByte(&cl->datagram, attenuation * 64);
        }

        // johnfitz -- PROTOCOL_FITZQUAKE
        if(field_mask & SND_LARGEENTITY)
        {
            // if((cl->protocol_pext2 & PEXT2_REPLACEMENTDELTAS) && ent >
            // 0x7fff)
            if(ent > 0x7fff)
            {
                MSG_WriteShort(&cl->datagram, (ent >> 8) | 0x8000);
                MSG_WriteByte(&cl->datagram, ent & 0xff);
            }
            else
            {
                MSG_WriteShort(&cl->datagram, ent);
            }
            MSG_WriteByte(&cl->datagram, channel);
        }
        else
        {
            MSG_WriteShort(&cl->datagram, (ent << 3) | channel);
        }
        // if((field_mask & SND_LARGESOUND) ||
        //     sv.protocol == PROTOCOL_VERSION_BJP3)
        if(field_mask & SND_LARGESOUND)
        {
            MSG_WriteShort(&cl->datagram, sound_num);
        }
        else
        {
            MSG_WriteByte(&cl->datagram, sound_num);
        }
        // johnfitz

        if(origin)
        {
            MSG_WriteVec3(&cl->datagram, *origin, sv.protocolflags);
        }
        else
        {
            MSG_WriteVec3(&cl->datagram,
                entity->v.origin + 0.5f * (entity->v.mins + entity->v.maxs),
                sv.protocolflags);
        }
    }
}

/*
==============================================================================

CLIENT SPAWNING

==============================================================================
*/

/*
================
SV_SendServerinfo

Sends the first message from the server to a connected client.
This will be sent on the initial connection and upon each server load.
================
*/
void SV_SendServerinfo(client_t* client)
{
    const char** s;
    char message[4096];
    unsigned int i; // johnfitz
    bool cantruncate;

    SV_VoiceInitClient(client);

    client->spawned = false; // need prespawn, spawn, etc

    // assume some safe defaults if we early out.
    client->limit_unreliable = 1024;
    client->limit_reliable = 8192;
    client->limit_entities = 0;
    client->limit_models = 0;
    client->limit_sounds = 0;

    if(!sv_protocol_pext2)
    {
        // server disabled pext completely, don't bother trying.
        // make sure we try reenabling it again on the next map though. mwahaha.
        client->pextknown = false;
    }
    else if(!client->pextknown)
    {
        MSG_WriteByte(&client->message, svc_stufftext);
        MSG_WriteString(&client->message, "cmd pext\n");
        client->sendsignon = true;
        return;
    }
    client->protocol_pext2 &= sv_protocol_pext2;

    if(!(client->protocol_pext2 & PEXT2_REPLACEMENTDELTAS))
    {
        client->protocol_pext2 &=
            ~PEXT2_PREDINFO; // stats can't be deltaed if there's no deltas, so
    }
    // just pretend its not supported on its own.

    client->limit_entities =
        (sv_protocol_pext2 &&
            NET_QSocketGetProQuakeAngleHack(client->netconnection))
            ? 2048
            : 600; // vanilla sucks. proquake supports more so assume we can use
                   // that limit if angles are also available (but only if we're
                   // allowing other non-vanilla extensions)
    client->limit_models = 256; // single byte
    client->limit_sounds = 256; // single byte

    // now we know their protocol, pick some real defaults
    if(sv.protocol != PROTOCOL_NETQUAKE || client->protocol_pext2)
    {
        client->limit_unreliable =
            DATAGRAM_MTU; // some safe ethernet limit. these clients should
                          // accept pretty much anything, but any routers will
                          // not.
        client->limit_reliable = MAX_DATAGRAM; // quite large, ip allows 16 bits
        client->limit_entities =
            qcvm->max_edicts; // we don't really know, 8k is probably a save
                              // guess but could be 32k, 65k, or even more...
        client->limit_models =
            MAX_MODELS; // not really sure, client's problem until >14bits
        client->limit_sounds =
            MAX_SOUNDS; // not really sure, client's problem until >14bits

        if(!Q_strcmp(
               NET_QSocketGetTrueAddressString(client->netconnection), "LOCAL"))
        { // override some other limits for localhost, because we can probably
          // get away with it.
            // only do this if we're using extensions, so we don't break demos
            client->limit_unreliable = client->limit_reliable = MAX_DATAGRAM;
        }
    }
    if(client->limit_entities > 0x8000 &&
        !(client->protocol_pext2 & PEXT2_REPLACEMENTDELTAS))
    {
        client->limit_entities =
            0x8000; // pext2 changes the encoding of entities to support 23 bits
    }
    // instead of dpp7's 15bits or vanilla's 16bits, but our
    // writeentity is lazy.

    // unfortunately we can't split this up, so if its oversized, we'll just let
    // the client complain instead of always kicking them
    client->message.maxsize = sizeof(client->msgbuf);
    if(client->message.maxsize > (int)client->limit_reliable)
    {
        client->message.maxsize = client->limit_reliable;
    }

    NET_QSocketSetMSS(client->netconnection, client->limit_unreliable);

    if(client->message.cursize)
    { // try and flush the reliable NOW, in case the qc is evil
        if(NET_CanSendMessage(host_client->netconnection))
        {
            if(NET_SendMessage(
                   host_client->netconnection, &host_client->message) != -1)
            {
                SZ_Clear(&host_client->message);
                host_client->last_message = realtime;
            }
        }
    }

    cantruncate = client->message.cursize == 0;

retry:
    MSG_WriteByte(&client->message, svc_print);

    sprintf(message, "%c\nQUAKE VR %s SERVER (%i CRC)\n", 2, QUAKEVR_VERSION,
        qcvm->crc); // johnfitz -- include fitzquake version

    MSG_WriteString(&client->message, message);

    //	lack of serverinfo means any csqc info might as well be sent the lame dp
    // way
    /*	if (sv.csqc_progsize)
        {
            MSG_WriteByte (&client->netconnection->message, svc_stufftext);
            MSG_WriteString (&client->netconnection->message, va("csqc_progname
       %s\n", sv.csqc_progname)); MSG_WriteByte
       (&client->netconnection->message, svc_stufftext); MSG_WriteString
       (&client->netconnection->message, va("csqc_progsize %i\n",
       sv.csqc_progsize)); MSG_WriteByte (&client->netconnection->message,
       svc_stufftext); MSG_WriteString (&client->netconnection->message,
       va("csqc_progcrc %i\n", sv.csqc_progcrc));
        }*/
    // let clients know that we support downloads
    // if (client->protocol_pext2 || sv.protocol == PROTOCOL_VERSION_DP7)
    {
        MSG_WriteByte(&client->message, svc_stufftext);
        MSG_WriteString(&client->message, "cl_serverextension_download 1\n");
    }

    MSG_WriteByte(&client->message, svc_serverinfo);

    if(client->protocol_pext2)
    { // pext stuff takes the form of modifiers to an underlaying protocol
        MSG_WriteLong(&client->message, PROTOCOL_FTE_PEXT2);
        MSG_WriteLong(&client->message,
            client->protocol_pext2); // active extensions that the client needs
                                     // to look out for
    }
    MSG_WriteLong(&client->message,
        sv.protocol); // johnfitz -- sv.protocol instead of PROTOCOL_VERSION
    if(sv.protocol == PROTOCOL_RMQ)
    {
        // mh - now send protocol flags so that the client knows the protocol
        // features to expect
        MSG_WriteLong(&client->message, sv.protocolflags);
    }

    if(client->protocol_pext2 & PEXT2_PREDINFO)
    {
        // if multiple gamedirs were used, we should list all the active ones
        // eg: "id1;hipnotic;rogue;quoth;mod". fixme: engine-specific forced
        // gamedirs like id1/ or qw/ or fte/ are redundant, so don't bother
        // listing them we don't really track that stuff, so I'm just going to
        // report the last one
        MSG_WriteString(&client->message, COM_GetGameNames(false));
    }

    MSG_WriteByte(&client->message, svs.maxclients);

    if(!coop.value && deathmatch.value)
    {
        MSG_WriteByte(&client->message, GAME_DEATHMATCH);
    }
    else
    {
        MSG_WriteByte(&client->message, GAME_COOP);
    }

    MSG_WriteString(&client->message, PR_GetString(qcvm->edicts->v.message));

    // johnfitz -- only send the first 256 model and sound precaches if protocol
    // is 15
    for(i = 1, s = sv.model_precache + 1; *s && i < client->limit_models;
        s++, i++)
    {
        MSG_WriteString(&client->message, *s);
    }
    MSG_WriteByte(&client->message, 0);

    for(i = 1, s = sv.sound_precache + 1; *s && i < client->limit_sounds;
        s++, i++)
    {
        MSG_WriteString(&client->message, *s);
    }
    MSG_WriteByte(&client->message, 0);
    // johnfitz

    // send music
    MSG_WriteByte(&client->message, svc_cdtrack);
    MSG_WriteByte(&client->message, qcvm->edicts->v.sounds);
    MSG_WriteByte(&client->message, qcvm->edicts->v.sounds);

    // set view
    MSG_WriteByte(&client->message, svc_setview);
    MSG_WriteShort(&client->message, NUM_FOR_EDICT(client->edict));

    MSG_WriteByte(&client->message, svc_signonnum);
    MSG_WriteByte(&client->message, 1);

    client->sendsignon = true;

    SVFTE_SetupFrames(client);

    if(client->message.overflowed && client->limit_sounds > 64 && cantruncate)
    {
        if(client->limit_models > client->limit_sounds)
        {
            client->limit_models /= 2;
        }
        else
        {
            client->limit_sounds /= 2;
        }
        SZ_Clear(&client->message);
        Con_Printf("Serverinfo too large for %s, truncating.\n",
            NET_QSocketGetTrueAddressString(client->netconnection));
        goto retry;
    }

    // try and flush the reliable NOW, in case the qc is evil
    if(NET_CanSendMessage(client->netconnection))
    {
        if(NET_SendMessage(client->netconnection, &client->message) != -1)
        {
            SZ_Clear(&client->message);
            client->last_message = realtime;
            client->sendsignon = false;
        }
    }

    // protocol 15 is too limited. let people know that they'll get a crappy
    // experience.
    if(client->limit_entities <= (unsigned)qcvm->num_edicts)
    {
        Con_Warning("Protocol limitation (entities) for %s\n",
            NET_QSocketGetTrueAddressString(client->netconnection));
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteByte(&client->message, 2);
        MSG_WriteString(&client->message, "WARNING: ");
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteString(&client->message,
            "The protocol in use is too limited. You will not be able to see "
            "all entities\n");
    }
    if(client->limit_models < MAX_MODELS &&
        sv.model_precache[client->limit_models])
    {
        Con_Warning("Protocol limitation (models) for %s\n",
            NET_QSocketGetTrueAddressString(client->netconnection));
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteByte(&client->message, 2);
        MSG_WriteString(&client->message, "WARNING: ");
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteString(&client->message,
            "The protocol in use is too limited. You will not be able to see "
            "all models\n");
    }
    if(client->limit_sounds < MAX_SOUNDS &&
        sv.sound_precache[client->limit_sounds])
    {
        Con_Warning("Protocol limitation (sounds) for %s\n",
            NET_QSocketGetTrueAddressString(client->netconnection));
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteByte(&client->message, 2);
        MSG_WriteString(&client->message, "WARNING: ");
        MSG_WriteByte(&client->message, svc_print);
        MSG_WriteString(&client->message,
            "The protocol in use is too limited. You will not be able to hear "
            "all sounds\n");
    }
}

void SV_Pext_f()
{
    // this only makes sense on the server. the clientside part only takes the
    // form of 'cmd pext', for compat with clients that don't support this.
    if(cmd_source == src_command)
    {
        if(!cls.state)
        {
            Con_Printf("Not connected\n");
            return;
        }
        Con_Printf("Current Protocols:\n");
        if(cl.protocol_pext2 & PEXT2_REPLACEMENTDELTAS)
        {
            Con_Printf("  Replacement Entity Deltas\n");
        }
        if(cl.protocol_pext2 & PEXT2_PREDINFO)
        {
            Con_Printf("  Replacement Stats ('predinfo')\n");
        }
        if(cl.protocol == PROTOCOL_NETQUAKE)
        {
            Con_Printf("  vanilla(15)\n");
        }
        else if(cl.protocol == PROTOCOL_FITZQUAKE)
        {
            Con_Printf("  fitzquake(666)\n");
        }
        else if(cl.protocol == PROTOCOL_RMQ)
        {
            Con_Printf("  rmq(999)\n");
        }
        else if(cl.protocol == PROTOCOL_QUAKEVR)
        {
            Con_Printf("  quakevr(8682)\n");
        }
        else
        {
            Con_Printf("  unknown protocol(%i)\n", cl.protocol);
        }
        return;
    }

    if(!host_client->pextknown && !host_client->spawned)
    {
        int i;
        int key;
        int value;
        for(i = 1; i < Cmd_Argc(); i += 2)
        {
            key = strtoul(Cmd_Argv(i), nullptr, 0);
            value = strtoul(Cmd_Argv(i + 1), nullptr, 0);

            if(key == PROTOCOL_FTE_PEXT2)
            {
                host_client->protocol_pext2 = value & PEXT2_SUPPORTED_SERVER;
            }
            // else some other extension that we don't know
        }

        host_client->pextknown = true;
        SV_SendServerinfo(host_client);
    }
}

/*
================
SV_ConnectClient

Initializes a client_t for a new net connection.  This will only be called
once for a player each game, not once for each level change.
================
*/
void SV_ConnectClient(int clientnum)
{
    float spawn_parms[NUM_TOTAL_SPAWN_PARMS];

    client_t* client = svs.clients + clientnum;

    // QSS
    if(client->netconnection)
    {
        Con_DPrintf("Client %s connected\n",
            NET_QSocketGetTrueAddressString(client->netconnection));
    }
    else
    {
        Con_DPrintf("Bot connected\n");
    }

    int edictnum = clientnum + 1;

    edict_t* ent = EDICT_NUM(edictnum);

    // set up the client_t
    struct qsocket_s* netconnection = client->netconnection;
    ++net_activeconnections;

    if(sv.loadgame)
    {
        memcpy(spawn_parms, client->spawn_parms, sizeof(spawn_parms));
    }

    memset(client, 0, sizeof(*client));
    client->netconnection = netconnection;

    strcpy(client->name, "unconnected");
    client->active = true;
    client->spawned = false;
    client->edict = ent;
    client->message.data = client->msgbuf;
    client->message.maxsize = sizeof(client->msgbuf);
    client->message.allowoverflow = true; // we can catch it

    // QSS
    client->datagram.data = client->datagram_buf;
    client->datagram.maxsize = sizeof(client->datagram_buf);
    client->datagram.allowoverflow = true; // simply ignored on overflow

    client->pextknown = false;
    client->protocol_pext2 = 0;

    if(sv.loadgame)
    {
        memcpy(client->spawn_parms, spawn_parms, sizeof(spawn_parms));
    }
    else
    {
        // call the progs to get default spawn parms for the new client
        PR_ExecuteProgram(pr_global_struct->SetNewParms);

        int i;
        for(i = 0; i < NUM_BASIC_SPAWN_PARMS; i++) // QSS
        {
            client->spawn_parms[i] = (&pr_global_struct->parm1)[i];
        }
        for(; i < NUM_TOTAL_SPAWN_PARMS; i++) // QSS
        {
            ddef_t* g = ED_FindGlobal(va("parm%i", i + 1));
            client->spawn_parms[i] = g ? qcvm->globals[g->ofs] : 0;
        }
    }

    SV_SendServerinfo(client);
}


/*
===================
SV_CheckForNewClients

===================
*/
void SV_CheckForNewClients()
{
    //
    // check for new connections
    //
    while(true)
    {
        struct qsocket_s* ret = NET_CheckNewConnections();
        if(!ret)
        {
            break;
        }

        //
        // init a new client structure
        //
        int i;
        for(i = 0; i < svs.maxclients; i++)
        {
            if(!svs.clients[i].active)
            {
                break;
            }
        }

        if(i == svs.maxclients)
        {
            Sys_Error("Host_CheckForNewClients: no free clients");
        }

        svs.clients[i].netconnection = ret;
        SV_ConnectClient(i);
    }
}


/*
===============================================================================

FRAME UPDATES

===============================================================================
*/

/*
==================
SV_ClearDatagram

==================
*/
void SV_ClearDatagram()
{
    SZ_Clear(&sv.datagram);
}

/*
=============================================================================

The PVS must include a small area around the client to allow head bobbing
or other small motion on the client side.  Otherwise, a bob might cause an
entity that should be visible to not show up, especially when the bob
crosses a waterline.

=============================================================================
*/

static int fatbytes;
static byte* fatpvs;
static int fatpvs_capacity;

void SV_AddToFatPVS(const qvec3& org, mnode_t* node,
    qmodel_t* worldmodel) // johnfitz -- added worldmodel as a parameter
{
    int i;
    byte* pvs;
    mplane_t* plane;
    float d;

    while(true)
    {
        // if this is a leaf, accumulate the pvs bits
        if(node->contents < 0)
        {
            if(node->contents != CONTENTS_SOLID)
            {
                pvs = Mod_LeafPVS((mleaf_t*)node,
                    worldmodel); // johnfitz -- worldmodel as a parameter
                for(i = 0; i < fatbytes; i++)
                {
                    fatpvs[i] |= pvs[i];
                }
            }
            return;
        }

        plane = node->plane;
        d = DotProduct(org, plane->normal) - plane->dist;
        if(d > 8)
        {
            node = node->children[0];
        }
        else if(d < -8)
        {
            node = node->children[1];
        }
        else
        {
            // go down both
            SV_AddToFatPVS(org, node->children[0],
                worldmodel); // johnfitz -- worldmodel as a parameter
            node = node->children[1];
        }
    }
}

/*
=============
SV_FatPVS

Calculates a PVS that is the inclusive or of all leafs within 8 pixels of
the given point.
=============
*/
byte* SV_FatPVS(const qvec3& org,
    qmodel_t* worldmodel) // johnfitz -- added worldmodel as a parameter
{
    fatbytes = (worldmodel->numleafs + 7) >>
               3; // ericw -- was +31, assumed to be a bug/typo
    if(fatpvs == nullptr || fatbytes > fatpvs_capacity)
    {
        fatpvs_capacity = fatbytes;
        fatpvs = (byte*)realloc(fatpvs, fatpvs_capacity);
        if(!fatpvs)
        {
            Sys_Error(
                "SV_FatPVS: realloc() failed on %d bytes", fatpvs_capacity);
        }
    }

    Q_memset(fatpvs, 0, fatbytes);
    SV_AddToFatPVS(org, worldmodel->nodes,
        worldmodel); // johnfitz -- worldmodel as a parameter
    return fatpvs;
}

/*
=============
SV_VisibleToClient -- johnfitz

PVS test encapsulated in a nice function
=============
*/
bool SV_VisibleToClient(edict_t* client, edict_t* test, qmodel_t* worldmodel)
{
    byte* pvs;
    qvec3 org;
    int i;

    org = client->v.origin + client->v.view_ofs;
    pvs = SV_FatPVS(org, worldmodel);

    for(i = 0; i < test->num_leafs; i++)
    {
        if(pvs[test->leafnums[i] >> 3] & (1 << (test->leafnums[i] & 7)))
        {
            return true;
        }
    }

    return false;
}

//=============================================================================

/*
=============
SV_WriteEntitiesToClient

=============
*/
void SV_WriteEntitiesToClient(client_t* client, sizebuf_t* msg)
{
    const unsigned int maxedict = std::min(
        static_cast<unsigned int>(qcvm->num_edicts), client->limit_entities);

    // try to avoid sounds getting lost. flickering entities are weird, but
    // missing sounds+particles are just eerie.
    const int maxsize =
        msg->maxsize - client->datagram.cursize - sv.datagram.cursize;

    // find the client's PVS
    const edict_t* clent = client->edict;
    const qvec3 org = clent->v.origin + clent->v.view_ofs;
    const byte* pvs = SV_FatPVS(org, qcvm->worldmodel);

    // send over all entities (excpet the client) that touch the pvs
    edict_t* ent = NEXT_EDICT(qcvm->edicts);
    for(unsigned int e = 1; e < maxedict; e++, ent = NEXT_EDICT(ent))
    {
        if(ent != clent) // `clent` is ALWAYS sent
        {
            // ignore ents without visible models
            if(!ent->v.modelindex || !PR_GetString(ent->v.model)[0])
            {
                continue;
            }

            /*
            // johnfitz -- don't send model>255 entities if protocol is 15
            if((unsigned int)ent->v.modelindex >= client->limit_models)
            {
                continue;
            }
            */

            // ignore if not touching a PV leaf
            unsigned int i = 0;
            for(; i < ent->num_leafs; i++)
            {
                if(pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
                {
                    break;
                }
            }

            // ericw -- added ent->num_leafs < MAX_ENT_LEAFS
            // condition.
            //
            // if ent->num_leafs == MAX_ENT_LEAFS, the ent is
            // visible from too many leafs for us to say whether
            // it's in the PVS, so don't try to vis cull it. this
            // commonly happens with rotators, because they often
            // have huge bboxes spanning the entire map, or really
            // tall lifts, etc.
            if(i == ent->num_leafs && ent->num_leafs < MAX_ENT_LEAFS)
            {
                continue; // not visible
            }
        }

        // johnfitz -- max size for protocol 15 is 18 bytes, not 16 as
        // originally assumed here.  And, for protocol 85 the max size is
        // actually 24 bytes.
        if(msg->cursize + 24 > maxsize)
        {
            // johnfitz -- less spammy overflow message
            if(!dev_overflows.packetsize ||
                dev_overflows.packetsize + CONSOLE_RESPAM_TIME < realtime)
            {
                Con_Printf("Packet overflow!\n");
                dev_overflows.packetsize = realtime;
            }

            goto stats;
            // johnfitz
        }

        // don't send invisible entities unless they have effects
        if(ent->alpha == ENTALPHA_ZERO && !ent->v.effects)
        {
            continue;
        }
        // johnfitz

        // send an update
        int bits = 0;

        for(int i = 0; i < 3; i++)
        {
            float miss = ent->v.origin[i] - ent->baseline.origin[i];
            if(miss < -0.1 || miss > 0.1)
            {
                bits |= U_ORIGIN1 << i;
            }
        }

        if(ent->v.angles[0] != ent->baseline.angles[0])
        {
            bits |= U_ANGLE1;
        }

        if(ent->v.angles[1] != ent->baseline.angles[1])
        {
            bits |= U_ANGLE2;
        }

        if(ent->v.angles[2] != ent->baseline.angles[2])
        {
            bits |= U_ANGLE3;
        }

        if(ent->v.model_scale != ent->baseline.model_scale)
        {
            bits |= U_SCALE;
        }

        if(ent->v.model_scale_origin != ent->baseline.model_scale_origin)
        {
            bits |= U_SCALE;
        }

        if(ent->v.movetype == MOVETYPE_STEP)
        {
            bits |= U_STEP; // don't mess up the step animation
        }

        if(ent->baseline.colormap != ent->v.colormap)
        {
            bits |= U_COLORMAP;
        }

        if(ent->baseline.skin != ent->v.skin)
        {
            bits |= U_SKIN;
        }

        if(ent->baseline.frame != ent->v.frame)
        {
            bits |= U_FRAME;
        }

        if(ent->baseline.effects != ent->v.effects)
        {
            bits |= U_EFFECTS;
        }

        if(ent->baseline.modelindex != ent->v.modelindex)
        {
            bits |= U_MODEL;
        }

        // johnfitz -- alpha
        // TODO: find a cleaner place to put this code
        if(eval_t* val = GetEdictFieldValue(ent, qcvm->extfields.alpha))
        {
            ent->alpha = ENTALPHA_ENCODE(val->_float);
        }

        // johnfitz -- PROTOCOL_QUAKEVR
        if(ent->baseline.alpha != ent->alpha)
        {
            bits |= U_ALPHA;
        }

        if(ent->baseline.model_scale != ent->v.model_scale)
        {
            bits |= U_SCALE;
        }

        if(ent->baseline.model_offset != ent->v.model_offset)
        {
            bits |= U_MODELOFFSET;
        }

        if(bits & U_FRAME && (int)ent->v.frame & 0xFF00)
        {
            bits |= U_FRAME2;
        }

        if(bits & U_MODEL && (int)ent->v.modelindex & 0xFF00)
        {
            bits |= U_MODEL2;
        }

        if(ent->sendinterval)
        {
            bits |= U_LERPFINISH;
        }

        if(bits >= 65536)
        {
            bits |= U_EXTEND1;
        }

        if(bits >= 16777216)
        {
            bits |= U_EXTEND2;
        }

        if(e >= 256)
        {
            bits |= U_LONGENTITY;
        }

        if(bits >= 256)
        {
            bits |= U_MOREBITS;
        }

        //
        // write the message
        //
        MSG_WriteByte(msg, bits | U_SIGNAL);

        if(bits & U_MOREBITS)
        {
            MSG_WriteByte(msg, bits >> 8);
        }

        // johnfitz -- PROTOCOL_QUAKEVR
        if(bits & U_EXTEND1)
        {
            MSG_WriteByte(msg, bits >> 16);
        }

        if(bits & U_EXTEND2)
        {
            MSG_WriteByte(msg, bits >> 24);
        }
        // johnfitz

        if(bits & U_LONGENTITY)
        {
            MSG_WriteShort(msg, e);
        }
        else
        {
            MSG_WriteByte(msg, e);
        }

        if(bits & U_MODEL)
        {
            MSG_WriteByte(msg, ent->v.modelindex);
        }

        if(bits & U_FRAME)
        {
            MSG_WriteByte(msg, ent->v.frame);
        }

        if(bits & U_COLORMAP)
        {
            MSG_WriteByte(msg, ent->v.colormap);
        }

        if(bits & U_SKIN)
        {
            MSG_WriteByte(msg, ent->v.skin);
        }

        if(bits & U_EFFECTS)
        {
            MSG_WriteByte(msg, ent->v.effects);
        }

        if(bits & U_ORIGIN1)
        {
            MSG_WriteCoord(msg, ent->v.origin[0], sv.protocolflags);
        }

        if(bits & U_ANGLE1)
        {
            MSG_WriteAngle(msg, ent->v.angles[0], sv.protocolflags);
        }

        if(bits & U_SCALE)
        {
            MSG_WriteCoord(msg, ent->v.model_scale[0], sv.protocolflags);
        }

        if(bits & U_ORIGIN2)
        {
            MSG_WriteCoord(msg, ent->v.origin[1], sv.protocolflags);
        }

        if(bits & U_ANGLE2)
        {
            MSG_WriteAngle(msg, ent->v.angles[1], sv.protocolflags);
        }

        if(bits & U_SCALE)
        {
            MSG_WriteCoord(msg, ent->v.model_scale[1], sv.protocolflags);
        }

        if(bits & U_ORIGIN3)
        {
            MSG_WriteCoord(msg, ent->v.origin[2], sv.protocolflags);
        }

        if(bits & U_ANGLE3)
        {
            MSG_WriteAngle(msg, ent->v.angles[2], sv.protocolflags);
        }

        if(bits & U_SCALE)
        {
            MSG_WriteCoord(msg, ent->v.model_scale[2], sv.protocolflags);
        }

        if(bits & U_SCALE)
        {
            MSG_WriteVec3(msg, ent->v.model_scale_origin, sv.protocolflags);
        }

        if(bits & U_MODELOFFSET)
        {
            MSG_WriteVec3(msg, ent->v.model_offset, sv.protocolflags);
        }

        // johnfitz -- PROTOCOL_QUAKEVR
        if(bits & U_ALPHA)
        {
            MSG_WriteByte(msg, ent->alpha);
        }

        if(bits & U_FRAME2)
        {
            MSG_WriteByte(msg, (int)ent->v.frame >> 8);
        }

        if(bits & U_MODEL2)
        {
            MSG_WriteByte(msg, (int)ent->v.modelindex >> 8);
        }

        if(bits & U_LERPFINISH)
        {
            MSG_WriteByte(
                msg, (byte)(Q_rint((ent->v.nextthink - qcvm->time) * 255)));
        }
        // johnfitz
    }

    // johnfitz -- devstats
stats:
    if(msg->cursize > 1024 && dev_peakstats.packetsize <= 1024)
    {
        Con_DWarning(
            "%i byte packet exceeds standard limit of 1024 (max = "
            "%d).\n",
            msg->cursize, msg->maxsize);
    }

    dev_stats.packetsize = msg->cursize;
    dev_peakstats.packetsize = q_max(msg->cursize, dev_peakstats.packetsize);
    // johnfitz
}

/*
=============
SV_CleanupEnts

=============
*/
void SV_CleanupEnts()
{
    int e;
    edict_t* ent;

    ent = NEXT_EDICT(qcvm->edicts);
    for(e = 1; e < qcvm->num_edicts; e++, ent = NEXT_EDICT(ent))
    {
        ent->v.effects = (int)ent->v.effects & ~EF_MUZZLEFLASH;
    }
}

/*
==================
SV_WriteDamageToMessage

==================
*/
void SV_WriteDamageToMessage(edict_t* ent, sizebuf_t* msg)
{
    edict_t* other;
    int i;

    //
    // send a damage message
    //
    if(ent->v.dmg_take || ent->v.dmg_save)
    {
        other = PROG_TO_EDICT(ent->v.dmg_inflictor);
        MSG_WriteByte(msg, svc_damage);
        MSG_WriteByte(msg, ent->v.dmg_save);
        MSG_WriteByte(msg, ent->v.dmg_take);
        for(i = 0; i < 3; i++)
        {
            MSG_WriteCoord(msg,
                other->v.origin[i] +
                    0.5 * (other->v.mins[i] + other->v.maxs[i]),
                sv.protocolflags);
        }

        ent->v.dmg_take = 0;
        ent->v.dmg_save = 0;
    }

    //
    // send the current viewpos offset from the view entity
    //
    SV_SetIdealPitch(); // how much to look up / down ideally

    // a fixangle might get lost in a dropped packet.  Oh well.
    if(ent->v.fixangle)
    {
        MSG_WriteByte(msg, svc_setangle);
        for(i = 0; i < 3; i++)
        {
            MSG_WriteAngle(msg, ent->v.angles[i], sv.protocolflags);
        }
        ent->v.fixangle = 0;
    }
}

/*
==================
SV_WriteClientdataToMessage

==================
*/
void SV_WriteClientdataToMessage(client_t* client, sizebuf_t* msg)
{
    edict_t* ent = client->edict;
    int bits;
    int i;
    int items;
    eval_t* val;
    unsigned int weaponmodelindex =
        SV_ModelIndex(PR_GetString(ent->v.weaponmodel));

    if(weaponmodelindex >= client->limit_models)
    {
        weaponmodelindex = 0;
    }

    bits = 0;

    if(ent->v.view_ofs[2] != DEFAULT_VIEWHEIGHT)
    {
        bits |= SU_VIEWHEIGHT;
    }

    if(ent->v.idealpitch)
    {
        bits |= SU_IDEALPITCH;
    }

    // stuff the sigil bits into the high bits of items for sbar, or else
    // mix in items2
    val = GetEdictFieldValue(ent, qcvm->extfields.items2);

    if(val)
    {
        items = (int)ent->v.items | ((int)val->_float << 23);
    }
    else
    {
        items = (int)ent->v.items | ((int)pr_global_struct->serverflags << 28);
    }

    bits |= SU_ITEMS;

    if(quake::util::hasFlag(ent, FL_ONGROUND))
    {
        bits |= SU_ONGROUND;
    }

    if(ent->v.waterlevel >= 2)
    {
        bits |= SU_INWATER;
    }

    for(i = 0; i < 3; i++)
    {
        if(ent->v.punchangle[i])
        {
            bits |= (SU_PUNCH1 << i);
        }

        if(ent->v.velocity[i])
        {
            bits |= (SU_VELOCITY1 << i);
        }
    }

    if(ent->v.weaponframe)
    {
        bits |= SU_WEAPONFRAME;
    }

    if(ent->v.armorvalue)
    {
        bits |= SU_ARMOR;
    }

    //	if (ent->v.weapon)
    bits |= SU_WEAPON;

    // johnfitz -- PROTOCOL_QUAKEVR
    // if(sv.protocol != PROTOCOL_NETQUAKE)
    // {
    if(bits & SU_WEAPON && weaponmodelindex & 0xFF00)
    {
        bits |= SU_WEAPON2;
    }

    if((int)ent->v.armorvalue & 0xFF00)
    {
        bits |= SU_ARMOR2;
    }

    if((int)ent->v.currentammo & 0xFF00 || (int)ent->v.currentammo2 & 0xFF00)
    {
        bits |= SU_AMMO2;
    }

    if((int)ent->v.ammo_shells & 0xFF00)
    {
        bits |= SU_SHELLS2;
    }

    if((int)ent->v.ammo_nails & 0xFF00)
    {
        bits |= SU_NAILS2;
    }

    if((int)ent->v.ammo_rockets & 0xFF00)
    {
        bits |= SU_ROCKETS2;
    }

    if((int)ent->v.ammo_cells & 0xFF00)
    {
        bits |= SU_CELLS2;
    }

    if(bits & SU_WEAPONFRAME && (int)ent->v.weaponframe & 0xFF00)
    {
        bits |= SU_WEAPONFRAME2;
    }

    if(bits & SU_WEAPON && ent->alpha != ENTALPHA_DEFAULT)
    {
        bits |= SU_WEAPONALPHA; // for now, weaponalpha = client
                                // entity
    }

    // alpha
    if(bits >= 65536)
    {
        bits |= SU_EXTEND1;
    }

    if(bits >= 16777216)
    {
        bits |= SU_EXTEND2;
    }

    bits |= SU_VR_WEAPON2;
    bits |= SU_VR_WEAPONFRAME2;

    const bool anyHolster =
        ent->v.holsterweapon0 || ent->v.holsterweapon1 ||
        ent->v.holsterweapon2 || ent->v.holsterweapon3 ||
        ent->v.holsterweapon4 || ent->v.holsterweapon5 ||
        ent->v.holsterweaponmodel0 || ent->v.holsterweaponmodel1 ||
        ent->v.holsterweaponmodel2 || ent->v.holsterweaponmodel3 ||
        ent->v.holsterweaponmodel4 || ent->v.holsterweaponmodel5 ||
        ent->v.holsterweaponflags0 || ent->v.holsterweaponflags1 ||
        ent->v.holsterweaponflags2 || ent->v.holsterweaponflags3 ||
        ent->v.holsterweaponflags4 || ent->v.holsterweaponflags5;

    if(anyHolster)
    {
        bits |= SU_VR_HOLSTERS;
    }
    // johnfitz

    // send the data

    MSG_WriteByte(msg, svc_clientdata);
    MSG_WriteLong(msg, bits);

    // johnfitz -- PROTOCOL_QUAKEVR
    if(bits & SU_EXTEND1)
    {
        MSG_WriteByte(msg, bits >> 16);
    }

    if(bits & SU_EXTEND2)
    {
        MSG_WriteByte(msg, bits >> 24);
    }
    // johnfitz

    if(bits & SU_VIEWHEIGHT)
    {
        MSG_WriteChar(msg, ent->v.view_ofs[2]);
    }

    if(bits & SU_IDEALPITCH)
    {
        MSG_WriteChar(msg, ent->v.idealpitch);
    }

    for(i = 0; i < 3; i++)
    {
        if(bits & (SU_PUNCH1 << i))
        {
            MSG_WriteChar(msg, ent->v.punchangle[i]);
        }

        if(bits & (SU_VELOCITY1 << i))
        {
            MSG_WriteChar(msg, ent->v.velocity[i] / 16);
        }
    }

    // [always sent]	if (bits & SU_ITEMS)
    MSG_WriteLong(msg, items);

    if(bits & SU_WEAPONFRAME)
    {
        MSG_WriteByte(msg, ent->v.weaponframe);
    }

    if(bits & SU_ARMOR)
    {
        MSG_WriteByte(msg, ent->v.armorvalue);
    }

    if(bits & SU_WEAPON)
    {
        MSG_WriteByte(msg, weaponmodelindex);
    }

    MSG_WriteShort(msg, ent->v.health);
    MSG_WriteByte(msg, ent->v.currentammo);
    MSG_WriteByte(msg, ent->v.currentammo2);
    MSG_WriteShort(msg, ent->v.ammocounter);
    MSG_WriteShort(msg, ent->v.ammocounter2);
    MSG_WriteByte(msg, ent->v.ammo_shells);
    MSG_WriteByte(msg, ent->v.ammo_nails);
    MSG_WriteByte(msg, ent->v.ammo_rockets);
    MSG_WriteByte(msg, ent->v.ammo_cells);

    if(standard_quake)
    {
        MSG_WriteByte(msg, ent->v.weapon);
    }
    else
    {
        for(i = 0; i < 32; i++)
        {
            if(((int)ent->v.weapon) & (1 << i))
            {
                MSG_WriteByte(msg, i);
                break;
            }
        }
    }

    // johnfitz -- PROTOCOL_QUAKEVR
    if(bits & SU_WEAPON2)
    {
        MSG_WriteByte(msg, weaponmodelindex >> 8);
    }

    if(bits & SU_ARMOR2)
    {
        MSG_WriteByte(msg, (int)ent->v.armorvalue >> 8);
    }

    if(bits & SU_AMMO2)
    {
        MSG_WriteByte(msg, (int)ent->v.currentammo >> 8);
        MSG_WriteByte(msg, (int)ent->v.currentammo2 >> 8);
    }

    if(bits & SU_SHELLS2)
    {
        MSG_WriteByte(msg, (int)ent->v.ammo_shells >> 8);
    }

    if(bits & SU_NAILS2)
    {
        MSG_WriteByte(msg, (int)ent->v.ammo_nails >> 8);
    }

    if(bits & SU_ROCKETS2)
    {
        MSG_WriteByte(msg, (int)ent->v.ammo_rockets >> 8);
    }

    if(bits & SU_CELLS2)
    {
        MSG_WriteByte(msg, (int)ent->v.ammo_cells >> 8);
    }

    if(bits & SU_WEAPONFRAME2)
    {
        MSG_WriteByte(msg, (int)ent->v.weaponframe >> 8);
    }

    if(bits & SU_WEAPONALPHA)
    {
        MSG_WriteByte(msg, ent->alpha); // for now, weaponalpha =
                                        // client entity alpha
    }
    // johnfitz

    // TODO VR: (P2) do we need all these bits?
    if(bits & SU_VR_WEAPON2)
    {
        MSG_WriteByte(msg, (int)ent->v.weapon2);
        MSG_WriteByte(msg, SV_ModelIndex(PR_GetString(ent->v.weaponmodel2)));
    }
    if(bits & SU_VR_WEAPONFRAME2)
    {
        MSG_WriteByte(msg, (int)ent->v.weaponframe2);
    }

    // TODO VR: (P2) weapon ids in holsters
    if(bits & SU_VR_HOLSTERS)
    {
        MSG_WriteByte(msg, (int)ent->v.holsterweapon0);
        MSG_WriteByte(msg, (int)ent->v.holsterweapon1);
        MSG_WriteByte(msg, (int)ent->v.holsterweapon2);
        MSG_WriteByte(msg, (int)ent->v.holsterweapon3);
        MSG_WriteByte(msg, (int)ent->v.holsterweapon4);
        MSG_WriteByte(msg, (int)ent->v.holsterweapon5);

        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel0)));
        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel1)));
        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel2)));
        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel3)));
        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel4)));
        MSG_WriteByte(
            msg, (int)SV_ModelIndex(PR_GetString(ent->v.holsterweaponmodel5)));

        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags0); // STAT_HOLSTERWEAPONFLAGS0
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags1); // STAT_HOLSTERWEAPONFLAGS1
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags2); // STAT_HOLSTERWEAPONFLAGS2
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags3); // STAT_HOLSTERWEAPONFLAGS3
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags4); // STAT_HOLSTERWEAPONFLAGS4
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponflags5); // STAT_HOLSTERWEAPONFLAGS5

        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip0); // STAT_HOLSTERWEAPONCLIP0
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip1); // STAT_HOLSTERWEAPONCLIP1
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip2); // STAT_HOLSTERWEAPONCLIP2
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip3); // STAT_HOLSTERWEAPONCLIP3
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip4); // STAT_HOLSTERWEAPONCLIP4
        MSG_WriteByte(
            msg, (int)ent->v.holsterweaponclip5); // STAT_HOLSTERWEAPONCLIP5
    }

    MSG_WriteByte(msg, (int)ent->v.weapon);  // STAT_MAINHAND_WID
    MSG_WriteByte(msg, (int)ent->v.weapon2); // STAT_OFFHAND_WID

    MSG_WriteByte(msg, (int)ent->v.weaponflags);  // STAT_WEAPONFLAGS
    MSG_WriteByte(msg, (int)ent->v.weaponflags2); // STAT_WEAPONFLAGS2

    MSG_WriteByte(msg, (int)ent->v.weaponclip);  // STAT_WEAPONCLIP
    MSG_WriteByte(msg, (int)ent->v.weaponclip2); // STAT_WEAPONCLIP2

    MSG_WriteByte(msg, (int)ent->v.weaponclipsize);  // STAT_WEAPONCLIPSIZE
    MSG_WriteByte(msg, (int)ent->v.weaponclipsize2); // STAT_WEAPONCLIPSIZE2


    // TODO VR: (P1) experiment with this
#if 0
    {
        VrGunWallCollision outGunWallCollision[2];

        const auto doHandAndGunCollisions = [&](const HandIdx index) {
            const auto worldHandPos = VR_GetWorldHandPos(index, ent->v.origin);

            const qvec3 adjPlayerOrigin =
                VR_GetAdjustedPlayerOrigin(ent->v.origin);

            qvec3 finalVec = worldHandPos;

            const auto resolvedHandPos =
                VR_GetResolvedHandPos(ent, worldHandPos, adjPlayerOrigin);

            finalVec = resolvedHandPos;

            finalVec = VR_UpdateGunWallCollisions(
                ent, index, outGunWallCollision[index], finalVec);
        };
    }
#endif
}

/*
=======================
SV_SendClientDatagram
=======================
*/
bool SV_SendClientDatagram(client_t* client)
{
    byte buf[MAX_DATAGRAM + 1000 /* QSS */];
    sizebuf_t msg;

    // QSS
    if(!client->netconnection)
    {
        // botclient, shouldn't be sent anything.
        SZ_Clear(&client->datagram);
        return true;
    }

    msg.allowoverflow = false; // QSS
    msg.data = buf;
    msg.maxsize = client->limit_unreliable;
    msg.cursize = 0;

    if(client->download.file)
    {
        msg.maxsize /= 2; // make sure there's space for download data
    }

    host_client = client;
    if(client->spawned)
    {
        sv_player = client->edict;

        if(client->protocol_pext2 & PEXT2_REPLACEMENTDELTAS)
        {
            SV_WriteDamageToMessage(client->edict, &msg);

            if(!(client->protocol_pext2 & PEXT2_PREDINFO))
            {
                SV_WriteClientdataToMessage(client, &msg);
            }
            else
            {
                SVFTE_WriteStats(client, &msg);
            }

            if(!client->snapshotresume)
            {
                SVFTE_BuildSnapshotForClient(client);
                SVFTE_CalcEntityDeltas(client);
            }
            SVFTE_WriteEntitiesToClient(
                client, &msg, sizeof(buf)); // must always write some data, or
                                            // the stats will break

            // this delta protocol doesn't wipe old state just because there's a
            // new packet. the server isn't required to sync with the client
            // frames either so we can just spam multiple packets to keep our
            // udp data under the MTU
            while(client->snapshotresume)
            {
                NET_SendUnreliableMessage(client->netconnection, &msg);
                SZ_Clear(&msg);
                SVFTE_WriteEntitiesToClient(client, &msg, sizeof(buf));
            }
        }
        else
        {
            MSG_WriteByte(&msg, svc_time);
            MSG_WriteFloat(&msg, qcvm->time);

            if(client->protocol_pext2 & PEXT2_PREDINFO)
            {
                MSG_WriteShort(&msg, (client->lastmovemessage & 0xffff));
            }

            // add the client specific data to the datagram
            SV_WriteDamageToMessage(client->edict, &msg);
            SV_WriteClientdataToMessage(client, &msg);

            SV_WriteEntitiesToClient(client, &msg);
        }

        // copy the private datagram if there is space
        if(msg.cursize + client->datagram.cursize < msg.maxsize &&
            !client->datagram.overflowed)
        {
            SZ_Write(&msg, client->datagram.data, client->datagram.cursize);
        }
        client->datagram.overflowed = false;
        SZ_Clear(&client->datagram);

        // copy the server datagram if there is space
        if(msg.cursize + sv.datagram.cursize < msg.maxsize)
        {
            SZ_Write(&msg, sv.datagram.data, sv.datagram.cursize);
        }
    }

    SV_VoiceSendPacket(client, &msg);

    msg.maxsize = client->limit_unreliable;
    Host_AppendDownloadData(client, &msg);


    // send the datagram
    if(msg.cursize &&
        NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
    {
        SV_DropClient(false); // if the message couldn't send, kick off
        return false;
    }

    return true;
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
void SV_UpdateToReliableMessages()
{
    int i;
    int j;
    client_t* client;

    // check for changes to be sent over the reliable streams
    for(i = 0, host_client = svs.clients; i < svs.maxclients;
        i++, host_client++)
    {
        if(host_client->old_frags != host_client->edict->v.frags)
        {
            for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
            {
                if(!client->knowntoqc)
                {
                    continue;
                }
                MSG_WriteByte(&client->message, svc_updatefrags);
                MSG_WriteByte(&client->message, i);
                MSG_WriteShort(&client->message, host_client->edict->v.frags);
            }

            host_client->old_frags = host_client->edict->v.frags;
        }
    }

    for(j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
    {
        if(!client->active)
        {
            continue;
        }
        SZ_Write(&client->message, sv.reliable_datagram.data,
            sv.reliable_datagram.cursize);
    }

    SZ_Clear(&sv.reliable_datagram);
}


/*
=======================
SV_SendNop

Send a nop message without trashing or sending the accumulated
client message buffer
=======================
*/
void SV_SendNop(client_t* client)
{
    sizebuf_t msg;
    byte buf[4];

    msg.data = buf;
    msg.maxsize = sizeof(buf);
    msg.cursize = 0;

    MSG_WriteChar(&msg, svc_nop);

    if(NET_SendUnreliableMessage(client->netconnection, &msg) == -1)
    {
        SV_DropClient(false); // if the message couldn't send, kick off
    }
    client->last_message = realtime;
}

int SV_SendPrespawnParticlePrecaches(int idx)
{
    size_t maxsize = host_client->message.maxsize; // we can go quite large
    if(!host_client->protocol_pext2)
    {
        return -1; // unsupported by this client.
    }
    for(; idx < MAX_PARTICLETYPES; idx++)
    {
        if(!sv.particle_precache[idx])
        {
            continue;
        }
        if(host_client->message.cursize + 4 +
                strlen(sv.particle_precache[idx]) >
            maxsize)
        {
            break;
        }
        MSG_WriteByte(&host_client->message, svcdp_precache);
        MSG_WriteShort(&host_client->message, 0x4000 | idx);
        MSG_WriteString(&host_client->message, sv.particle_precache[idx]);
    }
    if(idx == MAX_PARTICLETYPES)
    {
        return -1;
    }
    return idx;
}
int SV_SendPrespawnStatics(int idx)
{
    entity_state_t* svent;
    int maxsize = host_client->message.maxsize - 128; // we can go quite large

    while(1)
    {
        if(idx >= sv.num_statics)
        {
            return -1;
        }
        svent = &sv.static_entities[idx];

        if(host_client->message.cursize > maxsize)
        {
            break;
        }
        idx++;

        if(svent->modelindex >= host_client->limit_models)
        {
            continue;
        }
        if(memcmp(&nullentitystate, svent, sizeof(nullentitystate)))
        {
            MSG_WriteStaticOrBaseLine(&host_client->message, -1, svent,
                host_client->protocol_pext2, sv.protocol, sv.protocolflags);
        }
    }
    return idx;
}
int SV_SendAmbientSounds(int idx)
{
    struct ambientsound_s* snd;
    int maxsize = host_client->message.maxsize - 128; // we can go quite large
    bool large;
    size_t i;

    while(1)
    {
        if(idx >= sv.num_ambients)
        {
            return -1;
        }
        snd = &sv.ambientsounds[idx];

        if(host_client->message.cursize > maxsize)
        {
            break;
        }
        idx++;

        if(snd->soundindex >= host_client->limit_sounds) continue;

        large = (snd->soundindex > 255);
        if(large)
        {
            MSG_WriteByte(&host_client->message,
                svc_spawnstaticsound2); // johnfitz -- PROTOCOL_FITZQUAKE
        }
        else
        {
            MSG_WriteByte(&host_client->message, svc_spawnstaticsound);
        }
        for(i = 0; i < 3; i++)
            MSG_WriteCoord(
                &host_client->message, snd->origin[i], sv.protocolflags);
        if(large)
            MSG_WriteShort(&host_client->message, snd->soundindex);
        else
            MSG_WriteByte(&host_client->message, snd->soundindex);
        MSG_WriteByte(&host_client->message, snd->volume * 255);
        MSG_WriteByte(&host_client->message, snd->attenuation * 64);
    }
    return idx;
}
int SV_SendPrespawnBaselines(int idx)
{
    edict_t* svent;
    int maxsize = host_client->message.maxsize - 128; // we can go quite large

    while(1)
    {
        if(idx >= qcvm->num_edicts)
        {
            return -1;
        }
        svent = EDICT_NUM(idx);

        if(host_client->message.cursize > maxsize)
        {
            break;
        }

        if(memcmp(&nullentitystate, &svent->baseline, sizeof(nullentitystate)))
        {
            MSG_WriteStaticOrBaseLine(&host_client->message, idx,
                &svent->baseline, host_client->protocol_pext2, sv.protocol,
                sv.protocolflags);
        }

        idx++;
    }
    return idx;
}

/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages()
{
    int i;

    // update frags, names, etc
    SV_UpdateToReliableMessages();

    // build individual updates
    for(i = 0, host_client = svs.clients; i < svs.maxclients;
        i++, host_client++)
    {
        if(!host_client->active)
        {
            continue;
        }

        if(!SV_SendClientDatagram(host_client))
        {
            continue;
        }
        if(!host_client->spawned)
        {
            // the player isn't totally in the game yet
            // send small keepalive messages if too much time has
            // passed send a full message when the next signon stage
            // has been requested some other message data (name
            // changes, etc) may accumulate between signon stages
            if(!host_client->sendsignon)
            {
                if(realtime - host_client->last_message > 5)
                {
                    SV_SendNop(host_client);
                }
                continue; // don't send out non-signon messages
            }
            if(host_client->sendsignon == 2)
            {
                host_client->signonidx =
                    SV_SendPrespawnParticlePrecaches(host_client->signonidx);
                if(host_client->signonidx < 0)
                {
                    host_client->signonidx = 0;
                    host_client->sendsignon++;
                }
            }
            if(host_client->sendsignon == 3)
            {
                host_client->signonidx =
                    SV_SendPrespawnBaselines(host_client->signonidx);
                if(host_client->signonidx < 0)
                {
                    host_client->signonidx = 0;
                    host_client->sendsignon++;
                }
            }
            if(host_client->sendsignon == 4)
            {
                host_client->signonidx =
                    SV_SendPrespawnStatics(host_client->signonidx);
                if(host_client->signonidx < 0)
                {
                    host_client->signonidx = 0;
                    host_client->sendsignon++;
                }
            }
            if(host_client->sendsignon == 5)
            {
                host_client->signonidx =
                    SV_SendAmbientSounds(host_client->signonidx);
                if(host_client->signonidx < 0)
                {
                    host_client->signonidx = 0;
                    host_client->sendsignon++;
                }
            }
            if(host_client->sendsignon == 6)
            {
                if(host_client->message.cursize + sv.signon.cursize + 2 <
                    host_client->message.maxsize)
                {
                    SZ_Write(&host_client->message, sv.signon.data,
                        sv.signon.cursize);
                    MSG_WriteByte(&host_client->message, svc_signonnum);
                    MSG_WriteByte(&host_client->message, 2);
                    host_client->sendsignon = true;
                }
            }
        }

        // check for an overflowed message.  Should only happen
        // on a very fucked up connection that backs up a lot, then
        // changes level
        if(host_client->message.overflowed)
        {
            SZ_Clear(&host_client->message);
            SV_DropClient(false);
            continue;
        }

        if(host_client->message.cursize || host_client->dropasap)
        {
            if(!NET_CanSendMessage(host_client->netconnection))
            {
                //				I_Printf ("can't write\n");
                continue;
            }

            if(host_client->dropasap)
            {
                SV_DropClient(false); // went to another level
            }
            else
            {
                if(NET_SendMessage(
                       host_client->netconnection, &host_client->message) == -1)
                {
                    SV_DropClient(false); // if the message couldn't
                                          // send, kick off
                }
                SZ_Clear(&host_client->message);
                host_client->last_message = realtime;
                if(host_client->sendsignon == true)
                {
                    host_client->sendsignon = false;
                }
            }
        }
    }


    // clear muzzle flashes
    SV_CleanupEnts();
}


/*
==============================================================================

SERVER SPAWNING

==============================================================================
*/

/*
================
SV_ModelIndex

================
*/
int SV_ModelIndex(const char* name)
{
    if(!name || !name[0])
    {
        return 0;
    }

    int i;
    for(i = 0; i < MAX_MODELS && sv.model_precache[i]; ++i)
    {
        if(!strcmp(sv.model_precache[i], name))
        {
            return i;
        }
    }

    if(i == MAX_MODELS || !sv.model_precache[i])
    {
        Sys_Error("SV_ModelIndex: model %s not precached", name);
    }

    return i;
}

/*
================
SV_CreateBaseline
================
*/
void SV_CreateBaseline()
{
    int i;
    int bits; // johnfitz -- PROTOCOL_QUAKEVR

    for(int entnum = 0; entnum < qcvm->num_edicts; entnum++)
    {
        // get the current server version
        edict_t* svent = EDICT_NUM(entnum);

        if(svent->free)
        {
            continue;
        }

        if(entnum > svs.maxclients && !svent->v.modelindex)
        {
            continue;
        }

        //
        // create entity baseline
        //
        svent->baseline.origin = svent->v.origin;
        svent->baseline.angles = svent->v.angles;
        svent->baseline.model_scale = svent->v.model_scale;
        svent->baseline.model_scale_origin = svent->v.model_scale_origin;
        svent->baseline.model_offset = svent->v.model_offset;
        svent->baseline.frame = svent->v.frame;
        svent->baseline.skin = svent->v.skin;

        if(entnum > 0 && entnum <= svs.maxclients)
        {
            svent->baseline.colormap = entnum;
            svent->baseline.modelindex = SV_ModelIndex("progs/player.mdl");
            svent->baseline.alpha =
                ENTALPHA_DEFAULT; // johnfitz -- alpha support
        }
        else
        {
            svent->baseline.colormap = 0;
            svent->baseline.modelindex =
                SV_ModelIndex(PR_GetString(svent->v.model));
            eval_t* val = GetEdictFieldValue(svent, qcvm->extfields.alpha);
            if(val)
            {
                svent->baseline.alpha = ENTALPHA_ENCODE(val->_float);
            }
            else
            {
                svent->baseline.alpha =
                    svent->alpha; // johnfitz -- alpha support
            }
        }

        // Spike -- baselines are now generated on a per-client basis.
        // FIXME: should merge the above with other edict->entity_state copies
        // (updates, baselines, spawnstatics) 1) this allows per-client
        // extensions. 2) this avoids pre-generating a single signon buffer,
        // splitting it over multiple packets.
        //   thereby allowing more than 3k or so entities
    }
}


/*
================
SV_SendReconnect

Tell all the clients that the server is changing levels
================
*/
void SV_SendReconnect()
{
    byte data[128];
    sizebuf_t msg;

    msg.data = data;
    msg.cursize = 0;
    msg.maxsize = sizeof(data);

    MSG_WriteChar(&msg, svc_stufftext);
    MSG_WriteString(&msg, "reconnect\n");
    NET_SendToAll(&msg, 5.0);

    if(!isDedicated)
    {
        Cmd_ExecuteString("reconnect\n", src_server);
    }
}


/*
================
SV_SaveSpawnparms

Grabs the current state of each client for saving across the
transition to another level
================
*/
void SV_SaveSpawnparms()
{
    svs.serverflags = pr_global_struct->serverflags;

    int i;
    for(i = 0, host_client = svs.clients; i < svs.maxclients;
        i++, host_client++)
    {
        if(!host_client->active)
        {
            continue;
        }

        // call the progs to get default spawn parms for the new
        // client
        pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
        PR_ExecuteProgram(pr_global_struct->SetChangeParms);

        int j;
        for(j = 0; j < NUM_BASIC_SPAWN_PARMS; j++)
        {
            host_client->spawn_parms[j] = (&pr_global_struct->parm1)[j];
        }
        for(; j < NUM_TOTAL_SPAWN_PARMS; j++)
        {
            ddef_t* g = ED_FindGlobal(va("parm%i", j + 1));
            host_client->spawn_parms[j] = g ? qcvm->globals[g->ofs] : 0;
        }
    }
}

// used for sv.qcvm.GetModel (so ssqc+csqc can share builtins)
qmodel_t* SV_ModelForIndex(int index)
{
    if(index < 0 || index >= MAX_MODELS)
    {
        return nullptr;
    }
    return sv.models[index];
}


/*
================
SV_SpawnServer

This is called at the start of each level
================
*/
extern float scr_centertime_off;

void SV_SpawnServer(const char* server, const SpawnServerSrc src)
{
    static char dummy[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    // let's not have any servers with no name
    if(hostname.string[0] == 0)
    {
        Cvar_Set("hostname", "UNNAMED");
    }

    scr_centertime_off = 0;

    Con_DPrintf("SpawnServer: %s\n", server);
    svs.changelevel_issued = false; // now safe to issue another

    //
    // tell all connected clients that we are going to a new level
    //
    if(sv.active)
    {
        PR_SwitchQCVM(nullptr);
        SV_SendReconnect();
        PR_SwitchQCVM(&sv.qcvm);
    }

    //
    // make cvars consistant
    //
    if(coop.value)
    {
        Cvar_Set("deathmatch", "0");
    }

    current_skill = std::clamp((int)(skill.value + 0.5), 0, 3);
    Cvar_SetValue("skill", (float)current_skill);

    //
    // set up the new server
    //
    Host_ClearMemory();

    q_strlcpy(sv.name, server, sizeof(sv.name));

    sv.protocol = sv_protocol; // johnfitz
    sv.protocolflags = 0;

    // load progs to get entity field count
    PR_LoadProgs("vrprogs.dat", true, pr_ssqcbuiltins, pr_ssqcnumbuiltins);

    // allocate server memory
    /* Host_ClearMemory() called above already cleared the whole
     * sv structure */
    qcvm->max_edicts = CLAMP(MIN_EDICTS, (int)max_edicts.value,
        MAX_EDICTS); // johnfitz -- max_edicts cvar
    qcvm->edicts = (edict_t*)malloc(
        qcvm->max_edicts * qcvm->edict_size); // ericw -- qcvm->edicts
                                              // switched to use malloc()

    sv.datagram.maxsize = sizeof(sv.datagram_buf);
    sv.datagram.cursize = 0;
    sv.datagram.data = sv.datagram_buf;

    sv.multicast.maxsize = sizeof(sv.multicast_buf);
    sv.multicast.cursize = 0;
    sv.multicast.data = sv.multicast_buf;

    sv.reliable_datagram.maxsize = sizeof(sv.reliable_datagram_buf);
    sv.reliable_datagram.cursize = 0;
    sv.reliable_datagram.data = sv.reliable_datagram_buf;

    sv.signon.maxsize = sizeof(sv.signon_buf);
    sv.signon.cursize = 0;
    sv.signon.data = sv.signon_buf;

    // leave slots at start for clients only
    qcvm->num_edicts = qcvm->reserved_edicts = svs.maxclients + 1;
    memset(qcvm->edicts, 0,
        qcvm->num_edicts * qcvm->edict_size); // ericw -- qcvm->edicts
                                              // switched to use malloc()
    for(int i = 0; i < svs.maxclients; i++)
    {
        edict_t* ent = EDICT_NUM(i + 1);
        svs.clients[i].edict = ent;
    }

    sv.state = ss_loading;
    sv.paused = false;

    qcvm->time = 1.0;

    // VR: This is where the map is changed upon creation of a server. `map`,
    // `changelevel`, and slipgates eventually all reach this point.
    q_strlcpy(sv.name, server, sizeof(sv.name));
    q_snprintf(sv.modelname, sizeof(sv.modelname), "maps/%s.bsp", server);

    qcvm->worldmodel = Mod_ForName(sv.modelname, false);
    if(!qcvm->worldmodel || qcvm->worldmodel->type != mod_brush)
    {
        Con_Printf("Couldn't spawn server %s\n", sv.modelname);
        sv.active = false;
        return;
    }

    sv.models[1] = qcvm->worldmodel;
    qcvm->GetModel = SV_ModelForIndex;

    //
    // clear world interaction links
    //
    SV_ClearWorld();

    // Initialize world text handles and buffers
    sv.initializeWorldTexts();

    sv.sound_precache[0] = dummy;
    sv.model_precache[0] = dummy;
    sv.model_precache[1] = sv.modelname;
    if(qcvm->worldmodel->numsubmodels > MAX_MODELS)
    {
        Con_Printf("too many inline models %s\n", sv.modelname);
        sv.active = false;
        return;
    }
    for(int i = 1; i < qcvm->worldmodel->numsubmodels; i++)
    {
        sv.model_precache[1 + i] = localmodels[i];
        sv.models[i + 1] = Mod_ForName(localmodels[i], false);
    }

    //
    // load the rest of the entities
    //
    edict_t* ent = EDICT_NUM(0);
    memset(&ent->v, 0, qcvm->progs->entityfields * 4);
    ent->free = false;
    ent->v.model = PR_SetEngineString(qcvm->worldmodel->name);
    ent->v.modelindex = 1; // world model
    ent->v.solid = SOLID_BSP;
    ent->v.movetype = MOVETYPE_PUSH;

    if(coop.value)
    {
        pr_global_struct->coop = coop.value;
    }
    else
    {
        pr_global_struct->deathmatch = deathmatch.value;
    }

    pr_global_struct->mapname = PR_SetEngineString(sv.name);

    // serverflags are for cross level information (sigils)
    pr_global_struct->serverflags = svs.serverflags;

    {
        Con_DPrintf("Calling QC 'OnSpawnServerBeforeLoad'.\n");

        pr_global_struct->spawnServerFromSaveFile =
            src == SpawnServerSrc::FromSaveFile;

        PR_ExecuteProgram(pr_global_struct->OnSpawnServerBeforeLoad);
    }

    ED_LoadFromFile(qcvm->worldmodel->entities);

    sv.active = true;

    SV_Precache_Model("progs/player.mdl"); // Spike -- SV_CreateBaseline depends
                                           // on this model.

    // all setup is completed, any further precache statements
    // are errors
    sv.state = ss_active;

    // run two frames to allow everything to settle
    host_frametime = 0.1;
    SV_Physics();
    SV_Physics();

    // create a baseline for more efficient communications
    SV_CreateBaseline();

    // johnfitz -- warn if signon buffer larger than standard
    // server can handle
    if(sv.signon.cursize > 8000 - 2)
    {
        // max size that will fit into 8000-sized client->message
        // buffer with 2 extra bytes on the end
        Con_DWarning(
            "%i byte signon buffer exceeds standard limit of "
            "7998 (max = "
            "%d).\n",
            sv.signon.cursize, sv.signon.maxsize);
    }
    // johnfitz

    // send serverinfo to all connected clients
    {
        int i;

        for(i = 0, host_client = svs.clients; i < svs.maxclients;
            i++, host_client++)
        {
            host_client->knowntoqc = false;
            if(host_client->active)
            {
                SV_SendServerinfo(host_client);
            }
        }
    }

    {
        Con_DPrintf("Calling QC 'OnSpawnServerAfterLoad'.\n");

        pr_global_struct->spawnServerFromSaveFile =
            src == SpawnServerSrc::FromSaveFile;

        PR_ExecuteProgram(pr_global_struct->OnSpawnServerAfterLoad);
    }

    {
        Con_DPrintf("Calling C++ 'VR_OnSpawnServer'.\n");
        VR_OnSpawnServer();
    }

    Con_DPrintf("Server spawned.\n");
}
