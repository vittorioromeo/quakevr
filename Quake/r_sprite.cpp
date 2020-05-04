/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
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
// r_sprite.c -- sprite model rendering

#include "quakedef.hpp"
#include "quakeglm.hpp"
#include "util.hpp"

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t* R_GetSpriteFrame(entity_t* currentent)
{
    msprite_t* psprite;
    mspritegroup_t* pspritegroup;
    mspriteframe_t* pspriteframe;
    int i;

    int numframes;

    int frame;
    float* pintervals;

    float fullinterval;

    float targettime;

    float time;

    psprite = (msprite_t*)currentent->model->cache.data;
    frame = currentent->frame;

    if((frame >= psprite->numframes) || (frame < 0))
    {
        Con_DPrintf("R_DrawSprite: no such frame %d for '%s'\n", frame,
            currentent->model->name);
        frame = 0;
    }

    if(psprite->frames[frame].type == SPR_SINGLE)
    {
        pspriteframe = psprite->frames[frame].frameptr;
    }
    else
    {
        pspritegroup = (mspritegroup_t*)psprite->frames[frame].frameptr;
        pintervals = pspritegroup->intervals;
        numframes = pspritegroup->numframes;
        fullinterval = pintervals[numframes - 1];

        time = cl.time + currentent->syncbase;

        // when loading in Mod_LoadSpriteGroup, we guaranteed all interval
        // values are positive, so we don't have to worry about division by 0
        targettime = time - ((int)(time / fullinterval)) * fullinterval;

        for(i = 0; i < (numframes - 1); i++)
        {
            if(pintervals[i] > targettime)
            {
                break;
            }
        }

        pspriteframe = pspritegroup->frames[i];
    }

    return pspriteframe;
}

/*
=================
R_DrawSpriteModel -- johnfitz -- rewritten: now supports all orientations
=================
*/
void R_DrawSpriteModel(entity_t* e)
{
    // TODO: frustum cull it?

    qvec3 v_forward;
    qvec3 v_right;
    qvec3 v_up;

    msprite_t* psprite = (msprite_t*)currententity->model->cache.data;

    switch(psprite->type)
    {
        case SPR_VP_PARALLEL_UPRIGHT:
        {
            // faces view plane, up is towards the
            // heavens
            v_up[0] = 0;
            v_up[1] = 0;
            v_up[2] = 1;
            break;
        }
        case SPR_FACING_UPRIGHT:
        {
            // faces camera origin, up is towards the
            // heavens
            v_forward = currententity->origin - r_origin;
            v_forward[2] = 0;
            v_forward = safeNormalize(v_forward);
            v_right[0] = v_forward[1];
            v_right[1] = -v_forward[0];
            v_right[2] = 0;
            v_up[0] = 0;
            v_up[1] = 0;
            v_up[2] = 1;
            break;
        }
        case SPR_VP_PARALLEL:
        {
            // faces view plane, up is towards the top of the
            // screen
            v_up = vup;
            v_right = vright;
            break;
        }
        case SPR_ORIENTED:
        {
            // pitch yaw roll are independent of camera
            std::tie(v_forward, v_right, v_up) =
                quake::util::getAngledVectors(currententity->angles);

            break;
        }
        case SPR_VP_PARALLEL_ORIENTED:
        {
            // faces view plane, but obeys roll value
            const float angle = currententity->angles[ROLL] * M_PI_DIV_180;
            const float sr = std::sin(angle);
            const float cr = std::cos(angle);
            v_right[0] = vright[0] * cr + vup[0] * sr;
            v_right[1] = vright[1] * cr + vup[1] * sr;
            v_right[2] = vright[2] * cr + vup[2] * sr;
            v_up[0] = vright[0] * -sr + vup[0] * cr;
            v_up[1] = vright[1] * -sr + vup[1] * cr;
            v_up[2] = vright[2] * -sr + vup[2] * cr;
            break;
        }
        default: return;
    }

    // johnfitz: offset decals
    if(psprite->type == SPR_ORIENTED)
    {
        GL_PolygonOffset(OFFSET_DECAL);
    }

    glColor3f(1, 1, 1);

    GL_DisableMultitexture();

    mspriteframe_t* frame = R_GetSpriteFrame(e);
    GL_Bind(frame->gltexture);

    // TODO VR: (P2) this could be optimized to use a single draw call...

    glEnable(GL_ALPHA_TEST);
    glBegin(GL_TRIANGLE_FAN); // was GL_QUADS, but changed to support r_showtris

    glTexCoord2f(0, frame->tmax);
    qvec3 point = e->origin + (frame->down * v_up);
    point += (frame->left * v_right);
    glVertex3fv(toGlVec(point));

    glTexCoord2f(0, 0);
    point = e->origin + (frame->up * v_up);
    point += (frame->left * v_right);
    glVertex3fv(toGlVec(point));

    glTexCoord2f(frame->smax, 0);
    point = e->origin + (frame->up * v_up);
    point += (frame->right * v_right);
    glVertex3fv(toGlVec(point));

    glTexCoord2f(frame->smax, frame->tmax);
    point = e->origin + (frame->down * v_up);
    point += (frame->right * v_right);
    glVertex3fv(toGlVec(point));

    glEnd();
    glDisable(GL_ALPHA_TEST);

    // johnfitz: offset decals
    if(psprite->type == SPR_ORIENTED)
    {
        GL_PolygonOffset(OFFSET_NONE);
    }
}
