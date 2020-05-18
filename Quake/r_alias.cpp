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

// r_alias.c -- alias model rendering

#include "quakeglm.hpp"
#include "quakedef.hpp"
#include "lerpdata.hpp"
#include "render.hpp"
#include "glquake.hpp"
#include "mathlib.hpp"
#include "quakedef_macros.hpp"
#include "shader.hpp"

extern cvar_t r_drawflat, gl_overbright_models, gl_fullbrights, r_lerpmodels,
    r_lerpmove; // johnfitz

// up to 16 color translated skins
gltexture_t* playertextures[MAX_SCOREBOARD]; // johnfitz -- changed to an array
                                             // of pointers

#define NUMVERTEXNORMALS 162

float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.hpp"
};

// johnfitz -- replaces "float shadelight" for lit support
extern qvec3 lightcolor;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "anorm_dots.hpp"
};

extern qvec3 lightspot;

float* shadedots = r_avertexnormal_dots[0];
qvec3 shadevector;

float entalpha; // johnfitz

bool overbright; // johnfitz

bool shading = true; // johnfitz -- if false, disable vertex shading for various
                     // reasons (fullbright, r_lightmap, showtris, etc)

static GLuint r_alias_program;
static GLuint r_aliasblended_program;

// uniforms used in vert shader
static GLuint blendLoc;
static GLuint zeroBlendLoc;
static GLuint shadevectorLoc;
static GLuint lightColorLoc;

// uniforms used in frag shader
static GLuint texLoc;
static GLuint fullbrightTexLoc;
static GLuint useFullbrightTexLoc;
static GLuint useOverbrightLoc;
static GLuint useAlphaTestLoc;

#define pose1VertexAttrIndex 0
#define pose1NormalAttrIndex 1
#define pose2VertexAttrIndex 2
#define pose2NormalAttrIndex 3
#define texCoordsAttrIndex 4
#define zeroBlendVertexAttrIndex 5

/*
=============
GLARB_GetXYZOffset

Returns the offset of the first vertex's meshxyz_t.xyz in the vbo for the given
model and pose.
=============
*/
static void* GLARB_GetXYZOffset(aliashdr_t* hdr, int pose)
{
    const int xyzoffs = offsetof(meshxyz_t, xyz);
    return (void*)(currententity->model->vboxyzofs +
                   (hdr->numverts_vbo * pose * sizeof(meshxyz_t)) + xyzoffs);
}

/*
=============
GLARB_GetNormalOffset

Returns the offset of the first vertex's meshxyz_t.normal in the vbo for the
given model and pose.
=============
*/
static void* GLARB_GetNormalOffset(aliashdr_t* hdr, int pose)
{
    const int normaloffs = offsetof(meshxyz_t, normal);
    return (void*)(currententity->model->vboxyzofs +
                   (hdr->numverts_vbo * pose * sizeof(meshxyz_t)) + normaloffs);
}

/*
=============
GLAlias_CreateShaders
=============
*/
void GLAlias_CreateShaders()
{
    const GLchar* vertSource = R"glsl(
#version 110

uniform float Blend;
uniform vec3 ShadeVector;
uniform vec4 LightColor;
attribute vec4 TexCoords; // only xy are used
attribute vec4 Pose1Vert;
attribute vec3 Pose1Normal;
attribute vec4 Pose2Vert;
attribute vec3 Pose2Normal;

varying float FogFragCoord;

float r_avertexnormal_dot(vec3 vertexnormal) // from MH
{
    float dot = dot(vertexnormal, ShadeVector);
    // wtf - this reproduces anorm_dots within as reasonable a degree of tolerance as the >= 0 case

    if (dot < 0.0)
        return 1.0 + dot * (13.0 / 44.0);
    else
        return 1.0 + dot;
}

void main()
{
    gl_TexCoord[0] = TexCoords;
    vec4 lerpedVert = mix(vec4(Pose1Vert.xyz, 1.0),
                          vec4(Pose2Vert.xyz, 1.0), Blend);
    gl_Position = gl_ModelViewProjectionMatrix * lerpedVert;
    FogFragCoord = gl_Position.w;
    float dot1 = r_avertexnormal_dot(Pose1Normal);
    float dot2 = r_avertexnormal_dot(Pose2Normal);
    gl_FrontColor = LightColor * vec4(vec3(mix(dot1, dot2, Blend)), 1.0);
})glsl";

    const GLchar* fragSource = R"glsl(
#version 110

uniform sampler2D Tex;
uniform sampler2D FullbrightTex;
uniform bool UseFullbrightTex;
uniform bool UseOverbright;
uniform bool UseAlphaTest;

varying float FogFragCoord;

void main()
{
    vec4 result = texture2D(Tex, gl_TexCoord[0].xy);
    if (UseAlphaTest && (result.a < 0.666))
        discard;

    result *= gl_Color;

    if (UseOverbright)
        result.rgb *= 2.0;

    if (UseFullbrightTex)
        result += texture2D(FullbrightTex, gl_TexCoord[0].xy);

    result = clamp(result, 0.0, 1.0);

    float fog = exp(-gl_Fog.density * gl_Fog.density * FogFragCoord * FogFragCoord);
    fog = clamp(fog, 0.0, 1.0);
    result = mix(gl_Fog.color, result, fog);

    // FIXME: This will make almos transparent things cut holes though heavy fo
    result.a = gl_Color.a;
    gl_FragColor = result;
})glsl";

    if(!gl_glsl_alias_able)
    {
        return;
    }

    r_alias_program =
        quake::gl_program_builder{}
            .add_shader({GL_VERTEX_SHADER, vertSource})
            .add_shader({GL_FRAGMENT_SHADER, fragSource})
            .add_attr_binding({"TexCoords", texCoordsAttrIndex})
            .add_attr_binding({"Pose1Vert", pose1VertexAttrIndex})
            .add_attr_binding({"Pose1Normal", pose1NormalAttrIndex})
            .add_attr_binding({"Pose2Vert", pose2VertexAttrIndex})
            .add_attr_binding({"Pose2Normal", pose2NormalAttrIndex})
            .compile_and_link();

    if(r_alias_program != 0)
    {
        // get uniform locations
        blendLoc = GL_GetUniformLocation(&r_alias_program, "Blend");
        shadevectorLoc = GL_GetUniformLocation(&r_alias_program, "ShadeVector");
        lightColorLoc = GL_GetUniformLocation(&r_alias_program, "LightColor");
        texLoc = GL_GetUniformLocation(&r_alias_program, "Tex");
        fullbrightTexLoc =
            GL_GetUniformLocation(&r_alias_program, "FullbrightTex");
        useFullbrightTexLoc =
            GL_GetUniformLocation(&r_alias_program, "UseFullbrightTex");
        useOverbrightLoc =
            GL_GetUniformLocation(&r_alias_program, "UseOverbright");
        useAlphaTestLoc =
            GL_GetUniformLocation(&r_alias_program, "UseAlphaTest");
    }
}

void GLAliasBlended_CreateShaders()
{
    const GLchar* vertSource = R"glsl(
#version 110

uniform float Blend;
uniform float ZeroBlend;
uniform vec3 ShadeVector;
uniform vec4 LightColor;
attribute vec4 TexCoords; // only xy are used
attribute vec4 Pose1Vert;
attribute vec3 Pose1Normal;
attribute vec4 Pose2Vert;
attribute vec3 Pose2Normal;
attribute vec4 ZeroBlendVert;

varying float FogFragCoord;

float r_avertexnormal_dot(vec3 vertexnormal) // from MH
{
    float dot = dot(vertexnormal, ShadeVector);
    // wtf - this reproduces anorm_dots within as reasonable a degree of tolerance as the >= 0 case

    if (dot < 0.0)
        return 1.0 + dot * (13.0 / 44.0);
    else
        return 1.0 + dot;
}

void main()
{
    gl_TexCoord[0] = TexCoords;

    vec4 lerpedVert = mix(vec4(Pose1Vert.xyz, 1.0),
                          vec4(Pose2Vert.xyz, 1.0), Blend);

    vec4 zeroBlendedVert = mix(lerpedVert, vec4(ZeroBlendVert.xyz, 1.0), ZeroBlend);

    gl_Position = gl_ModelViewProjectionMatrix * zeroBlendedVert;
    FogFragCoord = gl_Position.w;
    float dot1 = r_avertexnormal_dot(Pose1Normal);
    float dot2 = r_avertexnormal_dot(Pose2Normal);
    gl_FrontColor = LightColor * vec4(vec3(mix(dot1, dot2, Blend)), 1.0);
})glsl";

    const GLchar* fragSource = R"glsl(
#version 110

uniform sampler2D Tex;
uniform sampler2D FullbrightTex;
uniform bool UseFullbrightTex;
uniform bool UseOverbright;
uniform bool UseAlphaTest;

varying float FogFragCoord;

void main()
{
    vec4 result = texture2D(Tex, gl_TexCoord[0].xy);
    if (UseAlphaTest && (result.a < 0.666))
        discard;

    result *= gl_Color;

    if (UseOverbright)
        result.rgb *= 2.0;

    if (UseFullbrightTex)
        result += texture2D(FullbrightTex, gl_TexCoord[0].xy);

    result = clamp(result, 0.0, 1.0);

    float fog = exp(-gl_Fog.density * gl_Fog.density * FogFragCoord * FogFragCoord);
    fog = clamp(fog, 0.0, 1.0);
    result = mix(gl_Fog.color, result, fog);

    // FIXME: This will make almos transparent things cut holes though heavy fo
    result.a = gl_Color.a;
    gl_FragColor = result;
})glsl";

    if(!gl_glsl_alias_able)
    {
        return;
    }

    r_aliasblended_program =
        quake::gl_program_builder{}
            .add_shader({GL_VERTEX_SHADER, vertSource})
            .add_shader({GL_FRAGMENT_SHADER, fragSource})
            .add_attr_binding({"TexCoords", texCoordsAttrIndex})
            .add_attr_binding({"Pose1Vert", pose1VertexAttrIndex})
            .add_attr_binding({"Pose1Normal", pose1NormalAttrIndex})
            .add_attr_binding({"Pose2Vert", pose2VertexAttrIndex})
            .add_attr_binding({"Pose2Normal", pose2NormalAttrIndex})
            .add_attr_binding({"ZeroBlendVert", zeroBlendVertexAttrIndex})
            .compile_and_link();

    if(r_aliasblended_program != 0)
    {
        // get uniform locations
        blendLoc = GL_GetUniformLocation(&r_aliasblended_program, "Blend");
        zeroBlendLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "ZeroBlend");
        shadevectorLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "ShadeVector");
        lightColorLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "LightColor");
        texLoc = GL_GetUniformLocation(&r_aliasblended_program, "Tex");
        fullbrightTexLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "FullbrightTex");
        useFullbrightTexLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "UseFullbrightTex");
        useOverbrightLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "UseOverbright");
        useAlphaTestLoc =
            GL_GetUniformLocation(&r_aliasblended_program, "UseAlphaTest");
    }
}

/*
=============
GL_DrawAliasFrame_GLSL -- ericw

Optimized alias model drawing codepath.
Compared to the original GL_DrawAliasFrame, this makes 1 draw call,
no vertex data is uploaded (it's already in the r_meshvbo and r_meshindexesvbo
static VBOs), and lerping and lighting is done in the vertex shader.

Supports optional overbright, optional fullbright pixels.

Based on code by MH from RMQEngine
=============
*/
void GL_DrawAliasFrame_GLSL(aliashdr_t* paliashdr, const lerpdata_t& lerpdata,
    gltexture_t* tx, gltexture_t* fb)
{
    float blend;

    if(lerpdata.pose1 != lerpdata.pose2)
    {
        blend = lerpdata.blend;
    }
    else // poses the same means either 1. the entity has paused its animation,
         // or 2. r_lerpmodels is disabled
    {
        blend = 0;
    }

    glUseProgram(r_alias_program);

    glBindBuffer(GL_ARRAY_BUFFER, currententity->model->meshvbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, currententity->model->meshindexesvbo);

    glEnableVertexAttribArray(texCoordsAttrIndex);
    glEnableVertexAttribArray(pose1VertexAttrIndex);
    glEnableVertexAttribArray(pose2VertexAttrIndex);
    glEnableVertexAttribArray(pose1NormalAttrIndex);
    glEnableVertexAttribArray(pose2NormalAttrIndex);

    glVertexAttribPointer(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0,
        (void*)(intptr_t)currententity->model->vbostofs);
    glVertexAttribPointer(pose1VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE,
        sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose1));
    glVertexAttribPointer(pose2VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE,
        sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose2));
    // GL_TRUE to normalize the signed bytes to [-1 .. 1]
    glVertexAttribPointer(pose1NormalAttrIndex, 4, GL_BYTE, GL_TRUE,
        sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose1));
    glVertexAttribPointer(pose2NormalAttrIndex, 4, GL_BYTE, GL_TRUE,
        sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose2));

    // set uniforms
    glUniform1f(blendLoc, blend);
    glUniform3f(shadevectorLoc, shadevector[0], shadevector[1], shadevector[2]);
    glUniform4f(
        lightColorLoc, lightcolor[0], lightcolor[1], lightcolor[2], entalpha);
    glUniform1i(texLoc, 0);
    glUniform1i(fullbrightTexLoc, 1);
    glUniform1i(useFullbrightTexLoc, (fb != nullptr) ? 1 : 0);
    glUniform1f(useOverbrightLoc, overbright ? 1 : 0);
    glUniform1i(
        useAlphaTestLoc, (currententity->model->flags & MF_HOLEY) ? 1 : 0);

    // set textures
    GL_SelectTexture(GL_TEXTURE0);
    GL_Bind(tx);

    if(fb)
    {
        GL_SelectTexture(GL_TEXTURE1);
        GL_Bind(fb);
    }

    // draw
    glDrawElements(GL_TRIANGLES, paliashdr->numindexes, GL_UNSIGNED_SHORT,
        (void*)(intptr_t)currententity->model->vboindexofs);

    // clean up
    glDisableVertexAttribArray(pose2NormalAttrIndex);
    glDisableVertexAttribArray(pose1NormalAttrIndex);
    glDisableVertexAttribArray(pose2VertexAttrIndex);
    glDisableVertexAttribArray(pose1VertexAttrIndex);
    glDisableVertexAttribArray(texCoordsAttrIndex);

    glUseProgram(0);
    GL_SelectTexture(GL_TEXTURE0);

    rs_aliaspasses += paliashdr->numtris;
}

void GL_DrawBlendedAliasFrame_GLSL(aliashdr_t* paliashdr,
    const lerpdata_t& lerpdata, const lerpdata_t& zeroLerpdata,
    const qfloat zeroBlend, gltexture_t* tx, gltexture_t* fb)
{
    const float blend =
        (lerpdata.pose1 != lerpdata.pose2) ? lerpdata.blend : 0.f;

    glUseProgram(r_aliasblended_program);

    glBindBuffer(GL_ARRAY_BUFFER, currententity->model->meshvbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, currententity->model->meshindexesvbo);

    glEnableVertexAttribArray(texCoordsAttrIndex);
    glEnableVertexAttribArray(pose1VertexAttrIndex);
    glEnableVertexAttribArray(pose2VertexAttrIndex);
    glEnableVertexAttribArray(pose1NormalAttrIndex);
    glEnableVertexAttribArray(pose2NormalAttrIndex);
    glEnableVertexAttribArray(zeroBlendVertexAttrIndex);

    glVertexAttribPointer(texCoordsAttrIndex, 2, GL_FLOAT, GL_FALSE, 0,
        (void*)(intptr_t)currententity->model->vbostofs);

    glVertexAttribPointer(pose1VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE,
        sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose1));

    glVertexAttribPointer(pose2VertexAttrIndex, 4, GL_UNSIGNED_BYTE, GL_FALSE,
        sizeof(meshxyz_t), GLARB_GetXYZOffset(paliashdr, lerpdata.pose2));

    // GL_TRUE to normalize the signed bytes to [-1 .. 1]
    glVertexAttribPointer(pose1NormalAttrIndex, 4, GL_BYTE, GL_TRUE,
        sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose1));

    glVertexAttribPointer(pose2NormalAttrIndex, 4, GL_BYTE, GL_TRUE,
        sizeof(meshxyz_t), GLARB_GetNormalOffset(paliashdr, lerpdata.pose2));

    glVertexAttribPointer(zeroBlendVertexAttrIndex, 4, GL_UNSIGNED_BYTE,
        GL_FALSE, sizeof(meshxyz_t),
        GLARB_GetXYZOffset(paliashdr, zeroLerpdata.pose1));

    // set uniforms
    glUniform1f(blendLoc, blend);
    glUniform1f(zeroBlendLoc, zeroBlend);
    glUniform3f(shadevectorLoc, shadevector[0], shadevector[1], shadevector[2]);
    glUniform4f(
        lightColorLoc, lightcolor[0], lightcolor[1], lightcolor[2], entalpha);
    glUniform1i(texLoc, 0);
    glUniform1i(fullbrightTexLoc, 1);
    glUniform1i(useFullbrightTexLoc, (fb != nullptr) ? 1 : 0);
    glUniform1f(useOverbrightLoc, overbright ? 1 : 0);
    glUniform1i(
        useAlphaTestLoc, (currententity->model->flags & MF_HOLEY) ? 1 : 0);

    // set textures
    GL_SelectTexture(GL_TEXTURE0);
    GL_Bind(tx);

    if(fb)
    {
        GL_SelectTexture(GL_TEXTURE1);
        GL_Bind(fb);
    }

    // draw
    glDrawElements(GL_TRIANGLES, paliashdr->numindexes, GL_UNSIGNED_SHORT,
        (void*)(intptr_t)currententity->model->vboindexofs);

    // clean up
    glDisableVertexAttribArray(zeroBlendVertexAttrIndex);
    glDisableVertexAttribArray(pose2NormalAttrIndex);
    glDisableVertexAttribArray(pose1NormalAttrIndex);
    glDisableVertexAttribArray(pose2VertexAttrIndex);
    glDisableVertexAttribArray(pose1VertexAttrIndex);
    glDisableVertexAttribArray(texCoordsAttrIndex);

    glUseProgram(0);
    GL_SelectTexture(GL_TEXTURE0);

    rs_aliaspasses += paliashdr->numtris;
}

[[nodiscard]] DrawAliasFrameData getDrawAliasFrameData(
    const aliashdr_t& paliashdr, const lerpdata_t& lerpdata,
    const lerpdata_t& zeroLerpdata) noexcept
{
    DrawAliasFrameData res;

    const auto vertsStart =
        (trivertx_t*)(((byte*)(&paliashdr)) + paliashdr.posedata);

    if(lerpdata.pose1 != lerpdata.pose2)
    {
        res.lerping = true;
        res.verts1 = vertsStart;
        res.verts2 = res.verts1;
        res.verts1 += lerpdata.pose1 * paliashdr.poseverts;
        res.verts2 += lerpdata.pose2 * paliashdr.poseverts;
        res.blend = lerpdata.blend;
        res.iblend = 1.0f - res.blend;
    }
    else // poses the same means either 1. the entity has paused its animation,
         // or 2. r_lerpmodels is disabled
    {
        res.lerping = false;
        res.verts1 = vertsStart;
        res.verts1 += lerpdata.pose1 * paliashdr.poseverts;

        res.verts2 = res.verts1;    // avoid bogus compiler warning
        res.blend = res.iblend = 0; // avoid bogus compiler warning
    }

    res.zeroverts1 = vertsStart;
    res.zeroverts1 += zeroLerpdata.pose1 * paliashdr.poseverts;

    return res;
}

[[nodiscard]] qvec3 getFinalVertexPosLerped(
    const DrawAliasFrameData& fd, const qfloat zeroBlend) noexcept
{
    const qvec3 v{//
        fd.verts1->v[0] * fd.iblend + fd.verts2->v[0] * fd.blend,
        fd.verts1->v[1] * fd.iblend + fd.verts2->v[1] * fd.blend,
        fd.verts1->v[2] * fd.iblend + fd.verts2->v[2] * fd.blend};

    const qvec3 zv{
        fd.zeroverts1->v[0], fd.zeroverts1->v[1], fd.zeroverts1->v[2]};

    return glm::mix(v, zv, zeroBlend);
}

[[nodiscard]] qvec3 getFinalVertexPosNonLerped(
    const DrawAliasFrameData& fd, const qfloat zeroBlend) noexcept
{
    const qvec3 v{fd.verts1->v[0], fd.verts1->v[1], fd.verts1->v[2]};
    const qvec3 zv{
        fd.zeroverts1->v[0], fd.zeroverts1->v[1], fd.zeroverts1->v[2]};

    return glm::mix(v, zv, zeroBlend);
}

// TODO VR: (P0) test (done, this works). Cleanup?
void GL_DrawBlendedAliasFrame(const aliashdr_t* paliashdr,
    const lerpdata_t& lerpdata, const lerpdata_t& zeroLerpdata,
    const qfloat zeroBlend)
{
    if(zeroBlend <= 0.001)
    {
        void GL_DrawAliasFrame(
            const aliashdr_t* paliashdr, const lerpdata_t& lerpdata);
        GL_DrawAliasFrame(paliashdr, lerpdata);
        return;
    }

    auto fd = getDrawAliasFrameData(*paliashdr, lerpdata, zeroLerpdata);
    auto& [verts1, verts2, zeroverts1, blend, iblend, lerping] = fd;

    int* commands = (int*)((byte*)paliashdr + paliashdr->commands);

    float vertcolor[4];
    vertcolor[3] = entalpha; // never changes, so there's no need to put this
                             // inside the loop

    while(true)
    {
        // get the vertex count and primitive type
        int count = *commands++;
        if(!count)
        {
            break; // done
        }

        if(count < 0)
        {
            count = -count;
            glBegin(GL_TRIANGLE_FAN);
        }
        else
        {
            glBegin(GL_TRIANGLE_STRIP);
        }

        do
        {
            float u = ((float*)commands)[0];
            float v = ((float*)commands)[1];
            if(mtexenabled)
            {
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, v);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, v);
            }
            else
            {
                glTexCoord2f(u, v);
            }

            commands += 2;

            if(shading)
            {
                if(r_drawflat_cheatsafe)
                {
                    srand(count * (unsigned int)(src_offset_t)commands);
                    glColor3f(rand() % 256 / 255.0, rand() % 256 / 255.0,
                        rand() % 256 / 255.0);
                }
                else if(lerping)
                {
                    vertcolor[0] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[0];
                    vertcolor[1] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[1];
                    vertcolor[2] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[2];
                    glColor4fv(vertcolor);
                }
                else
                {
                    vertcolor[0] =
                        shadedots[verts1->lightnormalindex] * lightcolor[0];
                    vertcolor[1] =
                        shadedots[verts1->lightnormalindex] * lightcolor[1];
                    vertcolor[2] =
                        shadedots[verts1->lightnormalindex] * lightcolor[2];
                    glColor4fv(vertcolor);
                }
            }

            if(lerping)
            {
                const qvec3 f = getFinalVertexPosLerped(fd, zeroBlend);
                glVertex3f(f[0], f[1], f[2]);

                verts1++;
                verts2++;

                zeroverts1++;
            }
            else
            {
                const qvec3 f = getFinalVertexPosNonLerped(fd, zeroBlend);
                glVertex3f(f[0], f[1], f[2]);

                verts1++;
                zeroverts1++;
            }
        } while(--count);

        glEnd();
    }

    rs_aliaspasses += paliashdr->numtris;
}

/*
=============
GL_DrawAliasFrame -- johnfitz -- rewritten to support colored light, lerping,
entalpha, multitexture, and r_drawflat
=============
*/
void GL_DrawAliasFrame(const aliashdr_t* paliashdr, const lerpdata_t& lerpdata)
{
    trivertx_t* verts1;
    trivertx_t* verts2;

    float blend;

    float iblend;
    bool lerping;

    if(lerpdata.pose1 != lerpdata.pose2)
    {
        lerping = true;
        verts1 = (trivertx_t*)((byte*)paliashdr + paliashdr->posedata);
        verts2 = verts1;
        verts1 += lerpdata.pose1 * paliashdr->poseverts;
        verts2 += lerpdata.pose2 * paliashdr->poseverts;
        blend = lerpdata.blend;
        iblend = 1.0f - blend;
    }
    else // poses the same means either 1. the entity has paused its animation,
         // or 2. r_lerpmodels is disabled
    {
        lerping = false;
        verts1 = (trivertx_t*)((byte*)paliashdr + paliashdr->posedata);
        verts2 = verts1; // avoid bogus compiler warning
        verts1 += lerpdata.pose1 * paliashdr->poseverts;
        blend = iblend = 0; // avoid bogus compiler warning
    }

    int* commands = (int*)((byte*)paliashdr + paliashdr->commands);

    float vertcolor[4];
    vertcolor[3] = entalpha; // never changes, so there's no need to put this
                             // inside the loop

    while(true)
    {
        // get the vertex count and primitive type
        int count = *commands++;
        if(!count)
        {
            break; // done
        }

        if(count < 0)
        {
            count = -count;
            glBegin(GL_TRIANGLE_FAN);
        }
        else
        {
            glBegin(GL_TRIANGLE_STRIP);
        }

        do
        {
            float u = ((float*)commands)[0];
            float v = ((float*)commands)[1];
            if(mtexenabled)
            {
                glMultiTexCoord2fARB(GL_TEXTURE0_ARB, u, v);
                glMultiTexCoord2fARB(GL_TEXTURE1_ARB, u, v);
            }
            else
            {
                glTexCoord2f(u, v);
            }

            commands += 2;

            if(shading)
            {
                if(r_drawflat_cheatsafe)
                {
                    srand(count * (unsigned int)(src_offset_t)commands);
                    glColor3f(rand() % 256 / 255.0, rand() % 256 / 255.0,
                        rand() % 256 / 255.0);
                }
                else if(lerping)
                {
                    vertcolor[0] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[0];
                    vertcolor[1] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[1];
                    vertcolor[2] =
                        (shadedots[verts1->lightnormalindex] * iblend +
                            shadedots[verts2->lightnormalindex] * blend) *
                        lightcolor[2];
                    glColor4fv(vertcolor);
                }
                else
                {
                    vertcolor[0] =
                        shadedots[verts1->lightnormalindex] * lightcolor[0];
                    vertcolor[1] =
                        shadedots[verts1->lightnormalindex] * lightcolor[1];
                    vertcolor[2] =
                        shadedots[verts1->lightnormalindex] * lightcolor[2];
                    glColor4fv(vertcolor);
                }
            }

            if(lerping)
            {
                glVertex3f(verts1->v[0] * iblend + verts2->v[0] * blend,
                    verts1->v[1] * iblend + verts2->v[1] * blend,
                    verts1->v[2] * iblend + verts2->v[2] * blend);
                verts1++;
                verts2++;
            }
            else
            {
                glVertex3f(verts1->v[0], verts1->v[1], verts1->v[2]);
                verts1++;
            }
        } while(--count);

        glEnd();
    }

    rs_aliaspasses += paliashdr->numtris;
}

/*
=================
R_SetupAliasFrame -- johnfitz -- rewritten to support lerping
=================
*/
void R_SetupAliasFrame(
    entity_t* e, const aliashdr_t& paliashdr, int frame, lerpdata_t* lerpdata)
{
    if((frame >= paliashdr.numframes) || (frame < 0))
    {
        Con_DPrintf("R_AliasSetupFrame: no such frame %d for '%s'\n", frame,
            e->model->name);
        frame = 0;
    }

    int posenum = paliashdr.frames[frame].firstpose;
    const int numposes = paliashdr.frames[frame].numposes;

    if(numposes > 1)
    {
        e->lerptime = paliashdr.frames[frame].interval;
        posenum += (int)(cl.time / e->lerptime) % numposes;
    }
    else
    {
        e->lerptime = 0.1;
    }

    if(e->lerpflags & LERP_RESETANIM) // kill any lerp in progress
    {
        e->lerpstart = 0;
        e->previouspose = posenum;
        e->currentpose = posenum;
        e->lerpflags -= LERP_RESETANIM;
    }
    else if(e->currentpose != posenum) // pose changed, start new lerp
    {
        if(e->lerpflags & LERP_RESETANIM2) // defer lerping one more time
        {
            e->lerpstart = 0;
            e->previouspose = posenum;
            e->currentpose = posenum;
            e->lerpflags -= LERP_RESETANIM2;
        }
        else
        {
            e->lerpstart = cl.time;
            e->previouspose = e->currentpose;
            e->currentpose = posenum;
        }
    }

    // set up values
    if(r_lerpmodels.value &&
        !(e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
    {
        if(e->lerpflags & LERP_FINISH && numposes == 1)
        {
            lerpdata->blend = CLAMP(0,
                (cl.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 1);
        }
        else
        {
            lerpdata->blend =
                CLAMP(0, (cl.time - e->lerpstart) / e->lerptime, 1);
        }
        lerpdata->pose1 = e->previouspose;
        lerpdata->pose2 = e->currentpose;
    }
    else // don't lerp
    {
        lerpdata->blend = 1;
        lerpdata->pose1 = posenum;
        lerpdata->pose2 = posenum;
    }
}

void R_SetupAliasFrameZero(
    const aliashdr_t& paliashdr, int frame, lerpdata_t* lerpdata)
{
    if((frame >= paliashdr.numframes) || (frame < 0))
    {
        frame = 0;
    }

    int posenum = paliashdr.frames[frame].firstpose;
    const int numposes = paliashdr.frames[frame].numposes;

    if(numposes > 1)
    {
        posenum += (int)(cl.time / 0.1) % numposes;
    }

    // set up values
    lerpdata->blend = 1;
    lerpdata->pose1 = lerpdata->pose2 = posenum;
}

/*
=================
R_SetupEntityTransform -- johnfitz -- set up transform part of lerpdata
=================
*/
void R_SetupEntityTransform(entity_t* e, lerpdata_t* lerpdata)
{
    float blend;

    // if LERP_RESETMOVE, kill any lerps in progress
    if(e->lerpflags & LERP_RESETMOVE)
    {
        e->movelerpstart = 0;
        e->previousorigin = e->origin;
        e->currentorigin = e->origin;
        e->previousangles = e->angles;
        e->currentangles = e->angles;
        e->lerpflags -= LERP_RESETMOVE;
    }
    else if(e->origin != e->currentorigin || e->angles != e->currentangles)
    {
        // origin/angles changed, start new lerp
        e->movelerpstart = cl.time;
        e->previousorigin = e->currentorigin;
        e->currentorigin = e->origin;
        e->previousangles = e->currentangles;
        e->currentangles = e->angles;
    }

    // set up values
    if(r_lerpmove.value && e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
    {
        if(e->lerpflags & LERP_FINISH)
        {
            blend = CLAMP(0,
                (cl.time - e->movelerpstart) /
                    (e->lerpfinish - e->movelerpstart),
                1);
        }
        else
        {
            blend = CLAMP(0, (cl.time - e->movelerpstart) / 0.1, 1);
        }

        // translation
        qvec3 d = e->currentorigin - e->previousorigin;
        lerpdata->origin[0] = e->previousorigin[0] + d[0] * blend;
        lerpdata->origin[1] = e->previousorigin[1] + d[1] * blend;
        lerpdata->origin[2] = e->previousorigin[2] + d[2] * blend;

        // rotation
        d = e->currentangles - e->previousangles;
        for(int i = 0; i < 3; i++)
        {
            if(d[i] > 180)
            {
                d[i] -= 360;
            }
            if(d[i] < -180)
            {
                d[i] += 360;
            }
        }
        lerpdata->angles[0] = e->previousangles[0] + d[0] * blend;
        lerpdata->angles[1] = e->previousangles[1] + d[1] * blend;
        lerpdata->angles[2] = e->previousangles[2] + d[2] * blend;
    }
    else // don't lerp
    {
        lerpdata->origin = e->origin;
        lerpdata->angles = e->angles;
    }
}

/*
=================
R_SetupAliasLighting -- johnfitz -- broken out from R_DrawAliasModel and
rewritten
=================
*/
void R_SetupAliasLighting(entity_t* e)
{
    R_LightPoint(e->origin);

    // add dlights
    for(int i = 0; i < MAX_DLIGHTS; i++)
    {
        if(cl_dlights[i].die >= cl.time)
        {
            const qvec3 dist = currententity->origin - cl_dlights[i].origin;
            const qfloat add = cl_dlights[i].radius - glm::length(dist);

            if(add > 0)
            {
                lightcolor += add * cl_dlights[i].color;
            }
        }
    }

    const bool isViewmodel =
        anyViewmodel(cl, [&](entity_t& ent) { return e == &ent; });

    // TODO VR: (P2) repetition here to check player view entities
    // minimum light value on gun (24)
    if(isViewmodel)
    {
        const qfloat add =
            72.0_qf - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
        if(add > 0.0_qf)
        {
            lightcolor[0] += add / 3.0_qf;
            lightcolor[1] += add / 3.0_qf;
            lightcolor[2] += add / 3.0_qf;
        }
    }

    // minimum light value on players (8)
    if(currententity > cl_entities &&
        currententity <= cl_entities + cl.maxclients)
    {
        const qfloat add =
            24.0_qf - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
        if(add > 0.0_qf)
        {
            lightcolor[0] += add / 3.0_qf;
            lightcolor[1] += add / 3.0_qf;
            lightcolor[2] += add / 3.0_qf;
        }
    }

    // clamp lighting so it doesn't overbright as much (96)
    if(overbright)
    {
        const qfloat add =
            288.0_qf / (lightcolor[0] + lightcolor[1] + lightcolor[2]);
        if(add < 1.0_qf)
        {
            lightcolor *= add;
        }
    }

    // hack up the brightness when fullbrights but no overbrights (256)
    if(gl_fullbrights.value && !gl_overbright_models.value)
    {
        if(e->model->flags & MOD_FBRIGHTHACK)
        {
            lightcolor[0] = 256.0_qf;
            lightcolor[1] = 256.0_qf;
            lightcolor[2] = 256.0_qf;
        }
    }

    const int quantizedangle =
        ((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1);

    // ericw -- shadevector is passed to the shader to compute shadedots inside
    // the shader, see GLAlias_CreateShaders()
    const float radiansangle = (quantizedangle / 16.0) * 2.0 * 3.14159;
    shadevector[0] = cos(-radiansangle);
    shadevector[1] = sin(-radiansangle);
    shadevector[2] = 1;
    shadevector = safeNormalize(shadevector);
    // ericw --

    shadedots = r_avertexnormal_dots[quantizedangle];
    lightcolor *= 1.0f / 200.0f;
}

/*
=================
R_DrawAliasModel -- johnfitz -- almost completely rewritten
=================
*/
void R_DrawAliasModel(entity_t* e)
{
    // Cannot move down due to goto.
    int anim;
    int skinnum;
    int i;
    gltexture_t* tx;
    gltexture_t* fb;

    const bool alphatest = !!(e->model->flags & MF_HOLEY);
    const qfloat zeroBlend = e->zeroBlend;

    //
    // setup pose/lerp data -- do it first so we don't miss updates due to
    // culling
    //
    aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(e->model);

    lerpdata_t zeroLerpdata;
    R_SetupAliasFrameZero(*paliashdr, 0, &zeroLerpdata);

    lerpdata_t lerpdata;
    R_SetupAliasFrame(e, *paliashdr, e->frame, &lerpdata);
    R_SetupEntityTransform(e, &lerpdata);

    //
    // cull it
    //
    if(R_CullModelForEntity(e))
    {
        return;
    }

    //
    // transform it
    //
    glPushMatrix();
    R_RotateForEntity(lerpdata.origin, lerpdata.angles);

    if(e->horizFlip)
    {
        glScalef(1.0f, -1.0f, 1.0f);
        glFrontFace(GL_CCW);
    }

    // TODO VR: (P1) document why we have +1
    glTranslatef(-e->scale_origin[0], -e->scale_origin[1], -e->scale_origin[2]);
    glScalef(e->scale[0] + 1.f, e->scale[1] + 1.f, e->scale[2] + 1.f);
    glTranslatef(e->scale_origin[0], e->scale_origin[1], e->scale_origin[2]);

    glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1],
        paliashdr->scale_origin[2]);
    glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

    //
    // random stuff
    //
    if(gl_smoothmodels.value && !r_drawflat_cheatsafe)
    {
        glShadeModel(GL_SMOOTH);
    }
    if(gl_affinemodels.value)
    {
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
    }
    overbright = gl_overbright_models.value;
    shading = true;

    //
    // set up for alpha blending
    //
    if(r_drawflat_cheatsafe || r_lightmap_cheatsafe)
    {
        // no alpha in drawflat or lightmap mode
        entalpha = 1;
    }
    else
    {
        entalpha = ENTALPHA_DECODE(e->alpha);
    }
    if(entalpha == 0)
    {
        goto cleanup;
    }
    if(entalpha < 1)
    {
        if(!gl_texture_env_combine)
        {
            overbright = false; // overbright can't be done in a single pass
        }
        // without combiners
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
    }
    else if(alphatest)
    {
        glEnable(GL_ALPHA_TEST);
    }

    //
    // set up lighting
    //
    rs_aliaspolys += paliashdr->numtris;
    R_SetupAliasLighting(e);

    //
    // set up textures
    //
    GL_DisableMultitexture();

    anim = (int)(cl.time * 10) & 3;
    skinnum = e->skinnum;
    if((skinnum >= paliashdr->numskins) || (skinnum < 0))
    {
        Con_DPrintf("R_DrawAliasModel: no such skin # %d for '%s'\n", skinnum,
            e->model->name);
        // ericw -- display skin 0 for winquake compatibility
        skinnum = 0;
    }
    tx = paliashdr->gltextures[skinnum][anim];
    fb = paliashdr->fbtextures[skinnum][anim];
    if(e->colormap != vid.colormap && !gl_nocolors.value)
    {
        i = e - cl_entities;
        if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
        {
            tx = playertextures[i - 1];
        }
    }
    if(!gl_fullbrights.value)
    {
        fb = nullptr;
    }

    //
    // draw it
    //
    if(r_drawflat_cheatsafe)
    {
        glDisable(GL_TEXTURE_2D);
        GL_DrawBlendedAliasFrame(paliashdr, lerpdata, zeroLerpdata, zeroBlend);
        glEnable(GL_TEXTURE_2D);
        srand((int)(cl.time * 1000)); // restore randomness
    }
    else if(r_fullbright_cheatsafe)
    {
        GL_Bind(tx);
        shading = false;
        glColor4f(1, 1, 1, entalpha);
        GL_DrawBlendedAliasFrame(paliashdr, lerpdata, zeroLerpdata, zeroBlend);
        if(fb)
        {
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_Bind(fb);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            glColor3f(entalpha, entalpha, entalpha);
            Fog_StartAdditive();
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            Fog_StopAdditive();
            glDepthMask(GL_TRUE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
        }
    }
    else if(r_lightmap_cheatsafe)
    {
        glDisable(GL_TEXTURE_2D);
        shading = false;
        glColor3f(1, 1, 1);
        GL_DrawBlendedAliasFrame(paliashdr, lerpdata, zeroLerpdata, zeroBlend);
        glEnable(GL_TEXTURE_2D);
    }
    // call fast path if possible. if the shader compliation failed for some
    // reason, r_alias_program will be 0.
    // TODO VR: (P0) test, restore? (done, this works. Cleanup)
    else if(/*r_alias_program != 0 && */ r_aliasblended_program != 0)
    {
        if(false && zeroBlend <= 0.001)
        {
            GL_DrawAliasFrame_GLSL(paliashdr, lerpdata, tx, fb);
        }
        else
        {
            GL_DrawBlendedAliasFrame_GLSL(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend, tx, fb);
        }
    }
    else if(overbright)
    {
        if(gl_texture_env_combine && gl_mtexable && gl_texture_env_add &&
            fb) // case 1: everything in one pass
        {
            GL_Bind(tx);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
            GL_EnableMultitexture(); // selects TEXTURE1
            GL_Bind(fb);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
            glEnable(GL_BLEND);
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            glDisable(GL_BLEND);
            GL_DisableMultitexture();
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
        else if(gl_texture_env_combine) // case 2: overbright in one pass, then
                                        // fullbright pass
        {
            // first pass
            GL_Bind(tx);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            // second pass
            if(fb)
            {
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                GL_Bind(fb);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
                shading = false;
                glColor3f(entalpha, entalpha, entalpha);
                Fog_StartAdditive();
                GL_DrawBlendedAliasFrame(
                    paliashdr, lerpdata, zeroLerpdata, zeroBlend);
                Fog_StopAdditive();
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_BLEND);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            }
        }
        else // case 3: overbright in two passes, then fullbright pass
        {
            // first pass
            GL_Bind(tx);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            // second pass -- additive with black fog, to double the object
            // colors but not the fog color
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            Fog_StartAdditive();
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            Fog_StopAdditive();
            glDepthMask(GL_TRUE);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            // third pass
            if(fb)
            {
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                GL_Bind(fb);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
                shading = false;
                glColor3f(entalpha, entalpha, entalpha);
                Fog_StartAdditive();
                GL_DrawBlendedAliasFrame(
                    paliashdr, lerpdata, zeroLerpdata, zeroBlend);
                Fog_StopAdditive();
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_BLEND);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
            }
        }
    }
    else
    {
        if(gl_mtexable && gl_texture_env_add &&
            fb) // case 4: fullbright mask using multitexture
        {
            GL_DisableMultitexture(); // selects TEXTURE0
            GL_Bind(tx);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_EnableMultitexture(); // selects TEXTURE1
            GL_Bind(fb);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
            glEnable(GL_BLEND);
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            glDisable(GL_BLEND);
            GL_DisableMultitexture();
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        }
        else // case 5: fullbright mask without multitexture
        {
            // first pass
            GL_Bind(tx);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            GL_DrawBlendedAliasFrame(
                paliashdr, lerpdata, zeroLerpdata, zeroBlend);
            // second pass
            if(fb)
            {
                GL_Bind(fb);
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                glDepthMask(GL_FALSE);
                shading = false;
                glColor3f(entalpha, entalpha, entalpha);
                Fog_StartAdditive();
                GL_DrawBlendedAliasFrame(
                    paliashdr, lerpdata, zeroLerpdata, zeroBlend);
                Fog_StopAdditive();
                glDepthMask(GL_TRUE);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_BLEND);
            }
        }
    }

cleanup:
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glShadeModel(GL_FLAT);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    if(alphatest)
    {
        glDisable(GL_ALPHA_TEST);
    }
    glColor3f(1, 1, 1);
    glPopMatrix();

    if(e->horizFlip)
    {
        glFrontFace(GL_CW);
    }
}

// johnfitz -- values for shadow matrix
#define SHADOW_SKEW_X -0.7 // skew along x axis. -0.7 to mimic glquake shadows
#define SHADOW_SKEW_Y 0    // skew along y axis. 0 to mimic glquake shadows
#define SHADOW_VSCALE 0    // 0=completely flat
#define SHADOW_HEIGHT 0.1  // how far above the floor to render the shadow
// johnfitz

/*
=============
GL_DrawAliasShadow -- johnfitz -- rewritten

TODO: orient shadow onto "lightplane" (a global mplane_t*)
=============
*/
void GL_DrawAliasShadow(entity_t* e)
{   
    // TODO VR: (P1) does this attempt to draw shadows for the world model...?

    float shadowmatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, SHADOW_SKEW_X,
        SHADOW_SKEW_Y, SHADOW_VSCALE, 0, 0, 0, SHADOW_HEIGHT, 1};
    float lheight;
    aliashdr_t* paliashdr;
    lerpdata_t lerpdata;

    if(R_CullModelForEntity(e))
    {
        return;
    }

    // TODO VR: (P2) viewent shadow looks weird
    if(/*e == &cl.viewent ||*/ e->model->flags & MOD_NOSHADOW)
    {
        return;
    }

    entalpha = ENTALPHA_DECODE(e->alpha);
    if(entalpha == 0)
    {
        return;
    }

    paliashdr = (aliashdr_t*)Mod_Extradata(e->model);
    R_SetupAliasFrame(e, *paliashdr, e->frame, &lerpdata);
    R_SetupEntityTransform(e, &lerpdata);
    R_LightPoint(e->origin);
    lheight = currententity->origin[2] - lightspot[2];

    // set up matrix
    glPushMatrix();
    glTranslatef(lerpdata.origin[0], lerpdata.origin[1], lerpdata.origin[2]);
    glTranslatef(0, 0, -lheight);
    glMultMatrixf(shadowmatrix);
    glTranslatef(0, 0, lheight);
    glRotatef(lerpdata.angles[1], 0, 0, 1);
    glRotatef(-lerpdata.angles[0], 0, 1, 0);
    glRotatef(lerpdata.angles[2], 1, 0, 0);
    glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1],
        paliashdr->scale_origin[2]);
    glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

    // draw it
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    GL_DisableMultitexture();
    glDisable(GL_TEXTURE_2D);
    shading = false;
    glColor4f(0, 0, 0, entalpha * 0.5);
    // TODO VR: (P1) can this use glsl?
    GL_DrawAliasFrame(paliashdr, lerpdata);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    // clean up
    glPopMatrix();
}

/*
=================
R_DrawAliasModel_ShowTris -- johnfitz
=================
*/
void R_DrawAliasModel_ShowTris(entity_t* e)
{
    aliashdr_t* paliashdr;
    lerpdata_t lerpdata;

    if(R_CullModelForEntity(e))
    {
        return;
    }

    paliashdr = (aliashdr_t*)Mod_Extradata(e->model);
    R_SetupAliasFrame(e, *paliashdr, e->frame, &lerpdata);
    R_SetupEntityTransform(e, &lerpdata);

    glPushMatrix();
    R_RotateForEntity(lerpdata.origin, lerpdata.angles);
    glTranslatef(paliashdr->scale_origin[0], paliashdr->scale_origin[1],
        paliashdr->scale_origin[2]);
    glScalef(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

    shading = false;
    glColor3f(1, 1, 1);
    GL_DrawAliasFrame(paliashdr, lerpdata);

    glPopMatrix();
}
