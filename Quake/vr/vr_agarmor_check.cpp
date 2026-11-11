// vr_agarmor_check.cpp -- vr_agarmor_check, the armour shells' self-check (moved out of vr_agarmor.cpp).
//
// vr_agarmor_check [model] [random poses]: the shells' fit measured on the authored rig's poses (its rest, every joint
// near the pieces swept through its limits, ragdoll-like random poses within them): body vertices inside a plate (the
// body poking into or through it: inside the closed shell, by its winding number) and the plates' outer vertices
// inside the body (the plate sunk under the skin: the body showing over it), each by the region (neck and head,
// shoulders and arms, waist and legs, the torso itself) and its deepest. Ends on "agarmor check: N passed, M failed":
// the shells made, made alike twice (the same hash), closed, and clean in the rest pose.

#include "vr_testkit.hpp"
#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_mdldequant.hpp"
#include "vr_rig2.hpp"
#include "vr_rigpose.hpp"
#include "vr_geom.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/String/String.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

namespace qvr::agarmor
{

using namespace detail;

namespace
{

using geom::pi;

// ---- The check ----
struct Hit
{
    int count{0};
    float deepest{0.f};
};
enum Region : int
{
    RNeck,
    RShoulders,
    RWaist,
    RTorso,
    NumRegions
};
constexpr const char* regionNames[NumRegions] = {"neck", "shoulders", "waist", "torso"};

[[nodiscard]] int regionOf(const rig2::Rig& rig, int bone)
{
    const char* n = bone >= 0 && bone < rig.numBones ? rig.bones[bone].name : "";
    if(!strcmp(n, "neck") || !strcmp(n, "head"))
    {
        return RNeck;
    }
    if(!strncmp(n, "clavicle", 8) || !strncmp(n, "upperarm", 8) || !strncmp(n, "forearm", 7) || !strncmp(n, "hand", 4) ||
       !strcmp(n, "weapon"))
    {
        return RShoulders;
    }
    if(!strcmp(n, "pelvis") || !strncmp(n, "thigh", 5) || !strncmp(n, "shin", 4) || !strncmp(n, "foot", 4))
    {
        return RWaist;
    }
    return RTorso;
}

struct Tally
{
    Hit bodyIn[NumPieces][NumRegions];  // body vertices inside a plate
    Hit plateIn[NumPieces][NumRegions]; // plate (outer) vertices inside the body
    int poses{0};
    int posesBad{0};
    char worst[64]{};
    int worstCount{0};
    void add(const Tally& o)
    {
        for(int p = 0; p < NumPieces; p++)
        {
            for(int r = 0; r < NumRegions; r++)
            {
                bodyIn[p][r].count += o.bodyIn[p][r].count;
                bodyIn[p][r].deepest = za::max(bodyIn[p][r].deepest, o.bodyIn[p][r].deepest);
                plateIn[p][r].count += o.plateIn[p][r].count;
                plateIn[p][r].deepest = za::max(plateIn[p][r].deepest, o.plateIn[p][r].deepest);
            }
        }
        poses += o.poses;
        posesBad += o.posesBad;
    }
    [[nodiscard]] int total() const
    {
        int n = 0;
        for(int p = 0; p < NumPieces; p++)
        {
            for(int r = 0; r < NumRegions; r++)
            {
                n += bodyIn[p][r].count + plateIn[p][r].count;
            }
        }
        return n;
    }
};

// The solid angle of triangle abc seen from the origin (Van Oosterom and Strackee), over 4 pi: a closed surface's
// sum is its winding number round the point.
[[nodiscard]] float solidAngle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const float la = glm::length(a), lb = glm::length(b), lc = glm::length(c);
    const float num = glm::dot(a, glm::cross(b, c));
    const float den = la * lb * lc + glm::dot(a, b) * lc + glm::dot(b, c) * la + glm::dot(c, a) * lb;
    return 2.f * za::atan2(num, den) / (4.f * pi);
}

[[nodiscard]] float closestDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    // (Ericson's closest point on a triangle)
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return glm::length(p - a);
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return glm::length(p - b);
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return glm::length(p - (a + ab * (d1 / (d1 - d3))));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return glm::length(p - c);
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return glm::length(p - (a + ac * (d2 / (d2 - d6))));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return glm::length(p - (b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)))));
    }
    const float den = 1.f / (va + vb + vc);
    return glm::length(p - (a + ab * (vb * den) + ac * (vc * den)));
}

struct CheckData
{
    za::Vector<glm::vec3> bodyAll, body, shell;
    za::Vector<int> bodyRegion, shellRegion;
    za::Vector<uint8_t> nearPiece[NumPieces]; // body vertices near a piece at rest (the ones measured against it)
    // At rest already (the body's own overlaps: a plate under a shoulder pad or an arm hanging against the torso; a
    // body vertex bulging past a plate's edge): the poses count what their motion adds.
    za::Vector<uint8_t> restBodyIn[NumPieces], restPlateIn;
    bool baseline{false};
};
CheckData cd;

void skinShell(const Shell& s, const rigpose::Rigid* F, za::Vector<glm::vec3>& out)
{
    out.resize(s.verts.size(), glm::vec3{0.f});
    for(int v = 0; v < count(s.verts); v++)
    {
        const Vert& x = at(s.verts, v);
        glm::vec3 p{0.f};
        for(int b = 0; b < rig2::maxBones; b++)
        {
            p += x.w.w[b] > 0.f ? rigpose::apply(F[b], x.p) * x.w.w[b] : glm::vec3{0.f};
        }
        at(out, v) = p;
    }
}

void measure(const Shell& s, const rig2::Rig& rig, const rig2::Pose& pose, Tally& t)
{
    rigpose::Rigid F[rig2::maxBones];
    rig2::forward(rig, pose, F);
    const Body& b = s.body;
    cd.bodyAll.resize(b.mdlRest.size(), glm::vec3{0.f});
    rig2::skin(rig, F, b.mdlRest.data(), count(b.mdlRest), cd.bodyAll.data(), static_cast<bool>(vr_rig2_dq.value));
    cd.body.resize(b.p.size(), glm::vec3{0.f});
    for(int v = 0; v < count(b.p); v++)
    {
        at(cd.body, v) = at(cd.bodyAll, at(b.rep, v));
    }
    skinShell(s, F, cd.shell);
    Tally one;
    one.poses = 1;
    // Body vertices inside a plate: the winding number of the piece's closed shell round them.
    int firstTri = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const int nt = s.pieceTris[piece];
        for(int v = 0; v < count(cd.body); v++)
        {
            if(!at(cd.nearPiece[piece], v))
            {
                continue;
            }
            const glm::vec3 p = at(cd.body, v);
            float wn = 0.f;
            for(int k = firstTri; k < firstTri + nt; k++)
            {
                wn += solidAngle(at(cd.shell, at(s.tris, 3 * k)) - p, at(cd.shell, at(s.tris, 3 * k + 1)) - p,
                    at(cd.shell, at(s.tris, 3 * k + 2)) - p);
            }
            if(cd.baseline)
            {
                at(cd.restBodyIn[piece], v) = za::abs(wn) > 0.5f ? 1 : 0;
            }
            else if(at(cd.restBodyIn[piece], v))
            {
                continue;
            }
            if(za::abs(wn) > 0.5f)
            {
                float d = 1e9f;
                for(int k = firstTri; k < firstTri + nt; k++)
                {
                    d = za::min(d, closestDistance(p, at(cd.shell, at(s.tris, 3 * k)), at(cd.shell, at(s.tris, 3 * k + 1)),
                                       at(cd.shell, at(s.tris, 3 * k + 2))));
                }
                Hit& h = one.bodyIn[piece][at(cd.bodyRegion, v)];
                h.count++;
                h.deepest = za::max(h.deepest, d);
            }
        }
        firstTri += nt;
    }
    // The plates' outer vertices inside the body (its winding number: the .mdl is closed enough).
    for(int v = 0; v < count(cd.shell); v++)
    {
        const Vert& x = at(s.verts, v);
        if(x.layer != 0)
        {
            continue;
        }
        const glm::vec3 p = at(cd.shell, v);
        float wn = 0.f;
        for(int k = 0; k + 2 < count(b.tris); k += 3)
        {
            wn += solidAngle(at(cd.body, at(b.tris, k)) - p, at(cd.body, at(b.tris, k + 1)) - p, at(cd.body, at(b.tris, k + 2)) - p);
        }
        if(cd.baseline)
        {
            at(cd.restPlateIn, v) = wn > 0.5f ? 1 : 0;
        }
        else if(at(cd.restPlateIn, v))
        {
            continue;
        }
        if(wn > 0.5f)
        {
            float d = 1e9f;
            for(int k = 0; k + 2 < count(b.tris); k += 3)
            {
                d = za::min(d, closestDistance(p, at(cd.body, at(b.tris, k)), at(cd.body, at(b.tris, k + 1)), at(cd.body, at(b.tris, k + 2))));
            }
            Hit& h = one.plateIn[x.piece][at(cd.shellRegion, v)];
            h.count++;
            h.deepest = za::max(h.deepest, d);
        }
    }
    one.posesBad = one.total() > 0 ? 1 : 0;
    t.add(one);
    if(one.total() > t.worstCount)
    {
        t.worstCount = one.total();
    }
}

void printTally(const char* what, const Tally& t)
{
    Con_Printf("agarmor check: %s: %d poses, %d with faults\n", what, t.poses, t.posesBad);
    for(int p = 0; p < NumPieces; p++)
    {
        char line[512];
        int at0 = q_snprintf(line, sizeof(line), "agarmor check:   %s: body in plate", pieceNames[p]);
        for(int r = 0; r < NumRegions; r++)
        {
            at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), " %s %d (%.2f)", regionNames[r], t.bodyIn[p][r].count,
                static_cast<double>(t.bodyIn[p][r].deepest));
        }
        at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), "; plate in body");
        for(int r = 0; r < NumRegions; r++)
        {
            at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), " %s %d (%.2f)", regionNames[r], t.plateIn[p][r].count,
                static_cast<double>(t.plateIn[p][r].deepest));
        }
        Con_Printf("%s\n", line);
    }
    if(t.worst[0])
    {
        Con_Printf("agarmor check:   worst: %s (%d)\n", t.worst, t.worstCount);
    }
}

void checkModel(const char* modelName, int randoms, int& passed, int& failed)
{
    Con_Printf("agarmor check: %s\n", modelName);
    const auto verdict = [&](bool ok, const char* what) {
        Con_Printf("agarmor check: %s: %s (%s)\n", ok ? "PASS" : "FAIL", what, modelName);
        (ok ? passed : failed)++;
    };
    qmodel_t* src = Mod_ForName(modelName, false);
    Shell* sp = src ? shellFor(src) : nullptr;
    const rig2::Rig* rig = src ? rig2::rigFor(src) : nullptr;
    verdict(sp && rig, "the shells made");
    if(!sp || !rig)
    {
        return;
    }
    // Made again: alike.
    Shell again;
    again.model = sp->model;
    (void)buildShell(src, again);
    Con_Printf("agarmor check: hash %08x, again %08x; %d vertices, %d triangles (cuirass %d, helmet %d), %d slots\n", sp->hash, again.hash,
        count(sp->verts), count(sp->tris) / 3, sp->pieceTris[Cuirass], sp->pieceTris[Helmet], sp->numSlots);
    Con_Printf("agarmor check: helmet fit: %s\n", sp->helmFit);
    verdict(again.hash == sp->hash && again.hash != 0u, "made alike twice");
    const Shell& s = *sp;
    // Closed: every edge of each piece's plates used twice.
    {
        EdgeMap uses;
        uses.init(count(s.tris));
        // (by place: the seam's and the rim's copies welded)
        za::Vector<int> weld(s.verts.size(), 0);
        for(int v = 0; v < count(s.verts); v++)
        {
            at(weld, v) = v;
            for(int u = 0; u < v; u++)
            {
                if(at(weld, u) == u && at(s.verts, u).piece == at(s.verts, v).piece && glm::length(at(s.verts, u).p - at(s.verts, v).p) < 1e-4f)
                {
                    at(weld, v) = u;
                    break;
                }
            }
        }
        for(int k = 0; k + 2 < count(s.tris); k += 3)
        {
            for(int e = 0; e < 3; e++)
            {
                bool fresh = false;
                int& n = uses.get(at(weld, at(s.tris, k + e)), at(weld, at(s.tris, k + (e + 1) % 3)), fresh);
                n = fresh ? 1 : n + 1;
            }
        }
        int open = 0;
        for(za::SizeT i = 0; i < uses.keys.size(); i++)
        {
            open += uses.keys[i] != ~0ull && uses.vals[i] != 2 ? 1 : 0;
        }
        Con_Printf("agarmor check: edges not shared by exactly two triangles: %d\n", open);
        verdict(open == 0, "each shell closed");
    }
    // The hero plates (vr_agarmor_hero; ROUND24.md, "Armour look: hero plates"): two plates of a piece whose faces meet along a side
    // (a's corners 1, 2 on b's 0, 3, their normals one) are one hero plate's columns. Every armoured humanoid: the breastplate's one
    // line, each pauldron's top row's two plates of three (two lines each), each elbow's and knee's cop of three (two each): 17.
    {
        int seams = 0, smooth = 0, firstVert = 0;
        for(int piece = 0; piece < NumPieces; piece++)
        {
            const int end = firstVert + s.pieceVerts[piece];
            for(int a = firstVert; piece != Helmet && a + plateVerts <= end; a += plateVerts)
            {
                for(int b = firstVert; b + plateVerts <= end; b += plateVerts)
                {
                    const bool meet = a != b && glm::distance(at(s.verts, a + 1).p, at(s.verts, b).p) < 0.02f &&
                                      glm::distance(at(s.verts, a + 2).p, at(s.verts, b + 3).p) < 0.02f;
                    seams += meet ? 1 : 0;
                    smooth += meet && glm::distance(at(s.verts, a + 2).n, at(s.verts, b + 3).n) < 1e-3f ? 1 : 0;
                }
            }
            firstVert = end;
        }
        const int want = vr_agarmor_hero.value != 0.f ? 17 : 0;
        Con_Printf("agarmor check: hero plates' lines: %d (%d shading as one; want %d)\n", seams, smooth, want);
        verdict(seams == want && smooth == seams, "hero plates whole");
        verdict(count(s.tris) / 3 <= 4000, "at most 4000 triangles a body (the full set)");
    }
    // What is measured against what: body vertices near each piece at rest; regions by heaviest bone.
    const Body& b = s.body;
    cd.bodyRegion.resize(b.p.size(), 0);
    for(int v = 0; v < count(b.p); v++)
    {
        at(cd.bodyRegion, v) = regionOf(*rig, heaviest(at(b.w, v)));
    }
    cd.shellRegion.resize(s.verts.size(), 0);
    for(int v = 0; v < count(s.verts); v++)
    {
        at(cd.shellRegion, v) = regionOf(*rig, heaviest(at(s.verts, v).w));
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for(const Vert& x : s.verts)
        {
            lo = x.piece == piece ? glm::min(lo, x.p) : lo;
            hi = x.piece == piece ? glm::max(hi, x.p) : hi;
        }
        cd.nearPiece[piece].clear();
        cd.nearPiece[piece].resize(b.p.size(), uint8_t{0});
        const glm::vec3 c = (lo + hi) * 0.5f;
        const float r = glm::length(hi - lo) * 0.5f + 6.f;
        for(int v = 0; v < count(b.p); v++)
        {
            at(cd.nearPiece[piece], v) = glm::length(at(b.p, v) - c) < r ? 1 : 0;
        }
    }
    const double t0 = Sys_DoubleTime();
    Tally rest;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        cd.restBodyIn[piece].clear();
        cd.restBodyIn[piece].resize(b.p.size(), uint8_t{0});
    }
    cd.restPlateIn.clear();
    cd.restPlateIn.resize(s.verts.size(), uint8_t{0});
    cd.baseline = true;
    measure(s, *rig, rig2::restPose(*rig), rest);
    cd.baseline = false;
    printTally("rest pose (the body's own overlaps; the poses below count only what their motion adds)", rest);
    int outer = 0;
    for(const Vert& x : s.verts)
    {
        outer += x.layer == 0 ? 1 : 0;
    }
    float restDeepest = 0.f;
    int restPlate = 0;
    for(int p = 0; p < NumPieces; p++)
    {
        for(int r = 0; r < NumRegions; r++)
        {
            restDeepest = za::max(restDeepest, rest.bodyIn[p][r].deepest);
            restPlate += rest.plateIn[p][r].count;
        }
    }
    Con_Printf("agarmor check: at rest %d of %d plate vertices inside the body (hidden by it), body in plate at most %.2f deep\n", restPlate,
        outer, static_cast<double>(restDeepest));
    // (deep within a plate's slab is under it, not through it; a slab on a slanting part of the grid, the yoke's, is deep)
    verdict(restDeepest < za::max(vr_agarmor_thick.value, 0.05f) * 2.5f && restPlate * 5 < outer,
        "the rest pose: no body vertex past a plate's slab, under a fifth of the plates hidden in the body (under the pads and arms)");
    // Every joint near the pieces through its limits: a ball's swing ends and diagonals, its twist's ends; a hinge's ends.
    const char* swept[] = {"spine", "chest", "neck", "head", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r"};
    Tally sweeps;
    for(const char* j : swept)
    {
        const int bone = rig2::boneByName(*rig, j);
        if(bone < 0)
        {
            continue;
        }
        const rig2::Bone& bb = rig->bones[bone];
        Tally one;
        const rig2::Pose restPose = rig2::restPose(*rig);
        const glm::vec3 r0 = restPose.a[bone];
        glm::vec3 tries[12];
        int n = 0;
        if(bb.joint == rig2::Joint::Ball)
        {
            tries[n++] = glm::vec3{bb.swingX.x, r0.y, r0.z};
            tries[n++] = glm::vec3{bb.swingX.y, r0.y, r0.z};
            tries[n++] = glm::vec3{r0.x, bb.swingY.x, r0.z};
            tries[n++] = glm::vec3{r0.x, bb.swingY.y, r0.z};
            tries[n++] = glm::vec3{r0.x, r0.y, bb.twist.x};
            tries[n++] = glm::vec3{r0.x, r0.y, bb.twist.y};
            for(int d = 0; d < 4; d++)
            {
                tries[n++] = glm::vec3{(d & 1) ? bb.swingX.y : bb.swingX.x, (d & 2) ? bb.swingY.y : bb.swingY.x, r0.z};
            }
        }
        else if(bb.joint == rig2::Joint::Hinge)
        {
            tries[n++] = glm::vec3{bb.range.x, 0.f, 0.f};
            tries[n++] = glm::vec3{bb.range.y, 0.f, 0.f};
        }
        for(int k = 0; k < n; k++)
        {
            rig2::Pose p = restPose;
            p.a[bone] = rig2::clampAngles(*rig, bone, tries[k]);
            Tally pt;
            measure(s, *rig, p, pt);
            if(pt.total() > one.worstCount)
            {
                one.worstCount = pt.total();
                q_snprintf(one.worst, sizeof(one.worst), "%s at (%.0f %.0f %.0f)", j, static_cast<double>(p.a[bone].x),
                    static_cast<double>(p.a[bone].y), static_cast<double>(p.a[bone].z));
            }
            one.add(pt);
        }
        char what[64];
        q_snprintf(what, sizeof(what), "sweep %s", j);
        printTally(what, one);
        const int worstCount = za::max(sweeps.worstCount, one.worstCount);
        if(one.worstCount > sweeps.worstCount)
        {
            q_strlcpy(sweeps.worst, one.worst, sizeof(sweeps.worst));
        }
        sweeps.add(one);
        sweeps.worstCount = worstCount;
    }
    printTally("all sweeps", sweeps);
    Tally random;
    uint32_t seed = 12345u;
    for(int i = 0; i < randoms; i++)
    {
        const rig2::Pose p = rig2::randomPose(*rig, seed);
        Tally pt;
        measure(s, *rig, p, pt);
        if(pt.total() > random.worstCount)
        {
            random.worstCount = pt.total();
            q_snprintf(random.worst, sizeof(random.worst), "random pose %d", i);
        }
        const int worstCount = random.worstCount;
        random.add(pt);
        random.worstCount = worstCount;
    }
    printTally("random poses", random);
    Con_Printf("agarmor check: measured in %.0f ms\n", (Sys_DoubleTime() - t0) * 1000.0);
}

} // namespace

namespace detail
{

// vr_agarmor_check [model | all] [random poses]: one model's shells, or (no model, "all") every authored wearer's.
void check_f()
{
    const bool all = Cmd_Argc() < 2 || !q_strcasecmp(Cmd_Argv(1), "all");
    const int randoms = Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 0, 5000) : 200;
    int passed = 0, failed = 0;
    if(all)
    {
        for(const char* m : checked)
        {
            checkModel(m, randoms, passed, failed);
        }
    }
    else
    {
        checkModel(Cmd_Argv(1), randoms, passed, failed);
    }
    qvr::testkit::verdict("agarmor check", passed, failed);
}

namespace
{

// The bones' motions for a pose: the rest pose (`bent` false), or knees and elbows well bent, the thighs and upper arms forward,
// the spine leaning: a body at work. Any wearer or body build, by its bones' names; each bone about its pivot under its parent.
struct LookPose
{
    glm::mat4 m[rig2::maxBones];
    bool done[rig2::maxBones]{};
};

[[nodiscard]] float lookBend(const char* name)
{
    constexpr struct
    {
        const char* name;
        float degrees;
    } bends[] = {{"forearm", -75.f}, {"shin", 70.f}, {"thigh", -40.f}, {"upperarm", -30.f}, {"spine", -12.f}, {"chest", -10.f}};
    for(const auto& b : bends)
    {
        if(!strncmp(name, b.name, strlen(b.name)))
        {
            return b.degrees;
        }
    }
    return 0.f;
}

void lookPoseBone(const rig2::Rig& rig, int b, bool bent, LookPose& pose)
{
    if(pose.done[b])
    {
        return;
    }
    pose.done[b] = true;
    const int parent = rig.bones[b].parent;
    glm::mat4 up{1.f};
    if(parent >= 0 && parent < rig.numBones)
    {
        lookPoseBone(rig, parent, bent, pose);
        up = pose.m[parent];
    }
    const float deg = bent ? lookBend(rig.bones[b].name) : 0.f;
    const float a = glm::radians(deg);
    glm::mat4 turn{1.f};
    turn[0][0] = za::cos(a);
    turn[0][2] = -za::sin(a);
    turn[2][0] = za::sin(a);
    turn[2][2] = za::cos(a);
    glm::mat4 toPivot{1.f}, fromPivot{1.f};
    toPivot[3] = glm::vec4{-rig.bones[b].pivot, 1.f};
    fromPivot[3] = glm::vec4{rig.bones[b].pivot, 1.f};
    pose.m[b] = up * fromPivot * turn * toPivot;
}

[[nodiscard]] glm::vec3 lookSkin(const LookPose& pose, const W& w, const glm::vec3& p, float pointW)
{
    glm::vec3 out{0.f};
    for(int b = 0; b < rig2::maxBones; b++)
    {
        out += w.w[b] > 0.f ? glm::vec3{pose.m[b] * glm::vec4{p, pointW}} * w.w[b] : glm::vec3{0.f};
    }
    return out;
}

[[nodiscard]] bool rayHitsTri(const glm::vec3& o, const glm::vec3& d, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float reach)
{
    const glm::vec3 e1 = b - a, e2 = c - a, pv = glm::cross(d, e2);
    const float det = glm::dot(e1, pv);
    if(za::abs(det) < 1e-7f)
    {
        return false;
    }
    const glm::vec3 tv = o - a, qv = glm::cross(tv, e1);
    const float u = glm::dot(tv, pv) / det, v = glm::dot(d, qv) / det, t = glm::dot(e2, qv) / det;
    return u >= -1e-3f && v >= -1e-3f && u + v <= 1.001f && t > 0.f && t < reach;
}

// (vr_agarmor_look_check <model> <piece>: that piece's uncovered vertices at rest listed)
const char* lookListModel = nullptr;
int lookListPiece = -1;
constexpr int lookFewest = 8; // a piece's skin measured if it has this many vertices

// How much of a piece's skin is under its plates in a pose: rays out from each of its vertices along its normal, hitting any plate
// within `reach` units. The share per piece.
void lookCoverage(const Shell& s, const rig2::Rig& rig, bool bent, float backCut, float* share)
{
    constexpr float reach = 9.f;
    LookPose pose;
    for(glm::mat4& m : pose.m)
    {
        m = glm::mat4{1.f};
    }
    for(int b = 0; b < rig.numBones; b++)
    {
        lookPoseBone(rig, b, bent, pose);
    }
    za::Vector<glm::vec3> posed(s.verts.size(), glm::vec3{0.f});
    for(int v = 0; v < count(s.verts); v++)
    {
        at(posed, v) = lookSkin(pose, at(s.verts, v).w, at(s.verts, v).p, 1.f);
    }
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const uint32_t bones = pieceRegionBones(rig, piece);
        int total = 0, covered = 0;
        for(int v = 0; piece != Helmet && v < count(s.body.p); v++)
        {
            const W& w = at(s.body.w, v);
            float region = 0.f;
            for(int b = 0; b < rig2::maxBones; b++)
            {
                region += ((bones >> static_cast<unsigned>(b)) & 1u) ? w.w[b] : 0.f;
            }
            if(region < 0.6f || (piece == Cuirass && at(s.body.p, v).x < backCut))
            {
                continue;
            }
            // (a limb's skin facing along it, into its open ends: a boot's sole, a shoulder's top, not what plates round it cover)
            if(piece != Cuirass)
            {
                glm::vec3 oa, ua, fa, la;
                pieceAxes(rig, piece, oa, ua, fa, la);
                if(za::abs(glm::dot(at(s.body.n, v), ua)) > 0.7f)
                {
                    continue;
                }
                // (past its joint's sleeve: the next part's skin, whatever the weights say; its plates no longer reach over the joint)
                const PlateTube& tb = s.tubes[piece];
                const float along = glm::dot(at(s.body.p, v) - tb.origin, tb.up);
                const bool upper = piece == PauldronL || piece == PauldronR || piece == ThighL || piece == ThighR;
                const bool lower = piece == BracerL || piece == BracerR || piece == ShinL || piece == ShinR;
                if((upper && along > tb.grid.sleeveEnd) || (lower && along < tb.grid.sleeveEnd))
                {
                    continue;
                }
                // (past a forearm's or a shin's last row: a humanoid's hand and foot, bare on purpose (his notes, 2026-11-06))
                if(lower && along > tb.grid.hBottom + 0.2f)
                {
                    continue;
                }
            }
            total++;
            const glm::vec3 o = lookSkin(pose, w, at(s.body.p, v), 1.f);
            const glm::vec3 d = geom::safeNormalize(lookSkin(pose, w, at(s.body.n, v), 0.f), glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
            bool hit = false;
            for(int k = 0; k + 2 < count(s.tris) && !hit; k += 3)
            {
                hit = rayHitsTri(o, d, at(posed, at(s.tris, k)), at(posed, at(s.tris, k + 1)), at(posed, at(s.tris, k + 2)), reach);
            }
            covered += hit ? 1 : 0;
            if(!hit && !bent && piece == lookListPiece && lookListModel && !strcmp(lookListModel, s.model.cStr()))
            {
                const PlateTube& tb = s.tubes[piece];
                const glm::vec3 q = at(s.body.p, v) - tb.origin;
                const int hb = heaviest(w);
                Con_Printf("agarmor look:   bare %s: along %.1f (plates %.1f .. %.1f, sleeve to %.1f), out %.1f, %s %.2f, normal along %.2f\n", pieceNames[piece], static_cast<double>(glm::dot(q, tb.up)), static_cast<double>(tb.grid.hTop),
                    static_cast<double>(tb.grid.hBottom), static_cast<double>(tb.grid.sleeveEnd), static_cast<double>(glm::length(q - tb.up * glm::dot(q, tb.up))), rig.bones[hb].name, static_cast<double>(w.w[hb]), static_cast<double>(glm::dot(at(s.body.n, v), tb.up)));
            }
        }
        // (a few vertices' share says nothing: a death knight's upper arm measures under 8, 3 of 4 covered; his thigh under his skirt two)
        share[piece] = total >= lookFewest ? static_cast<float>(covered) / static_cast<float>(total) : 1.f;
    }
}

// How the plates follow the body at rest, piece by piece: per row, the plates' width across (the piece's side axis, its middle band)
// against the skin's at the same height, the most by which they exceed it, and how far the thinnest ring's width over the thickest
// is the body's; the least step from a plate's lower edge down to the top of the row beneath, along the way out.
struct LookFit
{
    float widthOver{0.f};     // units: the plates' width past the skin's, the worst row
    int widthPiece{0}, taperPiece{0};
    float taperOff{0.f};      // the plates' thinnest-to-thickest row width ratio less the body's, the worst piece
    float taperBody{1.f}, taperPlates{1.f}; // of the piece with the most taper
    float step{1e9f};         // units
    float standoff{0.f};      // units: the mean of the plates' feet off the skin (half the width over, per side), all rows
};

void lookFit(const Shell& s, const rig2::Rig& rig, bool skipUpperArms, LookFit& fit)
{
    int firstVert = 0;
    float standoffSum = 0.f;
    int standoffN = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const PlateGrid& g = s.tubes[piece].grid;
        const int plates = s.pieceVerts[piece] / plateVerts;
        if(piece != Helmet && g.rows)
        {
            const uint32_t bones = pieceRegionBones(rig, piece);
            const PlateTube& tube = s.tubes[piece];
            const int rowFirst = g.yoke > 0.f ? 1 : 0;
            float thinB = 1e9f, thickB = 0.f, thinP = 1e9f, thickP = 0.f;
            // (not the end rings: they lie past the region's edge, where the skin's slice is a sliver or nothing)
            const bool skip = skipUpperArms && (piece == PauldronL || piece == PauldronR);
            for(int r = rowFirst + 1; r < g.rows && !skip; r++)
            {
                // Ring r: the skin's slice through it (the piece's triangles' edges crossing its plane), against the plates' feet on it.
                const float h = g.hAt(r);
                float bLo = 1e9f, bHi = -1e9f, pLo = 1e9f, pHi = -1e9f;
                // (where the region's slice is a sliver, the skin weighing on it less: an enforcer's forearm half way down weighs on
                // the upper arm and the hand too, its slice there one edge 0.2 units long against an arm 7.6 across)
                for(float cut = 0.5f; cut > 0.2f && bHi - bLo < 1.f; cut -= 0.2f)
                {
                bLo = 1e9f;
                bHi = -1e9f;
                for(int k = 0; k + 2 < count(s.body.tris); k += 3)
                {
                    float region = 0.f;
                    for(int e = 0; e < 3; e++)
                    {
                        for(int b = 0; b < rig2::maxBones; b++)
                        {
                            region += ((bones >> static_cast<unsigned>(b)) & 1u) ? at(s.body.w, at(s.body.tris, k + e)).w[b] / 3.f : 0.f;
                        }
                    }
                    if(region < cut)
                    {
                        continue;
                    }
                    for(int e = 0; e < 3; e++)
                    {
                        const glm::vec3 p0 = at(s.body.p, at(s.body.tris, k + e)) - tube.origin, p1 = at(s.body.p, at(s.body.tris, k + (e + 1) % 3)) - tube.origin;
                        const float a0 = glm::dot(p0, tube.up) - h, a1 = glm::dot(p1, tube.up) - h;
                        if((a0 < 0.f) != (a1 < 0.f))
                        {
                            const float across = glm::dot(p0 + (p1 - p0) * (a0 / (a0 - a1)), tube.left);
                            bLo = za::min(bLo, across);
                            bHi = za::max(bHi, across);
                        }
                    }
                }
                }
                // (each plate's foot: its underside, the last four of its vertices)
                for(int p = 0; p < za::min(plates, g.rows * g.cols); p++)
                {
                    const int base = firstVert + p * plateVerts + plateVerts - 4;
                    for(int c = 0; c < 4; c++)
                    {
                        const glm::vec3 q = at(s.verts, base + c).p - tube.origin;
                        if(za::abs(glm::dot(q, tube.up) - h) < 0.02f)
                        {
                            const float across = glm::dot(q, tube.left);
                            pLo = za::min(pLo, across);
                            pHi = za::max(pHi, across);
                        }
                    }
                }
                if(bLo < bHi && pLo < pHi)
                {
                    const float over = (pHi - pLo) - (bHi - bLo);
                    fit.widthPiece = over > fit.widthOver ? piece : fit.widthPiece;
                    fit.widthOver = za::max(fit.widthOver, over);
                    standoffSum += over * 0.5f;
                    standoffN++;
                    thinB = za::min(thinB, bHi - bLo);
                    thickB = za::max(thickB, bHi - bLo);
                    thinP = za::min(thinP, pHi - pLo);
                    thickP = za::max(thickP, pHi - pLo);
                }
            }
            if(thickB > 0.f && thickP > 0.f)
            {
                const float tb = thinB / thickB, tp = thinP / thickP;
                if(za::abs(tp - tb) > fit.taperOff)
                {
                    fit.taperOff = za::abs(tp - tb);
                    fit.taperPiece = piece;
                    fit.taperBody = tb;
                    fit.taperPlates = tp;
                }
            }
            // The steps: a plate's lower edge's middle (its face's corners 2, 3) over the next row's top (0, 1), the same column.
            if(plates >= g.rows * g.cols)
            {
                for(int r = 0; r + 1 < g.rows; r++)
                {
                    for(int c = 0; c < g.cols; c++)
                    {
                        const int upper = firstVert + (r * g.cols + c) * plateVerts, lower = firstVert + ((r + 1) * g.cols + c) * plateVerts;
                        const glm::vec3 lip = (at(s.verts, upper + 2).p + at(s.verts, upper + 3).p) * 0.5f, top = (at(s.verts, lower).p + at(s.verts, lower + 1).p) * 0.5f;
                        // (along the way out from the piece's axis there)
                        const glm::vec2 mid = at(g.axis, r + 1);
                        const glm::vec3 q = top - tube.origin - tube.fwd * mid.x - tube.left * mid.y;
                        const glm::vec3 radial = geom::safeNormalize(q - tube.up * glm::dot(q, tube.up), tube.fwd, 1e-6f);
                        fit.step = za::min(fit.step, glm::dot(lip - top, radial));
                    }
                }
            }
        }
        firstVert += s.pieceVerts[piece];
    }
    fit.standoff = standoffSum / static_cast<float>(za::max(standoffN, 1));
}

// Left against right, at rest: each outer vertex of `a`, mirrored, against the nearest of `b`'s (world mirrored about y 0 if `up` is
// null; else in the pieces' own frames, each limb's rest pose its own). The mean distance.
[[nodiscard]] float lookMirror(const Shell& s, const rig2::Rig& rig, int a, int b, bool local)
{
    glm::vec3 oa{0.f}, ob{0.f}, ua{0.f, 0.f, 1.f}, ub{0.f, 0.f, 1.f}, fa{1.f, 0.f, 0.f}, fb{1.f, 0.f, 0.f}, la{0.f, 1.f, 0.f}, lb{0.f, 1.f, 0.f};
    if(local)
    {
        pieceAxes(rig, a, oa, ua, fa, la);
        pieceAxes(rig, b, ob, ub, fb, lb);
    }
    float sum = 0.f;
    int n = 0;
    for(const Vert& v : s.verts)
    {
        if(v.piece != a || v.layer != 0)
        {
            continue;
        }
        const glm::vec3 q = v.p - oa;
        const glm::vec3 want{glm::dot(q, fa), -glm::dot(q, la), glm::dot(q, ua)};
        float best = 1e9f;
        for(const Vert& u : s.verts)
        {
            if(u.piece == b && u.layer == 0)
            {
                const glm::vec3 r = u.p - ob;
                best = za::min(best, glm::distance(want, glm::vec3{glm::dot(r, fb), glm::dot(r, lb), glm::dot(r, ub)}));
            }
        }
        sum += best;
        n++;
    }
    return sum / static_cast<float>(za::max(n, 1));
}

} // namespace

// vr_agarmor_look_check: the plates' generator on every wearer and body build: the same seed the same armour, another seed
// another; every piece there; each body's triangles within the budget; left and right each other's mirror image; each covered
// piece's skin under its plates, at rest and bent. The paint: the three tiers' hues apart (the plates' mean colour, full-colour
// skins), red still red and held back, each tier's plates one hue. Ends on "agarmor look: N passed, M failed".
void lookCheck_f()
{
    constexpr int triBudget = 3400;   // a body's whole set (the player's default was 23 000, a grunt's 3 436)
    constexpr float hueApart = 30.f;  // degrees between any two tiers' means
    constexpr float redSatMost = 0.82f, redSatLeast = 0.35f;
    constexpr float mirrorMost = 0.25f;  // units: left against right, the plates' outer vertices (the player's symmetric body)
    constexpr float mirrorMostAuthored = 4.f; // (a humanoid's: its two arms' rest poses and meshes differ)
    constexpr float coverRest = 0.85f, coverBent = 0.8f; // a covered piece's skin under plates: rays out from its vertices
    constexpr float tileHueMost = 14.f;  // degrees: one tier's tiles, most apart
    constexpr float widthOverMost = 5.5f; // units: a row of plates' width past the skin's at that height (the player's)
    constexpr float widthOverMostAuthored = 7.5f; // (a low-poly humanoid's: a slice's few edges)
    constexpr float taperOffMost = 0.25f; // a piece's thinnest-to-thickest row, the plates' against the skin's (the player's)
    constexpr float taperOffMostAuthored = 0.45f;
    constexpr float stepLeast = 0.25f;    // units: a plate's lower edge above the top of the row beneath
    constexpr const char* all[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl", "progs/vrknight.mdl", "progs/vrhknight.mdl", "progs/vrbody.mdl",
        "progs/vrbody_bare.mdl", "progs/vrbody_lean.mdl", "progs/vrbody_lean_bare.mdl", "progs/vrbody_brawny.mdl", "progs/vrbody_brawny_bare.mdl",
        "progs/vrbody_fem.mdl", "progs/vrbody_fem_bare.mdl"};
    int passed = 0, failed = 0;
    const auto check = [&](bool ok, const char* what) {
        Con_Printf("agarmor look: %s: %s\n", ok ? "PASS" : "FAIL", what);
        (ok ? passed : failed)++;
    };
    const float seed = vr_agarmor_seed.value;
    lookListModel = Cmd_Argc() > 2 ? Cmd_Argv(1) : nullptr;
    lookListPiece = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : -1;
    for(const char* m : all)
    {
        qmodel_t* src = Mod_ForName(m, false);
        Shell* sp = src ? shellFor(src) : nullptr;
        if(!sp)
        {
            check(false, va("%s: no plates", m));
            continue;
        }
        Shell again;
        again.model = sp->model;
        (void)buildShell(src, again);
        Cvar_SetValueQuick(&vr_agarmor_seed, seed + 1.f);
        Shell other;
        other.model = sp->model;
        (void)buildShell(src, other);
        Cvar_SetValueQuick(&vr_agarmor_seed, seed);
        const int tris = count(sp->tris) / 3;
        check(again.hash == sp->hash && sp->hash != 0u && other.hash != sp->hash, va("%s: the same seed the same plates, another seed others (%08x %08x %08x)", m, sp->hash, again.hash, other.hash));
        check(tris <= triBudget, va("%s: %d triangles (budget %d; cuirass %d, helmet %d, thigh %d, shin %d, upper arm %d, forearm %d)", m, tris, triBudget, sp->pieceTris[Cuirass],
            sp->pieceTris[Helmet], sp->pieceTris[ThighL], sp->pieceTris[ShinL], sp->pieceTris[PauldronL], sp->pieceTris[BracerL]));
        bool every = true;
        for(int p = 0; p < NumPieces; p++)
        {
            every = every && sp->pieceTris[p] > 0;
        }
        check(every, va("%s: every piece has plates", m));
        // Left against right.
        const rig2::Rig* rig = rigOfShell(src);
        const bool player = !strncmp(m, "progs/vrbody", 12);
        const float widthLimit = player ? widthOverMost : widthOverMostAuthored, taperLimit = player ? taperOffMost : taperOffMostAuthored;
        float worst = lookMirror(*sp, *rig, Cuirass, Cuirass, !player);
        for(int p = PauldronL; p + 1 < NumPieces; p += 2)
        {
            worst = za::max(worst, lookMirror(*sp, *rig, p, p + 1, !player));
        }
        // (a humanoid's limbs each fit their own limb and rest pose: their cuirass is the mirrored part; 0 limit for the player's own mesh)
        const float mirrorLimit = player ? mirrorMost : mirrorMostAuthored;
        check(worst <= mirrorLimit, va("%s: left and right mirror each other: %.3f units at worst, %.2f the limit (%s)", m, static_cast<double>(worst), static_cast<double>(mirrorLimit),
            player ? "as it stands" : "its limbs in their own frames, their rest poses differ"));
        // The fit: the plates' width against the skin's, row by row; the taper; the step down each row.
        LookFit fit;
        lookFit(*sp, *rig, strstr(m, "hknight") != nullptr, fit); // (a death knight's upper arms: its shoulder shells, not its upper arm's skin, are what they cover)
        check(fit.widthOver <= widthLimit, va("%s: each row follows the body: at most %.2f units wider than the skin (%s; limit %.2f), %.2f on each side on average", m, static_cast<double>(fit.widthOver), pieceNames[fit.widthPiece], static_cast<double>(widthLimit), static_cast<double>(fit.standoff)));
        check(fit.taperOff <= taperLimit, va("%s: the tapers the body's: thinnest to thickest row off by %.2f at worst (%s; limit %.2f; plates %.2f, body %.2f)", m, static_cast<double>(fit.taperOff), pieceNames[fit.taperPiece], static_cast<double>(taperLimit), static_cast<double>(fit.taperPlates), static_cast<double>(fit.taperBody)));
        check(fit.step >= stepLeast, va("%s: a real step at every row: %.2f units at least (limit %.2f)", m, static_cast<double>(fit.step), static_cast<double>(stepLeast)));
        // The skin under the plates.
        float rest[NumPieces], bent[NumPieces];
        const float cut = strstr(m, "enforcer") ? -9.f : -1e9f;
        lookCoverage(*sp, *rig, false, cut, rest);
        lookCoverage(*sp, *rig, true, cut, bent);
        float restLeast = 1.f, bentLeast = 1.f;
        for(int p = 0; p < NumPieces; p++)
        {
            restLeast = za::min(restLeast, rest[p]);
            bentLeast = za::min(bentLeast, bent[p]);
        }
        Con_Printf("agarmor look: %s: covered at rest / bent: cuirass %.2f %.2f, upper arm %.2f %.2f, forearm %.2f %.2f, thigh %.2f %.2f, shin %.2f %.2f\n", m,
            static_cast<double>(rest[Cuirass]), static_cast<double>(bent[Cuirass]), static_cast<double>(za::min(rest[PauldronL], rest[PauldronR])), static_cast<double>(za::min(bent[PauldronL], bent[PauldronR])),
            static_cast<double>(za::min(rest[BracerL], rest[BracerR])), static_cast<double>(za::min(bent[BracerL], bent[BracerR])), static_cast<double>(za::min(rest[ThighL], rest[ThighR])),
            static_cast<double>(za::min(bent[ThighL], bent[ThighR])), static_cast<double>(za::min(rest[ShinL], rest[ShinR])), static_cast<double>(za::min(bent[ShinL], bent[ShinR])));
        // (a death knight's shins: 82% at rest, their skin past the sleeves, which hug it; ROUND24.md, "Armour round four's loose ends")
        const float restLimit = strstr(m, "hknight") ? 0.8f : coverRest;
        check(restLeast >= restLimit, va("%s: no body showing at rest: at least %.0f%% of each covered piece's skin under plates (%.0f%%)", m, static_cast<double>(restLimit * 100.f), static_cast<double>(restLeast * 100.f)));
        // (a death knight's upper arms: his shoulder shells stay on his clavicles, his arm bends out from under them)
        const float bentLimit = strstr(m, "hknight") ? 0.6f : coverBent;
        check(bentLeast >= bentLimit, va("%s: bent: at least %.0f%% (%.0f%%)", m, static_cast<double>(bentLimit * 100.f), static_cast<double>(bentLeast * 100.f)));
    }
    float hue[3]{}, sat[3]{};
    for(int t = 0; t < 3; t++)
    {
        // (the 8-bit skin's texels in full colour, in the skin model's hunk copy)
        qmodel_t* skins = Mod_ForName("progs/vragarmor.mdl", false);
        const auto* kh = skins && skins->type == mod_alias ? static_cast<const aliashdr_t*>(Mod_Extradata(skins)) : nullptr;
        const int w = kh ? kh->skinwidth : 0, h = kh ? kh->skinheight : 0;
        const byte* indexed = kh ? reinterpret_cast<const byte*>(kh) + kh->texels[t] : nullptr;
        za::Vector<byte> rgba(static_cast<za::SizeT>(za::max(w * h, 1) * 4), byte{0});
        for(int i = 0; indexed && i < w * h; i++)
        {
            memcpy(&rgba[static_cast<za::SizeT>(i * 4)], &d_8to24table[indexed[i]], 4);
        }
        const byte* px = indexed ? rgba.data() : nullptr;
        const enum srcformat fmt = SRC_RGBA;
        double sum[3]{0.0, 0.0, 0.0};
        const int xEnd = w * 160 / 256; // (the plates' block: make_agarmor.py)
        for(int y = 0; px && fmt == SRC_RGBA && y < h; y++)
        {
            for(int x = 0; x < xEnd; x++)
            {
                for(int c = 0; c < 3; c++)
                {
                    sum[c] += static_cast<double>(px[(y * w + x) * 4 + c]);
                }
            }
        }
        const double n = za::max(static_cast<double>(xEnd) * static_cast<double>(h), 1.0);
        const float r = static_cast<float>(sum[0] / n), g = static_cast<float>(sum[1] / n), b = static_cast<float>(sum[2] / n);
        const float hi = za::max(r, za::max(g, b)), lo = za::min(r, za::min(g, b));
        const float d = za::max(hi - lo, 1e-3f);
        hue[t] = 60.f * (hi == r ? (g - b) / d : hi == g ? 2.f + (b - r) / d : 4.f + (r - g) / d);
        hue[t] = hue[t] < 0.f ? hue[t] + 360.f : hue[t];
        sat[t] = d / za::max(hi, 1e-3f);
        Con_Printf("agarmor look: tier %d: mean %.0f %.0f %.0f, hue %.0f, saturation %.2f\n", t + 1, static_cast<double>(r), static_cast<double>(g), static_cast<double>(b),
            static_cast<double>(hue[t]), static_cast<double>(sat[t]));
        // Each tile's own hue: a tier's plates one colour, whatever their tone and wear.
        float tileLo = 1e9f, tileHi = -1e9f;
        for(int ty = 0; px && fmt == SRC_RGBA && ty < 6; ty++)
        {
            for(int tx = 0; tx < 5; tx++)
            {
                // (not the emblem's tile, 28 (vr_agarmor.cpp tileEmblem): its rune is a second colour on purpose, red on green
                // and yellow; ROUND24.md, "Armour look: fewer, bigger plates"; nor the hero plates' wide emblem's, 15 and 16:
                // tileWideEmblem)
                if(ty * 5 + tx == 28 || ty * 5 + tx == 15 || ty * 5 + tx == 16)
                {
                    continue;
                }
                double tile[3]{0.0, 0.0, 0.0};
                const int x0 = xEnd * tx / 5, x1 = xEnd * (tx + 1) / 5, y0 = h * ty / 6, y1 = h * (ty + 1) / 6;
                for(int y = y0; y < y1; y++)
                {
                    for(int x = x0; x < x1; x++)
                    {
                        for(int c = 0; c < 3; c++)
                        {
                            tile[c] += static_cast<double>(px[(y * w + x) * 4 + c]);
                        }
                    }
                }
                const float tr = static_cast<float>(tile[0]), tg = static_cast<float>(tile[1]), tb = static_cast<float>(tile[2]);
                const float thi = za::max(tr, za::max(tg, tb)), tlo = za::min(tr, za::min(tg, tb));
                const float td = za::max(thi - tlo, 1e-3f);
                float th = 60.f * (thi == tr ? (tg - tb) / td : thi == tg ? 2.f + (tb - tr) / td : 4.f + (tr - tg) / td);
                th = th < -60.f ? th + 360.f : th;
                tileLo = za::min(tileLo, th);
                tileHi = za::max(tileHi, th);
            }
        }
        check(tileHi - tileLo <= tileHueMost, va("tier %d: its plates one hue: %.0f degrees apart at most (limit %.0f)", t + 1, static_cast<double>(tileHi - tileLo), static_cast<double>(tileHueMost)));
    }
    const auto apart = [&](int a, int b) {
        const float x = za::abs(hue[a] - hue[b]);
        return za::min(x, 360.f - x);
    };
    check(apart(0, 1) >= hueApart && apart(1, 2) >= hueApart && apart(0, 2) >= hueApart,
        va("the tiers' hues apart by %.0f degrees at least (green-yellow %.0f, yellow-red %.0f, green-red %.0f)", static_cast<double>(hueApart), static_cast<double>(apart(0, 1)),
            static_cast<double>(apart(1, 2)), static_cast<double>(apart(0, 2))));
    check(sat[2] <= redSatMost && sat[2] >= redSatLeast, va("red held back, still red: saturation %.2f within %.2f .. %.2f", static_cast<double>(sat[2]), static_cast<double>(redSatLeast), static_cast<double>(redSatMost)));
    check(sat[2] >= sat[1] * 0.5f && sat[2] >= sat[0] * 0.5f, "red not washed out against the others");
    qvr::testkit::verdict("agarmor look", passed, failed);
}

// ---- Pop-through: the body against the plates, per region, per pose, per set of pieces worn ----
namespace
{

constexpr int popGroups = 6;
int popListMask = 0; // (vr_agarmor_pop_check <model> <pose> <mask>: what's counted listed)
// Units: a region's plates' faces off the skin on average, at most (before the joints round, 2026-11-05: 2.70 the most, a grunt's
// thighs; after, 2.94, the same thighs' bulkier half left).
constexpr float popBulkMost = 3.2f;
[[nodiscard]] constexpr int popGroupOf(int piece)
{
    return piece == Cuirass ? 0 : piece == PauldronL || piece == PauldronR ? 1 : piece == BracerL || piece == BracerR ? 2 : piece == ThighL || piece == ThighR ? 3 : piece == ShinL || piece == ShinR ? 4 : 5;
}

struct PopDef
{
    const char* name;
    float forearm, shin, thigh, upperarm, spine; // degrees, as lookBend's: elbows and knees forward
};
constexpr PopDef popDefs[] = {{"rest", 0.f, 0.f, 0.f, 0.f, 0.f}, {"elbow45", -45.f, 0.f, 0.f, 0.f, 0.f}, {"elbow90", -90.f, 0.f, 0.f, 0.f, 0.f}, {"elbow130", -130.f, 0.f, 0.f, 0.f, 0.f},
    {"knee45", 0.f, 45.f, -25.f, 0.f, 0.f}, {"knee90", 0.f, 90.f, -50.f, 0.f, 0.f}, {"knee130", 0.f, 130.f, -70.f, 0.f, 0.f}, {"armsup", -60.f, 0.f, 0.f, -70.f, 0.f}, {"crouch", -90.f, 100.f, -70.f, -30.f, -15.f}};
constexpr int numPopDefs = static_cast<int>(sizeof(popDefs) / sizeof(popDefs[0]));

struct PopState
{
    const char* name;
    int mask;
};
constexpr PopState popStates[] = {{"all", 1023}, {"cuirass", 1}, {"upperarm", 12}, {"forearm", 48}, {"upper+cuirass", 13}, {"fore+cuirass", 49}, {"arms", 60}, {"thigh", 192}, {"shin", 768}, {"legs", 960}};
constexpr int numPopStates = static_cast<int>(sizeof(popStates) / sizeof(popStates[0]));

struct PopTally
{
    int through[popGroups]{}; // a vertex standing out past a plate it came through (behind it, inside the body), none over it
    int cross[popGroups]{};   // a skin's edge through a worn plate's outer surface
    float deepest{0.f};       // units: the most a vertex stands past the plate it came through
};

struct PopTotals
{
    PopTally states[numPopStates];
    int buried[popGroups]{};  // a plate's face's middle inside the body, its piece's group
    int plates[popGroups]{};
    int detached{0};          // the worst pose: plates not joined to the rest of their piece
    int poses{0};
};

[[nodiscard]] glm::vec3 popSkin(const glm::mat4* m, const W& w, const glm::vec3& p, float pointW)
{
    glm::vec3 out{0.f};
    for(int b = 0; b < rig2::maxBones; b++)
    {
        out += w.w[b] > 0.f ? glm::vec3{m[b] * glm::vec4{p, pointW}} * w.w[b] : glm::vec3{0.f};
    }
    return out;
}

[[nodiscard]] bool rayTriT(const glm::vec3& o, const glm::vec3& d, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, float& t)
{
    const glm::vec3 e1 = b - a, e2 = c - a, pv = glm::cross(d, e2);
    const float det = glm::dot(e1, pv);
    if(za::abs(det) < 1e-9f)
    {
        return false;
    }
    const glm::vec3 tv = o - a, qv = glm::cross(tv, e1);
    const float u = glm::dot(tv, pv) / det, v = glm::dot(d, qv) / det;
    t = glm::dot(e2, qv) / det;
    return u >= 0.f && v >= 0.f && u + v <= 1.f;
}

// The worn pieces' outer surfaces, posed, a dozen triangles a box.
struct PopChunk
{
    int piece{0}, first{0}, n{0};
    glm::vec3 lo{1e9f}, hi{-1e9f};
};

// How many plates of a piece aren't joined to the rest of it in this pose: two are joined when their feet's corners meet.
[[nodiscard]] int popDetached(const Shell& s, const za::Vector<glm::vec3>& posed)
{
    constexpr float meet = 0.6f; // units
    int worst = 0, firstVert = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const int end = firstVert + s.pieceVerts[piece];
        za::Vector<int> plates;
        for(int vi = firstVert; piece != Helmet && vi + plateVerts <= end; vi += plateVerts)
        {
            plates.pushBack(vi);
        }
        const int np = count(plates);
        za::Vector<int> up(static_cast<za::SizeT>(za::max(np, 1)), 0);
        for(int i = 0; i < np; i++)
        {
            at(up, i) = i;
        }
        const auto root = [&](int i) {
            while(at(up, i) != i)
            {
                at(up, i) = at(up, at(up, i));
                i = at(up, i);
            }
            return i;
        };
        for(int i = 0; i < np; i++)
        {
            for(int j = i + 1; j < np; j++)
            {
                bool joined = false;
                for(int a = 0; a < 4 && !joined; a++)
                {
                    for(int b = 0; b < 4 && !joined; b++)
                    {
                        joined = glm::distance(at(posed, at(plates, i) + plateVerts - 4 + a), at(posed, at(plates, j) + plateVerts - 4 + b)) <= meet;
                    }
                }
                if(joined)
                {
                    at(up, root(i)) = root(j);
                }
            }
        }
        za::Vector<int> size(static_cast<za::SizeT>(za::max(np, 1)), 0);
        int most = 0;
        for(int i = 0; i < np; i++)
        {
            most = za::max(most, ++at(size, root(i)));
        }
        worst += np - most;
        firstVert = end;
    }
    return worst;
}

// One pose (each bone's rest -> posed) added to the tallies.
void popPose(const Shell& s, const rig2::Rig& rig, const glm::mat4* pose, PopTotals& out)
{
    const Body& b = s.body;
    za::Vector<glm::vec3> posed(s.verts.size(), glm::vec3{0.f}), bodyP(b.p.size(), glm::vec3{0.f}), bodyN(b.p.size(), glm::vec3{0.f});
    for(int v = 0; v < count(s.verts); v++)
    {
        at(posed, v) = popSkin(pose, at(s.verts, v).w, at(s.verts, v).p, 1.f);
    }
    for(int v = 0; v < count(b.p); v++)
    {
        at(bodyP, v) = popSkin(pose, at(b.w, v), at(b.p, v), 1.f);
        at(bodyN, v) = geom::safeNormalize(popSkin(pose, at(b.w, v), at(b.n, v), 0.f), glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
    }
    // Each vertex's region: the piece it weighs most on, past half.
    uint32_t bones[NumPieces];
    for(int p = 0; p < NumPieces; p++)
    {
        bones[p] = p == Helmet ? 0u : pieceRegionBones(rig, p);
    }
    za::Vector<int> group(b.p.size(), 5);
    for(int v = 0; v < count(b.p); v++)
    {
        float best = 0.5f;
        for(int p = 0; p < NumPieces; p++)
        {
            float region = 0.f;
            for(int i = 0; i < rig2::maxBones; i++)
            {
                region += ((bones[p] >> static_cast<unsigned>(i)) & 1u) ? at(b.w, v).w[i] : 0.f;
            }
            if(region > best)
            {
                best = region;
                at(group, v) = popGroupOf(p);
            }
        }
    }
    // The outer surfaces, boxed.
    za::Vector<PopChunk> chunks;
    for(int k = 0; k + 2 < count(s.tris); k += 3)
    {
        const Vert& v0 = at(s.verts, at(s.tris, k));
        if(v0.layer == 1 || v0.layer == 2)
        {
            continue;
        }
        if(chunks.empty() || chunks.back().piece != v0.piece || chunks.back().n >= 12 || chunks.back().first + chunks.back().n * 3 != k)
        {
            PopChunk c;
            c.piece = v0.piece;
            c.first = k;
            chunks.pushBack(c);
        }
        PopChunk& c = chunks.back();
        c.n++;
        for(int e = 0; e < 3; e++)
        {
            c.lo = glm::min(c.lo, at(posed, at(s.tris, k + e)));
            c.hi = glm::max(c.hi, at(posed, at(s.tris, k + e)));
        }
    }
    // Rays out of each vertex along its normal: the pieces over it, and the pieces it came through.
    constexpr float over = 9.f, past = 5.f;
    const int nb = count(b.p);
    za::Vector<uint16_t> front(static_cast<za::SizeT>(nb), uint16_t{0}), back(static_cast<za::SizeT>(nb), uint16_t{0});
    za::Vector<float> backT(static_cast<za::SizeT>(nb * NumPieces), 1e9f);
    for(int v = 0; v < nb; v++)
    {
        const glm::vec3 o = at(bodyP, v), d = at(bodyN, v);
        const glm::vec3 e0 = o + d * over, e1 = o - d * past;
        const glm::vec3 lo = glm::min(e0, e1), hi = glm::max(e0, e1);
        for(const PopChunk& c : chunks)
        {
            if(c.lo.x > hi.x || c.lo.y > hi.y || c.lo.z > hi.z || c.hi.x < lo.x || c.hi.y < lo.y || c.hi.z < lo.z)
            {
                continue;
            }
            for(int t = 0; t < c.n; t++)
            {
                const int k = c.first + t * 3;
                float x = 0.f;
                if(rayTriT(o, d, at(posed, at(s.tris, k)), at(posed, at(s.tris, k + 1)), at(posed, at(s.tris, k + 2)), x))
                {
                    if(x >= 0.f && x <= over)
                    {
                        at(front, v) = static_cast<uint16_t>(at(front, v) | (1u << static_cast<unsigned>(c.piece)));
                    }
                    else if(x < 0.f && x >= -past)
                    {
                        at(back, v) = static_cast<uint16_t>(at(back, v) | (1u << static_cast<unsigned>(c.piece)));
                        at(backT, v * NumPieces + c.piece) = za::min(at(backT, v * NumPieces + c.piece), -x);
                    }
                }
            }
        }
        // (the plate behind it only if it's reached inside the body: past the skin's far side, the vertex is merely in front of it, a forearm before a chest)
        if(at(back, v))
        {
            float exit = 1e9f;
            for(int k = 0; k + 2 < count(b.tris); k += 3)
            {
                const int i0 = at(b.tris, k), i1 = at(b.tris, k + 1), i2 = at(b.tris, k + 2);
                float x = 0.f;
                if(i0 != v && i1 != v && i2 != v && rayTriT(o, -d, at(bodyP, i0), at(bodyP, i1), at(bodyP, i2), x) && x > 1e-3f)
                {
                    exit = za::min(exit, x);
                }
            }
            for(int p = 0; p < NumPieces; p++)
            {
                if(at(backT, v * NumPieces + p) >= exit)
                {
                    at(back, v) = static_cast<uint16_t>(at(back, v) & ~(1u << static_cast<unsigned>(p)));
                }
            }
        }
    }
    // The skin's edges through a surface.
    EdgeMap em;
    em.init(count(b.tris));
    za::Vector<uint16_t> edgeHits;
    za::Vector<int> edgeGroup;
    for(int k = 0; k + 2 < count(b.tris); k += 3)
    {
        for(int e = 0; e < 3; e++)
        {
            const int i0 = at(b.tris, k + e), i1 = at(b.tris, k + (e + 1) % 3);
            bool fresh = false;
            int& val = em.get(i0, i1, fresh);
            if(!fresh)
            {
                continue;
            }
            val = 0;
            const glm::vec3 o = at(bodyP, i0), d = at(bodyP, i1) - o;
            const glm::vec3 lo = glm::min(o, at(bodyP, i1)), hi = glm::max(o, at(bodyP, i1));
            uint16_t hits = 0;
            for(const PopChunk& c : chunks)
            {
                if(c.lo.x > hi.x || c.lo.y > hi.y || c.lo.z > hi.z || c.hi.x < lo.x || c.hi.y < lo.y || c.hi.z < lo.z)
                {
                    continue;
                }
                for(int t = 0; t < c.n; t++)
                {
                    const int j = c.first + t * 3;
                    float x = 0.f;
                    if(rayTriT(o, d, at(posed, at(s.tris, j)), at(posed, at(s.tris, j + 1)), at(posed, at(s.tris, j + 2)), x) && x >= 0.f && x <= 1.f)
                    {
                        hits = static_cast<uint16_t>(hits | (1u << static_cast<unsigned>(c.piece)));
                    }
                }
            }
            edgeHits.pushBack(hits);
            edgeGroup.pushBack(at(group, i0) != 5 ? at(group, i0) : at(group, i1));
        }
    }
    for(int st = 0; st < numPopStates; st++)
    {
        PopTally& tally = out.states[st];
        const unsigned mask = static_cast<unsigned>(popStates[st].mask);
        for(int v = 0; v < nb; v++)
        {
            if((at(back, v) & mask) && !(at(front, v) & mask))
            {
                if(popListMask && st == 0)
                {
                    const unsigned m = static_cast<unsigned>(popListMask);
                    if((at(back, v) & m) && !(at(front, v) & m))
                    {
                        const int hb = heaviest(at(b.w, v));
                        float past = 1e9f;
                        for(int p = 0; p < NumPieces; p++)
                        {
                            past = ((m >> static_cast<unsigned>(p)) & 1u) ? za::min(past, at(backT, v * NumPieces + p)) : past;
                        }
                        Con_Printf("agpop:   through %s (%s %.2f) behind %03x by %.2f at rest %.1f %.1f %.1f\n", hb < rig.numBones ? rig.bones[hb].name : "?", "w", static_cast<double>(at(b.w, v).w[hb]), at(back, v) & m,
                            static_cast<double>(past), static_cast<double>(at(b.p, v).x), static_cast<double>(at(b.p, v).y), static_cast<double>(at(b.p, v).z));
                    }
                }
                tally.through[at(group, v)]++;
                float least = 1e9f;
                for(int p = 0; p < NumPieces; p++)
                {
                    least = ((mask >> static_cast<unsigned>(p)) & 1u) && (at(back, v) & (1u << static_cast<unsigned>(p))) ? za::min(least, at(backT, v * NumPieces + p)) : least;
                }
                tally.deepest = za::max(tally.deepest, least);
            }
        }
        for(int e = 0; e < count(edgeHits); e++)
        {
            tally.cross[at(edgeGroup, e)] += (at(edgeHits, e) & mask) ? 1 : 0;
        }
    }
    // Plates inside the body: from the middle of a face out, the skin's inside met first.
    int firstVert = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const int end = firstVert + s.pieceVerts[piece];
        for(int vi = firstVert; piece != Helmet && vi + plateVerts <= end; vi += plateVerts)
        {
            const glm::vec3 mid = (at(posed, vi) + at(posed, vi + 1) + at(posed, vi + 2) + at(posed, vi + 3)) * 0.25f;
            const glm::vec3 n = geom::safeNormalize(popSkin(pose, at(s.verts, vi).w, at(s.verts, vi).n, 0.f), glm::vec3{0.f, 0.f, 1.f}, 1e-6f);
            float nearest = 6.f;
            bool inside = false;
            for(int k = 0; k + 2 < count(b.tris); k += 3)
            {
                const glm::vec3& a = at(bodyP, at(b.tris, k));
                const glm::vec3& c = at(bodyP, at(b.tris, k + 1));
                const glm::vec3& d = at(bodyP, at(b.tris, k + 2));
                float x = 0.f;
                if(rayTriT(mid, n, a, c, d, x) && x > 0.f && x < nearest)
                {
                    nearest = x;
                    inside = glm::dot(glm::cross(c - a, d - a), n) > 0.f;
                }
            }
            if(popListMask && inside && ((popListMask >> piece) & 1))
            {
                const PlateGrid& g = s.tubes[piece].grid;
                const int idx = (vi - firstVert) / plateVerts;
                Con_Printf("agpop:   inside %s row %d col %d (%d x %d), %.2f to the skin out, at rest %.1f %.1f %.1f\n", pieceNames[piece], g.cols ? idx / g.cols : 0, g.cols ? idx % g.cols : 0, g.rows, g.cols, static_cast<double>(nearest),
                    static_cast<double>(at(s.verts, vi).p.x), static_cast<double>(at(s.verts, vi).p.y), static_cast<double>(at(s.verts, vi).p.z));
            }
            out.buried[popGroupOf(piece)] += inside ? 1 : 0;
            out.plates[popGroupOf(piece)]++;
        }
        firstVert = end;
    }
    out.detached = za::max(out.detached, popDetached(s, posed));
    out.poses++;
}

void popPrint(const char* m, const char* what, const PopTotals& t)
{
    for(int st = 0; st < numPopStates; st++)
    {
        const PopTally& x = t.states[st];
        Con_Printf("agpop: %s %s %s: through torso %d uarm %d farm %d thigh %d shin %d other %d; cross %d %d %d %d %d %d; deepest %.2f\n", m, what, popStates[st].name, x.through[0], x.through[1], x.through[2], x.through[3], x.through[4],
            x.through[5], x.cross[0], x.cross[1], x.cross[2], x.cross[3], x.cross[4], x.cross[5], static_cast<double>(x.deepest));
    }
    Con_Printf("agpop: %s %s: %d poses, plates inside the body torso %d/%d uarm %d/%d farm %d/%d thigh %d/%d shin %d/%d; detached %d\n", m, what, t.poses, t.buried[0], t.plates[0], t.buried[1], t.plates[1], t.buried[2], t.plates[2], t.buried[3],
        t.plates[3], t.buried[4], t.plates[4], t.detached);
}

// The plates' standoff at rest: from each face's middle in along its normal to the skin. Mean and most, per group.
void popBulk(const Shell& s, float* mean, float* most)
{
    float sum[popGroups]{};
    int n[popGroups]{};
    int firstVert = 0;
    for(int piece = 0; piece < NumPieces; piece++)
    {
        const int end = firstVert + s.pieceVerts[piece];
        for(int vi = firstVert; piece != Helmet && vi + plateVerts <= end; vi += plateVerts)
        {
            const glm::vec3 mid = (at(s.verts, vi).p + at(s.verts, vi + 1).p + at(s.verts, vi + 2).p + at(s.verts, vi + 3).p) * 0.25f;
            float nearest = 9.f;
            bool hit = false;
            for(int k = 0; k + 2 < count(s.body.tris); k += 3)
            {
                float x = 0.f;
                if(rayTriT(mid, -at(s.verts, vi).n, at(s.body.p, at(s.body.tris, k)), at(s.body.p, at(s.body.tris, k + 1)), at(s.body.p, at(s.body.tris, k + 2)), x) && x > 0.f && x < nearest)
                {
                    nearest = x;
                    hit = true;
                }
            }
            if(hit)
            {
                sum[popGroupOf(piece)] += nearest;
                n[popGroupOf(piece)]++;
                most[popGroupOf(piece)] = za::max(most[popGroupOf(piece)], nearest);
            }
        }
        firstVert = end;
    }
    for(int g = 0; g < popGroups; g++)
    {
        mean[g] = n[g] ? sum[g] / static_cast<float>(n[g]) : 0.f;
    }
}

void popPoseBone(const rig2::Rig& rig, int b, const PopDef& def, LookPose& pose)
{
    if(pose.done[b])
    {
        return;
    }
    pose.done[b] = true;
    const int parent = rig.bones[b].parent;
    glm::mat4 up{1.f};
    if(parent >= 0 && parent < rig.numBones)
    {
        popPoseBone(rig, parent, def, pose);
        up = pose.m[parent];
    }
    const char* name = rig.bones[b].name;
    const auto is = [&](const char* prefix) { return !strncmp(name, prefix, strlen(prefix)); };
    const float deg = is("forearm") ? def.forearm : is("shin") ? def.shin : is("thigh") ? def.thigh : is("upperarm") ? def.upperarm : is("spine") ? def.spine : is("chest") ? def.spine * 0.8f : 0.f;
    const float a = glm::radians(deg);
    glm::mat4 turn{1.f};
    turn[0][0] = za::cos(a);
    turn[0][2] = -za::sin(a);
    turn[2][0] = za::sin(a);
    turn[2][2] = za::cos(a);
    glm::mat4 toPivot{1.f}, fromPivot{1.f};
    toPivot[3] = glm::vec4{-rig.bones[b].pivot, 1.f};
    fromPivot[3] = glm::vec4{rig.bones[b].pivot, 1.f};
    pose.m[b] = up * fromPivot * turn * toPivot;
}

// The live sampler's sums, per shell's model (vr_agarmor_pop_sample, vr_agarmor_pop_report).
constexpr int popLiveMost = 16;
struct PopLive
{
    za::String model;
    PopTotals totals;
};
PopLive popLive[popLiveMost];
int popLiveCount = 0;

} // namespace

// vr_agarmor_pop_check [model]: the body against the plates on every wearer and on your default body: the poses (rest, elbows and
// knees bent, arms up, a crouch) against each set of pieces worn (all, one limb's part alone...). Ends on "agarmor pop: N passed, M failed".
void popCheck_f()
{
    constexpr const char* all[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl", "progs/vrknight.mdl", "progs/vrhknight.mdl", "progs/vrbody_bare.mdl"};
    int passed = 0, failed = 0;
    const auto check = [&](bool ok, const char* what) {
        Con_Printf("agpop: %s: %s\n", ok ? "PASS" : "FAIL", what);
        (ok ? passed : failed)++;
    };
    for(const char* m : all)
    {
        if(Cmd_Argc() > 1 && strcmp(Cmd_Argv(1), m))
        {
            continue;
        }
        qmodel_t* src = Mod_ForName(m, false);
        const Shell* sp = src ? shellFor(src) : nullptr;
        const rig2::Rig* rig = src ? rigOfShell(src) : nullptr;
        if(!sp || !rig)
        {
            check(false, va("%s: no plates", m));
            continue;
        }
        const double t0 = Sys_DoubleTime();
        for(int p = PauldronL; p < NumPieces; p += 2)
        {
            const PlateTube& tb = sp->tubes[p];
            Con_Printf("agpop: %s %s: rows %d, %.1f .. %.1f along a bone %.1f long\n", m, pieceNames[p], tb.grid.rows, static_cast<double>(tb.grid.hTop), static_cast<double>(tb.grid.hBottom), static_cast<double>(tb.length));
        }
        za::Vector<PopTotals> perPose(static_cast<za::SizeT>(numPopDefs));
        PopTotals sum;
        for(int d = 0; d < numPopDefs; d++)
        {
            LookPose pose;
            for(glm::mat4& x : pose.m)
            {
                x = glm::mat4{1.f};
            }
            for(int b = 0; b < rig->numBones; b++)
            {
                popPoseBone(*rig, b, popDefs[d], pose);
            }
            PopTotals& one = at(perPose, d);
            popListMask = Cmd_Argc() > 3 && !strcmp(Cmd_Argv(2), popDefs[d].name) ? atoi(Cmd_Argv(3)) : 0;
            popPose(*sp, *rig, pose.m, one);
            popListMask = 0;
            const PopTally& x = one.states[0];
            Con_Printf("agpop: %s pose %s, all worn: through %d %d %d %d %d %d; cross %d %d %d %d %d %d; inside %d %d %d %d %d; detached %d\n", m, popDefs[d].name, x.through[0], x.through[1], x.through[2], x.through[3], x.through[4], x.through[5],
                x.cross[0], x.cross[1], x.cross[2], x.cross[3], x.cross[4], x.cross[5], one.buried[0], one.buried[1], one.buried[2], one.buried[3], one.buried[4], one.detached);
            for(int st = 0; st < numPopStates; st++)
            {
                for(int g = 0; g < popGroups; g++)
                {
                    sum.states[st].through[g] += one.states[st].through[g];
                    sum.states[st].cross[g] += one.states[st].cross[g];
                }
                sum.states[st].deepest = za::max(sum.states[st].deepest, one.states[st].deepest);
            }
            for(int g = 0; g < popGroups; g++)
            {
                sum.buried[g] += one.buried[g];
                sum.plates[g] += one.plates[g];
            }
            sum.detached = za::max(sum.detached, one.detached);
            sum.poses++;
        }
        popPrint(m, "sum", sum);
        float mean[popGroups]{}, most[popGroups]{};
        popBulk(*sp, mean, most);
        Con_Printf("agpop: %s bulk (face off skin, mean/most): torso %.2f/%.2f uarm %.2f/%.2f farm %.2f/%.2f thigh %.2f/%.2f shin %.2f/%.2f; %d triangles; %.0f ms\n", m, static_cast<double>(mean[0]), static_cast<double>(most[0]), static_cast<double>(mean[1]),
            static_cast<double>(most[1]), static_cast<double>(mean[2]), static_cast<double>(most[2]), static_cast<double>(mean[3]), static_cast<double>(most[3]), static_cast<double>(mean[4]), static_cast<double>(most[4]), count(sp->tris) / 3, (Sys_DoubleTime() - t0) * 1000.0);
        check(sum.detached == 0, va("%s: no plate off its neighbours in any pose (%d)", m, sum.detached));
        float worstBulk = 0.f;
        for(int g = 0; g < 5; g++)
        {
            worstBulk = za::max(worstBulk, mean[g]);
        }
        check(worstBulk <= popBulkMost, va("%s: no bulk: every region's plates on average within %.1f units of the skin (%.2f)", m, static_cast<double>(popBulkMost), static_cast<double>(worstBulk)));
    }
    qvr::testkit::verdict("agarmor pop", passed, failed);
}

// vr_agarmor_pop_sample: every armoured body drawn now, in the pose it's drawn in, added to the sums.
void popSample_f()
{
    for(int k = 0; k < wornCount(); k++)
    {
        int num = 0;
        const Shell* s = wornAt(k, num);
        qmodel_t* src = s ? Mod_ForName(s->model.cStr(), false) : nullptr;
        const rig2::Rig* rig = src ? rigOfShell(src) : nullptr;
        if(!rig)
        {
            continue;
        }
        glm::mat4 m[rig2::maxBones];
        bool drawn = false;
        for(int b = 0; b < rig->numBones; b++)
        {
            glm::quat rot;
            glm::vec3 pos;
            m[b] = glm::mat4{1.f};
            if(rig2::drawnBone(num, b, rot, pos))
            {
                m[b] = glm::mat4_cast(rot);
                m[b][3] = glm::vec4{pos, 1.f};
                drawn = true;
            }
        }
        if(!drawn)
        {
            continue;
        }
        int slot = 0;
        while(slot < popLiveCount && strcmp(popLive[slot].model.cStr(), s->model.cStr()))
        {
            slot++;
        }
        if(slot >= popLiveMost)
        {
            continue;
        }
        if(slot == popLiveCount)
        {
            popLive[slot].model = s->model;
            popLive[slot].totals = PopTotals{};
            popLiveCount++;
        }
        popPose(*s, *rig, m, popLive[slot].totals);
    }
}

// vr_agarmor_pop_report: the sums so far, per model, and started again.
void popReport_f()
{
    for(int i = 0; i < popLiveCount; i++)
    {
        popPrint(popLive[i].model.cStr(), "live", popLive[i].totals);
    }
    Con_Printf("agpop: live report done (%d models)\n", popLiveCount);
    popLiveCount = 0;
}

} // namespace detail

} // namespace qvr::agarmor
