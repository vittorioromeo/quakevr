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
    // Closed: every edge of each piece's shell used twice.
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
    verdict(restDeepest < za::max(vr_agarmor_thick.value, 0.05f) && restPlate * 5 < outer,
        "the rest pose: no body vertex through a plate's thickness, under a fifth of the plates hidden in the body (under the pads and arms)");
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

} // namespace detail

} // namespace qvr::agarmor
