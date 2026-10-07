// vr_monstermods.cpp -- changes to monster models as they load.
//
// The knights drop their swords when they die (QC: a usable weapon, v_ksword.mdl / v_hksword.mdl,
// made by Misc/quakevr/make_swords.py): their death frames must not show the sword any more. The
// sword's vertices of every "death*" frame are collapsed to a point (its triangles vanish). The
// sword is found as make_swords.py finds it: in Quake VR's own models by their known vertices; in
// id's, the knight's by the blade's strips on the skin and the hell knight's as the longest separate
// piece of its mesh (a mod's models laid out the same way work too; others are left alone).
//
// The ogres drop their chainsaws likewise (QC VR_DropOgreChainsaw: v_chainsaw.mdl, made by
// Misc/quakevr/make_chainsaw.py): Quake VR's ogre's chainsaw is hidden in its death frames ("death*" and
// "bdeath*") by its known vertices; id's ogre (or another) is left alone; Dawn of the Machine's rocket ogre's too (MG3's
// model, its chainsaw in his right hand). So are the grunts' shotguns and the
// enforcers' laser rifles (QC vr_enemyguns.qc: v_gruntgun.mdl and v_enfrifle.mdl, made by
// Misc/quakevr/make_enemyguns.py): Quake VR's soldier's and enforcer's guns, by their known vertices.
//
// Dawn of the Machine's Super Axe loses the first-person arm its model holds it with (superAxePoses).

#include "vr_modelmetadata.hpp"
#include "vr_engine.hpp"

#include "Zancle/Algorithm/Iota.hpp"
#include "Zancle/Algorithm/MaxElement.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>

namespace
{

[[nodiscard]] int uvS(const stvert_t* st, const dtriangle_t& t, int v, int skinWidth)
{
    const int s = st[v].s;
    return st[v].onseam && !t.facesfront ? s + skinWidth / 2 : s;
}

// The knight's sword: triangles on the blade's strips (the left of each half of the skin).
za::Vector<int> knightSword(const aliashdr_t* hdr, const stvert_t* st, const dtriangle_t* tris)
{
    za::Vector<int> out;
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
            out.pushBack(i);
        }
    }
    return out;
}

// The hell knight's sword: the longest separate piece of the mesh, apart from the body.
za::Vector<int> separatePieceSword(const aliashdr_t* hdr, const dtriangle_t* tris, const trivertx_t* pose)
{
    za::Vector<int> parent(hdr->numverts);
    za::iota(parent.begin(), parent.end(), 0);
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

    za::Vector<int> count(hdr->numverts, 0);
    for(int v = 0; v < hdr->numverts; v++)
    {
        count[find(v)]++;
    }
    const int body = static_cast<int>(za::maxElement(count.begin(), count.end()) - count.begin());

    // Each piece's length: its bounding box's diagonal.
    za::Vector<float> lo(hdr->numverts * 3, 1e9f), hi(hdr->numverts * 3, -1e9f);
    for(int v = 0; v < hdr->numverts; v++)
    {
        const int r = find(v);
        for(int k = 0; k < 3; k++)
        {
            lo[r * 3 + k] = za::min(lo[r * 3 + k], static_cast<float>(pose[v].v[k]) * hdr->scale[k]);
            hi[r * 3 + k] = za::max(hi[r * 3 + k], static_cast<float>(pose[v].v[k]) * hdr->scale[k]);
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
        const float length = za::sqrt(dx * dx + dy * dy + dz * dz);
        if(length > bestLength)
        {
            best = r;
            bestLength = length;
        }
    }

    za::Vector<int> out;
    for(int i = 0; i < hdr->numtris && best >= 0; i++)
    {
        if(find(tris[i].vertindex[0]) == best)
        {
            out.pushBack(i);
        }
    }
    return out;
}

// Quake VR's own knight models (quakevr/progs, higher detail than id's): their swords' vertices,
// found by hand (Misc/quakevr/make_swords.py uses the same lists; the hell knight's here also has the blade's guard, a
// piece his hand holds: hidden too, so his corpse and ragdoll hold nothing). The knight's frames are numbered,
// not named: its death frames are id's (the last 21, death1 .. deathb11). And Quake VR's ogre's chainsaw
// (Misc/quakevr/make_chainsaw.py: vertices 416..496, a separate piece), its soldier's shotgun and its enforcer's laser
// rifle (Misc/quakevr/make_enemyguns.py: separate pieces). The soldier's frames are not named either: its death frames
// are id's 8..28 (death1 .. deathc11).
struct KnownSword
{
    qvr::modelmeta::Id model;
    int numverts, numtris;
    za::Vector<int> verts;
    int firstDeath, lastDeath; // frame indices, or -1: by name
};

const KnownSword knownSwords[] = {
    {qvr::modelmeta::Id::Knight, 655, 697,
        {334, 335, 336, 363, 364, 365, 524, 535, 536, 537, 538, 539, 540, 557, 558, 559, 560, 561, 562, 563, 564, 565,
            566, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591, 592, 593, 594, 595, 596, 597, 598, 599, 600, 601, 602,
            603, 604, 607, 608, 609, 612, 613, 614, 615, 617, 626, 627, 628, 639, 640, 645, 646},
        76, 96},
    // (The hell knight's blade, and its guard: a piece of its own his hand holds, the dropped sword has its own hilt.)
    {qvr::modelmeta::Id::Hknight, 538, 1000, [] {
         za::Vector<int> v{43, 45, 46, 47, 48, 526, 527, 531, 532, 42, 44, 525, 528, 529, 530};
         for(int i = 49; i <= 58; i++)
         {
             v.pushBack(i);
         }
         for(int i = 332; i <= 365; i++)
         {
             v.pushBack(i);
         }
         return v;
     }(),
        -1, -1},
    {qvr::modelmeta::Id::Ogre, 497, 1290, [] {
         za::Vector<int> v(81);
         za::iota(v.begin(), v.end(), 416);
         return v;
     }(),
        -1, -1},
    {qvr::modelmeta::Id::Soldier, 555, 810, [] {
         za::Vector<int> v(86);
         za::iota(v.begin(), v.end(), 463);
         return v;
     }(),
        8, 28},
    {qvr::modelmeta::Id::Enforcer, 479, 984, [] {
         za::Vector<int> v{22, 23, 100};
         for(int i = 400; i <= 430; i++)
         {
             v.pushBack(i);
         }
         for(int i = 455; i <= 478; i++)
         {
             v.pushBack(i);
         }
         return v;
     }(),
        -1, -1},
    // Dawn of the Machine's rocket ogre (owned/mg3/progs/ogre_rocket.mdl, read in place): the chainsaw in his right hand
    // (a box and a long bar behind him, a piece of its own: 0..47, 649..652, 940..981), hidden in his deaths (frames
    // unnamed, id's ogre's order: death1-14 and bdeath1-10, 112..135); he drops one (QC ogre_die), his ragdoll holds none.
    {qvr::modelmeta::Id::Mg3OgreRocket, 982, 1367, [] {
         za::Vector<int> v(48);
         za::iota(v.begin(), v.end(), 0);
         for(int i = 649; i <= 652; i++)
         {
             v.pushBack(i);
         }
         for(int i = 940; i <= 981; i++)
         {
             v.pushBack(i);
         }
         return v;
     }(),
        112, 135},
    // Hipnotic's gremlin: the gun he steals (its own piece, tucked inside his body but in his g* frames), hidden in his
    // deaths (death1-12, flip1-8: he drops it, gremlin_die), so his ragdoll holds none (ROUND21.md, "Ragdolls 5").
    {qvr::modelmeta::Id::Grem, 123, 245, [] {
         za::Vector<int> v(38);
         za::iota(v.begin(), v.end(), 85);
         return v;
     }(),
        104, 123},
};

// What a monster's model drops (the log's name for it).
[[nodiscard]] const char* droppedName(const char* model)
{
    const auto id = qvr::modelmeta::identifyPath(model);
    if(id == qvr::modelmeta::Id::Ogre || id == qvr::modelmeta::Id::Mg3OgreRocket)
    {
        return "chainsaw";
    }
    if(id == qvr::modelmeta::Id::Soldier)
    {
        return "shotgun";
    }
    if(id == qvr::modelmeta::Id::Enforcer)
    {
        return "laser rifle";
    }
    if(id == qvr::modelmeta::Id::Grem)
    {
        return "stolen gun";
    }
    return "sword";
}

// Dawn of the Machine's Super Axe (owned/mg3/progs/v_hammer.mdl and its glowing twin, read from the owned MG3 pack in
// place: vr_gamedir.cpp VR_OwnedFile; QC vr_mg3_weapons.qc): a first-person model with the arm that holds it. Quake VR's
// hand holds it instead. The arm (every piece of the mesh but the axe, the largest; pieces joined where their vertices
// coincide: the seams' copies) is collapsed onto one point of the handle; every pose is the first (the swing's other
// frames move the axe across the view: the hand moves it here); and the axe is turned and moved to lie as Quake VR's axe
// lies (its handle's end and line on the axe's, its blade the same way; Misc/quakevr/fit_superaxe.py measures it), so the
// axe's weapon settings, moved by the new scale_origin, hold it (slot 24: vr_weapons.inc). Only the release measured
// (795 vertices, 1242 triangles); another is left as it is (held as drawn in the view, its arm too).
void superAxePoses(const char* name, aliashdr_t* hdr, const dtriangle_t* tris, trivertx_t** poses)
{
    constexpr int knownVerts = 795, knownTris = 1242;
    if(hdr->numverts != knownVerts || hdr->numtris != knownTris || hdr->numposes < 1)
    {
        Con_DPrintf("VR: %s: not the Super Axe measured (%d vertices, %d triangles): left as it is\n", name, hdr->numverts,
            hdr->numtris);
        return;
    }
    // fit_superaxe.py: the model's point p lies at R p + t (R a rotation, rows below).
    constexpr float rot[3][3] = {{0.82941f, 0.44756f, -0.33432f}, {-0.46879f, 0.8831f, 0.01921f}, {0.30383f, 0.14079f, 0.94226f}};
    constexpr float move[3] = {-18.621f, 20.0023f, 30.9663f};

    const int n = hdr->numverts;
    const trivertx_t* first = poses[0];
    za::Vector<int> parent(n);
    za::iota(parent.begin(), parent.end(), 0);
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
    for(int a = 0; a < n; a++)
    {
        for(int b = a + 1; b < n; b++)
        {
            if(first[a].v[0] == first[b].v[0] && first[a].v[1] == first[b].v[1] && first[a].v[2] == first[b].v[2])
            {
                parent[find(a)] = find(b);
            }
        }
    }
    za::Vector<int> count(n, 0);
    for(int v = 0; v < n; v++)
    {
        count[find(v)]++;
    }
    const int axe = static_cast<int>(za::maxElement(count.begin(), count.end()) - count.begin());

    // Each vertex where it lies now (model units), the arm's on the axe's vertex nearest the arm's middle.
    za::Vector<glm::vec3> at(n);
    glm::vec3 armMiddle{0.f};
    int armCount = 0;
    for(int v = 0; v < n; v++)
    {
        const glm::vec3 p{first[v].v[0] * hdr->scale[0] + hdr->scale_origin[0], first[v].v[1] * hdr->scale[1] + hdr->scale_origin[1],
            first[v].v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        for(int k = 0; k < 3; k++)
        {
            at[v][k] = rot[k][0] * p.x + rot[k][1] * p.y + rot[k][2] * p.z + move[k];
        }
        if(find(v) != axe)
        {
            armMiddle += at[v];
            armCount++;
        }
    }
    if(armCount > 0)
    {
        armMiddle /= static_cast<float>(armCount);
        int onto = -1;
        float best = 1e9f;
        for(int v = 0; v < n; v++)
        {
            const float d = glm::length(at[v] - armMiddle);
            if(find(v) == axe && d < best)
            {
                best = d;
                onto = v;
            }
        }
        for(int v = 0; v < n && onto >= 0; v++)
        {
            if(find(v) != axe)
            {
                at[v] = at[onto];
            }
        }
    }

    glm::vec3 lo{1e9f}, hi{-1e9f};
    float radius = 0.f;
    for(const glm::vec3& p : at)
    {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
        radius = za::max(radius, glm::length(p));
    }
    const glm::vec3 scale = glm::max((hi - lo) / 255.f, glm::vec3{1e-4f});

    // The normals turned with it (the nearest of Quake's 162).
    za::Vector<trivertx_t> pose(n);
    for(int v = 0; v < n; v++)
    {
        const float* o = r_avertexnormals[za::min<int>(first[v].lightnormalindex, NUMVERTEXNORMALS - 1)];
        glm::vec3 turned;
        for(int k = 0; k < 3; k++)
        {
            turned[k] = rot[k][0] * o[0] + rot[k][1] * o[1] + rot[k][2] * o[2];
        }
        int normal = 0;
        float dot = -2.f;
        for(int i = 0; i < NUMVERTEXNORMALS; i++)
        {
            const float d = turned.x * r_avertexnormals[i][0] + turned.y * r_avertexnormals[i][1] + turned.z * r_avertexnormals[i][2];
            if(d > dot)
            {
                dot = d;
                normal = i;
            }
        }
        for(int k = 0; k < 3; k++)
        {
            pose[v].v[k] = static_cast<byte>(za::min(255.f, za::max(0.f, (at[v][k] - lo[k]) / scale[k] + 0.5f)));
        }
        pose[v].lightnormalindex = static_cast<byte>(normal);
    }
    for(int p = 0; p < hdr->numposes; p++)
    {
        memcpy(poses[p], pose.data(), sizeof(trivertx_t) * static_cast<size_t>(n));
    }
    trivertx_t boxLo{}, boxHi{};
    for(int k = 0; k < 3; k++)
    {
        boxLo.v[k] = 0;
        boxHi.v[k] = 255;
    }
    for(int f = 0; f < hdr->numframes; f++)
    {
        hdr->frames[f].bboxmin = boxLo;
        hdr->frames[f].bboxmax = boxHi;
    }
    for(int k = 0; k < 3; k++)
    {
        hdr->scale[k] = scale[k];
        hdr->scale_origin[k] = lo[k];
    }
    hdr->boundingradius = radius;
    Con_DPrintf("VR: %s: the arm (%d vertices) taken away, laid as the axe (scale_origin %.3f %.3f %.3f)\n", name, armCount,
        lo.x, lo.y, lo.z);
}

} // namespace

static void aliasPosesLoaded(const char* name, void* aliashdr, const stvert_t* stverts, const dtriangle_t* tris,
    trivertx_t** poses);

// Mod_LoadAliasModel, after the frames are read (poses writable, before the bounds and the vertex
// buffer are made).
extern "C" void VR_AliasPosesLoaded(const char* name, void* aliashdr, const stvert_t* stverts, const dtriangle_t* tris,
    trivertx_t** poses)
{
    const double t0 = Sys_DoubleTime(); // load timing (vr_startup_times)
    aliasPosesLoaded(name, aliashdr, stverts, tris, poses);
    VR_TimeAdd("  their VR poses hook", Sys_DoubleTime() - t0);
}

static void aliasPosesLoaded(const char* name, void* aliashdr, const stvert_t* stverts, const dtriangle_t* tris,
    trivertx_t** poses)
{
    aliashdr_t* hdr = static_cast<aliashdr_t*>(aliashdr);
    const auto id = qvr::modelmeta::identifyPath(name);
    if(id == qvr::modelmeta::Id::Mg3SuperAxe || id == qvr::modelmeta::Id::Mg3SuperAxeGlow)
    {
        superAxePoses(name, hdr, tris, poses);
        return;
    }
    const bool knight = id == qvr::modelmeta::Id::Knight;
    const bool hellKnight = id == qvr::modelmeta::Id::Hknight;
    // The ogre, the soldier and the enforcer: only Quake VR's own models (by their known vertices); the gremlin: Hipnotic's;
    // the rocket ogre: MG3's.
    const bool knownOnly = id == qvr::modelmeta::Id::Ogre || id == qvr::modelmeta::Id::Soldier ||
                           id == qvr::modelmeta::Id::Enforcer || id == qvr::modelmeta::Id::Grem ||
                           id == qvr::modelmeta::Id::Mg3OgreRocket;
    if(!knight && !hellKnight && !knownOnly)
    {
        return;
    }

    const KnownSword* known = nullptr;
    for(const KnownSword& k : knownSwords)
    {
        if(id == k.model && hdr->numverts == k.numverts && hdr->numtris == k.numtris)
        {
            known = &k;
        }
    }

    za::Vector<int> sword;
    if(known)
    {
        za::Vector<char> in(hdr->numverts, 0);
        for(int v : known->verts)
        {
            in[v] = 1;
        }
        for(int i = 0; i < hdr->numtris; i++)
        {
            if(in[tris[i].vertindex[0]] && in[tris[i].vertindex[1]] && in[tris[i].vertindex[2]])
            {
                sword.pushBack(i);
            }
        }
    }
    else if(!knownOnly)
    {
        sword = knight ? knightSword(hdr, stverts, tris) : separatePieceSword(hdr, tris, poses[0]);
    }
    if(sword.empty())
    {
        return;
    }

    // The sword's own vertices (not the hand's, which the knight's hilt shares).
    za::Vector<char> inSword(hdr->numverts, 0), inRest(hdr->numverts, 0);
    za::Vector<char> isSwordTri(hdr->numtris, 0);
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
    za::Vector<int> own, shared;
    for(int v = 0; v < hdr->numverts; v++)
    {
        if(inSword[v])
        {
            (inRest[v] ? shared : own).pushBack(v);
        }
    }

    // Onto the hilt (the hand) if it shares it, else the sword's middle.
    const za::Vector<int>& anchor = shared.empty() ? own : shared;
    int frames = 0;
    for(int f = 0; f < hdr->numframes; f++)
    {
        const maliasframedesc_t& fd = hdr->frames[f];
        const bool death = known && known->firstDeath >= 0 ? f >= known->firstDeath && f <= known->lastDeath
                                                           : strncmp(fd.name, "death", 5) == 0 || strncmp(fd.name, "bdeath", 6) == 0 ||
                                                                 strncmp(fd.name, "fdeath", 6) == 0;
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
    Con_DPrintf("VR: %s: the %s (%d vertices) hidden in %d death frames\n", name, droppedName(name),
        static_cast<int>(own.size()), frames);
}
