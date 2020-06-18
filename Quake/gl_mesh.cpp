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
// gl_mesh.c: triangle model functions

#include "quakedef.hpp"
#include "glquake.hpp"
#include "client.hpp"
#include "gl_texmgr.hpp"
#include "console.hpp"
#include "sys.hpp"
#include "byteorder.hpp"
#include "mathlib.hpp"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

struct qmodel_t;

qmodel_t* aliasmodel;
aliashdr_t* paliashdr;

int used[8192]; // bool

// the command list holds counts and s/t values that are valid for
// every frame
int commands[8192];
int numcommands;

// all frames will have their vertexes rearranged and expanded
// so they are in the order expected by the command list
int vertexorder[8192];
int numorder;

int allverts, alltris;

int stripverts[128];
int striptris[128];
int stripcount;

/*
================
StripLength
================
*/
int StripLength(int starttri, int startv)
{
    int m1;

    int m2;
    int j;
    mtriangle_t* last;

    mtriangle_t* check;
    int k;

    used[starttri] = 2;

    last = &triangles[starttri];

    stripverts[0] = last->vertindex[(startv) % 3];
    stripverts[1] = last->vertindex[(startv + 1) % 3];
    stripverts[2] = last->vertindex[(startv + 2) % 3];

    striptris[0] = starttri;
    stripcount = 1;

    m1 = last->vertindex[(startv + 2) % 3];
    m2 = last->vertindex[(startv + 1) % 3];

    // look for a matching triangle
nexttri:
    for(j = starttri + 1, check = &triangles[starttri + 1];
        j < pheader->numtris; j++, check++)
    {
        if(check->facesfront != last->facesfront)
        {
            continue;
        }
        for(k = 0; k < 3; k++)
        {
            if(check->vertindex[k] != m1)
            {
                continue;
            }
            if(check->vertindex[(k + 1) % 3] != m2)
            {
                continue;
            }

            // this is the next part of the fan

            // if we can't use this triangle, this tristrip is done
            if(used[j])
            {
                goto done;
            }

            // the new edge
            if(stripcount & 1)
            {
                m2 = check->vertindex[(k + 2) % 3];
            }
            else
            {
                m1 = check->vertindex[(k + 2) % 3];
            }

            stripverts[stripcount + 2] = check->vertindex[(k + 2) % 3];
            striptris[stripcount] = j;
            stripcount++;

            used[j] = 2;
            goto nexttri;
        }
    }
done:

    // clear the temp used flags
    for(j = starttri + 1; j < pheader->numtris; j++)
    {
        if(used[j] == 2)
        {
            used[j] = 0;
        }
    }

    return stripcount;
}

/*
===========
FanLength
===========
*/
int FanLength(int starttri, int startv)
{
    int m1;

    int m2;
    int j;
    mtriangle_t* last;

    mtriangle_t* check;
    int k;

    used[starttri] = 2;

    last = &triangles[starttri];

    stripverts[0] = last->vertindex[(startv) % 3];
    stripverts[1] = last->vertindex[(startv + 1) % 3];
    stripverts[2] = last->vertindex[(startv + 2) % 3];

    striptris[0] = starttri;
    stripcount = 1;

    m1 = last->vertindex[(startv + 0) % 3];
    m2 = last->vertindex[(startv + 2) % 3];


    // look for a matching triangle
nexttri:
    for(j = starttri + 1, check = &triangles[starttri + 1];
        j < pheader->numtris; j++, check++)
    {
        if(check->facesfront != last->facesfront)
        {
            continue;
        }
        for(k = 0; k < 3; k++)
        {
            if(check->vertindex[k] != m1)
            {
                continue;
            }
            if(check->vertindex[(k + 1) % 3] != m2)
            {
                continue;
            }

            // this is the next part of the fan

            // if we can't use this triangle, this tristrip is done
            if(used[j])
            {
                goto done;
            }

            // the new edge
            m2 = check->vertindex[(k + 2) % 3];

            stripverts[stripcount + 2] = m2;
            striptris[stripcount] = j;
            stripcount++;

            used[j] = 2;
            goto nexttri;
        }
    }
done:

    // clear the temp used flags
    for(j = starttri + 1; j < pheader->numtris; j++)
    {
        if(used[j] == 2)
        {
            used[j] = 0;
        }
    }

    return stripcount;
}


/*
================
BuildTris

Generate a list of trifans or strips
for the model, which holds for all frames
================
*/
void BuildTris()
{
    int i;

    int j;

    int k;
    int startv;
    float s;

    float t;
    int len;

    int bestlen;

    int besttype;
    int bestverts[1024];
    int besttris[1024];
    int type;

    //
    // build tristrips
    //
    numorder = 0;
    numcommands = 0;
    memset(used, 0, sizeof(used));
    for(i = 0; i < pheader->numtris; i++)
    {
        // pick an unused triangle and start the trifan
        if(used[i])
        {
            continue;
        }

        bestlen = 0;
        besttype = 0;
        for(type = 0; type < 2; type++)
        //	type = 1;
        {
            for(startv = 0; startv < 3; startv++)
            {
                if(type == 1)
                {
                    len = StripLength(i, startv);
                }
                else
                {
                    len = FanLength(i, startv);
                }
                if(len > bestlen)
                {
                    besttype = type;
                    bestlen = len;
                    for(j = 0; j < bestlen + 2; j++)
                    {
                        bestverts[j] = stripverts[j];
                    }
                    for(j = 0; j < bestlen; j++)
                    {
                        besttris[j] = striptris[j];
                    }
                }
            }
        }

        // mark the tris on the best strip as used
        for(j = 0; j < bestlen; j++)
        {
            used[besttris[j]] = 1;
        }

        if(besttype == 1)
        {
            commands[numcommands++] = (bestlen + 2);
        }
        else
        {
            commands[numcommands++] = -(bestlen + 2);
        }

        for(j = 0; j < bestlen + 2; j++)
        {
            int tmp;

            // emit a vertex into the reorder buffer
            k = bestverts[j];
            vertexorder[numorder++] = k;

            // emit s/t coords into the commands stream
            s = stverts[k].s;
            t = stverts[k].t;
            if(!triangles[besttris[0]].facesfront && stverts[k].onseam)
            {
                s += pheader->skinwidth / 2; // on back side
            }
            s = (s + 0.5) / pheader->skinwidth;
            t = (t + 0.5) / pheader->skinheight;

            //	*(float *)&commands[numcommands++] = s;
            //	*(float *)&commands[numcommands++] = t;
            // NOTE: 4 == sizeof(int)
            //	   == sizeof(float)
            memcpy(&tmp, &s, 4);
            commands[numcommands++] = tmp;
            memcpy(&tmp, &t, 4);
            commands[numcommands++] = tmp;
        }
    }

    commands[numcommands++] = 0; // end of list marker

    Con_DPrintf2(
        "%3i tri %3i vert %3i cmd\n", pheader->numtris, numorder, numcommands);

    allverts += numorder;
    alltris += pheader->numtris;
}

static void GL_MakeAliasModelDisplayLists_VBO();

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists(qmodel_t* m, aliashdr_t* hdr)
{
    int i;

    int j;
    int count;     // johnfitz -- precompute texcoords for padded skins
    int* loadcmds; // johnfitz

    // johnfitz -- padded skins
    const float hscale =
        (float)hdr->skinwidth / (float)TexMgr_PadConditional(hdr->skinwidth);

    // johnfitz -- padded skins
    const float vscale =
        (float)hdr->skinheight / (float)TexMgr_PadConditional(hdr->skinheight);
    // johnfitz

    aliasmodel = m;
    paliashdr = hdr; // (aliashdr_t *)Mod_Extradata (m);

    // johnfitz -- generate meshes
    Con_DPrintf2("meshing %s...\n", m->name);
    BuildTris();

    // save the data out

    paliashdr->poseverts = numorder;

    int* cmds = (int*)Hunk_Alloc(numcommands * 4);
    paliashdr->commands = (byte*)cmds - (byte*)paliashdr;

    // johnfitz -- precompute texcoords for padded skins
    loadcmds = commands;
    while(true)
    {
        *cmds++ = count = *loadcmds++;

        if(!count)
        {
            break;
        }

        if(count < 0)
        {
            count = -count;
        }

        do
        {
            *(float*)cmds++ = hscale * (*(float*)loadcmds++);
            *(float*)cmds++ = vscale * (*(float*)loadcmds++);
        } while(--count);
    }
    // johnfitz

    trivertx_t* verts = (trivertx_t*)Hunk_Alloc(
        paliashdr->nummorphposes * paliashdr->poseverts * sizeof(trivertx_t));
    paliashdr->posedata = (byte*)verts - (byte*)paliashdr;
    for(i = 0; i < paliashdr->nummorphposes; i++)
    {
        for(j = 0; j < numorder; j++)
        {
            *verts++ = poseverts_mdl[i][vertexorder[j]];
        }
    }

    // ericw
    GL_MakeAliasModelDisplayLists_VBO();
}

unsigned int r_meshindexbuffer = 0;
unsigned int r_meshvertexbuffer = 0;

/*
================
GL_MakeAliasModelDisplayLists_VBO

Saves data needed to build the VBO for this model on the hunk.
Afterwards this is copied to Mod_Extradata.

Original code by MH from RMQEngine
================
*/
void GL_MakeAliasModelDisplayLists_VBO()
{
    int i;

    int j;
    int maxverts_vbo;
    trivertx_t* verts;
    unsigned short* indexes;
    aliasmesh_t* desc;

    if(!gl_glsl_alias_able)
    {
        return;
    }

    // first, copy the verts onto the hunk
    verts = (trivertx_t*)Hunk_Alloc(
        paliashdr->nummorphposes * paliashdr->numverts * sizeof(trivertx_t));
    paliashdr->vertexes = (byte*)verts - (byte*)paliashdr;
    for(i = 0; i < paliashdr->nummorphposes; i++)
    {
        for(j = 0; j < paliashdr->numverts; j++)
        {
            verts[i * paliashdr->numverts + j] = poseverts_mdl[i][j];
        }
    }

    // there can never be more than this number of verts and we
    // just put them all on the hunk
    maxverts_vbo = pheader->numtris * 3;
    desc = (aliasmesh_t*)Hunk_Alloc(sizeof(aliasmesh_t) * maxverts_vbo);

    // there will always be this number of indexes
    indexes =
        (unsigned short*)Hunk_Alloc(sizeof(unsigned short) * maxverts_vbo);

    pheader->indexes = (intptr_t)indexes - (intptr_t)pheader;
    pheader->meshdesc = (intptr_t)desc - (intptr_t)pheader;
    pheader->numindexes = 0;
    pheader->numverts_vbo = 0;

    for(i = 0; i < pheader->numtris; i++)
    {
        for(j = 0; j < 3; j++)
        {
            int v;

            // index into hdr->vertexes
            unsigned short vertindex = triangles[i].vertindex[j];

            // basic s/t coords
            int s = stverts[vertindex].s;
            int t = stverts[vertindex].t;

            // check for back side and adjust texcoord s
            if(!triangles[i].facesfront && stverts[vertindex].onseam)
            {
                s += pheader->skinwidth / 2;
            }

            // see does this vert already exist
            for(v = 0; v < pheader->numverts_vbo; v++)
            {
                // it could use the same xyz but have different
                // s and t
                if(desc[v].vertindex == vertindex && (int)desc[v].st[0] == s &&
                    (int)desc[v].st[1] == t)
                {
                    // exists; emit an index for it
                    indexes[pheader->numindexes++] = v;

                    // no need to check any more
                    break;
                }
            }

            if(v == pheader->numverts_vbo)
            {
                // doesn't exist; emit a new vert and index
                indexes[pheader->numindexes++] = pheader->numverts_vbo;

                desc[pheader->numverts_vbo].vertindex = vertindex;
                desc[pheader->numverts_vbo].st[0] = s;
                desc[pheader->numverts_vbo++].st[1] = t;
            }
        }
    }

    // upload immediately
    GLMesh_LoadVertexBuffer(aliasmodel, pheader);
}

#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];

/*
================
GLMesh_LoadVertexBuffer

Upload the given alias model's mesh to a VBO

Original code by MH from RMQEngine
================
*/
void GLMesh_LoadVertexBuffer(qmodel_t* m, const aliashdr_t* hdr)
{
    int totalvbosize = 0;
    const aliasmesh_t* desc;
    const short* indexes;
    const trivertx_t* trivertexes;
    byte* vbodata;
    int f;

    if(!gl_glsl_alias_able)
    {
        return;
    }

    // count the sizes we need

    // ericw -- RMQEngine stored these vbo*ofs values in
    // aliashdr_t, but we must not mutate Mod_Extradata since it
    // might be reloaded from disk, so I moved them to qmodel_t
    // (test case: roman1.bsp from arwop, 64mb heap)
    m->vboindexofs = 0;

    m->vboxyzofs = 0;
    totalvbosize += (hdr->nummorphposes * hdr->numverts_vbo *
                     sizeof(meshxyz_t)); // ericw -- what RMQEngine
                                         // called nummeshframes is
                                         // called numposes in QuakeSpasm

    m->vbostofs = totalvbosize;
    totalvbosize += (hdr->numverts_vbo * sizeof(meshst_t));

    if(!hdr->numindexes)
    {
        return;
    }
    if(!totalvbosize)
    {
        return;
    }

    // grab the pointers to data in the extradata

    desc = (aliasmesh_t*)((byte*)hdr + hdr->meshdesc);
    indexes = (short*)((byte*)hdr + hdr->indexes);
    trivertexes = (trivertx_t*)((byte*)hdr + hdr->vertexes);

    // upload indices buffer

    glDeleteBuffersARB(1, &m->meshindexesvbo);
    glGenBuffersARB(1, &m->meshindexesvbo);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER, m->meshindexesvbo);
    glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER,
        hdr->numindexes * sizeof(unsigned short), indexes, GL_STATIC_DRAW);

    // create the vertex buffer (empty)

    vbodata = (byte*)malloc(totalvbosize);
    memset(vbodata, 0, totalvbosize);

    // fill in the vertices at the start of the buffer
    for(f = 0; f < hdr->nummorphposes;
        f++) // ericw -- what RMQEngine called nummeshframes is
             // called numposes in QuakeSpasm
    {
        int v;
        auto* xyz =
            (meshxyz_t*)(vbodata + (f * hdr->numverts_vbo * sizeof(meshxyz_t)));
        const trivertx_t* tv = trivertexes + (hdr->numverts * f);

        for(v = 0; v < hdr->numverts_vbo; v++)
        {
            trivertx_t trivert = tv[desc[v].vertindex];

            xyz[v].xyz[0] = trivert.v[0];
            xyz[v].xyz[1] = trivert.v[1];
            xyz[v].xyz[2] = trivert.v[2];
            xyz[v].xyz[3] = 1; // need w 1 for 4 byte vertex compression

            // map the normal coordinates in [-1..1] to
            // [-127..127] and store in an unsigned char. this
            // introduces some error (less than 0.004), but the
            // normals were very coarse to begin with
            xyz[v].normal[0] =
                127 * r_avertexnormals[trivert.lightnormalindex][0];
            xyz[v].normal[1] =
                127 * r_avertexnormals[trivert.lightnormalindex][1];
            xyz[v].normal[2] =
                127 * r_avertexnormals[trivert.lightnormalindex][2];
            xyz[v].normal[3] = 0; // unused; for 4-byte alignment
        }
    }

    // fill in the ST coords at the end of the buffer
    {
        meshst_t* st;
        float hscale;

        float vscale;

        // johnfitz -- padded skins
        hscale = (float)hdr->skinwidth /
                 (float)TexMgr_PadConditional(hdr->skinwidth);
        vscale = (float)hdr->skinheight /
                 (float)TexMgr_PadConditional(hdr->skinheight);
        // johnfitz

        st = (meshst_t*)(vbodata + m->vbostofs);
        for(f = 0; f < hdr->numverts_vbo; f++)
        {
            st[f].st[0] =
                hscale * ((float)desc[f].st[0] + 0.5f) / (float)hdr->skinwidth;
            st[f].st[1] =
                vscale * ((float)desc[f].st[1] + 0.5f) / (float)hdr->skinheight;
        }
    }

    // upload vertexes buffer
    glDeleteBuffersARB(1, &m->meshvbo);
    glGenBuffersARB(1, &m->meshvbo);
    glBindBufferARB(GL_ARRAY_BUFFER, m->meshvbo);
    glBufferDataARB(GL_ARRAY_BUFFER, totalvbosize, vbodata, GL_STATIC_DRAW);

    free(vbodata);

    // invalidate the cached bindings
    GL_ClearBufferBindings();
}

/*
================
GLMesh_LoadVertexBuffers

Loop over all precached alias models, and upload each one to a
VBO.
================
*/
void GLMesh_LoadVertexBuffers()
{
    int j;
    qmodel_t* m;
    const aliashdr_t* hdr;

    if(!gl_glsl_alias_able)
    {
        return;
    }

    for(j = 1; j < MAX_MODELS; j++)
    {
        if(!(m = cl.model_precache[j]))
        {
            break;
        }
        if(m->type != mod_alias)
        {
            continue;
        }

        hdr = (const aliashdr_t*)Mod_Extradata(m);

        GLMesh_LoadVertexBuffer(m, hdr);
    }
}

/*
================
GLMesh_DeleteVertexBuffers

Delete VBOs for all loaded alias models
================
*/
void GLMesh_DeleteVertexBuffers()
{
    int j;
    qmodel_t* m;

    if(!gl_glsl_alias_able)
    {
        return;
    }

    for(j = 1; j < MAX_MODELS; j++)
    {
        if(!(m = cl.model_precache[j]))
        {
            break;
        }
        if(m->type != mod_alias)
        {
            continue;
        }

        glDeleteBuffersARB(1, &m->meshvbo);
        m->meshvbo = 0;

        glDeleteBuffersARB(1, &m->meshindexesvbo);
        m->meshindexesvbo = 0;
    }

    GL_ClearBufferBindings();
}

// from gl_model.c
extern char loadname[]; // for hunk tags
void Mod_CalcAliasBounds(aliashdr_t* a);


#define MD3_VERSION 15
// structures from Tenebrae
typedef struct
{
    int ident;
    int version;

    char name[64];

    int flags; // assumed to match quake1 models, for lack of somewhere better.

    int numFrames;
    int numTags;
    int numSurfaces;

    int numSkins;

    int ofsFrames;
    int ofsTags;
    int ofsSurfaces;
    int ofsEnd;
} md3Header_t;

// then has header->numFrames of these at header->ofs_Frames
typedef struct md3Frame_s
{
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    char name[16];
} md3Frame_t;

// there are header->numSurfaces of these at header->ofsSurfaces, following from
// ofsEnd
typedef struct
{
    int ident; //

    char name[64]; // polyset name

    int flags;
    int numFrames; // all surfaces in a model should have the same

    int numShaders; // all surfaces in a model should have the same
    int numVerts;

    int numTriangles;
    int ofsTriangles;

    int ofsShaders;    // offset from start of md3Surface_t
    int ofsSt;         // texture coords are common for all frames
    int ofsXyzNormals; // numVerts * numFrames

    int ofsEnd; // next surface follows
} md3Surface_t;

// at surf+surf->ofsXyzNormals
/*typedef struct {
    short		xyz[3];
    byte		latlong[2];
} md3XyzNormal_t;*/

// surf->numTriangles at surf+surf->ofsTriangles
typedef struct
{
    int indexes[3];
} md3Triangle_t;

// surf->numVerts at surf+surf->ofsSt
typedef struct
{
    float s;
    float t;
} md3St_t;

typedef struct
{
    char name[64];
    int shaderIndex;
} md3Shader_t;



void Mod_LoadMD3Model(qmodel_t* mod, void* buffer)
{
    md3Header_t* pinheader;
    md3Surface_t* pinsurface;
    md3Frame_t* pinframes;
    md3Triangle_t* pintriangle;
    unsigned short* poutindexes;
    md3XyzNormal_t* pinvert;
    md3XyzNormal_t* poutvert;
    md3St_t* pinst;
    aliasmesh_t* poutst;
    md3Shader_t* pinshader;
    int size;
    int start, end, total;
    int ival, j;
    int numsurfs, surf;
    int numframes;
    aliashdr_t* outhdr;

    start = Hunk_LowMark();

    pinheader = (md3Header_t*)buffer;

    ival = LittleLong(pinheader->version);
    if(ival != MD3_VERSION)
    {
        Sys_Error("%s has wrong version number (%i should be %i)", mod->name,
            ival, MD3_VERSION);
    }

    numsurfs = LittleLong(pinheader->numSurfaces);
    numframes = LittleLong(pinheader->numFrames);

    if(numframes > MAXALIASFRAMES)
    {
        Sys_Error("%s has too many frames (%i vs %i)", mod->name, numframes,
            MAXALIASFRAMES);
    }
    if(!numsurfs)
    {
        Sys_Error("%s has nosurfaces", mod->name);
    }

    pinframes = (md3Frame_t*)((byte*)buffer + LittleLong(pinheader->ofsFrames));
    //
    // allocate space for a working header, plus all the data except the frames,
    // skin and group info
    //
    size = sizeof(aliashdr_t) + (numframes - 1) * sizeof(outhdr->frames[0]);
    outhdr = (aliashdr_t*)Hunk_AllocName(size * numsurfs, loadname);

    for(surf = 0,
    pinsurface =
            (md3Surface_t*)((byte*)buffer + LittleLong(pinheader->ofsSurfaces));
        surf < numsurfs; surf++,
    pinsurface =
            (md3Surface_t*)((byte*)pinsurface + LittleLong(pinsurface->ofsEnd)))
    {
        aliashdr_t* osurf = (aliashdr_t*)((byte*)outhdr + size * surf);
        if(LittleLong(pinsurface->ident) !=
            (('I' << 0) | ('D' << 8) | ('P' << 16) | ('3' << 24)))
        {
            Sys_Error("%s corrupt surface ident", mod->name);
        }
        if(LittleLong(pinsurface->numFrames) != numframes)
        {
            Sys_Error("%s mismatched framecounts", mod->name);
        }

        if(surf + 1 < numsurfs)
        {
            osurf->nextsurface = size;
        }
        else
        {
            osurf->nextsurface = 0;
        }

        osurf->poseverttype = aliashdr_t::PV_QUAKE3;
        osurf->numverts_vbo = osurf->numverts =
            LittleLong(pinsurface->numVerts);
        pinvert = (md3XyzNormal_t*)((byte*)pinsurface +
                                    LittleLong(pinsurface->ofsXyzNormals));
        poutvert = (md3XyzNormal_t*)Hunk_Alloc(
            numframes * osurf->numverts * sizeof(*poutvert));
        osurf->vertexes = (byte*)poutvert - (byte*)osurf;
        for(ival = 0; ival < numframes; ival++)
        {
            osurf->frames[ival].firstpose = ival;
            osurf->frames[ival].numposes = 1;
            osurf->frames[ival].interval = 0.1;

            q_strlcpy(osurf->frames[ival].name, pinframes->name,
                sizeof(osurf->frames[ival].name));
            for(j = 0; j < 3; j++)
            { // fixme...
                osurf->frames[ival].bboxmin.v[j] = 0;
                osurf->frames[ival].bboxmax.v[j] = 255;
            }

            for(j = 0; j < osurf->numverts; j++)
            {
                poutvert[j] = pinvert[j];
            }
            poutvert += osurf->numverts;
            pinvert += osurf->numverts;
        }
        osurf->nummorphposes = osurf->numframes = numframes;

        osurf->numtris = LittleLong(pinsurface->numTriangles);
        osurf->numindexes = osurf->numtris * 3;
        pintriangle = (md3Triangle_t*)((byte*)pinsurface +
                                       LittleLong(pinsurface->ofsTriangles));
        poutindexes = (unsigned short*)Hunk_Alloc(
            sizeof(*poutindexes) * osurf->numindexes);
        osurf->indexes = (intptr_t)poutindexes - (intptr_t)osurf;
        for(ival = 0; ival < osurf->numtris;
            ival++, pintriangle++, poutindexes += 3)
        {
            for(j = 0; j < 3; j++)
            {
                poutindexes[j] = LittleLong(pintriangle->indexes[j]);
            }
        }

        for(j = 0; j < 3; j++)
        {
            osurf->scale_origin[j] = 0;
            osurf->scale[j] = 1 / 64.0;
        }

        // guess at skin sizes
        osurf->skinwidth = 320;
        osurf->skinheight = 200;

        // load the textures
        if(!isDedicated)
        {
            pinshader = (md3Shader_t*)((byte*)pinsurface +
                                       LittleLong(pinsurface->ofsShaders));
            osurf->numskins = LittleLong(pinsurface->numShaders);
            for(j = 0; j < osurf->numskins; j++, pinshader++)
            {
                char texturename[MAX_QPATH];
                char fullbrightname[MAX_QPATH];
                char* ext;
                // texture names in md3s are kinda fucked. they could be just
                // names relative to the mdl, or full paths, or just simple
                // shader names. our texture manager is too lame to scan all
                // 1000 possibilities
                if(strchr(pinshader->name, '/') ||
                    strchr(pinshader->name, '\\'))
                { // so if there's a path then we want to use that.
                    q_strlcpy(
                        texturename, pinshader->name, sizeof(texturename));
                }
                else
                { // and if there's no path then we want to prefix it with our
                  // own.
                    q_strlcpy(texturename, mod->name, sizeof(texturename));
                    *(char*)COM_SkipPath(texturename) = 0;
                    // and concat the specified name
                    q_strlcat(
                        texturename, pinshader->name, sizeof(texturename));
                }
                // and make sure there's no extensions. these get ignored in q3,
                // which is kinda annoying, but this is an md3 and standards are
                // standards (and it makes luma easier).
                ext = (char*)COM_FileGetExtension(texturename);
                if(*ext)
                {
                    *--ext = 0;
                }
                // luma has an extra postfix.
                q_snprintf(fullbrightname, sizeof(fullbrightname), "%s_luma",
                    texturename);
                osurf->gltextures[j][0] = TexMgr_LoadImage(mod, texturename,
                    osurf->skinwidth, osurf->skinheight, SRC_EXTERNAL, nullptr,
                    texturename, 0,
                    TEXPREF_PAD | TEXPREF_ALPHA | TEXPREF_NOBRIGHT |
                        TEXPREF_MIPMAP);
                osurf->fbtextures[j][0] = TexMgr_LoadImage(mod, fullbrightname,
                    osurf->skinwidth, osurf->skinheight, SRC_EXTERNAL, nullptr,
                    texturename, 0,
                    TEXPREF_PAD | TEXPREF_ALPHA | TEXPREF_FULLBRIGHT |
                        TEXPREF_MIPMAP);
                osurf->gltextures[j][3] = osurf->gltextures[j][2] =
                    osurf->gltextures[j][1] = osurf->gltextures[j][0];
                osurf->fbtextures[j][3] = osurf->fbtextures[j][2] =
                    osurf->fbtextures[j][1] = osurf->fbtextures[j][0];
            }
            if(osurf->numskins)
            {
                osurf->skinwidth = osurf->gltextures[0][0]->source_width;
                osurf->skinheight = osurf->gltextures[0][0]->source_height;
            }
        }

        // and figure out the texture coords properly, now we know the actual
        // sizes.
        pinst = (md3St_t*)((byte*)pinsurface + LittleLong(pinsurface->ofsSt));
        poutst = (aliasmesh_t*)Hunk_Alloc(sizeof(*poutst) * osurf->numverts);
        osurf->meshdesc = (intptr_t)poutst - (intptr_t)osurf;
        for(j = 0; j < osurf->numverts; j++)
        {
            poutst[j].vertindex = j; // how is this useful?
            poutst[j].st[0] = pinst->s;
            poutst[j].st[1] = pinst->t;
        }
    }
    GLMesh_LoadVertexBuffer(mod, outhdr);

    // small violation of the spec, but it seems like noone else uses it.
    mod->flags = LittleLong(pinheader->flags);


    mod->type = mod_alias;

    Mod_CalcAliasBounds(outhdr); // johnfitz

    //
    // move the complete, relocatable alias model to the cache
    //
    end = Hunk_LowMark();
    total = end - start;

    Cache_Alloc(&mod->cache, total, loadname);
    if(!mod->cache.data)
    {
        return;
    }
    memcpy(mod->cache.data, outhdr, total);

    Hunk_FreeToLowMark(start);
}

/*
=================================================================
InterQuake Models.
=================================================================

Header:
*/
// Copyright (c) 2010-2019 Lee Salzman
// MIT License etc at: https://github.com/lsalzman/iqm
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

struct iqmheader
{
    char magic[16];
    unsigned int version;
    unsigned int filesize;
    unsigned int flags;
    unsigned int num_text, ofs_text;     // text strings
    unsigned int num_meshes, ofs_meshes; // surface info
    unsigned int num_vertexarrays, num_vertexes,
        ofs_vertexarrays; // for loading vertex data
    unsigned int num_triangles, ofs_triangles,
        ofs_adjacency; // the index data+neighbours(which we ignore)
    unsigned int num_joints, ofs_joints; // mesh joints (base pose info)
    unsigned int num_poses,
        ofs_poses; // animated joints (num_poses should match num_joints)
    unsigned int num_anims, ofs_anims; // animations info
    unsigned int num_frames, num_framechannels, ofs_frames,
        ofs_bounds; // the actual per-pose(aka:single-frame) data
    unsigned int num_comment, ofs_comment;       // extra stuff
    unsigned int num_extensions, ofs_extensions; // extra stuff
};

struct iqmmesh
{
    unsigned int name;
    unsigned int material;
    unsigned int first_vertex, num_vertexes;
    unsigned int first_triangle, num_triangles;
};

enum
{
    IQM_POSITION = 0,
    IQM_TEXCOORD = 1,
    IQM_NORMAL = 2,
    IQM_TANGENT = 3,
    IQM_BLENDINDEXES = 4,
    IQM_BLENDWEIGHTS = 5,
    IQM_COLOR = 6,
    IQM_CUSTOM = 0x10
};

enum
{
    IQM_BYTE = 0,
    IQM_UBYTE = 1,
    IQM_SHORT = 2,
    IQM_USHORT = 3,
    IQM_INT = 4,
    IQM_UINT = 5,
    IQM_HALF = 6,
    IQM_FLOAT = 7,
    IQM_DOUBLE = 8
};

/*struct iqmtriangle
{
    unsigned int vertex[3];
};

struct iqmadjacency
{
    unsigned int triangle[3];
};

struct iqmjointv1
{
    unsigned int name;
    int parent;
    float translate[3], rotate[3], scale[3];
};*/

struct iqmjoint
{
    unsigned int name;
    int parent;
    float translate[3], rotate[4], scale[3];
};

/*struct iqmposev1
{
    int parent;
    unsigned int mask;
    float channeloffset[9];
    float channelscale[9];
};*/

struct iqmpose
{
    int parent;
    unsigned int mask;
    float channeloffset[10];
    float channelscale[10];
};

struct iqmanim
{
    unsigned int name;
    unsigned int first_frame, num_frames;
    float framerate;
    unsigned int flags;
};

enum
{
    IQM_LOOP = 1 << 0
};

struct iqmvertexarray
{
    unsigned int type;
    unsigned int flags;
    unsigned int format;
    unsigned int size;
    unsigned int offset;
};

/*struct iqmbounds
{
    float bbmin[3], bbmax[3];
    float xyradius, radius;
};

struct iqmextension
{
    unsigned int name;
    unsigned int num_data, ofs_data;
    unsigned int ofs_extensions; // pointer to next extension
};*/

// IQM Implementation: Copyright 2019 spike, licensed like the rest of
// quakespasm.
static void IQM_LoadVertexes_Float(float* o, size_t c, size_t numverts,
    const byte* buffer, const struct iqmvertexarray* va)
{
    size_t j, k;
    if(c != va->size)
    {
        return; // erk, too lazy to handle weirdness.
    }
    switch(va->format)
    {
            //	case IQM_BYTE:
        case IQM_UBYTE:
        { // weights+colours are often normalised bytes.
            const byte* in = (const byte*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k] / 255.0;
                }
            }
        }
        break;
            //	case IQM_SHORT:
            //	case IQM_USHORT:
            //	case IQM_INT:
            //	case IQM_UINT:
            //	case IQM_HALF:
        case IQM_FLOAT:
        {
            const float* in = (const float*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k];
                }
            }
        }
        break;
        case IQM_DOUBLE:
        { // truncate, sorry...
            const double* in = (const double*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k];
                }
            }
        }
        break;
        default: return; // oh bum. my laziness strikes again.
    }
}

static void IQM_LoadVertexes_Index(byte* o, size_t c, size_t numverts,
    const byte* buffer, const struct iqmvertexarray* va)
{
    size_t j, k;
    if(c != va->size)
    {
        return; // erk, too lazy to handle weirdness.
    }
    switch(va->format)
    {
            //	case IQM_BYTE:
        case IQM_UBYTE:
        {
            const byte* in = (const byte*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k];
                }
            }
        }
        break;
            //	case IQM_SHORT:
        case IQM_USHORT:
        { // truncate...
            const unsigned short* in =
                (const unsigned short*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k];
                }
            }
        }
        break;
            //	case IQM_INT:
        case IQM_UINT:
        { // truncate... noesis likes writing these.
            const unsigned int* in = (const unsigned int*)(buffer + va->offset);
            for(j = 0; j < numverts;
                j++, in += va->size, o += sizeof(iqmvert_t) / sizeof(*o))
            {
                for(k = 0; k < c; k++)
                {
                    o[k] = in[k];
                }
            }
        }
        break;
            //	case IQM_HALF:
            //	case IQM_FLOAT:
            //	case IQM_DOUBLE:
        default: return; // oh bum. my laziness strikes again.
    }
}

static void GenMatrixPosQuat4Scale(
    const qvec3& pos, const qvec4& quat, const qvec3& scale, float result[12])
{
    float xx, xy, xz, xw, yy, yz, yw, zz, zw;
    float x2, y2, z2;
    float s;
    x2 = quat[0] + quat[0];
    y2 = quat[1] + quat[1];
    z2 = quat[2] + quat[2];

    xx = quat[0] * x2;
    xy = quat[0] * y2;
    xz = quat[0] * z2;
    yy = quat[1] * y2;
    yz = quat[1] * z2;
    zz = quat[2] * z2;
    xw = quat[3] * x2;
    yw = quat[3] * y2;
    zw = quat[3] * z2;

    s = scale[0];
    result[0 * 4 + 0] = s * (1.0f - (yy + zz));
    result[1 * 4 + 0] = s * (xy + zw);
    result[2 * 4 + 0] = s * (xz - yw);

    s = scale[1];
    result[0 * 4 + 1] = s * (xy - zw);
    result[1 * 4 + 1] = s * (1.0f - (xx + zz));
    result[2 * 4 + 1] = s * (yz + xw);

    s = scale[2];
    result[0 * 4 + 2] = s * (xz + yw);
    result[1 * 4 + 2] = s * (yz - xw);
    result[2 * 4 + 2] = s * (1.0f - (xx + yy));

    result[0 * 4 + 3] = pos[0];
    result[1 * 4 + 3] = pos[1];
    result[2 * 4 + 3] = pos[2];
}
static void Matrix3x4_Invert_Simple(const float* in1, float* out)
{
    // we only support uniform scaling, so assume the first row is enough
    // (note the lack of sqrt here, because we're trying to undo the scaling,
    // this means multiplying by the inverse scale twice - squaring it, which
    // makes the sqrt a waste of time)
#if 1
    double scale = 1.0 / (in1[0] * in1[0] + in1[1] * in1[1] + in1[2] * in1[2]);
#else
    double scale =
        3.0 / sqrt(in1->m[0][0] * in1->m[0][0] + in1->m[0][1] * in1->m[0][1] +
                   in1->m[0][2] * in1->m[0][2] + in1->m[1][0] * in1->m[1][0] +
                   in1->m[1][1] * in1->m[1][1] + in1->m[1][2] * in1->m[1][2] +
                   in1->m[2][0] * in1->m[2][0] + in1->m[2][1] * in1->m[2][1] +
                   in1->m[2][2] * in1->m[2][2]);
    scale *= scale;
#endif

    // invert the rotation by transposing and multiplying by the squared
    // recipricol of the input matrix scale as described above
    out[0] = in1[0] * scale;
    out[1] = in1[4] * scale;
    out[2] = in1[8] * scale;
    out[4] = in1[1] * scale;
    out[5] = in1[5] * scale;
    out[6] = in1[9] * scale;
    out[8] = in1[2] * scale;
    out[9] = in1[6] * scale;
    out[10] = in1[10] * scale;

    // invert the translate
    out[3] = -(in1[3] * out[0] + in1[7] * out[1] + in1[11] * out[2]);
    out[7] = -(in1[3] * out[4] + in1[7] * out[5] + in1[11] * out[6]);
    out[11] = -(in1[3] * out[8] + in1[7] * out[9] + in1[11] * out[10]);
}

void Mod_LoadIQMModel(qmodel_t* mod, const void* buffer)
{
    const struct iqmheader* pinheader;
    const char* pintext;
    const struct iqmmesh* pinsurface;
    const struct iqmanim* pinframes;
    const unsigned int* pintriangle;
    unsigned short* poutindexes;
    iqmvert_t* poutvert;
    int size;
    int start, end, total;
    int ival, j, a;
    int numsurfs, surf;
    aliashdr_t* outhdr;
    int numverts, firstidx, firstvert;
    int numanims;

    bonepose_t* outposes;
    boneinfo_t* outbones;
    int numposes, numjoints;

    start = Hunk_LowMark();

    pinheader = (const struct iqmheader*)buffer;


    if(strcmp(pinheader->magic, IQM_MAGIC))
    {
        Sys_Error("%s has invalid magic for iqm file", mod->name);
    }
    if(LittleLong(pinheader->version) != IQM_VERSION)
    { // v1 is outdated.
        Sys_Error("%s is an unsupported version, %i must be %i", mod->name,
            LittleLong(pinheader->version), IQM_VERSION);
    }

    pintext = (const char*)buffer + LittleLong(pinheader->ofs_text);

    numsurfs = LittleLong(pinheader->num_meshes);
    if(!numsurfs)
    {
        Sys_Error("%s has no surfaces (animation-only iqms are not supported)",
            mod->name);
    }
    if(pinheader->num_vertexes > 0xffff)
    { // indexes is an unsigned short.
        Sys_Error("%s has too many verts (%u>%u)", mod->name,
            pinheader->num_vertexes, 0xffffu);
    }

    numanims = LittleLong(pinheader->num_anims);
    size =
        sizeof(aliashdr_t) + q_max(1, numanims - 1) * sizeof(outhdr->frames[0]);
    outhdr = (aliashdr_t*)Hunk_AllocName(size * numsurfs, loadname);

    numverts = LittleLong(pinheader->num_vertexes);
    poutvert = (iqmvert_t*)Hunk_Alloc(sizeof(*poutvert) * numverts);
    for(j = 0; j < numverts; j++)
    { // initialise verts, just in case.
        poutvert[j].rgba[0] = poutvert[j].rgba[1] = poutvert[j].rgba[2] =
            poutvert[j].rgba[3] = poutvert[j].weight[0] = 1;
    }
    for(a = 0; a < LittleLong(pinheader->num_vertexarrays); a++)
    {
        const struct iqmvertexarray* va =
            (const struct iqmvertexarray*)((const byte*)buffer +
                                           LittleLong(
                                               pinheader->ofs_vertexarrays)) +
            a;
        switch(va->type)
        {
            case IQM_POSITION:
                IQM_LoadVertexes_Float(
                    poutvert->xyz, 3, numverts, (byte*)buffer, va);
                break;
            case IQM_TEXCOORD:
                IQM_LoadVertexes_Float(
                    poutvert->st, 2, numverts, (byte*)buffer, va);
                break;
            case IQM_NORMAL:
                IQM_LoadVertexes_Float(
                    poutvert->norm, 3, numverts, (byte*)buffer, va);
                break;
            // case IQM_TANGENT:		IQM_LoadVertexes_Float(poutvert->tang,
            // 4, numverts, buffer, va); break; //bitangent must be calced using
            // a crossproduct and the fourth component (for direction). we don't
            // need this (unless you want rtlights or bumpmaps)
            case IQM_COLOR:
                IQM_LoadVertexes_Float(
                    poutvert->rgba, 4, numverts, (byte*)buffer, va);
                break;
            case IQM_BLENDINDEXES:
                IQM_LoadVertexes_Index(
                    poutvert->idx, 4, numverts, (byte*)buffer, va);
                break;
            case IQM_BLENDWEIGHTS:
                IQM_LoadVertexes_Float(
                    poutvert->weight, 4, numverts, (byte*)buffer, va);
                break;
            default: continue; // no idea what it is. probably custom
        }
    }

    numposes = LittleLong(pinheader->num_frames);
    numjoints = LittleLong(pinheader->num_poses);
    if(pinheader->num_poses == pinheader->num_joints)
    {
        const unsigned short* pinframedata =
            (const unsigned short*)((const byte*)buffer +
                                    pinheader->ofs_frames);
        const struct iqmpose *pinajoint =
                                 (const struct iqmpose*)((const byte*)buffer +
                                                         pinheader->ofs_poses),
                             *p;
        qvec3 pos, scale;
        qvec4 quat;
        outposes =
            (bonepose_t*)Hunk_Alloc(sizeof(*outposes) * numposes * numjoints);
        for(a = 0; a < numposes; a++)
        {
            for(j = 0, p = pinajoint; j < numjoints; j++, p++)
            {
                unsigned int mask = LittleLong(p->mask);
                pos[0] = LittleFloat(p->channeloffset[0]);
                if(mask & 1)
                {
                    pos[0] += (unsigned short)LittleShort(*pinframedata++) *
                              LittleFloat(p->channelscale[0]);
                }
                pos[1] = LittleFloat(p->channeloffset[1]);
                if(mask & 2)
                {
                    pos[1] += (unsigned short)LittleShort(*pinframedata++) *
                              LittleFloat(p->channelscale[1]);
                }
                pos[2] = LittleFloat(p->channeloffset[2]);
                if(mask & 4)
                {
                    pos[2] += (unsigned short)LittleShort(*pinframedata++) *
                              LittleFloat(p->channelscale[2]);
                }
                quat[0] = LittleFloat(p->channeloffset[3]);
                if(mask & 8)
                {
                    quat[0] += (unsigned short)LittleShort(*pinframedata++) *
                               LittleFloat(p->channelscale[3]);
                }
                quat[1] = LittleFloat(p->channeloffset[4]);
                if(mask & 16)
                {
                    quat[1] += (unsigned short)LittleShort(*pinframedata++) *
                               LittleFloat(p->channelscale[4]);
                }
                quat[2] = LittleFloat(p->channeloffset[5]);
                if(mask & 32)
                {
                    quat[2] += (unsigned short)LittleShort(*pinframedata++) *
                               LittleFloat(p->channelscale[5]);
                }
                quat[3] = LittleFloat(p->channeloffset[6]);
                if(mask & 64)
                {
                    quat[3] += (unsigned short)LittleShort(*pinframedata++) *
                               LittleFloat(p->channelscale[6]);
                }
                scale[0] = LittleFloat(p->channeloffset[7]);
                if(mask & 128)
                {
                    scale[0] += (unsigned short)LittleShort(*pinframedata++) *
                                LittleFloat(p->channelscale[7]);
                }
                scale[1] = LittleFloat(p->channeloffset[8]);
                if(mask & 256)
                {
                    scale[1] += (unsigned short)LittleShort(*pinframedata++) *
                                LittleFloat(p->channelscale[8]);
                }
                scale[2] = LittleFloat(p->channeloffset[9]);
                if(mask & 512)
                {
                    scale[2] += (unsigned short)LittleShort(*pinframedata++) *
                                LittleFloat(p->channelscale[9]);
                }

                // fixme: should probably save the 10 values above and slerp,
                // but its simpler to just save+lerp a matrix (although this
                // does result in denormalisation when interpolating).
                GenMatrixPosQuat4Scale(
                    pos, quat, scale, outposes[(a * numjoints + j)].mat);
            }
        }
    }
    else
    { // panic! panic! something weird is going on!
        numposes = 0;
        numjoints = 0;
        outposes = nullptr;
    }

    {
        const struct iqmjoint* pinbjoint =
            (const struct iqmjoint*)((const byte*)buffer +
                                     pinheader->ofs_joints);
        bonepose_t basepose[256], rel;
        qvec3 pos, scale;
        qvec4 quat;
        outbones = (boneinfo_t*)Hunk_Alloc(sizeof(*outbones) * numjoints);
        for(j = 0; j < numjoints; j++)
        {
            outbones[j].parent = LittleLong(pinbjoint[j].parent);
            q_strlcpy(outbones[j].name, pintext + LittleLong(pinbjoint[j].name),
                sizeof(outbones[j].name));

            pos[0] = LittleFloat(pinbjoint[j].translate[0]);
            pos[1] = LittleFloat(pinbjoint[j].translate[1]);
            pos[2] = LittleFloat(pinbjoint[j].translate[2]);
            quat[0] = LittleFloat(pinbjoint[j].rotate[0]);
            quat[1] = LittleFloat(pinbjoint[j].rotate[1]);
            quat[2] = LittleFloat(pinbjoint[j].rotate[2]);
            quat[3] = LittleFloat(pinbjoint[j].rotate[3]);
            scale[0] = LittleFloat(pinbjoint[j].scale[0]);
            scale[1] = LittleFloat(pinbjoint[j].scale[1]);
            scale[2] = LittleFloat(pinbjoint[j].scale[2]);
            GenMatrixPosQuat4Scale(pos, quat, scale, rel.mat);
            // urgh, these are relative.
            if(outbones[j].parent < 0)
            {
                memcpy(basepose[j].mat, rel.mat, sizeof(rel.mat));
            }
            else
            {
                R_ConcatTransforms(
                    (float(*)[4])basepose[outbones[j].parent].mat,
                    (float(*)[4])rel.mat, (float(*)[4])basepose[j].mat);
            }

            Matrix3x4_Invert_Simple(basepose[j].mat, outbones[j].inverse.mat);
            // and now we have the inversion matrix to use to undo the bone
            // positions baked into the vertex data.
        }
    }

    mod->numframes = q_max(1, numanims);

    for(surf = 0,
    pinsurface = (const struct iqmmesh*)((const byte*)buffer +
                                         LittleLong(pinheader->ofs_meshes));
        surf < numsurfs; surf++, pinsurface++)
    {
        aliashdr_t* osurf = (aliashdr_t*)((byte*)outhdr + size * surf);

        if(surf + 1 < numsurfs)
        {
            osurf->nextsurface = size;
        }
        else
        {
            osurf->nextsurface = 0;
        }

        osurf->poseverttype = aliashdr_t::PV_IQM;
        osurf->numverts_vbo = osurf->numverts =
            LittleLong(pinsurface->num_vertexes);

        firstvert = LittleLong(pinsurface->first_vertex);
        osurf->vertexes = (intptr_t)(poutvert + firstvert) - (intptr_t)osurf;
        osurf->numverts = LittleLong(pinsurface->num_vertexes);
        osurf->nummorphposes =
            1; // as a skeletal model, we do all our animations via bones rather
               // than vertex morphs.

        osurf->numtris = LittleLong(pinsurface->num_triangles);
        osurf->numindexes = osurf->numtris * 3;
        poutindexes = (unsigned short*)Hunk_Alloc(
            sizeof(*poutindexes) * osurf->numindexes);
        osurf->indexes = (intptr_t)poutindexes - (intptr_t)osurf;
        pintriangle =
            (const unsigned int*)((const byte*)buffer +
                                  LittleLong(pinheader->ofs_triangles));
        firstidx = LittleLong(pinsurface->first_triangle) * 3;
        pintriangle += firstidx;
        for(j = 0; j < osurf->numindexes; j++)
        {
            poutindexes[j] = pintriangle[j] - firstvert;
        }

        pinframes =
            (const struct iqmanim*)((const byte*)buffer + pinheader->ofs_anims);
        for(a = 0; a < numanims; a++, pinframes++)
        {
            osurf->frames[a].firstpose = LittleLong(pinframes->first_frame);
            osurf->frames[a].numposes = LittleLong(pinframes->num_frames);
            osurf->frames[a].interval = LittleFloat(pinframes->framerate);
            if(!osurf->frames[a].interval)
            {
                osurf->frames[a].interval = 20;
            }
            osurf->frames[a].interval = 1.0 / osurf->frames[a].interval;
            if(LittleLong(pinframes->flags) & IQM_LOOP)
            { /*FIXME*/
                ;
            }

            q_strlcpy(osurf->frames[a].name,
                pintext + LittleLong(pinframes->name),
                sizeof(osurf->frames[ival].name));
            for(j = 0; j < 3; j++)
            { // fixme...
                osurf->frames[a].bboxmin.v[j] = 0;
                osurf->frames[a].bboxmax.v[j] = 255;
            }
        }
        for(; a < 1; a++, pinframes++)
        { // unanimated models need to pick their morphpose without warnings.
            osurf->frames[a].firstpose = 0;
            osurf->frames[a].numposes = 1;
            osurf->frames[a].interval = 0.1;

            q_strlcpy(
                osurf->frames[a].name, "", sizeof(osurf->frames[ival].name));
            for(j = 0; j < 3; j++)
            { // fixme...
                osurf->frames[a].bboxmin.v[j] = 0;
                osurf->frames[a].bboxmax.v[j] = 255;
            }
        }
        osurf->numframes = a;
        if(numposes)
        {
            osurf->numboneposes = numposes;
            osurf->boneposedata = (intptr_t)outposes - (intptr_t)osurf;
        }
        osurf->numbones = numjoints;
        osurf->boneinfo = (intptr_t)outbones - (intptr_t)osurf;

        for(j = 0; j < 3; j++)
        {
            osurf->scale_origin[j] = 0;
            osurf->scale[j] = 1.0;
        }

        // skin size is irrelevant
        osurf->skinwidth = 1;
        osurf->skinheight = 1;

        // load the textures
        if(!isDedicated)
        {
            const char* pinshader = pintext + LittleLong(pinsurface->material);
            osurf->numskins = 1;
            for(j = 0; j < 1; j++, pinshader++)
            {
                char texturename[MAX_QPATH];
                char fullbrightname[MAX_QPATH];
                char* ext;
                // texture names in md3s are kinda fucked. they could be just
                // names relative to the mdl, or full paths, or just simple
                // shader names. our texture manager is too lame to scan all
                // 1000 possibilities
                if(strchr(pinshader, '/') || strchr(pinshader, '\\'))
                { // so if there's a path then we want to use that.
                    q_strlcpy(texturename, pinshader, sizeof(texturename));
                }
                else
                { // and if there's no path then we want to prefix it with our
                  // own.
                    q_strlcpy(texturename, mod->name, sizeof(texturename));
                    *(char*)COM_SkipPath(texturename) = 0;
                    // and concat the specified name
                    q_strlcat(texturename, pinshader, sizeof(texturename));
                }
                // and make sure there's no extensions. these get ignored in q3,
                // which is kinda annoying, but this is an md3 and standards are
                // standards (and it makes luma easier).
                ext = (char*)COM_FileGetExtension(texturename);
                if(*ext)
                {
                    *--ext = 0;
                }
                // luma has an extra postfix.
                q_snprintf(fullbrightname, sizeof(fullbrightname), "%s_luma",
                    texturename);
                osurf->gltextures[j][0] = TexMgr_LoadImage(mod, texturename,
                    osurf->skinwidth, osurf->skinheight, SRC_EXTERNAL, nullptr,
                    texturename, 0,
                    TEXPREF_PAD | TEXPREF_ALPHA | TEXPREF_NOBRIGHT |
                        TEXPREF_MIPMAP);
                osurf->fbtextures[j][0] =
                    nullptr; // TexMgr_LoadImage(mod, fullbrightname,
                             // osurf->skinwidth, osurf->skinheight,
                             // SRC_EXTERNAL, nullptr, fullbrightname, 0,
                             // TEXPREF_PAD|TEXPREF_ALPHA|TEXPREF_FULLBRIGHT|TEXPREF_MIPMAP);
                osurf->gltextures[j][3] = osurf->gltextures[j][2] =
                    osurf->gltextures[j][1] = osurf->gltextures[j][0];
                osurf->fbtextures[j][3] = osurf->fbtextures[j][2] =
                    osurf->fbtextures[j][1] = osurf->fbtextures[j][0];
            }
            if(osurf->numskins)
            {
                osurf->skinwidth = osurf->gltextures[0][0]->source_width;
                osurf->skinheight = osurf->gltextures[0][0]->source_height;
            }
        }
    }
    GLMesh_LoadVertexBuffer(mod, outhdr);

    // small violation of the spec, but it seems like noone else uses it.
    mod->flags = LittleLong(pinheader->flags);


    mod->type = mod_alias;

    Mod_CalcAliasBounds(outhdr); // johnfitz

    //
    // move the complete, relocatable alias model to the cache
    //
    end = Hunk_LowMark();
    total = end - start;

    Cache_Alloc(&mod->cache, total, loadname);
    if(!mod->cache.data)
    {
        return;
    }
    memcpy(mod->cache.data, outhdr, total);

    Hunk_FreeToLowMark(start);
}
