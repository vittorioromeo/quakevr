/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
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
// r_brush.c: brush model rendering. renamed from r_surf.c

#include "quakedef.hpp"
#include "util.hpp"
#include "quakeglm_qvec3.hpp"
#include "quakeglm_qvec3_togl.hpp"
#include "glquake.hpp"
#include "quakedef_macros.hpp"
#include "entity.hpp"
#include "client.hpp"
#include "gl_texmgr.hpp"
#include "sys.hpp"
#include "console.hpp"

extern cvar_t gl_fullbrights, r_drawflat, gl_overbright, r_oldwater; // johnfitz
extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix

int gl_lightmap_format;
int lightmap_bytes;

#define MAX_SANITY_LIGHTMAPS (1u << 20)
struct lightmap_s* lightmap;
int lightmap_count;
int last_lightmap_allocated;
int allocated[LMBLOCK_WIDTH];

unsigned blocklights[LMBLOCK_WIDTH * LMBLOCK_HEIGHT *
                     3]; // johnfitz -- was 18*18, added lit support (*3) and
                         // loosened surface extents maximum
                         // (LMBLOCK_WIDTH*LMBLOCK_HEIGHT)


/*
===============
R_TextureAnimation -- johnfitz -- added "frame" param to eliminate use of
"currententity" global

Returns the proper texture for a given time and base texture
===============
*/
texture_t* R_TextureAnimation(texture_t* base, int frame)
{
    int relative;
    int count;

    if(frame)
    {
        if(base->alternate_anims)
        {
            base = base->alternate_anims;
        }
    }

    if(!base->anim_total)
    {
        return base;
    }

    relative = (int)(cl.time * 10) % base->anim_total;

    count = 0;
    while(base->anim_min > relative || base->anim_max <= relative)
    {
        base = base->anim_next;
        if(!base)
        {
            Sys_Error("R_TextureAnimation: broken cycle");
        }
        if(++count > 100)
        {
            Sys_Error("R_TextureAnimation: infinite cycle");
        }
    }

    return base;
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly(glpoly_t* p)
{
    float* v;
    int i;

    glBegin(GL_POLYGON);
    v = p->verts[0];
    for(i = 0; i < p->numverts; i++, v += VERTEXSIZE)
    {
        glTexCoord2f(v[3], v[4]);
        glVertex3fv(v);
    }
    glEnd();
}

/*
================
DrawGLTriangleFan -- johnfitz -- like DrawGLPoly but for r_showtris
================
*/
void DrawGLTriangleFan(glpoly_t* p)
{
    float* v;
    int i;

    glBegin(GL_TRIANGLE_FAN);
    v = p->verts[0];
    for(i = 0; i < p->numverts; i++, v += VERTEXSIZE)
    {
        glVertex3fv(v);
    }
    glEnd();
}

/*
=============================================================

    BRUSH MODELS

=============================================================
*/

#if 0
/*
================
R_DrawSequentialPoly -- johnfitz -- rewritten
================
*/
void R_DrawSequentialPoly (msurface_t *s)
{
    glpoly_t	*p;
    texture_t	*t;
    float		*v;
    float		entalpha;
    int			i;

    t = R_TextureAnimation (s->texinfo->texture, currententity->frame);
    entalpha = ENTALPHA_DECODE(currententity->alpha);

// drawflat
    if (r_drawflat_cheatsafe)
    {
        if ((s->flags & SURF_DRAWTURB) && r_oldwater.value)
        {
            for (p = s->polys->next; p; p = p->next)
            {
                srand((unsigned int) (uintptr_t) p);
                glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
                DrawGLPoly (p);
                rs_brushpasses++;
            }
            return;
        }

        srand((unsigned int) (uintptr_t) s->polys);
        glColor3f (rand()%256/255.0, rand()%256/255.0, rand()%256/255.0);
        DrawGLPoly (s->polys);
        rs_brushpasses++;
        return;
    }

// fullbright
    if ((r_fullbright_cheatsafe) && !(s->flags & SURF_DRAWTILED))
    {
        if (entalpha < 1)
        {
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(1, 1, 1, entalpha);
        }

        if (s->flags & SURF_DRAWFENCE)
            glEnable (GL_ALPHA_TEST); // Flip on alpha test

        GL_Bind (t->gltexture);
        DrawGLPoly (s->polys);
        rs_brushpasses++;

        if (s->flags & SURF_DRAWFENCE)
            glDisable (GL_ALPHA_TEST); // Flip alpha test back off

        if (entalpha < 1)
        {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor3f(1, 1, 1);
        }

        goto fullbrights;
    }

// r_lightmap
    if (r_lightmap_cheatsafe)
    {
        if (s->flags & SURF_DRAWTILED)
        {
            glDisable (GL_TEXTURE_2D);
            DrawGLPoly (s->polys);
            glEnable (GL_TEXTURE_2D);
            rs_brushpasses++;
            return;
        }

        R_RenderDynamicLightmaps (s);
        GL_Bind (lightmap_textures[s->lightmaptexturenum]);
        if (!gl_overbright.value)
        {
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor3f(0.5, 0.5, 0.5);
        }
        glBegin (GL_POLYGON);
        v = s->polys->verts[0];
        for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
        {
            glTexCoord2f (v[5], v[6]);
            glVertex3fv (v);
        }
        glEnd ();
        if (!gl_overbright.value)
        {
            glColor3f(1,1,1);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
        rs_brushpasses++;
        return;
    }

// sky poly -- skip it, already handled in gl_sky.c
    if (s->flags & SURF_DRAWSKY)
    {
        return;
    }

// water poly
    if (s->flags & SURF_DRAWTURB)
    {
        if (currententity->alpha == ENTALPHA_DEFAULT)
        {
            entalpha = CLAMP(0.0, GL_WaterAlphaForSurface(s), 1.0);
        }

        if (entalpha < 1)
        {
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(1, 1, 1, entalpha);
        }

        if (r_oldwater.value)
        {
            GL_Bind (s->texinfo->texture->gltexture);
            for (p = s->polys->next; p; p = p->next)
            {
                DrawWaterPoly (p);
                rs_brushpasses++;
            }
            rs_brushpasses++;
        }
        else
        {
            GL_Bind (s->texinfo->texture->warpimage);
            s->texinfo->texture->update_warp = true; // FIXME: one frame too late!
            DrawGLPoly (s->polys);
            rs_brushpasses++;
        }

        if (entalpha < 1)
        {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor3f(1, 1, 1);
        }

        return;
    }

// missing texture
    if (s->flags & SURF_NOTEXTURE)
    {
        if (entalpha < 1)
        {
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glColor4f(1, 1, 1, entalpha);
        }

        GL_Bind (t->gltexture);
        DrawGLPoly (s->polys);
        rs_brushpasses++;

        if (entalpha < 1)
        {
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glColor3f(1, 1, 1);
        }

        return;
    }

// lightmapped poly
    if (entalpha < 1)
    {
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor4f(1, 1, 1, entalpha);
    }
    else
        glColor3f(1, 1, 1);

    if (s->flags & SURF_DRAWFENCE)
        glEnable (GL_ALPHA_TEST); // Flip on alpha test

    if (gl_overbright.value)
    {
        if (gl_texture_env_combine && gl_mtexable) //case 1: texture and lightmap in one pass, overbright using texture combiners
        {
            GL_DisableMultitexture(); // selects TEXTURE0
            GL_Bind (t->gltexture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_EnableMultitexture(); // selects TEXTURE1
            GL_Bind (lightmap_textures[s->lightmaptexturenum]);
            R_RenderDynamicLightmaps (s);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
            glBegin(GL_POLYGON);
            v = s->polys->verts[0];
            for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
            {
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, v[3], v[4]);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, v[5], v[6]);
                glVertex3fv (v);
            }
            glEnd ();
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_DisableMultitexture ();
            rs_brushpasses++;
        }
        else if (entalpha < 1 || (s->flags & SURF_DRAWFENCE)) //case 2: can't do multipass if entity has alpha, so just draw the texture
        {
            GL_Bind (t->gltexture);
            DrawGLPoly (s->polys);
            rs_brushpasses++;
        }
        else //case 3: texture in one pass, lightmap in second pass using 2x modulation blend func, fog in third pass
        {
            //first pass -- texture with no fog
            Fog_DisableGFog ();
            GL_Bind (t->gltexture);
            DrawGLPoly (s->polys);
            Fog_EnableGFog ();
            rs_brushpasses++;

            //second pass -- lightmap with black fog, modulate blended
            R_RenderDynamicLightmaps (s);
            GL_Bind (lightmap_textures[s->lightmaptexturenum]);
            glDepthMask (GL_FALSE);
            glEnable (GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR); //2x modulate
            Fog_StartAdditive ();
            glBegin (GL_POLYGON);
            v = s->polys->verts[0];
            for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
            {
                glTexCoord2f (v[5], v[6]);
                glVertex3fv (v);
            }
            glEnd ();
            Fog_StopAdditive ();
            rs_brushpasses++;

            //third pass -- black geo with normal fog, additive blended
            if (Fog_GetDensity() > 0)
            {
                glBlendFunc(GL_ONE, GL_ONE); //add
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glColor3f(0,0,0);
                DrawGLPoly (s->polys);
                glColor3f(1,1,1);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
                rs_brushpasses++;
            }

            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable (GL_BLEND);
            glDepthMask (GL_TRUE);
        }
    }
    else
    {
        if (gl_mtexable) //case 4: texture and lightmap in one pass, regular modulation
        {
            GL_DisableMultitexture(); // selects TEXTURE0
            GL_Bind (t->gltexture);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_EnableMultitexture(); // selects TEXTURE1
            GL_Bind (lightmap_textures[s->lightmaptexturenum]);
            R_RenderDynamicLightmaps (s);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            glBegin(GL_POLYGON);
            v = s->polys->verts[0];
            for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
            {
                glMultiTexCoord2fARB (GL_TEXTURE0_ARB, v[3], v[4]);
                glMultiTexCoord2fARB (GL_TEXTURE1_ARB, v[5], v[6]);
                glVertex3fv (v);
            }
            glEnd ();
            GL_DisableMultitexture ();
            rs_brushpasses++;
        }
        else if (entalpha < 1 || (s->flags & SURF_DRAWFENCE)) //case 5: can't do multipass if entity has alpha, so just draw the texture
        {
            GL_Bind (t->gltexture);
            DrawGLPoly (s->polys);
            rs_brushpasses++;
        }
        else //case 6: texture in one pass, lightmap in a second pass, fog in third pass
        {
            //first pass -- texture with no fog
            Fog_DisableGFog ();
            GL_Bind (t->gltexture);
            DrawGLPoly (s->polys);
            Fog_EnableGFog ();
            rs_brushpasses++;

            //second pass -- lightmap with black fog, modulate blended
            R_RenderDynamicLightmaps (s);
            GL_Bind (lightmap_textures[s->lightmaptexturenum]);
            glDepthMask (GL_FALSE);
            glEnable (GL_BLEND);
            glBlendFunc (GL_ZERO, GL_SRC_COLOR); //modulate
            Fog_StartAdditive ();
            glBegin (GL_POLYGON);
            v = s->polys->verts[0];
            for (i=0 ; i<s->polys->numverts ; i++, v+= VERTEXSIZE)
            {
                glTexCoord2f (v[5], v[6]);
                glVertex3fv (v);
            }
            glEnd ();
            Fog_StopAdditive ();
            rs_brushpasses++;

            //third pass -- black geo with normal fog, additive blended
            if (Fog_GetDensity() > 0)
            {
                glBlendFunc(GL_ONE, GL_ONE); //add
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glColor3f(0,0,0);
                DrawGLPoly (s->polys);
                glColor3f(1,1,1);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
                rs_brushpasses++;
            }

            glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable (GL_BLEND);
            glDepthMask (GL_TRUE);

        }
    }
    if (entalpha < 1)
    {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1, 1, 1);
    }

    if (s->flags & SURF_DRAWFENCE)
        glDisable (GL_ALPHA_TEST); // Flip alpha test back off

fullbrights:
    if (gl_fullbrights.value && t->fullbright)
    {
        glDepthMask (GL_FALSE);
        glEnable (GL_BLEND);
        glBlendFunc (GL_ONE, GL_ONE);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glColor3f (entalpha, entalpha, entalpha);
        GL_Bind (t->fullbright);
        Fog_StartAdditive ();
        DrawGLPoly (s->polys);
        Fog_StopAdditive ();
        glColor3f(1, 1, 1);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable (GL_BLEND);
        glDepthMask (GL_TRUE);
        rs_brushpasses++;
    }
}
#endif
/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel(entity_t* e)
{
    if(R_CullModelForEntity(e))
    {
        return;
    }

    currententity = e;
    qmodel_t* clmodel = e->model;

    modelorg = r_refdef.vieworg - e->origin;
    if(e->angles[0] || e->angles[1] || e->angles[2])
    {
        const qvec3 temp = modelorg;

        const auto [forward, right, up] =
            quake::util::getAngledVectors(e->angles);

        modelorg[0] = DotProduct(temp, forward);
        modelorg[1] = -DotProduct(temp, right);
        modelorg[2] = DotProduct(temp, up);
    }

    msurface_t* psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    // calculate dynamic lighting for bmodel if it's not an
    // instanced model
    if(clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
    {
        for(int k = 0; k < MAX_DLIGHTS; ++k)
        {
            if((cl_dlights[k].die < cl.time) || (!cl_dlights[k].radius))
            {
                continue;
            }

            const qvec3 lightorg = cl_dlights[k].origin - e->origin;
            R_MarkLights(&cl_dlights[k], lightorg, k,
                clmodel->nodes + clmodel->hulls[0].firstclipnode);
        }
    }

    glPushMatrix();
    e->angles[0] = -e->angles[0]; // stupid quake bug
    if(gl_zfix.value)
    {
        e->origin[0] -= DIST_EPSILON;
        e->origin[1] -= DIST_EPSILON;
        e->origin[2] -= DIST_EPSILON;
    }
    R_RotateForEntity(e->origin, e->angles, e->netstate.scale);
    if(gl_zfix.value)
    {
        e->origin[0] += DIST_EPSILON;
        e->origin[1] += DIST_EPSILON;
        e->origin[2] += DIST_EPSILON;
    }
    e->angles[0] = -e->angles[0]; // stupid quake bug

    if(e->horizFlip)
    {
        glScalef(1.0f, -1.0f, 1.0f);
        glFrontFace(GL_CCW);
    }

    // TODO VR: (P1) document why we have +1, code repetition with alias
    glTranslatef(-e->model_scale_origin[0], -e->model_scale_origin[1],
        -e->model_scale_origin[2]);
    glScalef(e->model_scale[0] + 1.f, e->model_scale[1] + 1.f,
        e->model_scale[2] + 1.f);
    glTranslatef(e->model_scale_origin[0], e->model_scale_origin[1],
        e->model_scale_origin[2]);

    glTranslatef(e->model_offset[0], e->model_offset[1], e->model_offset[2]);

    const bool scaled = (e->model_scale[0] != 0.f) &&
                        (e->model_scale[1] != 0.f) &&
                        (e->model_scale[2] != 0.f);

    R_ClearTextureChains(clmodel, chain_model);

    int i;
    for(i = 0; i < clmodel->nummodelsurfaces; ++i, ++psurf)
    {
        mplane_t* pplane = psurf->plane;
        const float dot = DotProduct(modelorg, pplane->normal) - pplane->dist;

        // TODO VR: (P2) hack for scaled brush models, dot is incorrenct and
        // faces get culled
        if(scaled ||
            (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
                (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON))))
        {
            R_ChainSurface(psurf, chain_model);
            rs_brushpolys++;
        }
    }

    R_DrawTextureChains(clmodel, e, chain_model);
    R_DrawTextureChains_Water(clmodel, e, chain_model);

    glPopMatrix();
}

/*
=================
R_DrawBrushModel_ShowTris -- johnfitz
=================
*/
void R_DrawBrushModel_ShowTris(entity_t* e)
{
    int i;
    msurface_t* psurf;
    float dot;
    mplane_t* pplane;
    qmodel_t* clmodel;
    glpoly_t* p;

    if(R_CullModelForEntity(e))
    {
        return;
    }

    currententity = e;
    clmodel = e->model;

    modelorg = r_refdef.vieworg - e->origin;
    if(e->angles[0] || e->angles[1] || e->angles[2])
    {
        qvec3 temp = modelorg;

        const auto [forward, right, up] =
            quake::util::getAngledVectors(e->angles);

        modelorg[0] = DotProduct(temp, forward);
        modelorg[1] = -DotProduct(temp, right);
        modelorg[2] = DotProduct(temp, up);
    }

    psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

    glPushMatrix();
    e->angles[0] = -e->angles[0]; // stupid quake bug
    R_RotateForEntity(e->origin, e->angles, e->netstate.scale);
    e->angles[0] = -e->angles[0]; // stupid quake bug

    if(e->horizFlip)
    {
        glScalef(1.0f, -1.0f, 1.0f);
        glFrontFace(GL_CCW);
    }

    // TODO VR: (P1) document why we have +1, code repetition with brush
    glTranslatef(-e->model_scale_origin[0], -e->model_scale_origin[1],
        -e->model_scale_origin[2]);
    glScalef(e->model_scale[0] + 1.f, e->model_scale[1] + 1.f,
        e->model_scale[2] + 1.f);
    glTranslatef(e->model_scale_origin[0], e->model_scale_origin[1],
        e->model_scale_origin[2]);

    glTranslatef(e->model_offset[0], e->model_offset[1], e->model_offset[2]);

    //
    // draw it
    //
    for(i = 0; i < clmodel->nummodelsurfaces; i++, psurf++)
    {
        pplane = psurf->plane;
        dot = DotProduct(modelorg, pplane->normal) - pplane->dist;
        if(((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
            (!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
        {
            if((psurf->flags & SURF_DRAWTURB) && r_oldwater.value)
            {
                for(p = psurf->polys->next; p; p = p->next)
                {
                    DrawGLTriangleFan(p);
                }
            }
            else
            {
                DrawGLTriangleFan(psurf->polys);
            }
        }
    }

    glPopMatrix();
}

/*
=============================================================

    LIGHTMAPS

=============================================================
*/

/*
================
R_RenderDynamicLightmaps
called during rendering
================
*/
void R_RenderDynamicLightmaps(qmodel_t* model, msurface_t* fa)
{
    byte* base;
    int maps;
    glRect_t* theRect;
    int smax;
    int tmax;

    if(fa->flags & SURF_DRAWTILED)
    {
        // johnfitz -- not a lightmapped surface
        return;
    }

    // add to lightmap chain
    fa->polys->chain = lightmap[fa->lightmaptexturenum].polys;
    lightmap[fa->lightmaptexturenum].polys = fa->polys;

    // check for lightmap modification
    for(maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != INVALID_LIGHTSTYLE;
        maps++)
    {
        if(d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
        {
            goto dynamic;
        }
    }

    if(fa->dlightframe == r_framecount // dynamic this frame
        || fa->cached_dlight)          // dynamic previously
    {
    dynamic:
        if(r_dynamic.value)
        {
            struct lightmap_s* lm = &lightmap[fa->lightmaptexturenum];
            lm->modified = true;
            theRect = &lm->rectchange;
            if(fa->light_t < theRect->t)
            {
                if(theRect->h)
                {
                    theRect->h += theRect->t - fa->light_t;
                }
                theRect->t = fa->light_t;
            }
            if(fa->light_s < theRect->l)
            {
                if(theRect->w)
                {
                    theRect->w += theRect->l - fa->light_s;
                }
                theRect->l = fa->light_s;
            }
            smax = (fa->extents[0] >> fa->lmshift) + 1;
            tmax = (fa->extents[1] >> fa->lmshift) + 1;
            if((theRect->w + theRect->l) < (fa->light_s + smax))
            {
                theRect->w = (fa->light_s - theRect->l) + smax;
            }
            if((theRect->h + theRect->t) < (fa->light_t + tmax))
            {
                theRect->h = (fa->light_t - theRect->t) + tmax;
            }
            base = lm->data;
            base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes +
                    fa->light_s * lightmap_bytes;
            R_BuildLightMap(model, fa, base, LMBLOCK_WIDTH * lightmap_bytes);
        }
    }
}

/*
========================
AllocBlock -- returns a texture number and the position inside it
========================
*/
int AllocBlock(int w, int h, int* x, int* y)
{
    int i;
    int j;
    int best;
    int best2;

    // ericw -- rather than searching starting at lightmap 0 every time,
    // start at the last lightmap we allocated a surface in.
    // This makes AllocBlock much faster on large levels (can shave off
    // 3+ seconds of load time on a level with 180 lightmaps), at a cost
    // of not quite packing lightmaps as tightly vs. not doing this
    // (uses ~5% more lightmaps)
    for(decltype(MAX_SANITY_LIGHTMAPS) texnum = last_lightmap_allocated;
        texnum < MAX_SANITY_LIGHTMAPS; texnum++)
    {
        if(texnum == static_cast<GLuint>(lightmap_count))
        {
            lightmap_count++;
            lightmap = (struct lightmap_s*)realloc(
                lightmap, sizeof(*lightmap) * lightmap_count);
            memset(&lightmap[texnum], 0, sizeof(lightmap[texnum]));
            /* FIXME: we leave 'gaps' in malloc()ed data,  CRC_Block()
             * later accesses that uninitialized data and valgrind
             * complains for it. use calloc() ? */
            lightmap[texnum].data =
                (byte*)malloc(4 * LMBLOCK_WIDTH * LMBLOCK_HEIGHT);
            // as we're only tracking one texture, we don't need
            // multiple copies of allocated any more.
            memset(allocated, 0, sizeof(allocated));
        }
        best = LMBLOCK_HEIGHT;

        for(i = 0; i < LMBLOCK_WIDTH - w; i++)
        {
            best2 = 0;

            for(j = 0; j < w; j++)
            {
                if(allocated[i + j] >= best)
                {
                    break;
                }
                if(allocated[i + j] > best2)
                {
                    best2 = allocated[i + j];
                }
            }
            if(j == w)
            {
                // this is a valid spot
                *x = i;
                *y = best = best2;
            }
        }

        if(best + h > LMBLOCK_HEIGHT)
        {
            continue;
        }

        for(i = 0; i < w; i++)
        {
            allocated[*x + i] = best + h;
        }

        last_lightmap_allocated = texnum;
        return texnum;
    }

    Sys_Error("AllocBlock: full");
    return 0; // johnfitz -- shut up compiler
}


mvertex_t* r_pcurrentvertbase;
qmodel_t* currentmodel;

int nColinElim;

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap(qmodel_t* model, msurface_t* surf)
{
    int smax;
    int tmax;
    byte* base;

    smax = (surf->extents[0] >> surf->lmshift) + 1;
    tmax = (surf->extents[1] >> surf->lmshift) + 1;

    surf->lightmaptexturenum =
        AllocBlock(smax, tmax, &surf->light_s, &surf->light_t);
    base = lightmap[surf->lightmaptexturenum].data;
    base += (surf->light_t * LMBLOCK_WIDTH + surf->light_s) * lightmap_bytes;
    R_BuildLightMap(model, surf, base, LMBLOCK_WIDTH * lightmap_bytes);
}

/*
================
BuildSurfaceDisplayList -- called at level load time
================
*/
void BuildSurfaceDisplayList(msurface_t* fa)
{
    int i;
    int lindex;
    int lnumverts;
    medge_t* pedges;
    medge_t* r_pedge;
    qfloat* vec;
    qfloat s;
    qfloat t;
    glpoly_t* poly;
    int lmscale = (1 << fa->lmshift);

    // reconstruct the polygon
    pedges = currentmodel->edges;
    lnumverts = fa->numedges;

    //
    // draw texture
    //
    poly = (glpoly_t*)Hunk_Alloc(
        sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE * sizeof(float));
    poly->next = fa->polys;
    fa->polys = poly;
    poly->numverts = lnumverts;

    for(i = 0; i < lnumverts; i++)
    {
        lindex = currentmodel->surfedges[fa->firstedge + i];

        if(lindex > 0)
        {
            r_pedge = &pedges[lindex];
            vec = toGlVec(r_pcurrentvertbase[r_pedge->v[0]].position);
        }
        else
        {
            r_pedge = &pedges[-lindex];
            vec = toGlVec(r_pcurrentvertbase[r_pedge->v[1]].position);
        }
        s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s /= fa->texinfo->texture->width;

        t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t /= fa->texinfo->texture->height;

        VectorCopy(vec, poly->verts[i]);
        poly->verts[i][3] = s;
        poly->verts[i][4] = t;

        //
        // lightmap texture coordinates
        //
        s = DotProduct(vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
        s -= fa->texturemins[0];
        s += fa->light_s * lmscale;
        s += lmscale / 2.0;
        s /= LMBLOCK_WIDTH * lmscale; // fa->texinfo->texture->width;

        t = DotProduct(vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
        t -= fa->texturemins[1];
        t += fa->light_t * lmscale;
        t += lmscale / 2.0;
        t /= LMBLOCK_HEIGHT * lmscale; // fa->texinfo->texture->height;

        poly->verts[i][5] = s;
        poly->verts[i][6] = t;
    }

    // johnfitz -- removed gl_keeptjunctions code

    poly->numverts = lnumverts;
}

/*
==================
GL_BuildLightmaps -- called at level load time

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps()
{
    char name[24];
    int i;
    int j;
    struct lightmap_s* lm;
    qmodel_t* m;

    r_framecount = 1; // no dlightcache

    // Spike -- wipe out all the lightmap data (johnfitz -- the
    // gltexture objects were already freed by Mod_ClearAll)
    for(i = 0; i < lightmap_count; i++)
    {
        free(lightmap[i].data);
    }
    free(lightmap);
    lightmap = nullptr;
    last_lightmap_allocated = 0;
    lightmap_count = 0;

    gl_lightmap_format = GL_RGBA; // FIXME: hardcoded for now!

    switch(gl_lightmap_format)
    {
        case GL_RGBA: lightmap_bytes = 4; break;
        case GL_BGRA: lightmap_bytes = 4; break;
        default: Sys_Error("GL_BuildLightmaps: bad lightmap format");
    }

    for(j = 1; j < MAX_MODELS; j++)
    {
        m = cl.model_precache[j];
        if(!m)
        {
            break;
        }
        if(m->name[0] == '*')
        {
            continue;
        }
        r_pcurrentvertbase = m->vertexes;
        currentmodel = m;
        for(i = 0; i < m->numsurfaces; i++)
        {
            // johnfitz -- rewritten to use SURF_DRAWTILED instead of
            // the sky/water flags
            if(m->surfaces[i].flags & SURF_DRAWTILED)
            {
                continue;
            }
            GL_CreateSurfaceLightmap(m, m->surfaces + i);
            BuildSurfaceDisplayList(m->surfaces + i);
            // johnfitz
        }
    }

    //
    // upload all lightmaps that were filled
    //
    for(i = 0; i < lightmap_count; i++)
    {
        lm = &lightmap[i];
        lm->modified = false;
        lm->rectchange.l = LMBLOCK_WIDTH;
        lm->rectchange.t = LMBLOCK_HEIGHT;
        lm->rectchange.w = 0;
        lm->rectchange.h = 0;

        // johnfitz -- use texture manager
        sprintf(name, "lightmap%07i", i);
        lm->texture = TexMgr_LoadImage(cl.worldmodel, name, LMBLOCK_WIDTH,
            LMBLOCK_HEIGHT, SRC_LIGHTMAP, lm->data, "", (src_offset_t)lm->data,
            TEXPREF_LINEAR | TEXPREF_NOPICMIP);
        // johnfitz
    }

    // johnfitz -- warn about exceeding old limits
    // GLQuake limit was 64 textures of 128x128. Estimate how many
    // 128x128 textures we would need given that we are using
    // lightmap_count of LMBLOCK_WIDTH x LMBLOCK_HEIGHT
    i = lightmap_count * ((LMBLOCK_WIDTH / 128) * (LMBLOCK_HEIGHT / 128));
    if(i > 64)
    {
        Con_DWarning("%i lightmaps exceeds standard limit of 64.\n", i);
    }
    // johnfitz
}

/*
=============================================================

    VBO support

=============================================================
*/

GLuint gl_bmodel_vbo = 0;

void GL_DeleteBModelVertexBuffer()
{
    if(!(gl_vbo_able && gl_mtexable && gl_max_texture_units >= 3))
    {
        return;
    }

    glDeleteBuffersARB(1, &gl_bmodel_vbo);
    gl_bmodel_vbo = 0;

    GL_ClearBufferBindings();
}

/*
==================
GL_BuildBModelVertexBuffer

Deletes gl_bmodel_vbo if it already exists, then rebuilds it with all
surfaces from world + all brush models
==================
*/
void GL_BuildBModelVertexBuffer()
{
    unsigned int numverts;
    unsigned int varray_bytes;
    unsigned int varray_index;
    int i;
    int j;
    qmodel_t* m;
    float* varray;

    if(!(gl_vbo_able && gl_mtexable && gl_max_texture_units >= 3))
    {
        return;
    }

    // ask GL for a name for our VBO
    glDeleteBuffersARB(1, &gl_bmodel_vbo);
    glGenBuffersARB(1, &gl_bmodel_vbo);

    // count all verts in all models
    numverts = 0;
    for(j = 1; j < MAX_MODELS; j++)
    {
        m = cl.model_precache[j];
        if(!m || m->name[0] == '*' || m->type != mod_brush)
        {
            continue;
        }

        for(i = 0; i < m->numsurfaces; i++)
        {
            numverts += m->surfaces[i].numedges;
        }
    }

    // build vertex array
    varray_bytes = VERTEXSIZE * sizeof(float) * numverts;
    varray = (float*)malloc(varray_bytes);
    varray_index = 0;

    for(j = 1; j < MAX_MODELS; j++)
    {
        m = cl.model_precache[j];
        if(!m || m->name[0] == '*' || m->type != mod_brush)
        {
            continue;
        }

        for(i = 0; i < m->numsurfaces; i++)
        {
            msurface_t* s = &m->surfaces[i];
            s->vbo_firstvert = varray_index;
            memcpy(&varray[VERTEXSIZE * varray_index], s->polys->verts,
                VERTEXSIZE * sizeof(float) * s->numedges);
            varray_index += s->numedges;
        }
    }

    // upload to GPU
    glBindBufferARB(GL_ARRAY_BUFFER, gl_bmodel_vbo);
    glBufferDataARB(GL_ARRAY_BUFFER, varray_bytes, varray, GL_STATIC_DRAW);
    free(varray);

    // invalidate the cached bindings
    GL_ClearBufferBindings();
}

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights(msurface_t* surf)
{
    int lnum;
    int sd;
    int td;
    float dist;
    float rad;
    float minlight;
    qvec3 impact;
    qvec3 local;
    int s;
    int t;
    int i;
    int smax;
    int tmax;
    mtexinfo_t* tex;
    // johnfitz -- lit support via lordhavoc
    float cred;
    float cgreen;
    float cblue;
    float brightness;
    unsigned* bl;
    // johnfitz
    vec3_t lightofs; // Spike: light surfaces based upon where they are now
                     // instead of their default position.
    int lmscale;

    smax = (surf->extents[0] >> surf->lmshift) + 1;
    tmax = (surf->extents[1] >> surf->lmshift) + 1;
    tex = surf->texinfo;
    lmscale = 1 << surf->lmshift;

    for(lnum = 0; lnum < MAX_DLIGHTS; lnum++)
    {
        if(!(surf->dlightbits[lnum >> 5] & (1U << (lnum & 31))))
        {
            continue; // not lit by this light
        }

        rad = cl_dlights[lnum].radius;
        VectorSubtract(
            cl_dlights[lnum].origin, currententity->origin, lightofs);
        dist = DotProduct(lightofs, surf->plane->normal) - surf->plane->dist;
        rad -= fabs(dist);
        minlight = cl_dlights[lnum].minlight;
        if(rad < minlight)
        {
            continue;
        }
        minlight = rad - minlight;

        for(i = 0; i < 3; i++)
        {
            impact[i] = lightofs[i] - surf->plane->normal[i] * dist;
        }

        local[0] = DotProduct(impact, tex->vecs[0]) + tex->vecs[0][3];
        local[1] = DotProduct(impact, tex->vecs[1]) + tex->vecs[1][3];

        local[0] -= surf->texturemins[0];
        local[1] -= surf->texturemins[1];

        // johnfitz -- lit support via lordhavoc
        bl = blocklights;
        cred = cl_dlights[lnum].color[0] * 256.0f;
        cgreen = cl_dlights[lnum].color[1] * 256.0f;
        cblue = cl_dlights[lnum].color[2] * 256.0f;
        // johnfitz
        for(t = 0; t < tmax; t++)
        {
            td = local[1] - t * lmscale;
            if(td < 0)
            {
                td = -td;
            }
            for(s = 0; s < smax; s++)
            {
                sd = local[0] - s * lmscale;
                if(sd < 0)
                {
                    sd = -sd;
                }
                if(sd > td)
                {
                    dist = sd + (td >> 1);
                }
                else
                {
                    dist = td + (sd >> 1);
                }
                if(dist < minlight)
                // johnfitz -- lit support via lordhavoc
                {
                    brightness = rad - dist;
                    bl[0] += (int)(brightness * cred);
                    bl[1] += (int)(brightness * cgreen);
                    bl[2] += (int)(brightness * cblue);
                }
                bl += 3;
                // johnfitz
            }
        }
    }
}


/*
===============
R_BuildLightMap -- johnfitz -- revised for lit support via lordhavoc

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap(qmodel_t* model, msurface_t* surf, byte* dest, int stride)
{
    int smax;
    int tmax;
    int r;
    int g;
    int b;
    int i;
    int j;
    int size;
    byte* lightmap;
    unsigned scale;
    int maps;
    unsigned* bl;

    surf->cached_dlight = (surf->dlightframe == r_framecount);

    smax = (surf->extents[0] >> surf->lmshift) + 1;
    tmax = (surf->extents[1] >> surf->lmshift) + 1;
    size = smax * tmax;
    lightmap = surf->samples;

    if(model->lightdata)
    {
        // clear to no light
        memset(&blocklights[0], 0,
            size * 3 * sizeof(unsigned int)); // johnfitz -- lit support
                                              // via lordhavoc

        // add all the lightmaps
        if(lightmap)
        {
            for(maps = 0;
                maps < MAXLIGHTMAPS && surf->styles[maps] != INVALID_LIGHTSTYLE;
                maps++)
            {
                scale = d_lightstylevalue[surf->styles[maps]];
                surf->cached_light[maps] = scale; // 8.8 fraction
                // johnfitz -- lit support via lordhavoc
                bl = blocklights;
                for(i = 0; i < size; i++)
                {
                    *bl++ += *lightmap++ * scale;
                    *bl++ += *lightmap++ * scale;
                    *bl++ += *lightmap++ * scale;
                }
                // johnfitz
            }
        }

        // add all the dynamic lights
        if(surf->dlightframe == r_framecount)
        {
            R_AddDynamicLights(surf);
        }
    }
    else
    {
        // set to full bright if no light data
        memset(&blocklights[0], 255,
            size * 3 * sizeof(unsigned int)); // johnfitz -- lit support
                                              // via lordhavoc
    }

    // bound, invert, and shift
    // store:
    switch(gl_lightmap_format)
    {
        case GL_RGBA:
            stride -= smax * 4;
            bl = blocklights;
            for(i = 0; i < tmax; i++, dest += stride)
            {
                for(j = 0; j < smax; j++)
                {
                    if(gl_overbright.value)
                    {
                        r = *bl++ >> 8;
                        g = *bl++ >> 8;
                        b = *bl++ >> 8;
                    }
                    else
                    {
                        r = *bl++ >> 7;
                        g = *bl++ >> 7;
                        b = *bl++ >> 7;
                    }
                    *dest++ = (r > 255) ? 255 : r;
                    *dest++ = (g > 255) ? 255 : g;
                    *dest++ = (b > 255) ? 255 : b;
                    *dest++ = 255;
                }
            }
            break;
        case GL_BGRA:
            stride -= smax * 4;
            bl = blocklights;
            for(i = 0; i < tmax; i++, dest += stride)
            {
                for(j = 0; j < smax; j++)
                {
                    if(gl_overbright.value)
                    {
                        r = *bl++ >> 8;
                        g = *bl++ >> 8;
                        b = *bl++ >> 8;
                    }
                    else
                    {
                        r = *bl++ >> 7;
                        g = *bl++ >> 7;
                        b = *bl++ >> 7;
                    }
                    *dest++ = (b > 255) ? 255 : b;
                    *dest++ = (g > 255) ? 255 : g;
                    *dest++ = (r > 255) ? 255 : r;
                    *dest++ = 255;
                }
            }
            break;
        default: Sys_Error("R_BuildLightMap: bad lightmap format");
    }
}

/*
===============
R_UploadLightmap -- johnfitz -- uploads the modified lightmap to opengl
if necessary

assumes lightmap texture is already bound
===============
*/
static void R_UploadLightmap(int lmap)
{
    struct lightmap_s* lm = &lightmap[lmap];

    if(!lm->modified)
    {
        return;
    }

    lm->modified = false;

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, lm->rectchange.t, LMBLOCK_WIDTH,
        lm->rectchange.h, gl_lightmap_format, GL_UNSIGNED_BYTE,
        lm->data + lm->rectchange.t * LMBLOCK_WIDTH * lightmap_bytes);
    lm->rectchange.l = LMBLOCK_WIDTH;
    lm->rectchange.t = LMBLOCK_HEIGHT;
    lm->rectchange.h = 0;
    lm->rectchange.w = 0;

    rs_dynamiclightmaps++;
}

void R_UploadLightmaps()
{
    int lmap;

    for(lmap = 0; lmap < lightmap_count; lmap++)
    {
        if(!lightmap[lmap].modified)
        {
            continue;
        }

        GL_Bind(lightmap[lmap].texture);
        R_UploadLightmap(lmap);
    }
}

/*
================
R_RebuildAllLightmaps -- johnfitz -- called when gl_overbright gets
toggled
================
*/
void R_RebuildAllLightmaps()
{
    int i;
    int j;
    qmodel_t* mod;
    msurface_t* fa;
    byte* base;

    if(!cl.worldmodel)
    {
        // is this the correct test?
        return;
    }

    // for each surface in each model, rebuild lightmap with new scale
    for(i = 1; i < MAX_MODELS; i++)
    {
        if(!(mod = cl.model_precache[i]))
        {
            continue;
        }
        fa = &mod->surfaces[mod->firstmodelsurface];
        for(j = 0; j < mod->nummodelsurfaces; j++, fa++)
        {
            if(fa->flags & SURF_DRAWTILED)
            {
                continue;
            }
            base = lightmap[fa->lightmaptexturenum].data;
            base += fa->light_t * LMBLOCK_WIDTH * lightmap_bytes +
                    fa->light_s * lightmap_bytes;
            R_BuildLightMap(mod, fa, base, LMBLOCK_WIDTH * lightmap_bytes);
        }
    }

    // for each lightmap, upload it
    for(i = 0; i < lightmap_count; i++)
    {
        GL_Bind(lightmap[i].texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, LMBLOCK_WIDTH, LMBLOCK_HEIGHT,
            gl_lightmap_format, GL_UNSIGNED_BYTE, lightmap[i].data);
    }
}
