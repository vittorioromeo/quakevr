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
// view.c -- player eye positioning

#include "quakedef.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "render.hpp"
#include "util.hpp"
#include "cmd.hpp"
#include "glquake.hpp"
#include "msg.hpp"
#include "client.hpp"
#include "gl_texmgr.hpp"
#include "screen.hpp"
#include "console.hpp"

/*

The view is allowed to move slightly from it's true position for bobbing,
but if it exceeds 8 pixels linear distance (spherical, not box), the list of
entities sent from the server may not include everything in the pvs, especially
when crossing a water boudnary.

*/

cvar_t scr_ofsx = {"scr_ofsx", "0", CVAR_NONE};
cvar_t scr_ofsy = {"scr_ofsy", "0", CVAR_NONE};
cvar_t scr_ofsz = {"scr_ofsz", "0", CVAR_NONE};

cvar_t cl_rollspeed = {"cl_rollspeed", "200", CVAR_NONE};
cvar_t cl_rollangle = {"cl_rollangle", "2.0", CVAR_NONE};

cvar_t cl_bob = {"cl_bob", "0.02", CVAR_NONE};
cvar_t cl_bobcycle = {"cl_bobcycle", "0.6", CVAR_NONE};
cvar_t cl_bobup = {"cl_bobup", "0.5", CVAR_NONE};

cvar_t v_kicktime = {"v_kicktime", "0.5", CVAR_NONE};
cvar_t v_kickroll = {"v_kickroll", "0.6", CVAR_NONE};
cvar_t v_kickpitch = {"v_kickpitch", "0.6", CVAR_NONE};
cvar_t v_gunkick = {"v_gunkick", "1", CVAR_NONE}; // johnfitz

cvar_t v_iyaw_cycle = {"v_iyaw_cycle", "2", CVAR_NONE};
cvar_t v_iroll_cycle = {"v_iroll_cycle", "0.5", CVAR_NONE};
cvar_t v_ipitch_cycle = {"v_ipitch_cycle", "1", CVAR_NONE};
cvar_t v_iyaw_level = {"v_iyaw_level", "0.3", CVAR_NONE};
cvar_t v_iroll_level = {"v_iroll_level", "0.1", CVAR_NONE};
cvar_t v_ipitch_level = {"v_ipitch_level", "0.3", CVAR_NONE};

cvar_t v_idlescale = {"v_idlescale", "0", CVAR_NONE};

cvar_t crosshair = {"crosshair", "0", CVAR_ARCHIVE};

cvar_t gl_cshiftpercent = {"gl_cshiftpercent", "100", CVAR_NONE};
cvar_t gl_cshiftpercent_contents = {
    "gl_cshiftpercent_contents", "100", CVAR_NONE}; // QuakeSpasm
cvar_t gl_cshiftpercent_damage = {
    "gl_cshiftpercent_damage", "100", CVAR_NONE}; // QuakeSpasm
cvar_t gl_cshiftpercent_bonus = {
    "gl_cshiftpercent_bonus", "100", CVAR_NONE}; // QuakeSpasm
cvar_t gl_cshiftpercent_powerup = {
    "gl_cshiftpercent_powerup", "100", CVAR_NONE}; // QuakeSpasm

cvar_t r_viewmodel_quake = {"r_viewmodel_quake", "0", CVAR_ARCHIVE};

float v_dmg_time, v_dmg_roll, v_dmg_pitch;

extern int in_forward, in_forward2, in_back;

qvec3 v_punchangles[2]; // johnfitz -- copied from cl.punchangle.  0 is
                        // current, 1 is previous value. never the same
                        // unless map just loaded



template <typename T>
[[nodiscard]] static constexpr T cvarCast(const float x)
{
    return static_cast<T>(static_cast<std::underlying_type_t<T>>(x));
}

/*
===============
V_CalcRoll

Used by view and sv_user
===============
*/
qfloat V_CalcRoll(const qvec3& angles, const qvec3& velocity)
{
    qfloat sign;
    qfloat side;
    qfloat value;

    const auto [forward, right, up] = quake::util::getAngledVectors(angles);

    side = DotProduct(velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabs(side);

    // VR: Don't roll view in VR.
    if(vr_enabled.value)
    {
        value = 0;
    }
    else
    {
        value = cl_rollangle.value;
    }


    //  if (cl.inwater)
    //     value *= 6;

    if(side < cl_rollspeed.value)
    {
        side = side * value / cl_rollspeed.value;
    }
    else
    {
        side = value;
    }

    return side * sign;
}


/*
===============
V_CalcBob

===============
*/
float V_CalcBob()
{
    float bob;
    float cycle;

    // VR: Don't bob if we're in VR.
    if(vr_enabled.value)
    {
        return 0.f;
    }

    cycle = cl.time - (int)(cl.time / cl_bobcycle.value) * cl_bobcycle.value;
    cycle /= cl_bobcycle.value;
    if(cycle < cl_bobup.value)
    {
        cycle = M_PI * cycle / cl_bobup.value;
    }
    else
    {
        cycle = M_PI + M_PI * (cycle - cl_bobup.value) / (1.0 - cl_bobup.value);
    }

    // bob is proportional to velocity in the xy plane
    // (don't count Z, or jumping messes it up)

    bob = sqrt(cl.velocity[0] * cl.velocity[0] +
               cl.velocity[1] * cl.velocity[1]) *
          cl_bob.value;
    // Con_Printf ("speed: %5.1f\n", glm::length(cl.velocity));
    bob = bob * 0.3 + bob * 0.7 * sin(cycle);
    if(bob > 4)
    {
        bob = 4;
    }
    else if(bob < -7)
    {
        bob = -7;
    }
    return bob;
}


//=============================================================================


cvar_t v_centermove = {"v_centermove", "0.15", CVAR_NONE};
cvar_t v_centerspeed = {"v_centerspeed", "500", CVAR_NONE};

void V_StartPitchDrift()
{
    if(VR_EnabledAndNotFake())
    {
        VR_ResetOrientation();
        return;
    }

    if(cl.laststop == cl.time)
    {
        return; // something else is keeping it from drifting
    }

    if(cl.nodrift || !cl.pitchvel)
    {
        cl.pitchvel = v_centerspeed.value;
        cl.nodrift = false;
        cl.driftmove = 0;
    }
}

void V_StopPitchDrift()
{
    cl.laststop = cl.time;
    cl.nodrift = true;
    cl.pitchvel = 0;
}

/*
===============
V_DriftPitch

Moves the client pitch angle towards cl.idealpitch sent by the server.

If the user is adjusting pitch manually, either with lookup/lookdown,
mlook and mouse, or klook and keyboard, pitch drifting is constantly stopped.

Drifting is enabled when the center view key is hit, mlook is released and
lookspring is non 0, or when
===============
*/
void V_DriftPitch()
{
    float delta;

    float move;

    if(noclip_anglehack || !cl.onground || cls.demoplayback || vr_enabled.value)
    // FIXME: noclip_anglehack is set on the server, so in a nonlocal game this
    // won't work.
    {
        cl.driftmove = 0;
        cl.pitchvel = 0;
        return;
    }

    // don't count small mouse motion
    if(cl.nodrift)
    {
        if(fabs(cl.cmd.forwardmove) < cl_forwardspeed.value)
        {
            cl.driftmove = 0;
        }
        else
        {
            cl.driftmove += host_frametime;
        }

        if(cl.driftmove > v_centermove.value)
        {
            if(lookspring.value)
            {
                V_StartPitchDrift();
            }
        }
        return;
    }

    delta = cl.statsf[STAT_IDEALPITCH] - cl.viewangles[PITCH];

    if(!delta)
    {
        cl.pitchvel = 0;
        return;
    }

    move = host_frametime * cl.pitchvel;
    cl.pitchvel += host_frametime * v_centerspeed.value;

    // Con_Printf ("move: %f (%f)\n", move, host_frametime);

    if(delta > 0)
    {
        if(move > delta)
        {
            cl.pitchvel = 0;
            move = delta;
        }
        cl.viewangles[PITCH] += move;
    }
    else if(delta < 0)
    {
        if(move > -delta)
        {
            cl.pitchvel = 0;
            move = -delta;
        }
        cl.viewangles[PITCH] -= move;
    }
}

/*
==============================================================================

    VIEW BLENDING

==============================================================================
*/

cshift_t cshift_empty = {{130, 80, 50}, 0};
cshift_t cshift_water = {{130, 80, 50}, 128};
cshift_t cshift_slime = {{0, 25, 5}, 150};
cshift_t cshift_lava = {{255, 80, 0}, 150};

float v_blend[4]; // rgba 0.0 - 1.0

// johnfitz -- deleted BuildGammaTable(), V_CheckGamma(), gammatable[], and
// ramps[][]

/*
===============
V_ParseDamage
===============
*/
void V_ParseDamage()
{
    int armor;

    int blood;
    qvec3 from;

    entity_t* ent;
    float side;
    float count;

    armor = MSG_ReadByte();
    blood = MSG_ReadByte();
    from = MSG_ReadVec3(cl.protocolflags);

    count = blood * 0.5 + armor * 0.5;
    if(count < 10)
    {
        count = 10;
    }

    cl.faceanimtime = cl.time + 0.2; // but sbar face into pain frame

    cl.cshifts[CSHIFT_DAMAGE].percent += 3 * count;
    if(cl.cshifts[CSHIFT_DAMAGE].percent < 0)
    {
        cl.cshifts[CSHIFT_DAMAGE].percent = 0;
    }
    if(cl.cshifts[CSHIFT_DAMAGE].percent > 150)
    {
        cl.cshifts[CSHIFT_DAMAGE].percent = 150;
    }

    if(armor > blood)
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 200;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 100;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 100;
    }
    else if(armor)
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 220;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 50;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 50;
    }
    else
    {
        cl.cshifts[CSHIFT_DAMAGE].destcolor[0] = 255;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[1] = 0;
        cl.cshifts[CSHIFT_DAMAGE].destcolor[2] = 0;
    }

    //
    // calculate view angle kicks
    //
    // check if we're out of vr or if vr viewkick is enabled
    if(!vr_enabled.value || (vr_enabled.value && vr_viewkick.value))
    {
        ent = &cl.entities[cl.viewentity];

        from -= ent->origin;
        from = glm::normalize(from);

        const auto [forward, right, up] =
            quake::util::getAngledVectors(ent->angles);

        side = DotProduct(from, right);
        v_dmg_roll = count * side * v_kickroll.value;

        side = DotProduct(from, forward);
        v_dmg_pitch = count * side * v_kickpitch.value;

        v_dmg_time = v_kicktime.value;
    }
}


/*
==================
V_cshift_f
==================
*/
void V_cshift_f()
{
    cshift_empty.destcolor[0] = atoi(Cmd_Argv(1));
    cshift_empty.destcolor[1] = atoi(Cmd_Argv(2));
    cshift_empty.destcolor[2] = atoi(Cmd_Argv(3));
    cshift_empty.percent = atoi(Cmd_Argv(4));
}


/*
==================
V_BonusFlash_f

When you run over an item, the server sends this command
==================
*/
void V_BonusFlash_f()
{
    cl.cshifts[CSHIFT_BONUS].destcolor[0] = 215;
    cl.cshifts[CSHIFT_BONUS].destcolor[1] = 186;
    cl.cshifts[CSHIFT_BONUS].destcolor[2] = 69;
    cl.cshifts[CSHIFT_BONUS].percent = 50;
}

/*
=============
V_SetContentsColor

Underwater, lava, etc each has a color shift
=============
*/
void V_SetContentsColor(int contents)
{
    switch(contents)
    {
        case CONTENTS_EMPTY:
        case CONTENTS_SOLID:
        case CONTENTS_SKY: // johnfitz -- no blend in sky
            cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
            break;
        case CONTENTS_LAVA: cl.cshifts[CSHIFT_CONTENTS] = cshift_lava; break;
        case CONTENTS_SLIME: cl.cshifts[CSHIFT_CONTENTS] = cshift_slime; break;
        default: cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
    }
}

/*
=============
V_CalcPowerupCshift
=============
*/
void V_CalcPowerupCshift()
{
    if(cl.items & IT_QUAD)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
        cl.cshifts[CSHIFT_POWERUP].percent = 30;
    }
    else if(cl.items & IT_SUIT)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
        cl.cshifts[CSHIFT_POWERUP].percent = 20;
    }
    else if(cl.items & IT_INVISIBILITY)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
        cl.cshifts[CSHIFT_POWERUP].percent = 100;
    }
    else if(cl.items & IT_INVULNERABILITY)
    {
        cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
        cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
        cl.cshifts[CSHIFT_POWERUP].percent = 30;
    }
    else
    {
        cl.cshifts[CSHIFT_POWERUP].percent = 0;
    }
}

/*
=============
V_CalcBlend
=============
*/
void V_CalcBlend()
{
    float r;

    float g;

    float b;

    float a;

    float a2;
    int j;
    cvar_t* cshiftpercent_cvars[NUM_CSHIFTS] = {&gl_cshiftpercent_contents,
        &gl_cshiftpercent_damage, &gl_cshiftpercent_bonus,
        &gl_cshiftpercent_powerup};

    r = 0;
    g = 0;
    b = 0;
    a = 0;

    for(j = 0; j < NUM_CSHIFTS; j++)
    {
        if(!gl_cshiftpercent.value)
        {
            continue;
        }

        // johnfitz -- only apply leaf contents color shifts during intermission
        if(cl.intermission && j != CSHIFT_CONTENTS)
        {
            continue;
        }
        // johnfitz

        a2 = ((cl.cshifts[j].percent * gl_cshiftpercent.value) / 100.0) / 255.0;
        // QuakeSpasm -- also scale by the specific gl_cshiftpercent_* cvar
        a2 *= (cshiftpercent_cvars[j]->value / 100.0);
        // QuakeSpasm
        if(!a2)
        {
            continue;
        }
        a += a2 * (1 - a);
        a2 = a2 / a;
        r = r * (1 - a2) + cl.cshifts[j].destcolor[0] * a2;
        g = g * (1 - a2) + cl.cshifts[j].destcolor[1] * a2;
        b = b * (1 - a2) + cl.cshifts[j].destcolor[2] * a2;
    }

    v_blend[0] = r / 255.0;
    v_blend[1] = g / 255.0;
    v_blend[2] = b / 255.0;
    v_blend[3] = a;
    if(v_blend[3] > 1)
    {
        v_blend[3] = 1;
    }
    if(v_blend[3] < 0)
    {
        v_blend[3] = 0;
    }
}

/*
=============
V_UpdateBlend -- johnfitz -- V_UpdatePalette cleaned up and renamed
=============
*/
void V_UpdateBlend()
{
    int i;

    int j;
    bool blend_changed;

    V_CalcPowerupCshift();

    blend_changed = false;

    for(i = 0; i < NUM_CSHIFTS; i++)
    {
        if(cl.cshifts[i].percent != cl.prev_cshifts[i].percent)
        {
            blend_changed = true;
            cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
        }
        for(j = 0; j < 3; j++)
        {
            if(cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
            {
                blend_changed = true;
                cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
            }
        }
    }

    // drop the damage value
    cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
    if(cl.cshifts[CSHIFT_DAMAGE].percent <= 0)
    {
        cl.cshifts[CSHIFT_DAMAGE].percent = 0;
    }

    // drop the bonus value
    cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
    if(cl.cshifts[CSHIFT_BONUS].percent <= 0)
    {
        cl.cshifts[CSHIFT_BONUS].percent = 0;
    }

    if(blend_changed)
    {
        V_CalcBlend();
    }
}

/*
============
V_PolyBlend -- johnfitz -- moved here from gl_rmain.c, and rewritten to use
glOrtho
============
*/
void V_PolyBlend()
{
    if(!gl_polyblend.value || !v_blend[3])
    {
        return;
    }

    GL_DisableMultitexture();

    glPushAttrib(GL_TRANSFORM_BIT);

    glDisable(GL_ALPHA_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -99999, 99999);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor4fv(v_blend);

    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(1, 0);
    glVertex2f(1, 1);
    glVertex2f(0, 1);
    glEnd();

    glColor3f(1, 1, 1);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_ALPHA_TEST);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glPopAttrib();
}

/*
==============================================================================

    VIEW RENDERING

==============================================================================
*/

float angledelta(float a)
{
    a = anglemod(a);
    if(a > 180)
    {
        a -= 360;
    }
    return a;
}

/*
==================
CalcGunAngle
==================
*/
void CalcGunAngle(const int wpnCvarEntry, entity_t* viewent,
    const qvec3& handrot, const qvec3& visual_handrot, bool horizFlip)
{
    // Skip everything if we're doing VR Controller aiming.
    if(vr_enabled.value &&
        cvarCast<VrAimMode::Enum>(vr_aimmode.value) == VrAimMode::e_CONTROLLER)
    {
        auto [oPitch, oYaw, oRoll] = VR_GetWpnAngleOffsets(wpnCvarEntry);
        auto [vhrPitch, vhrYaw, vhrRoll] = visual_handrot;

        if(horizFlip)
        {
            oYaw *= -1.f;
            oRoll *= -1.f;

            vhrYaw *= -1.f;
            vhrRoll *= -1.f;
        }

        // viewent->angles[PITCH] = -(handrot[PITCH]) + oPitch + vhrPitch;
        // viewent->angles[YAW] = handrot[YAW] + oYaw + vhrYaw;
        // viewent->angles[ROLL] = handrot[ROLL] + oRoll + vhrRoll;

        viewent->angles[PITCH] = -(visual_handrot[PITCH]) + oPitch;
        viewent->angles[YAW] = visual_handrot[YAW] + oYaw;
        viewent->angles[ROLL] = visual_handrot[ROLL] + oRoll;

        return;
    }

    static float oldyaw = 0;
    static float oldpitch = 0;

    float yaw = r_refdef.aimangles[YAW];
    yaw = angledelta(yaw - r_refdef.viewangles[YAW]) * 0.4;

    if(yaw > 10)
    {
        yaw = 10;
    }
    else if(yaw < -10)
    {
        yaw = -10;
    }

    float pitch = -r_refdef.aimangles[PITCH];
    pitch = angledelta(-pitch - r_refdef.viewangles[PITCH]) * 0.4;

    if(pitch > 10)
    {
        pitch = 10;
    }
    else if(pitch < -10)
    {
        pitch = -10;
    }

    const float move = host_frametime * 20;
    if(yaw > oldyaw)
    {
        if(oldyaw + move < yaw)
        {
            yaw = oldyaw + move;
        }
    }
    else
    {
        if(oldyaw - move > yaw)
        {
            yaw = oldyaw - move;
        }
    }

    if(pitch > oldpitch)
    {
        if(oldpitch + move < pitch)
        {
            pitch = oldpitch + move;
        }
    }
    else
    {
        if(oldpitch - move > pitch)
        {
            pitch = oldpitch - move;
        }
    }

    oldyaw = yaw;
    oldpitch = pitch;

    viewent->angles[YAW] = r_refdef.viewangles[YAW] + yaw;
    viewent->angles[PITCH] = -(r_refdef.viewangles[PITCH] + pitch);

    viewent->angles[ROLL] -= v_idlescale.value *
                             sin(cl.time * v_iroll_cycle.value) *
                             v_iroll_level.value;
    viewent->angles[PITCH] -= v_idlescale.value *
                              sin(cl.time * v_ipitch_cycle.value) *
                              v_ipitch_level.value;
    viewent->angles[YAW] -= v_idlescale.value *
                            sin(cl.time * v_iyaw_cycle.value) *
                            v_iyaw_level.value;
}

/*
==============
V_BoundOffsets
==============
*/
void V_BoundOffsets()
{
    entity_t* ent;

    ent = &cl.entities[cl.viewentity];

    // absolutely bound refresh reletive to entity clipping hull
    // so the view can never be inside a solid wall

    if(r_refdef.vieworg[0] < ent->origin[0] - 14)
    {
        r_refdef.vieworg[0] = ent->origin[0] - 14;
    }
    else if(r_refdef.vieworg[0] > ent->origin[0] + 14)
    {
        r_refdef.vieworg[0] = ent->origin[0] + 14;
    }

    if(r_refdef.vieworg[1] < ent->origin[1] - 14)
    {
        r_refdef.vieworg[1] = ent->origin[1] - 14;
    }
    else if(r_refdef.vieworg[1] > ent->origin[1] + 14)
    {
        r_refdef.vieworg[1] = ent->origin[1] + 14;
    }

    if(r_refdef.vieworg[2] < ent->origin[2] - 22)
    {
        r_refdef.vieworg[2] = ent->origin[2] - 22;
    }
    else if(r_refdef.vieworg[2] > ent->origin[2] + 30)
    {
        r_refdef.vieworg[2] = ent->origin[2] + 30;
    }
}

/*
==============
V_AddIdle

Idle swaying
==============
*/
void V_AddIdle()
{
    r_refdef.viewangles[ROLL] += v_idlescale.value *
                                 sin(cl.time * v_iroll_cycle.value) *
                                 v_iroll_level.value;
    r_refdef.viewangles[PITCH] += v_idlescale.value *
                                  sin(cl.time * v_ipitch_cycle.value) *
                                  v_ipitch_level.value;
    r_refdef.viewangles[YAW] += v_idlescale.value *
                                sin(cl.time * v_iyaw_cycle.value) *
                                v_iyaw_level.value;
}


/*
==============
V_CalcViewRoll

Roll is induced by movement and damage
==============
*/
void V_CalcViewRoll()
{
    float side;

    side = V_CalcRoll(cl.entities[cl.viewentity].angles, cl.velocity);
    r_refdef.viewangles[ROLL] += side;

    if(v_dmg_time > 0)
    {
        r_refdef.viewangles[ROLL] += v_dmg_time / v_kicktime.value * v_dmg_roll;
        r_refdef.viewangles[PITCH] +=
            v_dmg_time / v_kicktime.value * v_dmg_pitch;
        v_dmg_time -= host_frametime;
    }

    if(cl.stats[STAT_HEALTH] <= 0 && !VR_EnabledAndNotFake())
    {
        r_refdef.viewangles[ROLL] = 80; // dead view angle
        return;
    }
}

/*
==================
V_CalcIntermissionRefdef

==================
*/
void V_CalcIntermissionRefdef()
{
    entity_t* ent;
    entity_t* view;
    float old;

    // ent is the player model (visible when out of body)
    ent = &cl.entities[cl.viewentity];
    // view is the weapon model (only visible from inside body)
    view = &cl.viewent;

    r_refdef.vieworg = ent->origin;
    r_refdef.viewangles = ent->angles;
    view->model = nullptr;

    if(VR_EnabledAndNotFake())
    {
        r_refdef.viewangles[PITCH] = 0;
        r_refdef.aimangles = r_refdef.viewangles;
        r_refdef.viewangles =
            VR_AddOrientationToViewAngles(r_refdef.viewangles);
        VR_SetAngles(r_refdef.viewangles);
    }

    // allways idle in intermission
    old = v_idlescale.value;
    v_idlescale.value = 1;
    V_AddIdle();
    v_idlescale.value = old;
}

static float playerOldZ = 0;

// TODO VR: (P2) add to vr view? the onground evaluates to false, this is
// why it doesnt work smooth out stair step ups - seems to work for weapon
// viewmodel
static void StairSmoothView(float& oldz, const entity_t* ent, entity_t* view)
{
    if(!noclip_anglehack && cl.onground && ent->origin[2] - oldz > 0)
    {
        // johnfitz -- added exception for noclip
        // FIXME: noclip_anglehack is set on the server, so in a nonlocal game
        // this won't work.

        auto steptime = cl.time - cl.oldtime;
        if(steptime < 0)
        {
            // FIXME	I_Error ("steptime < 0");
            steptime = 0;
        }

        oldz += steptime * 80;

        if(oldz > ent->origin[2])
        {
            oldz = ent->origin[2];
        }

        if(ent->origin[2] - oldz > 12)
        {
            oldz = ent->origin[2] - 12;
        }

        // r_refdef.vieworg[2] += oldz - ent->origin[2];
        view->origin[2] += oldz - ent->origin[2];
    }
    else
    {
        oldz = ent->origin[2];
    }
}

/*
==================
V_CalcRefdef
==================
*/
void V_CalcRefdef(
    const int cvarEntry, const qvec3& handpos, const qvec3& gunOffset)
{
    static qvec3 punch{vec3_zero}; // johnfitz -- v_gunkick

    V_DriftPitch();

    // ent is the player model (visible when out of body)
    entity_t* ent = &cl.entities[cl.viewentity];

    // view is the weapon model (only visible from inside body)
    entity_t* view = &cl.viewent;

    // transform the view offset by the model's matrix to get the offset from
    // model origin for the view

    ent->angles[YAW] = VR_GetBodyYawAngle();

    // the model should face the view dir
    ent->angles[PITCH] = -cl.viewangles[PITCH];

    float bob = V_CalcBob();

    // refresh position
    if(VR_EnabledAndNotFake())
    {
        extern qvec3 vr_viewOffset;
        r_refdef.vieworg = ent->origin + vr_viewOffset;
    }
    else
    {
        r_refdef.vieworg = ent->origin;
        r_refdef.vieworg[2] += cl.stats[STAT_VIEWHEIGHT] + bob;
    }

    // never let it sit exactly on a node line, because a water plane can
    // dissapear when viewed with the eye exactly on it.
    // the server protocol only specifies to 1/16 pixel, so add 1/32 in each
    // axis
    r_refdef.vieworg[0] += 1.0 / 32;
    r_refdef.vieworg[1] += 1.0 / 32;
    r_refdef.vieworg[2] += 1.0 / 32;

    r_refdef.viewangles = cl.viewangles;
    V_CalcViewRoll();
    V_AddIdle();

    // offsets
    qvec3 angles;
    angles[PITCH] =
        -ent->angles[PITCH]; // because entity pitches are actually backward
    angles[YAW] = ent->angles[YAW];
    angles[ROLL] = ent->angles[ROLL];

    const auto [forward, right, up] = quake::util::getAngledVectors(angles);

    if(cl.maxclients <= 1)
    {
        // johnfitz -- moved cheat-protection here from V_RenderView
        for(int i = 0; i < 3; i++)
        {
            r_refdef.vieworg[i] += scr_ofsx.value * forward[i] +
                                   scr_ofsy.value * right[i] +
                                   scr_ofsz.value * up[i];
        }
    }

    if(!vr_enabled.value)
    {
        V_BoundOffsets();
    }

    // set up gun position
    view->angles = cl.viewangles;

    CalcGunAngle(cvarEntry, view, cl.handrot[cVR_MainHand],
        cl.visual_handrot[cVR_MainHand], false);

    // VR controller aiming configuration
    if(vr_enabled.value &&
        cvarCast<VrAimMode::Enum>(vr_aimmode.value) == VrAimMode::e_CONTROLLER)
    {
        // VR: This sets the weapon model's position.
        view->origin = handpos + cl.vmeshoffset + gunOffset;
    }
    else
    {
        view->origin = ent->origin;
        view->origin[2] += cl.stats[STAT_VIEWHEIGHT];

        for(int i = 0; i < 3; i++)
        {
            view->origin[i] += forward[i] * bob * 0.4;
        }

        view->origin[2] += bob;
    }

    // johnfitz -- removed all gun position fudging code (was used to keep
    // gun from getting covered by sbar) MarkV -- restored this with
    // r_viewmodel_quake cvar
    if(r_viewmodel_quake.value)
    {
        if(scr_viewsize.value == 110)
        {
            view->origin[2] += 1;
        }
        else if(scr_viewsize.value == 100)
        {
            view->origin[2] += 2;
        }
        else if(scr_viewsize.value == 90)
        {
            view->origin[2] += 1;
        }
        else if(scr_viewsize.value == 80)
        {
            view->origin[2] += 0.5;
        }
    }

    view->model = cl.model_precache[cl.stats[STAT_WEAPON]];

    // TODO VR: (P2) hack
    const auto isHandMdl = [](const char* mdlname)
    {
        return (strcmp(mdlname, "progs/hand.mdl") == 0) ||
               (strcmp(mdlname, "progs/hand_base.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_thumb.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_index.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_middle.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_ring.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_pinky.mdl") == 0);
    };

    if(view->model && isHandMdl(view->model->name))
    {
        view->hidden = true;
    }
    else
    {
        view->hidden = false;
    }

    view->frame = cl.stats[STAT_WEAPONFRAME];
    view->colormap = vid.colormap;

    // johnfitz -- v_gunkick
    if(v_gunkick.value == 1 && !(vr_enabled.value && !vr_viewkick.value))
    {
        // original quake kick
        r_refdef.viewangles += cl.punchangle;
    }

    if(v_gunkick.value == 2 && !(vr_enabled.value && !vr_viewkick.value))
    {
        // lerped kick
        for(int i = 0; i < 3; i++)
        {
            if(punch[i] != v_punchangles[0][i])
            {
                // speed determined by how far we need to lerp in 1/10th of
                // a second
                float delta = (v_punchangles[0][i] - v_punchangles[1][i]) *
                              host_frametime * 10;

                if(delta > 0)
                {
                    punch[i] = q_min(punch[i] + delta, v_punchangles[0][i]);
                }
                else if(delta < 0)
                {
                    punch[i] = q_max(punch[i] + delta, v_punchangles[0][i]);
                }
            }
        }

        r_refdef.viewangles += punch;
    }
    // johnfitz

    StairSmoothView(playerOldZ, ent, view);

    if(chase_active.value)
    {
        Chase_UpdateForDrawing(r_refdef, view); // johnfitz
    }
}

void V_SetupOffHandWpnViewEnt(
    const int cvarEntry, const qvec3& handpos, const qvec3& gunOffset)
{
    // view is the weapon model (only visible from inside body)
    entity_t& view = cl.offhand_viewent;

    // set up gun position
    CalcGunAngle(cvarEntry, &view, cl.handrot[cVR_OffHand],
        cl.visual_handrot[cVR_OffHand], true);

    // VR controller aiming configuration
    if(vr_enabled.value &&
        cvarCast<VrAimMode::Enum>(vr_aimmode.value) == VrAimMode::e_CONTROLLER)
    {
        view.origin = handpos + cl.vmeshoffset + gunOffset;
    }
    else
    {
        // No off-hand without VR.
    }

    view.model = cl.model_precache[cl.stats[STAT_WEAPONMODEL2]];

    // TODO VR: (P2) hack
    const auto isHandMdl = [](const char* mdlname)
    {
        return (strcmp(mdlname, "progs/hand.mdl") == 0) ||
               (strcmp(mdlname, "progs/hand_base.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_thumb.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_index.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_middle.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_ring.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_pinky.mdl") == 0);
    };

    if(view.model && isHandMdl(view.model->name))
    {
        view.hidden = true;
    }
    else
    {
        view.hidden = false;
    }

    view.frame = cl.stats[STAT_WEAPONFRAME2];
    view.colormap = vid.colormap;
    view.horizFlip = true;

    StairSmoothView(playerOldZ, &cl.entities[cl.viewentity], &view);

    if(chase_active.value)
    {
        Chase_UpdateForDrawing(r_refdef, &view); // johnfitz
    }
}

void V_SetupVRTorsoViewEnt()
{
    entity_t& view = cl.vrtorso;

    const qvec3 playerYawOnly{0, VR_GetBodyYawAngle(), 0};
    const auto [vFwd, vRight, vUp] =
        quake::util::getAngledVectors(playerYawOnly);

    const auto heightRatio =
        quake::util::qclamp(VR_GetCrouchRatio(), 0._qf, 0.8_qf);

    view.angles[PITCH] = 0.f + vr_vrtorso_pitch.value - (heightRatio * 35._qf);
    view.angles[YAW] = VR_GetBodyYawAngle() + vr_vrtorso_yaw.value;
    view.angles[ROLL] = 0._qf + vr_vrtorso_roll.value;

    view.model = Mod_ForName("progs/vrtorso.mdl", true);
    view.frame = 0;
    view.colormap = vid.colormap;

    view.origin = cl.entities[cl.viewentity].origin;
    view.origin += vFwd * qfloat(vr_vrtorso_x_offset.value);
    view.origin -= vFwd * (heightRatio * 14._qf);
    view.origin += vRight * qfloat(vr_vrtorso_y_offset.value);
    view.origin[2] += VR_GetHeadOrigin()[2] * vr_vrtorso_head_z_mult.value;
    view.origin[2] += vr_vrtorso_z_offset.value;

    StairSmoothView(playerOldZ, &cl.entities[cl.viewentity], &view);
}

void V_SetupHolsterSlotViewEnt(const int hotspot, const qvec3& pos,
    entity_t* view, const qfloat pitch, const qfloat yaw, const qfloat roll,
    const bool horizFlip)
{
    const bool hovered = (cl.hotspot[cVR_OffHand] == hotspot) ||
                         (cl.hotspot[cVR_MainHand] == hotspot);

    if(hovered)
    {
        view->lightmod = EntityLightModifier::Multiply;
        view->lightmodvalue = {6.f, 6.f, 6.f};
    }
    else
    {
        view->lightmod = EntityLightModifier::None;
    }

    view->angles[PITCH] = pitch;
    view->angles[YAW] = yaw;
    view->angles[ROLL] = roll;

    view->origin = pos;
    view->model = Mod_ForName("progs/legholster.mdl", true);

    view->frame = 0;
    view->colormap = vid.colormap;

    view->horizFlip = horizFlip;

    StairSmoothView(playerOldZ, &cl.entities[cl.viewentity], view);

    if(chase_active.value)
    {
        Chase_UpdateForDrawing(r_refdef, view); // johnfitz
    }
}

void V_SetupHolsterViewEnt(const int hotspot, const int modelId,
    const qvec3& pos, entity_t* view, const qfloat pitch, const qfloat yaw,
    const qfloat roll, const bool horizFlip)
{
    const bool hovered = (cl.hotspot[cVR_OffHand] == hotspot) ||
                         (cl.hotspot[cVR_MainHand] == hotspot);

    view->angles[PITCH] = pitch;
    view->angles[YAW] = yaw;
    view->angles[ROLL] = roll;

    if(hovered)
    {
        view->lightmod = EntityLightModifier::Multiply;
        view->lightmodvalue = {6.f, 6.f, 6.f};
    }
    else
    {
        view->lightmod = EntityLightModifier::None;
    }

    view->origin = pos;
    view->model = cl.model_precache[modelId];

    // TODO VR: (P2) hack
    const auto isHandMdl = [](const char* mdlname)
    {
        return (strcmp(mdlname, "progs/hand.mdl") == 0) ||
               (strcmp(mdlname, "progs/hand_base.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_thumb.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_index.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_middle.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_ring.mdl") == 0) ||
               (strcmp(mdlname, "progs/finger_pinky.mdl") == 0);
    };

    if(view->model && isHandMdl(view->model->name))
    {
        view->model = nullptr;
    }

    view->frame = 0;
    view->colormap = vid.colormap;

    view->horizFlip = horizFlip;

    if(chase_active.value)
    {
        Chase_UpdateForDrawing(r_refdef, view); // johnfitz
    }
}

static const char* fingerIdxToModelName(const FingerIdx fingerIdx)
{
    if(fingerIdx == FingerIdx::Thumb)
    {
        return "progs/finger_thumb.mdl";
    }

    if(fingerIdx == FingerIdx::Index)
    {
        return "progs/finger_index.mdl";
    }

    if(fingerIdx == FingerIdx::Middle)
    {
        return "progs/finger_middle.mdl";
    }

    if(fingerIdx == FingerIdx::Ring)
    {
        return "progs/finger_ring.mdl";
    }

    if(fingerIdx == FingerIdx::Pinky)
    {
        return "progs/finger_pinky.mdl";
    }

    assert(fingerIdx == FingerIdx::Base);
    return "progs/hand_base.mdl";
};

static qvec3 fingerIdxToOffset(const FingerIdx fingerIdx, const int handIdx)
{
    qvec3 result = quake::vr::get_fingers_and_base_xyz();

    if(handIdx == cVR_OffHand)
    {
        result += quake::vr::get_fingers_and_base_offhand_xyz();
    }

    if(fingerIdx == FingerIdx::Base)
    {
        return result + quake::vr::get_finger_base_xyz();
    }

    result += quake::vr::get_fingers_xyz();

    if(fingerIdx == FingerIdx::Thumb)
    {
        result += quake::vr::get_finger_thumb_xyz();
    }
    else if(fingerIdx == FingerIdx::Index)
    {
        result += quake::vr::get_finger_index_xyz();
    }
    else if(fingerIdx == FingerIdx::Middle)
    {
        result += quake::vr::get_finger_middle_xyz();
    }
    else if(fingerIdx == FingerIdx::Ring)
    {
        result += quake::vr::get_finger_ring_xyz();
    }
    else
    {
        assert(fingerIdx == FingerIdx::Pinky);
        result += quake::vr::get_finger_pinky_xyz();
    }

    return result;
}

static void V_SetupHandViewEnt(const FingerIdx fingerIdx, int anchorWpnCvar,
    entity_t* const anchor, entity_t* const hand, qvec3 handRot,
    const qvec3& extraOffset, const bool horizFlip, const bool ghost)
{
    assert(anchor->model != nullptr);

    auto [oPitch, oYaw, oRoll] =
        VR_GetWpnAngleOffsets(vr_hardcoded_wpn_cvar_fist);

    if(horizFlip)
    {
        oYaw *= -1.f;
        oRoll *= -1.f;
    }

    handRot[PITCH] += oPitch;
    handRot[YAW] += oYaw;
    handRot[ROLL] += oRoll;

    if(ghost)
    {
        anchorWpnCvar = vr_hardcoded_wpn_cvar_fist;
    }

    const auto extraOffsets = VR_GetWpnHandOffsets(anchorWpnCvar) + extraOffset;

    const int anchorVertex = static_cast<int>(
        VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::HandAnchorVertex));

    const bool hideHand =
        static_cast<bool>(VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::HideHand));

    const int handIdx = horizFlip ? cVR_OffHand : cVR_MainHand;

    auto pos = VR_GetScaledAndAngledAliasVertexPosition(
        anchor, anchorVertex, extraOffsets, handRot, horizFlip);

    if(anchorWpnCvar == vr_hardcoded_wpn_cvar_fist)
    {
        pos = cl.handpos[handIdx];
    }

    const int otherHandIdx = VR_OtherHand(handIdx);
    const auto otherWpnCvar = VR_GetWpnCvarEntry(otherHandIdx);

    const bool otherWpnTwoHDisplayModeFixed =
        quake::util::cvarToEnum<Wpn2HDisplayMode>(VR_GetWpnCVar(
            otherWpnCvar, WpnCVar::TwoHDisplayMode)) == Wpn2HDisplayMode::Fixed;

    if(otherWpnTwoHDisplayModeFixed && vr_2h_aim_transition[otherHandIdx] > 0.f)
    {
        // Blend when leaving 2H mode

        entity_t* const otherAnchor =
            anchor == &cl.viewent ? &cl.offhand_viewent : &cl.viewent;

        const auto twoHFixedPos = VR_GetWpnFixed2HFinalPosition(otherAnchor,
            otherWpnCvar, horizFlip, extraOffset, cl.handrot[otherHandIdx]);

        hand->origin =
            glm::mix(pos, twoHFixedPos, vr_2h_aim_transition[otherHandIdx]);
    }
    else
    {
        hand->origin = pos;
    }

    auto foff = fingerIdxToOffset(fingerIdx, handIdx);
    if(horizFlip)
    {
        foff[1] *= -1.f;
    }

    hand->origin += quake::util::redirectVector(foff, handRot);
    hand->model = Mod_ForName(fingerIdxToModelName(fingerIdx), true);

    {
        const auto handHdr = (aliashdr_t*)Mod_Extradata(hand->model);
        // TODO VR: (P2) hardcoded fist cvar number
        ApplyMod_Weapon(vr_hardcoded_wpn_cvar_fist, handHdr);
    }

    hand->hidden = hideHand;
    hand->frame = vr_fingertracking_frame[(int)handIdx][(int)fingerIdx];
    hand->colormap = vid.colormap;
    hand->horizFlip = horizFlip;

    hand->angles[PITCH] = -handRot[PITCH];
    hand->angles[YAW] = handRot[YAW];
    hand->angles[ROLL] = handRot[ROLL];
}

static void V_SetupFixedHelpingHandViewEnt(const FingerIdx fingerIdx,
    const int helpingHand, const int otherWpnCvar, entity_t* const anchor,
    entity_t* const hand, const qvec3& handRot, qvec3 otherHandRot,
    const qvec3& extraOffset, const bool horizFlip)
{
    (void)handRot;

    assert(anchor->model != nullptr);

    auto [oPitch, oYaw, oRoll] = VR_GetWpnFixed2HHandAngles(otherWpnCvar);

    if(!horizFlip)
    {
        oYaw *= -1.f;
        oRoll *= -1.f;
    }

    otherHandRot[PITCH] += oPitch;
    otherHandRot[YAW] += oYaw;
    otherHandRot[ROLL] += oRoll;

    const auto pos = VR_GetWpnFixed2HFinalPosition(
        anchor, otherWpnCvar, horizFlip, extraOffset, otherHandRot);

    hand->origin = glm::mix(cl.handpos[helpingHand], pos,
        vr_2h_aim_transition[VR_OtherHand(helpingHand)]);

    const int handIdx = horizFlip ? cVR_OffHand : cVR_MainHand;

    auto foff = fingerIdxToOffset(fingerIdx, handIdx);
    if(horizFlip)
    {
        foff[1] *= -1.f;
    }

    hand->origin += quake::util::redirectVector(foff, otherHandRot);
    hand->model = Mod_ForName(fingerIdxToModelName(fingerIdx), true);

    {
        const auto handHdr = (aliashdr_t*)Mod_Extradata(hand->model);
        // TODO VR: (P2) hardcoded fist cvar number
        ApplyMod_Weapon(vr_hardcoded_wpn_cvar_fist, handHdr);
    }

    hand->frame = vr_fingertracking_frame[(int)handIdx][(int)fingerIdx];
    hand->colormap = vid.colormap;
    hand->horizFlip = horizFlip;

    hand->angles[PITCH] = -otherHandRot[PITCH];
    hand->angles[YAW] = otherHandRot[YAW];
    hand->angles[ROLL] = otherHandRot[ROLL];
}

static void V_SetupWpnButtonViewEnt(const int anchorWpnCvar,
    entity_t* const anchor, entity_t* const wpnButton, const qvec3& handRot,
    const qvec3& extraOffset, const bool horizFlip)
{
    assert(anchor->model != nullptr);

    auto extraOffsets = VR_GetWpnButtonOffsets(anchorWpnCvar) + extraOffset;

    if(horizFlip)
    {
        // extraOffsets[1] *= -1.f;
    }

    const int anchorVertex = static_cast<int>(
        VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::WpnButtonAnchorVertex));

    const bool hideButton =
        static_cast<WpnButtonMode>(VR_GetWpnCVarValue(
            anchorWpnCvar, WpnCVar::WpnButtonMode)) == WpnButtonMode::None;

    const qvec3 pos = VR_GetScaledAndAngledAliasVertexPosition(
        anchor, anchorVertex, extraOffsets, handRot, horizFlip);

    // const int handIdx = horizFlip ? cVR_OffHand : cVR_MainHand;

    qvec3 angles = VR_GetWpnButtonAngles(anchorWpnCvar);
    if(horizFlip)
    {
        angles[ROLL] *= -1.f;
    }

    wpnButton->origin = pos;
    wpnButton->model = Mod_ForName("progs/wpnbutton.mdl", true);
    wpnButton->hidden = hideButton;
    wpnButton->frame = 0;
    wpnButton->colormap = vid.colormap;
    wpnButton->horizFlip = horizFlip;
    wpnButton->angles = handRot + angles;
    wpnButton->angles[PITCH] *= -1.f;

    // TODO VR: (P2) hardcoded fist cvar number
    // if(hand->model != nullptr)
    // {
    //     const auto handHdr = (aliashdr_t*)Mod_Extradata(hand->model);
    //     ApplyMod_Weapon(vr_hardcoded_wpn_cvar_fist, handHdr);
    // }
}

static void V_SetupWpnTextViewEnt(const int anchorWpnCvar,
    entity_t* const anchor, textentity_t* const wpnText, const qvec3& handRot,
    const qvec3& extraOffset, const bool horizFlip)
{
    assert(anchor->model != nullptr);

    auto extraOffsets = VR_GetWpnTextOffsets(anchorWpnCvar) + extraOffset;

    const int anchorVertex = static_cast<int>(
        VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::WpnTextAnchorVertex));

    const bool hideText =
        static_cast<WpnTextMode>(VR_GetWpnCVarValue(
            anchorWpnCvar, WpnCVar::WpnTextMode)) == WpnTextMode::None;

    const qvec3 pos = VR_GetScaledAndAngledAliasVertexPosition(
        anchor, anchorVertex, extraOffsets, handRot, horizFlip);

    qvec3 angles = VR_GetWpnTextAngles(anchorWpnCvar);
    if(horizFlip)
    {
        angles[ROLL] *= -1.f;
    }

    wpnText->origin = pos;
    wpnText->hidden = hideText;
    wpnText->horizFlip = horizFlip;
    wpnText->angles = handRot + angles;
    wpnText->scale = VR_GetWpnCVarValue(anchorWpnCvar, WpnCVar::WpnTextScale);
}

static void V_RenderView_WeaponModels()
{
    // -------------------------------------------------------------------
    // VR: Setup main hand weapon, player model entity, and refdef.
    {
        const auto cvarEntry = VR_GetMainHandWpnCvarEntry();
        const auto gunOffset = VR_GetWpnGunOffsets(cvarEntry);

        V_CalcRefdef(cvarEntry, cl.handpos[cVR_MainHand], gunOffset);
    }

    // -------------------------------------------------------------------
    // VR: Setup off hand weapon.
    {
        const auto cvarEntry = VR_GetOffHandWpnCvarEntry();
        auto gunOffset = VR_GetWpnGunOffsets(cvarEntry);
        gunOffset[1] *= -1.f;

        V_SetupOffHandWpnViewEnt(cvarEntry, cl.handpos[cVR_OffHand], gunOffset);
    }
}

static void V_RenderView_HolsteredWeaponModels()
{
    // -------------------------------------------------------------------
    // VR: Setup holstered weapons.
    const auto playerBodyYaw = VR_GetBodyYawAngle();

    V_SetupHolsterViewEnt(QVR_HS_LEFT_HIP_HOLSTER,
        cl.stats[STAT_HOLSTERWEAPONMODEL2], VR_GetLeftHipPos(),
        &cl.left_hip_holster, -90.f, 0.f, -playerBodyYaw + 10.f, true);

    V_SetupHolsterViewEnt(QVR_HS_RIGHT_HIP_HOLSTER,
        cl.stats[STAT_HOLSTERWEAPONMODEL3], VR_GetRightHipPos(),
        &cl.right_hip_holster, -90.f, 0.f, -playerBodyYaw - 10.f, false);

    V_SetupHolsterViewEnt(QVR_HS_LEFT_UPPER_HOLSTER,
        cl.stats[STAT_HOLSTERWEAPONMODEL4], VR_GetLeftUpperPos(),
        &cl.left_upper_holster, -20.f, playerBodyYaw + 180.f, 0.f, true);

    V_SetupHolsterViewEnt(QVR_HS_RIGHT_UPPER_HOLSTER,
        cl.stats[STAT_HOLSTERWEAPONMODEL5], VR_GetRightUpperPos(),
        &cl.right_upper_holster, -20.f, playerBodyYaw + 180.f, 0.f, false);
}

static void V_RenderView_HolsterModels()
{
    // -------------------------------------------------------------------
    // VR: Setup holsters.
    const auto playerBodyYaw = VR_GetBodyYawAngle();

    // TODO VR: (P2) code repetition between holsters and slots
    if(vr_leg_holster_model_enabled.value)
    {
        V_SetupHolsterSlotViewEnt(QVR_HS_LEFT_HIP_HOLSTER, VR_GetLeftHipPos(),
            &cl.left_hip_holster_slot, -0.f, playerBodyYaw - 10.f, 0.f, true);

        V_SetupHolsterSlotViewEnt(QVR_HS_RIGHT_HIP_HOLSTER, VR_GetRightHipPos(),
            &cl.right_hip_holster_slot, -0.f, playerBodyYaw + 10.f, 0.f, false);

        V_SetupHolsterSlotViewEnt(QVR_HS_LEFT_UPPER_HOLSTER,
            VR_GetLeftUpperPos(), &cl.left_upper_holster_slot, -30.f,
            playerBodyYaw - 10.f, 0.f, true);

        V_SetupHolsterSlotViewEnt(QVR_HS_RIGHT_UPPER_HOLSTER,
            VR_GetRightUpperPos(), &cl.right_upper_holster_slot, -30.f,
            playerBodyYaw + 10.f, 0.f, false);
    }
}

static void V_RenderView_HandModels()
{
    const auto doHand =
        [&](const FingerIdx fingerIdx, entity_t* wpnEnt, const int wpnCvar,
            entity_t* otherWpnEnt, entity_t* handEnt, const int hand,
            const qvec3& extraOffset, const bool horizFlip, const bool ghost)
    {
        if(otherWpnEnt->model != nullptr)
        {
            const int otherWpnCvar = VR_GetWpnCVarFromModel(otherWpnEnt->model);

            const bool twoHDisplayModeFixed =
                quake::util::cvarToEnum<Wpn2HDisplayMode>(
                    VR_GetWpnCVar(otherWpnCvar, WpnCVar::TwoHDisplayMode)) ==
                Wpn2HDisplayMode::Fixed;

            const bool inFixed2HAiming =
                VR_IsActive2HHelpingHand(hand) && twoHDisplayModeFixed;

            if(inFixed2HAiming)
            {
                // setup zeroblend
                otherWpnEnt->zeroBlend =
                    VR_GetWpnCVarValue(otherWpnCvar, WpnCVar::TwoHZeroBlend);

                const auto otherHand = VR_OtherHand(hand);

                V_SetupFixedHelpingHandViewEnt(fingerIdx, hand, otherWpnCvar,
                    otherWpnEnt, handEnt, cl.handrot[hand],
                    cl.handrot[otherHand], extraOffset, horizFlip);

                return;
            }

            // setup zeroblend
            otherWpnEnt->zeroBlend =
                VR_GetWpnCVarValue(otherWpnCvar, WpnCVar::ZeroBlend);
        }

        V_SetupHandViewEnt(fingerIdx, wpnCvar, wpnEnt, handEnt,
            cl.handrot[hand], extraOffset, horizFlip, ghost);
    };

    const auto doHandEntities = [&](auto& handEntities, entity_t& wpnEnt,
                                    const int handIdx, const qvec3& extraOffset,
                                    const bool horizFlip, const bool ghost)
    {
        if(wpnEnt.model == nullptr)
        {
            return;
        }

        const int wpnCvar = VR_GetWpnCVarFromModel(wpnEnt.model);

        entity_t& otherWpnEnt =
            &wpnEnt == &cl.viewent ? cl.offhand_viewent : cl.viewent;

        const auto doHandImpl =
            [&](const FingerIdx fingerIdx, entity_t& handEnt)
        {
            doHand(fingerIdx, &wpnEnt, wpnCvar, &otherWpnEnt, &handEnt, handIdx,
                extraOffset, horizFlip, ghost);
        };

        doHandImpl(FingerIdx::Base, handEntities.base);
        doHandImpl(FingerIdx::Thumb, handEntities.f_thumb);
        doHandImpl(FingerIdx::Index, handEntities.f_index);
        doHandImpl(FingerIdx::Middle, handEntities.f_middle);
        doHandImpl(FingerIdx::Ring, handEntities.f_ring);
        doHandImpl(FingerIdx::Pinky, handEntities.f_pinky);
    };

    // -------------------------------------------------------------------
    // VR: Setup main hand.
    doHandEntities(cl.right_hand_entities, cl.viewent, cVR_MainHand, vec3_zero,
        false, false);

    if(false)
    { // TODO VR: (P1) cvar, transparency
        doHandEntities(cl.right_hand_ghost_entities, cl.viewent, cVR_MainHand,
            vec3_zero, false, true);
    }

    // -------------------------------------------------------------------
    // VR: Setup off hand.
    doHandEntities(cl.left_hand_entities, cl.offhand_viewent, cVR_OffHand,
        vec3_zero, true, false);

    if(false)
    { // TODO VR: (P1) cvar, transparency
        doHandEntities(cl.left_hand_ghost_entities, cl.offhand_viewent,
            cVR_OffHand, vec3_zero, true, true);
    }
}

static void V_RenderView_VRTorsoModel()
{
    // -------------------------------------------------------------------
    // VR: Setup VR torso.
    if(vr_vrtorso_enabled.value == 1)
    {
        V_SetupVRTorsoViewEnt();
    }
}

static void V_RenderView_WeaponButtonModels()
{
    // -------------------------------------------------------------------
    // VR: Setup weapon buttons.
    const auto doWpnButton = [&](entity_t* wpnEnt, entity_t* buttonEnt,
                                 const int hand, const qvec3& extraOffset,
                                 const bool horizFlip)
    {
        if(wpnEnt->model == nullptr)
        {
            return;
        }

        const int wpnCvar = VR_GetWpnCVarFromModel(wpnEnt->model);

        V_SetupWpnButtonViewEnt(wpnCvar, wpnEnt, buttonEnt, cl.handrot[hand],
            extraOffset, horizFlip);
    };

    doWpnButton(
        &cl.viewent, &cl.mainhand_wpn_button, cVR_MainHand, vec3_zero, false);
    doWpnButton(&cl.offhand_viewent, &cl.offhand_wpn_button, cVR_OffHand,
        vec3_zero, true);
}

static void V_RenderView_WeaponText()
{
    // -------------------------------------------------------------------
    // VR: Setup weapon text.
    const auto doWpnText = [&](entity_t* wpnEnt, textentity_t* textEnt,
                               const int hand, const qvec3& extraOffset,
                               const bool horizFlip)
    {
        if(wpnEnt->model == nullptr)
        {
            return;
        }

        const int wpnCvar = VR_GetWpnCVarFromModel(wpnEnt->model);

        V_SetupWpnTextViewEnt(
            wpnCvar, wpnEnt, textEnt, cl.handrot[hand], extraOffset, horizFlip);
    };

    doWpnText(
        &cl.viewent, &cl.mainhand_wpn_text, cVR_MainHand, vec3_zero, false);
    doWpnText(&cl.offhand_viewent, &cl.offhand_wpn_text, cVR_OffHand, vec3_zero,
        true);
}

/*
==================
V_RenderView

The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
the entity origin, so any view position inside that will be valid
==================
*/
void V_RenderView()
{
    if(con_forcedup)
    {
        return;
    }

    if(cl.intermission)
    {
        V_CalcIntermissionRefdef();
        R_RenderView();
    }
    else if(!cl.paused /* && (cl.maxclients > 1 || key_dest == key_game) */)
    {
        V_RenderView_WeaponModels();
        V_RenderView_HolsteredWeaponModels();
        V_RenderView_HolsterModels();
        V_RenderView_HandModels();
        V_RenderView_VRTorsoModel();
        V_RenderView_WeaponButtonModels();
        V_RenderView_WeaponText();

        R_RenderView();
    }

    // johnfitz -- removed lcd code
    V_PolyBlend(); // johnfitz -- moved here from R_Renderview ();
}

/*
==============================================================================

    INIT

==============================================================================
*/

/*
=============
V_Init
=============
*/
void V_Init()
{
    Cmd_AddCommand("v_cshift", V_cshift_f);
    Cmd_AddCommand("bf", V_BonusFlash_f);
    Cmd_AddCommand("centerview", V_StartPitchDrift);

    Cvar_RegisterVariable(&v_centermove);
    Cvar_RegisterVariable(&v_centerspeed);

    Cvar_RegisterVariable(&v_iyaw_cycle);
    Cvar_RegisterVariable(&v_iroll_cycle);
    Cvar_RegisterVariable(&v_ipitch_cycle);
    Cvar_RegisterVariable(&v_iyaw_level);
    Cvar_RegisterVariable(&v_iroll_level);
    Cvar_RegisterVariable(&v_ipitch_level);

    Cvar_RegisterVariable(&v_idlescale);
    Cvar_RegisterVariable(&crosshair);
    Cvar_RegisterVariable(&gl_cshiftpercent);
    Cvar_RegisterVariable(&gl_cshiftpercent_contents); // QuakeSpasm
    Cvar_RegisterVariable(&gl_cshiftpercent_damage);   // QuakeSpasm
    Cvar_RegisterVariable(&gl_cshiftpercent_bonus);    // QuakeSpasm
    Cvar_RegisterVariable(&gl_cshiftpercent_powerup);  // QuakeSpasm

    Cvar_RegisterVariable(&scr_ofsx);
    Cvar_RegisterVariable(&scr_ofsy);
    Cvar_RegisterVariable(&scr_ofsz);
    Cvar_RegisterVariable(&cl_rollspeed);
    Cvar_RegisterVariable(&cl_rollangle);
    Cvar_RegisterVariable(&cl_bob);
    Cvar_RegisterVariable(&cl_bobcycle);
    Cvar_RegisterVariable(&cl_bobup);

    Cvar_RegisterVariable(&v_kicktime);
    Cvar_RegisterVariable(&v_kickroll);
    Cvar_RegisterVariable(&v_kickpitch);
    Cvar_RegisterVariable(&v_gunkick);         // johnfitz
    Cvar_RegisterVariable(&r_viewmodel_quake); // MarkV
}
