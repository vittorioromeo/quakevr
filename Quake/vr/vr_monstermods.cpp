// vr_monstermods.cpp -- changes to monster models as they load.
//
// The knights drop their swords when they die (QC: a usable weapon, v_ksword.mdl / v_hksword.mdl,
// made by Misc/quakevr/make_swords.py): their death frames must not show the sword any more. The
// sword's vertices of every "death*" frame are collapsed to a point (its triangles vanish). The
// sword is found as make_swords.py finds it: in Quake VR's own models by their known vertices; in
// id's, the knight's by the blade's strips on the skin and the hell knight's as the longest separate
// piece of its mesh (a mod's models laid out the same way work too; others are left alone).

#include "vr_engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

namespace
{

[[nodiscard]] int uvS(const stvert_t* st, const dtriangle_t& t, int v, int skinWidth)
{
    const int s = st[v].s;
    return st[v].onseam && !t.facesfront ? s + skinWidth / 2 : s;
}

// The knight's sword: triangles on the blade's strips (the left of each half of the skin).
std::vector<int> knightSword(const aliashdr_t* hdr, const stvert_t* st, const dtriangle_t* tris)
{
    std::vector<int> out;
    const int half = hdr->skinwidth / 2;
    for(int i = 0; i < hdr->numtris; i++)
    {
        bool all = true;
        for(int k = 0; k < 3 && all; k++)
        {
            const int v = tris[i].vertindex[k];
            const int s = uvS(st, tris[i], v, hdr->skinwidth);
            all = (s < 22 || (s >= half - 2 && s < half + 22)) && st[v].t < 120;
        }
        if(all)
        {
            out.push_back(i);
        }
    }
    return out;
}

// The hell knight's sword: the longest separate piece of the mesh, apart from the body.
std::vector<int> separatePieceSword(const aliashdr_t* hdr, const dtriangle_t* tris, const trivertx_t* pose)
{
    std::vector<int> parent(hdr->numverts);
    std::iota(parent.begin(), parent.end(), 0);
    const auto find = [&](int a) {
        while(parent[a] != a)
        {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
    };
    for(int i = 0; i < hdr->numtris; i++)
    {
        parent[find(tris[i].vertindex[0])] = find(tris[i].vertindex[1]);
        parent[find(tris[i].vertindex[1])] = find(tris[i].vertindex[2]);
    }

    std::vector<int> count(hdr->numverts, 0);
    for(int v = 0; v < hdr->numverts; v++)
    {
        count[find(v)]++;
    }
    const int body = static_cast<int>(std::max_element(count.begin(), count.end()) - count.begin());

    // Each piece's length: its bounding box's diagonal.
    std::vector<float> lo(hdr->numverts * 3, 1e9f), hi(hdr->numverts * 3, -1e9f);
    for(int v = 0; v < hdr->numverts; v++)
    {
        const int r = find(v);
        for(int k = 0; k < 3; k++)
        {
            lo[r * 3 + k] = std::min(lo[r * 3 + k], static_cast<float>(pose[v].v[k]) * hdr->scale[k]);
            hi[r * 3 + k] = std::max(hi[r * 3 + k], static_cast<float>(pose[v].v[k]) * hdr->scale[k]);
        }
    }
    int best = -1;
    float bestLength = 0.f;
    for(int r = 0; r < hdr->numverts; r++)
    {
        if(r == body || count[r] < 4 || find(r) != r)
        {
            continue;
        }
        const float dx = hi[r * 3] - lo[r * 3], dy = hi[r * 3 + 1] - lo[r * 3 + 1], dz = hi[r * 3 + 2] - lo[r * 3 + 2];
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        if(length > bestLength)
        {
            best = r;
            bestLength = length;
        }
    }

    std::vector<int> out;
    for(int i = 0; i < hdr->numtris && best >= 0; i++)
    {
        if(find(tris[i].vertindex[0]) == best)
        {
            out.push_back(i);
        }
    }
    return out;
}

// Quake VR's own knight models (quakevr/progs, higher detail than id's): their swords' vertices,
// found by hand (Misc/quakevr/make_swords.py uses the same lists). The knight's frames are numbered,
// not named: its death frames are id's (the last 21, death1 .. deathb11).
struct KnownSword
{
    const char* model;
    int numverts, numtris;
    std::vector<int> verts;
    int firstDeath, lastDeath; // frame indices, or -1: by name
};

const KnownSword knownSwords[] = {
    {"progs/knight.mdl", 655, 697,
        {334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559, 560, 561, 562, 563, 564, 565,
            566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602,
            603, 604, 607, 608, 609, 612, 613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646},
        76, 96},
    {"progs/hknight.mdl", 538, 1000, {43, 45, 46, 47, 48, 526, 527, 531, 532}, -1, -1},
};

} // namespace

// Mod_LoadAliasModel, after the frames are read (poses writable, before the bounds and the vertex
// buffer are made).
extern "C" void VR_AliasPosesLoaded(const char* name, void* aliashdr, const stvert_t* stverts, const dtriangle_t* tris,
    trivertx_t** poses)
{
    aliashdr_t* hdr = static_cast<aliashdr_t*>(aliashdr);
    const bool knight = !strcmp(name, "progs/knight.mdl");
    const bool hellKnight = !strcmp(name, "progs/hknight.mdl");
    if(!knight && !hellKnight)
    {
        return;
    }

    const KnownSword* known = nullptr;
    for(const KnownSword& k : knownSwords)
    {
        if(!strcmp(name, k.model) && hdr->numverts == k.numverts && hdr->numtris == k.numtris)
        {
            known = &k;
        }
    }

    std::vector<int> sword;
    if(known)
    {
        std::vector<char> in(hdr->numverts, 0);
        for(int v : known->verts)
        {
            in[v] = 1;
        }
        for(int i = 0; i < hdr->numtris; i++)
        {
            if(in[tris[i].vertindex[0]] && in[tris[i].vertindex[1]] && in[tris[i].vertindex[2]])
            {
                sword.push_back(i);
            }
        }
    }
    else
    {
        sword = knight ? knightSword(hdr, stverts, tris) : separatePieceSword(hdr, tris, poses[0]);
    }
    if(sword.empty())
    {
        return;
    }

    // The sword's own vertices (not the hand's, which the knight's hilt shares).
    std::vector<char> inSword(hdr->numverts, 0), inRest(hdr->numverts, 0);
    std::vector<char> isSwordTri(hdr->numtris, 0);
    for(int i : sword)
    {
        isSwordTri[i] = 1;
    }
    for(int i = 0; i < hdr->numtris; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            (isSwordTri[i] ? inSword : inRest)[tris[i].vertindex[k]] = 1;
        }
    }
    std::vector<int> own, shared;
    for(int v = 0; v < hdr->numverts; v++)
    {
        if(inSword[v])
        {
            (inRest[v] ? shared : own).push_back(v);
        }
    }

    // Onto the hilt (the hand) if it shares it, else the sword's middle.
    const std::vector<int>& anchor = shared.empty() ? own : shared;
    int frames = 0;
    for(int f = 0; f < hdr->numframes; f++)
    {
        const maliasframedesc_t& fd = hdr->frames[f];
        const bool death = known && known->firstDeath >= 0 ? f >= known->firstDeath && f <= known->lastDeath
                                                           : strncmp(fd.name, "death", 5) == 0;
        if(!death)
        {
            continue;
        }
        frames++;
        for(int p = fd.firstpose; p < fd.firstpose + fd.numposes; p++)
        {
            trivertx_t* pose = poses[p];
            int sum[3]{};
            for(int v : anchor)
            {
                for(int k = 0; k < 3; k++)
                {
                    sum[k] += pose[v].v[k];
                }
            }
            for(int v : own)
            {
                for(int k = 0; k < 3; k++)
                {
                    pose[v].v[k] = static_cast<byte>(sum[k] / static_cast<int>(anchor.size()));
                }
            }
        }
    }
    Con_DPrintf("VR: %s: the sword (%d vertices) hidden in %d death frames\n", name, static_cast<int>(own.size()), frames);
}
