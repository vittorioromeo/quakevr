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

#pragma once

#include "quakeglm_qvec3.hpp"
#include "modelgen.hpp"
#include "gl_model.hpp"
#include "efrag.hpp"
#include "entity.hpp"
#include "refdef.hpp"

#include <string>

// refresh.h -- public interface to refresh functions

#define MAXCLIPPLANES 11

#define TOP_RANGE 16 // soldier uniform colors
#define BOTTOM_RANGE 96

//=============================================================================

// johnfitz -- for lerping
#define LERP_MOVESTEP \
    (1 << 0) // this is a MOVETYPE_STEP entity, enable movement lerp
#define LERP_RESETANIM (1 << 1) // disable anim lerping until next anim frame
#define LERP_RESETANIM2 \
    (1 << 2) // set this and previous flag to disable anim lerping for two anim
             // frames
#define LERP_RESETMOVE \
    (1 << 3) // disable movement lerping until next origin/angles change
#define LERP_FINISH \
    (1 << 4) // use lerpfinish time from server update instead of assuming
             // interval of 0.1
// johnfitz


//
// refresh
//
extern int reinit_surfcache;

extern qvec3 r_origin, vpn, vright, vup;


void R_Init();
void R_InitTextures();
void R_InitEfrags();
void R_RenderView(); // must set r_refdef first
void R_RenderScene();
void R_DrawViewModel(entity_t* viewent);
void R_ViewChanged(vrect_t* pvrect, int lineadj, float aspect);
// called whenever r_refdef or vid change
// void R_InitSky (struct texture_s *mt);	// called at level load

void R_CheckEfrags(); // johnfitz
void R_AddEfrags(entity_t* ent);

void R_NewMap();


void R_ParseParticleEffect();
void R_ParseParticle2Effect();
void R_RunParticle2Effect(
    const qvec3& org, const qvec3& dir, int preset, int count);
void R_RunParticleEffect_BulletPuff(
    const qvec3& org, const qvec3& dir, int color, int count);
void R_RunParticleEffect_LavaSpike(
    const qvec3& org, const qvec3& dir, int count);
void R_RocketTrail(qvec3 start, const qvec3& end, int type);
void R_EntityParticles(entity_t* ent);
void R_BlobExplosion(const qvec3& org);
void R_ParticleExplosion(const qvec3& org);
void R_ParticleExplosion2(const qvec3& org, int colorStart, int colorLength);
void R_LavaSplash(const qvec3& org);
void R_TeleportSplash(const qvec3& org);

void R_PushDlights();

struct aliashdr_t;
struct lerpdata_t;

void R_SetupAliasFrame(
    entity_t* e, const aliashdr_t& paliashdr, int frame, lerpdata_t* lerpdata);

void R_SetupEntityTransform(entity_t* e, lerpdata_t* lerpdata);


//
// surface cache related
//
extern int reinit_surfcache; // if 1, surface cache is currently empty and
extern bool r_cache_thrash;  // set if thrashing the surface cache

int D_SurfaceCacheForRes(int width, int height);
void D_FlushCaches();
void D_DeleteSurfaceCache();
void D_InitCaches(void* buffer, int size);
void R_SetVrect(vrect_t* pvrect, vrect_t* pvrectin, int lineadj);

struct DrawAliasFrameData
{
    trivertx_t* verts1;
    trivertx_t* verts2;
    trivertx_t* zeroverts1;
    float blend;
    float iblend;
    bool lerping;
};

void R_SetupAliasFrameZero(
    const aliashdr_t& paliashdr, int frame, lerpdata_t* lerpdata);

[[nodiscard]] DrawAliasFrameData getDrawAliasFrameData(
    const aliashdr_t& paliashdr, const lerpdata_t& lerpdata,
    const lerpdata_t& zeroLerpdata) noexcept;

[[nodiscard]] qvec3 getFinalVertexPosLerped(
    const DrawAliasFrameData& fd, const qfloat zeroBlend) noexcept;

[[nodiscard]] qvec3 getFinalVertexPosNonLerped(
    const DrawAliasFrameData& fd, const qfloat zeroBlend) noexcept;
