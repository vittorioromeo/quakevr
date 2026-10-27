// vr_rig2_check.cpp -- vr_rig2_check (vr_rig2.hpp): the authored rig measured through its whole range of motion, beside
// the old derived rig (vr_ragdoll.cpp, as its ragdolls draw it: each vertex on its bone, an armed model's arms on their
// skin's blend) moved the same way (each old bone as its namesake moves). Ends on "rig2 check: N passed, M failed".
//
// Poses: every joint alone through its range (a ball's swing round its ellipse's edge and half way, at its twist's ends
// and none; a hinge in 30 steps), each joint with its child (the parent round its edge, the child at its ends) and 1000
// ragdoll-like random poses within every limit (a fixed seed). Measured in each:
// - see-through: lines through each joint's centre and each bone's middle from 14 directions: the first triangle a line
//   meets from outside must face it (a closed mesh seen through a hole or an inverted fold shows its back first, and
//   back faces are culled: the floor shows through);
// - flipped triangles (facing against their bone's turn) and collapsed ones (under a tenth of their area);
// - folds: the angle between two triangles along an edge (0 flat): past 150 degrees and 40 more than at rest;
// - stretch: an edge's length against its rest length;
// - the hinges' bend from the posed bones (the knee's shin behind it: below 0);
// - a coarse self-intersection count near the joint swept (an edge through a triangle not its own, more than at rest).

#include "vr_rig2.hpp"
#include "vr_agarms.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hitreact.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rigpose.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdlib.h>
#include <string.h>

namespace qvr::rig2
{

namespace
{

constexpr float pi = 3.14159265358979f;
constexpr float deg = pi / 180.f;
constexpr float foldLimit = 150.f;     // degrees between two triangles along a ridge (0 flat; 180 folded back onto itself)
constexpr float foldGrowth = 60.f;     // ... more than at rest
constexpr float collapseRatio = 0.1f;  // a triangle's area against its rest area
constexpr float stretchHigh = 2.5f, stretchLow = 0.3f;
constexpr float rayReach = 80.f;
constexpr int randomPoses = 1000;

struct Edge
{
    int a{0}, b{0};   // .mdl vertices
    int t0{-1}, t1{-1}; // its two triangles
    int o1{-1};         // the second's corner off the edge
};

// The model's surface: rest vertices, triangles wound outwards, welded edges.
struct Surface
{
    int nv{0};
    za::Vector<glm::vec3> rest;
    za::Vector<int> tris; // 3 a triangle, outward (cross(b - a, c - a))
    za::Vector<int> weld; // per vertex: its weld's first vertex
    za::Vector<Edge> edges;
    za::Vector<glm::vec3> restN;
    za::Vector<float> restArea, restFold, restLen;
    za::Vector<uint8_t> sliver; // per triangle
};

// One rig's measures over a set of poses (a joint's sweeps, its pairs, the random poses).
struct Tally
{
    int poses{0};
    int seeThrough{0}, seeThroughPoses{0};
    int flipped{0}, collapsed{0};
    float foldMax{0.f}, foldGrowthMax{0.f}, creaseMax{0.f};
    int folds{0};
    float stretchMax{1.f}, stretchMin{1.f};
    float bendMin{1e9f}, bendMax{-1e9f};
    int selfX{0};
    char worst[96]{}; // the worst pose's description
    [[nodiscard]] bool clean() const
    {
        return seeThrough == 0 && flipped == 0 && collapsed == 0 && folds == 0 && stretchMax <= stretchHigh && stretchMin >= stretchLow;
    }
    void add(const Tally& o)
    {
        poses += o.poses;
        seeThrough += o.seeThrough;
        seeThroughPoses += o.seeThroughPoses;
        flipped += o.flipped;
        collapsed += o.collapsed;
        folds += o.folds;
        foldMax = za::max(foldMax, o.foldMax);
        foldGrowthMax = za::max(foldGrowthMax, o.foldGrowthMax);
        creaseMax = za::max(creaseMax, o.creaseMax);
        stretchMax = za::max(stretchMax, o.stretchMax);
        stretchMin = za::min(stretchMin, o.stretchMin);
        bendMin = za::min(bendMin, o.bendMin);
        bendMax = za::max(bendMax, o.bendMax);
        selfX = za::max(selfX, o.selfX);
    }
};

// The check's state (the main thread's; made per run)
struct CheckState
{
    Surface s;
    const Rig* rig{nullptr};
    const ragdoll::Rig* old{nullptr};
    za::Vector<int> oldBone;            // per vertex: the old rig's bone
    za::Vector<Weights> oldWeights;     // per vertex: the old rig's (as its ragdolls draw it)
    int oldMap[ragdoll::maxBones]{};    // old bone -> rig2 bone (its namesake)
    za::Vector<glm::vec3> posed, n;
    za::Vector<float> area;
    za::Vector<int> dom;                // per triangle: its heaviest bone (new rig)
    za::Vector<int> domOld;             // ... the old rig's
    za::Vector<glm::vec3> pointsScratch;
    int restSeeThrough{0};
    int restSelfX[maxBones]{};
    int restSelfXOld[maxBones]{};
    int detail{0}; // lines of detail left to print (vr_rig2_check <model> <lines>)
    const Stretches* stretch{nullptr}; // the pose measured's IK stretch (a recorded armed one's; the skin's)
    const qmodel_t* model{nullptr}; // made for (its rig's generation)
    int generation{-1};
};
CheckState cs;

[[nodiscard]] glm::vec3 mdlVert(const aliashdr_t* hdr, int pose, int v)
{
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                     static_cast<za::SizeT>(pose) * static_cast<za::SizeT>(hdr->numverts);
    const trivertx_t& t = tv[v];
    return glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
        t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
}

bool buildSurface(const aliashdr_t* hdr, Surface& s)
{
    s = Surface{};
    s.nv = hdr->numverts;
    s.rest.resize(static_cast<za::SizeT>(s.nv));
    for(int v = 0; v < s.nv; v++)
    {
        s.rest[static_cast<za::SizeT>(v)] = mdlVert(hdr, 0, v);
    }
    // Welds: the same place in every pose (the skin's seams' copies)
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    s.weld.resize(static_cast<za::SizeT>(s.nv));
    for(int v = 0; v < s.nv; v++)
    {
        s.weld[static_cast<za::SizeT>(v)] = v;
        for(int u = 0; u < v; u++)
        {
            if(s.weld[static_cast<za::SizeT>(u)] != u)
            {
                continue;
            }
            bool same = true;
            for(int p = 0; p < hdr->numposes && same; p++)
            {
                const trivertx_t& a = tv[static_cast<za::SizeT>(p) * static_cast<za::SizeT>(s.nv) + static_cast<za::SizeT>(u)];
                const trivertx_t& b = tv[static_cast<za::SizeT>(p) * static_cast<za::SizeT>(s.nv) + static_cast<za::SizeT>(v)];
                same = a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
            }
            if(same)
            {
                s.weld[static_cast<za::SizeT>(v)] = u;
                break;
            }
        }
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    double vol = 0.0;
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        int t[3];
        for(int k = 0; k < 3; k++)
        {
            t[k] = za::clamp(static_cast<int>(desc[idx[i + k]].vertindex), 0, s.nv - 1);
        }
        const int w0 = s.weld[static_cast<za::SizeT>(t[0])], w1 = s.weld[static_cast<za::SizeT>(t[1])], w2 = s.weld[static_cast<za::SizeT>(t[2])];
        if(w0 == w1 || w1 == w2 || w0 == w2)
        {
            continue;
        }
        const glm::vec3 &a = s.rest[static_cast<za::SizeT>(t[0])], &b = s.rest[static_cast<za::SizeT>(t[1])], &c = s.rest[static_cast<za::SizeT>(t[2])];
        if(glm::length(glm::cross(b - a, c - a)) < 1e-5f)
        {
            continue;
        }
        vol += static_cast<double>(glm::dot(a, glm::cross(b, c)));
        for(int k = 0; k < 3; k++)
        {
            s.tris.pushBack(t[k]);
        }
    }
    if(vol < 0.0)
    {
        for(za::SizeT i = 0; i + 2 < s.tris.size(); i += 3)
        {
            const int x = s.tris[i + 1];
            s.tris[i + 1] = s.tris[i + 2];
            s.tris[i + 2] = x;
        }
    }
    const int nt = static_cast<int>(s.tris.size() / 3);
    // Edges (welded)
    for(int t = 0; t < nt; t++)
    {
        for(int k = 0; k < 3; k++)
        {
            const int a = s.tris[static_cast<za::SizeT>(3 * t + k)], b = s.tris[static_cast<za::SizeT>(3 * t + (k + 1) % 3)];
            const int wa = s.weld[static_cast<za::SizeT>(a)], wb = s.weld[static_cast<za::SizeT>(b)];
            bool found = false;
            for(Edge& e : s.edges)
            {
                const int ea = s.weld[static_cast<za::SizeT>(e.a)], eb = s.weld[static_cast<za::SizeT>(e.b)];
                if((ea == wa && eb == wb) || (ea == wb && eb == wa))
                {
                    if(e.t1 < 0)
                    {
                        e.t1 = t;
                    }
                    found = true;
                    break;
                }
            }
            if(!found)
            {
                Edge e;
                e.a = a;
                e.b = b;
                e.t0 = t;
                s.edges.pushBack(e);
            }
        }
    }
    for(Edge& e : s.edges)
    {
        for(int k = 0; e.t1 >= 0 && k < 3; k++)
        {
            const int v = s.tris[static_cast<za::SizeT>(3 * e.t1 + k)];
            const int wv = s.weld[static_cast<za::SizeT>(v)];
            if(wv != s.weld[static_cast<za::SizeT>(e.a)] && wv != s.weld[static_cast<za::SizeT>(e.b)])
            {
                e.o1 = v;
            }
        }
        e.t1 = e.o1 >= 0 ? e.t1 : -1;
    }
    return nt > 0;
}

void triNormals(const Surface& s, const za::Vector<glm::vec3>& p, za::Vector<glm::vec3>& n, za::Vector<float>& area)
{
    const int nt = static_cast<int>(s.tris.size() / 3);
    n.resize(static_cast<za::SizeT>(nt));
    area.resize(static_cast<za::SizeT>(nt));
    for(int t = 0; t < nt; t++)
    {
        const glm::vec3& a = p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t)])];
        const glm::vec3 c = glm::cross(p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t + 1)])] - a,
            p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t + 2)])] - a);
        const float l = glm::length(c);
        area[static_cast<za::SizeT>(t)] = 0.5f * l;
        n[static_cast<za::SizeT>(t)] = l > 1e-9f ? c / l : glm::vec3{0.f};
    }
}

[[nodiscard]] float foldOf(const glm::vec3& n0, const glm::vec3& n1)
{
    return za::acos(za::clamp(glm::dot(n0, n1), -1.f, 1.f)) / deg;
}

void prepareRest(Surface& s)
{
    za::Vector<glm::vec3> n;
    za::Vector<float> a;
    triNormals(s, s.rest, n, a);
    s.restN = n;
    s.restArea = a;
    // Slivers (under a tenth of a square unit, or narrower than 0.15 units across): their flips and collapses not counted
    s.sliver.resize(a.size());
    for(za::SizeT t = 0; t < a.size(); t++)
    {
        float longest = 0.f;
        for(int k = 0; k < 3; k++)
        {
            longest = za::max(longest, glm::length(s.rest[static_cast<za::SizeT>(s.tris[3 * t + static_cast<za::SizeT>(k)])] -
                                                   s.rest[static_cast<za::SizeT>(s.tris[3 * t + static_cast<za::SizeT>((k + 1) % 3)])]));
        }
        s.sliver[t] = a[t] < 0.1f || 2.f * a[t] / za::max(longest, 1e-6f) < 0.15f ? 1 : 0;
    }
    s.restFold.resize(s.edges.size());
    s.restLen.resize(s.edges.size());
    for(za::SizeT i = 0; i < s.edges.size(); i++)
    {
        const Edge& e = s.edges[i];
        s.restFold[i] = e.t1 >= 0 ? foldOf(n[static_cast<za::SizeT>(e.t0)], n[static_cast<za::SizeT>(e.t1)]) : 0.f;
        s.restLen[i] = glm::length(s.rest[static_cast<za::SizeT>(e.a)] - s.rest[static_cast<za::SizeT>(e.b)]);
    }
}

// Whether the ray o + t d (t in (0, reach)) first meets a triangle's back: -1 no hit, 0 a front first, 1 a back first.
[[nodiscard]] int firstHit(const Surface& s, const za::Vector<glm::vec3>& p, const glm::vec3& o, const glm::vec3& d, float reach)
{
    float best = reach;
    int kind = -1;
    const int nt = static_cast<int>(s.tris.size() / 3);
    for(int t = 0; t < nt; t++)
    {
        const glm::vec3& a = p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t)])];
        const glm::vec3 e1 = p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t + 1)])] - a;
        const glm::vec3 e2 = p[static_cast<za::SizeT>(s.tris[static_cast<za::SizeT>(3 * t + 2)])] - a;
        const glm::vec3 q = glm::cross(d, e2);
        const float det = glm::dot(e1, q);
        if(za::abs(det) < 1e-9f)
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 sv = o - a;
        const float u = glm::dot(sv, q) * inv;
        if(u < 0.f || u > 1.f)
        {
            continue;
        }
        const glm::vec3 r = glm::cross(sv, e1);
        const float v = glm::dot(d, r) * inv;
        if(v < 0.f || u + v > 1.f)
        {
            continue;
        }
        const float tt = glm::dot(e2, r) * inv;
        if(tt > 1e-4f && tt < best)
        {
            best = tt;
            kind = det > 0.f ? 0 : 1; // (det > 0: the ray against the outward normal: its front)
        }
    }
    return kind;
}

// Whether segment p0..p1 crosses triangle abc (away from its ends).
[[nodiscard]] bool segTri(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 d = p1 - p0, e1 = b - a, e2 = c - a;
    const glm::vec3 q = glm::cross(d, e2);
    const float det = glm::dot(e1, q);
    if(za::abs(det) < 1e-9f)
    {
        return false;
    }
    const float inv = 1.f / det;
    const glm::vec3 sv = p0 - a;
    const float u = glm::dot(sv, q) * inv;
    if(u < 0.f || u > 1.f)
    {
        return false;
    }
    const glm::vec3 r = glm::cross(sv, e1);
    const float v = glm::dot(d, r) * inv;
    if(v < 0.f || u + v > 1.f)
    {
        return false;
    }
    const float t = glm::dot(e2, r) * inv;
    return t > 0.01f && t < 0.99f;
}

// The 14 directions (the cube's faces and corners), a little off the axes (no line along an edge).
[[nodiscard]] glm::vec3 rayDir(int i)
{
    constexpr float dirs[14][3] = {{1, 0.03f, 0.05f}, {-1, 0.04f, -0.02f}, {0.02f, 1, 0.05f}, {-0.03f, -1, 0.04f}, {0.05f, 0.02f, 1},
        {0.04f, -0.03f, -1}, {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1}, {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1}};
    return glm::normalize(glm::vec3{dirs[i][0], dirs[i][1] * 1.07f, dirs[i][2] * 0.93f});
}

// The old rig's vertices for rig2 bones' motions F2 (each old bone as its namesake).
void skinOld(const rigpose::Rigid* F2, za::Vector<glm::vec3>& out)
{
    rigpose::Rigid Fo[ragdoll::maxBones];
    for(int o = 0; o < cs.old->numBones; o++)
    {
        Fo[o] = F2[cs.oldMap[o]];
    }
    out.resize(static_cast<za::SizeT>(cs.s.nv));
    for(int v = 0; v < cs.s.nv; v++)
    {
        const Weights& w = cs.oldWeights[static_cast<za::SizeT>(v)];
        glm::vec3 q{0.f};
        for(int i = 0; i < w.n; i++)
        {
            q += rigpose::apply(Fo[w.bone[i]], cs.s.rest[static_cast<za::SizeT>(v)]) * w.w[i];
        }
        out[static_cast<za::SizeT>(v)] = q;
    }
}


void describe(const Rig& rig, int v)
{
    Con_Printf(" v%d(", v);
    if(v < static_cast<int>(rig.weights.size()))
    {
        const Weights& w = rig.weights[static_cast<za::SizeT>(v)];
        for(int i = 0; i < w.n; i++)
        {
            Con_Printf("%s%s %.2f", i ? " " : "", rig.bones[w.bone[i]].name, w.w[i]);
        }
    }
    const glm::vec3& r = cs.s.rest[static_cast<za::SizeT>(v)];
    Con_Printf(" @%.1f %.1f %.1f)", r.x, r.y, r.z);
}

// The measure of one pose. `focus`: the joint swept (its rays and its self-intersections; -1 all joints' rays).
void measure(const rigpose::Rigid* F2, bool old, int focus, int focus2, Tally& t, const char* what)
{
    const Rig& rig = *cs.rig;
    za::Vector<glm::vec3>& p = cs.posed;
    if(old)
    {
        skinOld(F2, p);
    }
    else
    {
        p.resize(static_cast<za::SizeT>(cs.s.nv));
        skin(rig, F2, cs.s.rest.data(), cs.s.nv, p.data(), vr_rig2_dq.value != 0.f, cs.stretch);
    }
    triNormals(cs.s, p, cs.n, cs.area);
    t.poses++;
    const int nt = static_cast<int>(cs.s.tris.size() / 3);
    int flips = 0, collapses = 0;
    for(int i = 0; i < nt; i++)
    {
        const za::SizeT ti = static_cast<za::SizeT>(i);
        if(cs.s.sliver[ti])
        {
            continue; // (a sliver: too small or thin to see turned over)
        }
        // Flipped: against its corners' bones' turns (blended by their weights) of its rest normal
        glm::vec3 ref{0.f};
        for(int k = 0; k < 3; k++)
        {
            const int v = cs.s.tris[3 * ti + static_cast<za::SizeT>(k)];
            const Weights* w = old ? &cs.oldWeights[static_cast<za::SizeT>(v)]
                                   : v < static_cast<int>(rig.weights.size()) ? &rig.weights[static_cast<za::SizeT>(v)] : nullptr;
            for(int j = 0; w && j < w->n; j++)
            {
                ref += F2[old ? cs.oldMap[w->bone[j]] : w->bone[j]].rot * cs.s.restN[ti] * w->w[j];
            }
        }
        const bool flip = glm::dot(cs.n[ti], ref) < 0.f, collapse = cs.area[ti] < collapseRatio * cs.s.restArea[ti];
        flips += flip ? 1 : 0;
        collapses += collapse ? 1 : 0;
        if((flip || collapse) && !old && cs.detail > 0)
        {
            cs.detail--;
            Con_Printf("  [%s] %s tri %d:", what, flip ? "flipped" : "collapsed", i);
            for(int k = 0; k < 3; k++)
            {
                describe(rig, cs.s.tris[3 * ti + static_cast<za::SizeT>(k)]);
            }
            Con_Printf("\n");
        }
    }
    t.flipped += flips;
    t.collapsed += collapses;
    for(za::SizeT i = 0; i < cs.s.edges.size(); i++)
    {
        const Edge& e = cs.s.edges[i];
        const float len = glm::length(p[static_cast<za::SizeT>(e.a)] - p[static_cast<za::SizeT>(e.b)]);
        const float r = cs.s.restLen[i] > 0.05f ? len / cs.s.restLen[i] : 1.f;
        t.stretchMax = za::max(t.stretchMax, r);
        t.stretchMin = za::min(t.stretchMin, r);
        if(e.t1 < 0)
        {
            continue;
        }
        const glm::vec3& n0 = cs.n[static_cast<za::SizeT>(e.t0)];
        const float f = foldOf(n0, cs.n[static_cast<za::SizeT>(e.t1)]);
        // (convex: the far corner of its second triangle behind the first's plane, a ridge seen from outside; else a
        // crease, the inside of a bend)
        const bool convex = glm::dot(n0, p[static_cast<za::SizeT>(e.o1)] - p[static_cast<za::SizeT>(e.a)]) < 0.f;
        if(!convex)
        {
            t.creaseMax = za::max(t.creaseMax, f);
            continue;
        }
        t.foldMax = za::max(t.foldMax, f);
        t.foldGrowthMax = za::max(t.foldGrowthMax, f - cs.s.restFold[i]);
        const bool fold = f > foldLimit && f - cs.s.restFold[i] > foldGrowth;
        t.folds += fold ? 1 : 0;
        if(fold && !old && cs.detail > 0)
        {
            cs.detail--;
            Con_Printf("  [%s] fold %.0f (rest %.0f) edge", what, f, cs.s.restFold[i]);
            describe(rig, e.a);
            describe(rig, e.b);
            Con_Printf(" |");
            describe(rig, e.o1);
            Con_Printf("\n");
        }
    }
    // See-through: lines through the joints' centres and the bones' middles
    za::Vector<glm::vec3>& pts = cs.pointsScratch;
    pts.clear();
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        if(bone.joint == Joint::Root || bone.joint == Joint::Fixed)
        {
            continue;
        }
        const bool near = focus < 0 || b == focus || b == focus2 || bone.parent == focus || b == rig.bones[za::max(focus, 0)].parent;
        if(!near)
        {
            continue;
        }
        pts.pushBack(rigpose::apply(F2[b], bone.pivot));
        if(focus >= 0)
        {
            pts.pushBack(rigpose::apply(F2[b], (bone.pivot + bone.end) * 0.5f));
        }
    }
    const int nDirs = focus < 0 ? 6 : 14;
    int holes = 0;
    for(const glm::vec3& c : pts)
    {
        for(int i = 0; i < nDirs; i++)
        {
            const glm::vec3 d = rayDir(i);
            if(firstHit(cs.s, p, c + d * rayReach, -d, 2.f * rayReach) != 1)
            {
                continue;
            }
            // (a hole only if lines a twentieth of a unit either side see it too: not a line through the model's own
            // sub-0.01 crack, check_mdl_holes.py's, or along an edge)
            glm::vec3 side = glm::cross(d, glm::vec3{0.f, 0.f, 1.f});
            side = glm::length(side) < 0.1f ? glm::vec3{1.f, 0.f, 0.f} : glm::normalize(side);
            const glm::vec3 up = glm::cross(side, d);
            const int also = (firstHit(cs.s, p, c + d * rayReach + side * 0.05f, -d, 2.f * rayReach) == 1 ? 1 : 0) +
                             (firstHit(cs.s, p, c + d * rayReach + up * 0.05f, -d, 2.f * rayReach) == 1 ? 1 : 0);
            holes += also >= 1 ? 1 : 0;
        }
    }
    t.seeThrough += holes;
    t.seeThroughPoses += holes > 0 ? 1 : 0;
    // The hinges' bends from the posed bones
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        if(bone.joint != Joint::Hinge || (focus >= 0 && b != focus && b != focus2))
        {
            continue;
        }
        const Bone& par = rig.bones[bone.parent];
        const glm::vec3 pd = F2[bone.parent].rot * glm::normalize(bone.pivot - par.pivot);
        const glm::vec3 cd = F2[b].rot * glm::normalize(bone.end - bone.pivot);
        const glm::vec3 ax = F2[bone.parent].rot * glm::normalize(bone.axis);
        const glm::vec3 pp = pd - ax * glm::dot(pd, ax), cc = cd - ax * glm::dot(cd, ax);
        const float bend = za::atan2(glm::dot(glm::cross(pp, cc), ax), glm::dot(pp, cc)) / deg;
        t.bendMin = za::min(t.bendMin, bend);
        t.bendMax = za::max(t.bendMax, bend);
    }
    // Coarse self-intersection near the joint swept
    if(focus >= 0)
    {
        const int par = rig.bones[focus].parent;
        const auto zone = [&](int v) {
            if(old)
            {
                const Weights& w = cs.oldWeights[static_cast<za::SizeT>(v)];
                for(int i = 0; i < w.n; i++)
                {
                    const int m = cs.oldMap[w.bone[i]];
                    if(m == focus || m == par)
                    {
                        return true;
                    }
                }
                return false;
            }
            return weightOn(rig, v, focus) > 0.f || (par >= 0 && weightOn(rig, v, par) > 0.f);
        };
        int count = 0;
        for(int i = 0; i < nt; i++)
        {
            const int a = cs.s.tris[static_cast<za::SizeT>(3 * i)], b = cs.s.tris[static_cast<za::SizeT>(3 * i + 1)], c = cs.s.tris[static_cast<za::SizeT>(3 * i + 2)];
            if(!zone(a) && !zone(b) && !zone(c))
            {
                continue;
            }
            const int wa = cs.s.weld[static_cast<za::SizeT>(a)], wb = cs.s.weld[static_cast<za::SizeT>(b)], wc = cs.s.weld[static_cast<za::SizeT>(c)];
            for(const Edge& e : cs.s.edges)
            {
                const int ea = cs.s.weld[static_cast<za::SizeT>(e.a)], eb = cs.s.weld[static_cast<za::SizeT>(e.b)];
                if(ea == wa || ea == wb || ea == wc || eb == wa || eb == wb || eb == wc || (!zone(e.a) && !zone(e.b)))
                {
                    continue;
                }
                count += segTri(p[static_cast<za::SizeT>(e.a)], p[static_cast<za::SizeT>(e.b)], p[static_cast<za::SizeT>(a)],
                             p[static_cast<za::SizeT>(b)], p[static_cast<za::SizeT>(c)])
                             ? 1
                             : 0;
            }
        }
        const int base = old ? cs.restSelfXOld[focus] : cs.restSelfX[focus];
        t.selfX = za::max(t.selfX, count - base);
    }
    if(holes > 0 && !t.worst[0])
    {
        q_strlcpy(t.worst, what, sizeof(t.worst));
    }
}

// How far each joint turns cleanly (no see-through, flip, collapse or fold), each way alone from the rest pose, in 5
// degree steps past its limits (to 180): "clean to" beside its limit (the limits' tuning: vr_rig2_check <model> 0 envelope).
void envelope(const Rig& rig, const Pose& rest)
{
    rigpose::Rigid F[maxBones];
    const int keep = cs.detail;
    cs.detail = 0;
    for(int b = 0; b < rig.numBones; b++)
    {
        const Bone& bone = rig.bones[b];
        if(bone.joint != Joint::Ball && bone.joint != Joint::Hinge)
        {
            continue;
        }
        char line[256];
        int n = q_snprintf(line, sizeof(line), "rig2 envelope %-11s", bone.name);
        const int ways = bone.joint == Joint::Ball ? 6 : 2;
        for(int w = 0; w < ways; w++)
        {
            const int axis = bone.joint == Joint::Hinge ? 0 : w / 2;
            const float sgn = w % 2 == 0 ? 1.f : -1.f;
            const float limit = bone.joint == Joint::Hinge ? (sgn > 0.f ? bone.range.y : bone.range.x)
                                : axis == 0                ? (sgn > 0.f ? bone.swingX.y : bone.swingX.x)
                                : axis == 1                ? (sgn > 0.f ? bone.swingY.y : bone.swingY.x)
                                                           : (sgn > 0.f ? bone.twist.y : bone.twist.x);
            float clean = rest.a[b][axis];
            for(float a = rest.a[b][axis]; sgn > 0.f ? a <= 180.f : a >= -180.f; a += 5.f * sgn)
            {
                Pose p = rest;
                p.a[b][axis] = a;
                forward(rig, p, F);
                Tally t;
                measure(F, false, b, -1, t, "envelope");
                if(t.seeThrough || t.flipped || t.collapsed || t.folds)
                {
                    if(keep > 0)
                    {
                        // (why: the first few faults there, vr_rig2_check <model> <lines> envelope)
                        char why[64];
                        q_snprintf(why, sizeof(why), "%s %c%d at %.0f", bone.name, sgn > 0.f ? '+' : '-', axis, a);
                        cs.detail = 3;
                        Tally d;
                        measure(F, false, b, -1, d, why);
                        cs.detail = 0;
                        if(d.seeThrough)
                        {
                            Con_Printf("  [%s] see-through %d\n", why, d.seeThrough);
                        }
                    }
                    break;
                }
                clean = a;
            }
            const char* names[3] = {"x", "y", "tw"};
            n += q_snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %s%c %4.0f/%-4.0f%s", bone.joint == Joint::Hinge ? "bend" : names[axis],
                sgn > 0.f ? '+' : '-', clean, limit, (sgn > 0.f ? clean < limit : clean > limit) ? "!" : " ");
        }
        Con_Printf("%s\n", line);
    }
    cs.detail = keep;
}

void printTally(const char* rigName, const char* what, const Tally& t)
{
    Con_Printf("rig2 check %s %-22s poses %4d: see-through %d (%d poses), flipped %d, collapsed %d, folds %d (max %.0f, +%.0f), "
               "crease %.0f, stretch %.2f..%.2f, selfx %d",
        rigName, what, t.poses, t.seeThrough, t.seeThroughPoses, t.flipped, t.collapsed, t.folds, t.foldMax, t.foldGrowthMax,
        t.creaseMax, t.stretchMin, t.stretchMax, t.selfX);
    if(t.bendMax >= t.bendMin)
    {
        Con_Printf(", bend %.0f..%.0f", t.bendMin, t.bendMax);
    }
    if(t.worst[0])
    {
        Con_Printf(" [first hole: %s]", t.worst);
    }
    Con_Printf("\n");
}

// The poses a joint is swept through (alone).
void jointPoses(const Rig& rig, int b, const Pose& rest, za::Vector<Pose>& out, bool edgeOnly)
{
    const Bone& bone = rig.bones[b];
    if(bone.joint == Joint::Hinge)
    {
        const int steps = edgeOnly ? 2 : 30;
        for(int i = 0; i <= steps; i++)
        {
            Pose p = rest;
            p.a[b].x = bone.range.x + (bone.range.y - bone.range.x) * static_cast<float>(i) / static_cast<float>(steps);
            out.pushBack(p);
        }
        return;
    }
    if(bone.joint != Joint::Ball)
    {
        return;
    }
    const int around = edgeOnly ? 8 : 16;
    const float twists[3] = {bone.twist.x, 0.f, bone.twist.y};
    for(int k = 0; k < (edgeOnly ? 1 : 3); k++)
    {
        for(int ring = 0; ring < (edgeOnly ? 1 : 2); ring++)
        {
            const float r = ring == 0 ? 1.f : 0.5f;
            for(int i = 0; i < around; i++)
            {
                const float phi = 2.f * pi * static_cast<float>(i) / static_cast<float>(around);
                const float cx = za::cos(phi), cy = za::sin(phi);
                Pose p = rest;
                p.a[b].x = r * cx * (cx >= 0.f ? bone.swingX.y : -bone.swingX.x);
                p.a[b].y = r * cy * (cy >= 0.f ? bone.swingY.y : -bone.swingY.x);
                p.a[b].z = edgeOnly ? 0.f : twists[k];
                out.pushBack(p);
            }
        }
    }
    if(!edgeOnly)
    {
        // the twist alone through its range
        for(int i = 0; i <= 8; i++)
        {
            Pose p = rest;
            p.a[b].z = bone.twist.x + (bone.twist.y - bone.twist.x) * static_cast<float>(i) / 8.f;
            out.pushBack(p);
        }
    }
}

[[nodiscard]] bool setup(const char* modelName)
{
    qmodel_t* model = Mod_ForName(modelName, false);
    if(!model || model->type != mod_alias)
    {
        Con_Printf("rig2 check: no model %s\n", modelName);
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    cs.rig = rigFor(model);
    cs.old = ragdoll::derivedRigFor(model);
    if(!hdr || !cs.rig || hdr->poseverttype != aliashdr_t::PV_QUAKE1)
    {
        Con_Printf("rig2 check: %s has no rig2 file (quakevr/rigs/<model>.rig2.txt: Misc/quakevr/make_rig2.py)\n", modelName);
        return false;
    }
    if(cs.model == model && cs.generation == cs.rig->generation && !cs.s.tris.empty())
    {
        return true; // (made already: the Rig Studio's live report asks each pose)
    }
    if(!buildSurface(hdr, cs.s))
    {
        return false;
    }
    cs.model = model;
    cs.generation = cs.rig->generation;
    prepareRest(cs.s);
    const Rig& rig = *cs.rig;
    // The triangles' heaviest bones
    const int nt = static_cast<int>(cs.s.tris.size() / 3);
    cs.dom.resize(static_cast<za::SizeT>(nt));
    cs.domOld.resize(static_cast<za::SizeT>(nt));
    for(int t = 0; t < nt; t++)
    {
        float sum[maxBones]{};
        for(int k = 0; k < 3; k++)
        {
            const int v = cs.s.tris[static_cast<za::SizeT>(3 * t + k)];
            for(int b = 0; b < rig.numBones; b++)
            {
                sum[b] += weightOn(rig, v, b);
            }
        }
        int best = 0;
        for(int b = 1; b < rig.numBones; b++)
        {
            best = sum[b] > sum[best] ? b : best;
        }
        cs.dom[static_cast<za::SizeT>(t)] = best;
    }
    // The old rig: its bones' namesakes, its vertices' weights as its ragdolls draw them
    if(cs.old)
    {
        const int weaponBone = boneByName(rig, "weapon");
        for(int o = 0; o < cs.old->numBones; o++)
        {
            const int m = boneByName(rig, cs.old->bones[o].name);
            cs.oldMap[o] = m >= 0 ? m : cs.old->bones[o].joint == ragdoll::Joint::Loose && weaponBone >= 0 ? weaponBone : 0;
        }
        glm::vec4 row{0.f};
        const bool armSkin = vr_ragdoll_arm_skin.value != 0.f && hitreact::armSkinRow(model, hdr, *cs.old, 0, row);
        cs.oldWeights.resize(static_cast<za::SizeT>(cs.s.nv));
        for(int v = 0; v < cs.s.nv; v++)
        {
            Weights& w = cs.oldWeights[static_cast<za::SizeT>(v)];
            w = Weights{};
            const int own = v < cs.old->numVerts ? cs.old->vertBone[static_cast<za::SizeT>(v)] : 0;
            int bl[3];
            float wl[3];
            if(armSkin && agarms::ragdollWeights(model, v, own, bl, wl))
            {
                for(int i = 0; i < 3; i++)
                {
                    if(wl[i] > 0.f)
                    {
                        w.bone[w.n] = bl[i];
                        w.w[w.n] = wl[i];
                        w.n++;
                    }
                }
            }
            if(w.n == 0)
            {
                w.n = 1;
                w.bone[0] = own;
                w.w[0] = 1.f;
            }
        }
        for(int t = 0; t < nt; t++)
        {
            int votes[ragdoll::maxBones]{};
            for(int k = 0; k < 3; k++)
            {
                const Weights& w = cs.oldWeights[static_cast<za::SizeT>(cs.s.tris[static_cast<za::SizeT>(3 * t + k)])];
                votes[w.bone[0]]++;
            }
            int best = 0;
            for(int b = 1; b < cs.old->numBones; b++)
            {
                best = votes[b] > votes[best] ? b : best;
            }
            cs.domOld[static_cast<za::SizeT>(t)] = best;
        }
    }
    return true;
}

void check_f()
{
    const char* modelName = Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/vrgrunt.mdl";
    if(!setup(modelName))
    {
        Con_Printf("rig2 check: 0 passed, 1 failed\n");
        return;
    }
    const Rig& rig = *cs.rig;
    const bool withOld = cs.old != nullptr;
    const Pose rest = restPose(rig);
    rigpose::Rigid F[maxBones];
    int passed = 0, failed = 0;
    // The rest pose: within the limits, no see-through; the self-intersections it has already (each joint's zone)
    {
        forward(rig, rest, F);
        for(int b = 0; b < rig.numBones; b++)
        {
            if(rig.bones[b].joint == Joint::Ball || rig.bones[b].joint == Joint::Hinge)
            {
                if(!withinLimits(rig, b, rest.a[b], 0.5f))
                {
                    Con_Printf("rig2 check: %s's rest pose (%.1f %.1f %.1f) is outside its limits\n", rig.bones[b].name, rest.a[b].x,
                        rest.a[b].y, rest.a[b].z);
                }
                Tally t0, t1;
                measure(F, false, b, -1, t0, "rest");
                cs.restSelfX[b] = t0.selfX;
                if(withOld)
                {
                    measure(F, true, b, -1, t1, "rest");
                    cs.restSelfXOld[b] = t1.selfX;
                }
            }
        }
        Tally t;
        measure(F, false, -1, -1, t, "rest");
        Con_Printf("rig2 check: %s, %d bones, %d vertices, %d triangles, %d edges (%d open); rest: see-through %d\n", modelName,
            rig.numBones, cs.s.nv, static_cast<int>(cs.s.tris.size() / 3), static_cast<int>(cs.s.edges.size()),
            static_cast<int>([&] {
                int open = 0;
                for(const Edge& e : cs.s.edges)
                {
                    open += e.t1 < 0 ? 1 : 0;
                }
                return open;
            }()),
            t.seeThrough);
        if(t.seeThrough || t.flipped || t.folds)
        {
            Con_Printf("rig2 check: FAIL the rest pose itself (see-through %d, flipped %d, folds %d)\n", t.seeThrough, t.flipped, t.folds);
            failed++;
        }
        else
        {
            passed++;
        }
    }
    Tally allNew, allOld;
    cs.detail = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 0;
    const char* only = Cmd_Argc() > 3 ? Cmd_Argv(3) : nullptr; // (a joint's name: its sweeps and pairs alone; "random")
    za::Vector<Pose> poses;
    char what[96];
    // Each joint alone
    for(int b = 0; b < rig.numBones; b++)
    {
        if((rig.bones[b].joint != Joint::Ball && rig.bones[b].joint != Joint::Hinge) || (only && strcmp(only, rig.bones[b].name)))
        {
            continue;
        }
        poses.clear();
        jointPoses(rig, b, rest, poses, false);
        Tally tn, to;
        for(za::SizeT i = 0; i < poses.size(); i++)
        {
            forward(rig, poses[i], F);
            q_snprintf(what, sizeof(what), "%s %.0f %.0f %.0f", rig.bones[b].name, poses[i].a[b].x, poses[i].a[b].y, poses[i].a[b].z);
            measure(F, false, b, -1, tn, what);
            if(withOld)
            {
                measure(F, true, b, -1, to, what);
            }
        }
        printTally("new", rig.bones[b].name, tn);
        if(withOld)
        {
            printTally("old", rig.bones[b].name, to);
        }
        allNew.add(tn);
        allOld.add(to);
        if(tn.clean())
        {
            passed++;
        }
        else
        {
            failed++;
            Con_Printf("rig2 check: FAIL joint %s\n", rig.bones[b].name);
        }
    }
    // Each joint with its child
    for(int c = 0; c < rig.numBones; c++)
    {
        const int b = rig.bones[c].parent;
        if(b < 0 || (rig.bones[c].joint != Joint::Ball && rig.bones[c].joint != Joint::Hinge) ||
            (rig.bones[b].joint != Joint::Ball && rig.bones[b].joint != Joint::Hinge) ||
            (only && strcmp(only, rig.bones[b].name) && strcmp(only, rig.bones[c].name)))
        {
            continue;
        }
        za::Vector<Pose> outer, inner;
        jointPoses(rig, b, rest, outer, true);
        jointPoses(rig, c, rest, inner, true);
        Tally tn, to;
        for(const Pose& po : outer)
        {
            for(const Pose& pi : inner)
            {
                Pose p = po;
                p.a[c] = pi.a[c];
                forward(rig, p, F);
                q_snprintf(what, sizeof(what), "%s %.0f %.0f %.0f + %s %.0f %.0f %.0f", rig.bones[b].name, p.a[b].x, p.a[b].y, p.a[b].z,
                    rig.bones[c].name, p.a[c].x, p.a[c].y, p.a[c].z);
                measure(F, false, c, b, tn, what);
                if(withOld)
                {
                    measure(F, true, c, b, to, what);
                }
            }
        }
        char name[64];
        q_snprintf(name, sizeof(name), "%s+%s", rig.bones[b].name, rig.bones[c].name);
        printTally("new", name, tn);
        if(withOld)
        {
            printTally("old", name, to);
        }
        allNew.add(tn);
        allOld.add(to);
        if(tn.clean())
        {
            passed++;
        }
        else
        {
            failed++;
            Con_Printf("rig2 check: FAIL pair %s\n", name);
        }
    }
    if(only && !strcmp(only, "envelope"))
    {
        envelope(rig, rest);
    }
    // His five situations (vrragtriggers 01-28-53 .. 01-30-04's screenshots; stage 1's eyeshots): each a pose, the old
    // rig moved alike beside it. Measured, not passed or failed (the old rig is what they showed).
    if(only && !strcmp(only, "situations"))
    {
        struct Situation
        {
            const char* name;
            const char* bone[2];
            glm::vec3 a[2];
        };
        const Situation sits[] = {
            {"elbow-to-hand curve (elbow 140, wrist flexed 25)", {"forearm_r", "hand_r"}, {{140.f, 0.f, 0.f}, {25.f, 0.f, 0.f}}},
            {"shoulder hole (30 forward, 40 out)", {"upperarm_r", nullptr}, {{30.f, 40.f, 0.f}, {}}},
            {"knee (115 under a hip at 65)", {"thigh_l", "shin_l"}, {{65.f, 0.f, 0.f}, {115.f, 0.f, 0.f}}},
            {"biceps gap (arm out 50, twisted 20)", {"upperarm_l", nullptr}, {{0.f, 50.f, 20.f}, {}}},
            {"wrist and ankle at their limits", {"hand_r", "foot_l"}, {{25.f, 10.f, 10.f}, {30.f, 10.f, 12.f}}},
        };
        for(const Situation& s : sits)
        {
            Pose p = rest;
            for(int k = 0; k < 2; k++)
            {
                const int b = s.bone[k] ? boneByName(rig, s.bone[k]) : -1;
                if(b >= 0)
                {
                    p.a[b] = s.a[k];
                }
            }
            forward(rig, p, F);
            Tally tn, to;
            measure(F, false, -1, -1, tn, s.name);
            if(withOld)
            {
                measure(F, true, -1, -1, to, s.name);
            }
            printTally("new", s.name, tn);
            if(withOld)
            {
                printTally("old", s.name, to);
            }
        }
    }
    // Poses from the game (vr_rig2_record: ragdolls' bodies from real deaths and knockdowns, armed ones' drawn arms):
    // measured as they are (a ragdoll's joints a little past their limits too), the old rig moved alike beside them.
    if(only && !strcmp(only, "recorded"))
    {
        for(int set = 0; set < 2; set++)
        {
            const za::Vector<Pose>& rec = recordedPoses(set);
            if(rec.empty())
            {
                continue;
            }
            Tally tn, to;
            const za::Vector<Stretches>& recSt = recordedStretches(set);
            int stretched = 0;
            float stretchMost = 1.f;
            for(za::SizeT i = 0; i < rec.size(); i++)
            {
                forward(rig, rec[i], F);
                // (an armed pose's IK stretch: its arm bones lengthened, their children at their ends, the skin so)
                const Stretches* st = i < recSt.size() && recSt[i].n > 0 ? &recSt[i] : nullptr;
                if(st)
                {
                    stretchChain(rig, F, *st);
                    stretched++;
                    for(int k = 0; k < st->n; k++)
                    {
                        stretchMost = za::max(stretchMost, st->b[k].by);
                    }
                }
                q_snprintf(what, sizeof(what), "%s %d", set == 0 ? "ragdoll" : "armed", static_cast<int>(i));
                cs.stretch = st;
                measure(F, false, -1, -1, tn, what);
                cs.stretch = nullptr;
                if(withOld)
                {
                    measure(F, true, -1, -1, to, what);
                }
            }
            if(stretched > 0)
            {
                Con_Printf("rig2 check: %s recorded: %d poses with the IK stretch (at most %.3f)\n", set == 0 ? "ragdoll" : "armed",
                    stretched, static_cast<double>(stretchMost));
            }
            q_snprintf(what, sizeof(what), "%s recorded (%d poses)", set == 0 ? "ragdoll" : "armed", static_cast<int>(rec.size()));
            printTally("new", what, tn);
            if(withOld)
            {
                printTally("old", what, to);
            }
        }
    }
    // Ragdoll-like random poses within the limits
    if(!only || !strcmp(only, "random"))
    {
        uint32_t seed = 0x2f6b9e1du;
        Tally tn, to;
        for(int i = 0; i < randomPoses; i++)
        {
            const Pose p = randomPose(rig, seed);
            forward(rig, p, F);
            q_snprintf(what, sizeof(what), "random %d", i);
            measure(F, false, -1, -1, tn, what);
            if(withOld)
            {
                measure(F, true, -1, -1, to, what);
            }
        }
        printTally("new", "random", tn);
        if(withOld)
        {
            printTally("old", "random", to);
        }
        allNew.add(tn);
        allOld.add(to);
        if(tn.clean())
        {
            passed++;
        }
        else
        {
            failed++;
            Con_Printf("rig2 check: FAIL random poses\n");
        }
    }
    printTally("new", "ALL", allNew);
    if(withOld)
    {
        printTally("old", "ALL", allOld);
    }
    Con_Printf("rig2 check: %d passed, %d failed\n", passed, failed);
}

void info_f()
{
    qmodel_t* model = Mod_ForName(Cmd_Argc() > 1 ? Cmd_Argv(1) : "progs/vrgrunt.mdl", false);
    const Rig* rig = model ? rigFor(model) : nullptr;
    if(!rig)
    {
        Con_Printf("rig2: no rig for that model\n");
        return;
    }
    Con_Printf("rig2: %s (%s): %d bones, %d vertices weighted\n", rig->model.cStr(), rig->path.cStr(), rig->numBones,
        static_cast<int>(rig->weights.size()));
    for(int b = 0; b < rig->numBones; b++)
    {
        const Bone& bone = rig->bones[b];
        const glm::vec3 r = restAngles(*rig, b);
        int n = 0;
        for(za::SizeT v = 0; v < rig->weights.size(); v++)
        {
            n += weightOn(*rig, static_cast<int>(v), b) > 0.f ? 1 : 0;
        }
        Con_Printf("  %-11s %-11s %s pivot %.2f %.2f %.2f rest %.1f %.1f %.1f, %d vertices", bone.name,
            bone.parent >= 0 ? rig->bones[bone.parent].name : "-",
            bone.joint == Joint::Ball ? "ball " : bone.joint == Joint::Hinge ? "hinge" : bone.joint == Joint::Fixed ? "fixed" : "root ",
            bone.pivot.x, bone.pivot.y, bone.pivot.z, r.x, r.y, r.z, n);
        if(bone.joint == Joint::Ball)
        {
            Con_Printf(" swing x %g..%g y %g..%g twist %g..%g", bone.swingX.x, bone.swingX.y, bone.swingY.x, bone.swingY.y, bone.twist.x,
                bone.twist.y);
        }
        else if(bone.joint == Joint::Hinge)
        {
            Con_Printf(" bend %g..%g", bone.range.x, bone.range.y);
        }
        Con_Printf("\n");
    }
}

void reload_f()
{
    reload();
    Con_Printf("rig2: the rig files read again\n");
}

} // namespace

bool measurePose(qmodel_t* model, const Pose& pose, bool old, Faults& out)
{
    out = Faults{};
    if(!model || !setup(model->name) || (old && !cs.old))
    {
        return false;
    }
    rigpose::Rigid F[maxBones];
    forward(*cs.rig, pose, F);
    Tally t;
    const int keep = cs.detail;
    cs.detail = 0;
    measure(F, old, -1, -1, t, "studio");
    cs.detail = keep;
    out.seeThrough = t.seeThrough;
    out.flipped = t.flipped;
    out.collapsed = t.collapsed;
    out.folds = t.folds;
    out.foldMax = t.foldMax;
    out.stretchMin = t.stretchMin;
    out.stretchMax = t.stretchMax;
    return true;
}

void init()
{
    Cmd_AddCommand("vr_rig2_check", check_f);
    Cmd_AddCommand("vr_rig2_info", info_f);
    Cmd_AddCommand("vr_rig2_draw_info", drawInfo);
    Cmd_AddCommand("vr_rig2_reload", reload_f);
    Cmd_AddCommand("vr_rig2_arms", armsReport_f);
    Cmd_AddCommand("vr_rig2_frames", framesCheck_f);
    Cmd_AddCommand("vr_rig2_live_check", liveCheck_f);
    Cmd_AddCommand("vr_rig2anim_check", animCheck_f);
    Cmd_AddCommand("vr_rig2_parts", partsReport_f);
    Cmd_AddCommand("vr_rig2_record", record_f);
}

} // namespace qvr::rig2
