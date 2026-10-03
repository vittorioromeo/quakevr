// vr_ragdoll.cpp -- ragdolls (experimental, the grunt only): the rig derived from a .mdl's vertex animation, the skinned
// model made from it in memory, and the client's drawing of the server's ragdolls. See vr_ragdoll.hpp; the bodies and
// joints are vr_box3d.cpp's ("Ragdolls"); ROUND21.md, "Ragdolls".

#include "vr_ragdoll.hpp"
#include "vr_cvars.hpp"
#include "vr_mem.hpp"

#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <cstring>

using namespace qvr;

namespace qvr::ragdoll
{

// A rig's heap memory (mem::Cache's count).
[[nodiscard]] za::SizeT heldBytes(const Rig& r)
{
    za::SizeT n = mem::heldBytes(r.vertBone) + mem::heldBytes(r.poseRot) + mem::heldBytes(r.posePos) + mem::heldBytes(r.poseHidden);
    for(const Bone& b : r.bones)
    {
        n += mem::heldBytes(b.points);
    }
    return n;
}

} // namespace qvr::ragdoll

namespace
{

using ragdoll::Bone;
using ragdoll::Joint;
using ragdoll::Rig;
using ragdoll::maxBones;

constexpr float deg = 0.017453292f;

// ----------------------------------------------------------------------------
// The seed tables: per model, its bones (where they are in the rest pose, pose 0) and joints. Numbers of ours (measured
// on the model, ROUND21.md "Ragdolls"), not the model's.

struct Seed
{
    const char* name;
    int parent;
    Joint joint;
    glm::vec3 centre; // about where the bone's vertices are (the clusters go to the nearest)
    glm::vec3 pivot;  // the joint with the parent (the root: its middle)
    glm::vec3 end;    // the far end
    float capsule;    // a capsule pivot..end of this radius besides its hull (units; 0: none)
    float cone;       // Ball: degrees
    float twist;      // Ball: degrees either way
    float flex;       // Hinge: how far it bends at most (degrees from straight)
    glm::vec3 hinge;  // Hinge: its axis when the rest pose is (nearly) straight; bending turns it that way
};

struct SeedTable
{
    const char* model;
    int numVerts; // the model the table was made for (Quake VR's grunt): another one is not rigged
    const Seed* seeds;
    int count;
    int deaths;
    int deathFirst[2], deathLast[2];
};

// Quake VR's grunt (quakevr/progs/soldier.mdl: 555 vertices, 120 frames). The rest pose: x forward, y left, z up; he
// crouches a little with the shotgun in both hands. Death frames 8-17 ($death1-10) and 18-28 ($deathc1-11).
constexpr Seed gruntSeeds[] = {
    {"pelvis", -1, Joint::Root, {-7.f, 0.f, 0.f}, {-5.5f, 0.f, -1.f}, {-4.7f, 0.9f, 4.8f}, 0.f, 0.f, 0.f, 0.f, {}},
    {"chest", 0, Joint::Ball, {-1.f, 0.f, 12.f}, {-4.7f, 0.9f, 4.8f}, {2.7f, 0.6f, 17.7f}, 0.f, 35.f, 25.f, 0.f, {}},
    {"head", 1, Joint::Ball, {5.f, 0.f, 21.f}, {2.7f, 0.6f, 17.7f}, {5.f, 0.f, 26.f}, 0.f, 45.f, 50.f, 0.f, {}},
    {"upperarm_l", 1, Joint::Ball, {1.6f, 8.8f, 9.6f}, {0.7f, 6.3f, 14.5f}, {3.5f, 10.1f, 4.2f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_l", 3, Joint::Hinge, {9.9f, 6.f, 2.4f}, {3.5f, 10.1f, 4.2f}, {13.f, 6.f, 2.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"upperarm_r", 1, Joint::Ball, {-0.4f, -9.7f, 12.1f}, {0.1f, -7.7f, 16.3f}, {0.2f, -10.8f, 7.f}, 0.f, 85.f, 45.f, 0.f, {}},
    {"forearm_r", 5, Joint::Hinge, {4.7f, -5.6f, 4.9f}, {0.2f, -10.8f, 7.f}, {7.5f, -3.5f, 5.5f}, 0.f, 0.f, 0.f, 145.f, {0.f, -1.f, 0.f}},
    {"thigh_l", 0, Joint::Ball, {-4.5f, 7.4f, -13.2f}, {-6.3f, 4.3f, -4.1f}, {-4.5f, 7.5f, -14.f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_l", 7, Joint::Hinge, {-9.7f, 7.6f, -23.f}, {-4.5f, 7.5f, -14.f}, {-9.f, 7.5f, -19.5f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
    {"thigh_r", 0, Joint::Ball, {3.1f, -6.7f, -10.5f}, {-4.5f, -5.3f, -1.5f}, {3.5f, -6.7f, -11.5f}, 3.2f, 70.f, 30.f, 0.f, {}},
    {"shin_r", 9, Joint::Hinge, {2.f, -7.2f, -23.f}, {3.5f, -6.7f, -11.5f}, {1.f, -7.f, -20.f}, 2.8f, 0.f, 0.f, 150.f, {0.f, 1.f, 0.f}},
};

constexpr SeedTable seedTables[] = {
    {"progs/soldier.mdl", 555, gruntSeeds, static_cast<int>(sizeof(gruntSeeds) / sizeof(gruntSeeds[0])), 2, {8, 18}, {17, 28}},
};

[[nodiscard]] const SeedTable* tableOf(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return nullptr;
    }
    for(const SeedTable& t : seedTables)
    {
        if(!strcmp(model->name, t.model))
        {
            return &t;
        }
    }
    return nullptr;
}

// ----------------------------------------------------------------------------
// The rigid fit (Horn, "Closed-form solution of absolute orientation using unit quaternions", 1987): the rotation that
// best carries centred points a onto centred points b is the eigenvector of the largest eigenvalue of a symmetric 4x4.

// A symmetric 4x4's eigenvector of its largest eigenvalue (cyclic Jacobi).
glm::quat largestEigenQuat(float n[4][4])
{
    float v[4][4] = {{1.f, 0.f, 0.f, 0.f}, {0.f, 1.f, 0.f, 0.f}, {0.f, 0.f, 1.f, 0.f}, {0.f, 0.f, 0.f, 1.f}};
    for(int sweep = 0; sweep < 24; sweep++)
    {
        float off = 0.f;
        for(int p = 0; p < 4; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                off += n[p][q] * n[p][q];
            }
        }
        if(off < 1e-18f)
        {
            break;
        }
        for(int p = 0; p < 3; p++)
        {
            for(int q = p + 1; q < 4; q++)
            {
                if(za::abs(n[p][q]) < 1e-20f)
                {
                    continue;
                }
                const float theta = (n[q][q] - n[p][p]) / (2.f * n[p][q]);
                const float t = (theta >= 0.f ? 1.f : -1.f) / (za::abs(theta) + za::sqrt(theta * theta + 1.f));
                const float c = 1.f / za::sqrt(t * t + 1.f), s = t * c;
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[k][p], b = n[k][q];
                    n[k][p] = c * a - s * b;
                    n[k][q] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = n[p][k], b = n[q][k];
                    n[p][k] = c * a - s * b;
                    n[q][k] = s * a + c * b;
                }
                for(int k = 0; k < 4; k++)
                {
                    const float a = v[k][p], b = v[k][q];
                    v[k][p] = c * a - s * b;
                    v[k][q] = s * a + c * b;
                }
            }
        }
    }
    int best = 0;
    for(int i = 1; i < 4; i++)
    {
        if(n[i][i] > n[best][best])
        {
            best = i;
        }
    }
    const glm::quat q{v[0][best], v[1][best], v[2][best], v[3][best]}; // (w, x, y, z)
    const float len = glm::length(q);
    return len > 1e-12f ? q / len : glm::quat{1.f, 0.f, 0.f, 0.f};
}

// Accumulates point pairs (rest a, posed b) and gives the best rigid transform b = rot * a + pos.
struct Fit
{
    glm::vec3 sa{0.f}, sb{0.f};
    float s[3][3]{}; // sum of a_i b_j
    int n{0};

    void add(const glm::vec3& a, const glm::vec3& b)
    {
        sa += a;
        sb += b;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                s[i][j] += a[i] * b[j];
            }
        }
        n++;
    }

    void solve(glm::quat& rot, glm::vec3& pos) const
    {
        if(n == 0)
        {
            rot = glm::quat{1.f, 0.f, 0.f, 0.f};
            pos = glm::vec3{0.f};
            return;
        }
        const float inv = 1.f / static_cast<float>(n);
        const glm::vec3 ca = sa * inv, cb = sb * inv;
        float m[3][3];
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m[i][j] = s[i][j] - static_cast<float>(n) * ca[i] * cb[j];
            }
        }
        const float sxx = m[0][0], sxy = m[0][1], sxz = m[0][2], syx = m[1][0], syy = m[1][1], syz = m[1][2], szx = m[2][0],
                    szy = m[2][1], szz = m[2][2];
        float nm[4][4] = {{sxx + syy + szz, syz - szy, szx - sxz, sxy - syx},
            {syz - szy, sxx - syy - szz, sxy + syx, szx + sxz},
            {szx - sxz, sxy + syx, -sxx + syy - szz, syz + szy},
            {sxy - syx, szx + sxz, syz + szy, -sxx - syy + szz}};
        rot = largestEigenQuat(nm);
        pos = cb - rot * ca;
    }
};

// ----------------------------------------------------------------------------
// The derivation.

struct Mesh
{
    const aliashdr_t* hdr{nullptr};
    int nv{0}, np{0};
    za::Vector<glm::vec3> p; // [pose * nv + v]
    za::Vector<int> rep;     // each vertex's welded representative (the first of its place in every pose)
    za::Vector<int> adjStart, adj; // the representatives' neighbours (CSR, by representative)

    [[nodiscard]] const glm::vec3& at(int pose, int v) const { return p[static_cast<za::SizeT>(pose * nv + v)]; }
};

bool loadMesh(const aliashdr_t* hdr, Mesh& m)
{
    m.hdr = hdr;
    m.nv = hdr->numverts;
    m.np = hdr->numposes;
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    m.p.resize(static_cast<za::SizeT>(m.nv * m.np));
    for(int pose = 0; pose < m.np; pose++)
    {
        for(int v = 0; v < m.nv; v++)
        {
            const trivertx_t& t = tv[pose * m.nv + v];
            m.p[static_cast<za::SizeT>(pose * m.nv + v)] = glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0],
                t.v[1] * hdr->scale[1] + hdr->scale_origin[1], t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        }
    }

    // Welded: the same bytes in every pose (the copies along the skin's seams).
    m.rep.resize(static_cast<za::SizeT>(m.nv));
    ankerl::unordered_dense::map<za::U64, int> first;
    for(int v = 0; v < m.nv; v++)
    {
        za::U64 h = 1469598103934665603ull;
        for(int pose = 0; pose < m.np; pose++)
        {
            const trivertx_t& t = tv[pose * m.nv + v];
            h = (h ^ (static_cast<za::U64>(t.v[0]) | static_cast<za::U64>(t.v[1]) << 8 | static_cast<za::U64>(t.v[2]) << 16)) *
                1099511628211ull;
        }
        m.rep[static_cast<za::SizeT>(v)] = v;
        const auto it = first.find(h);
        if(it == first.end())
        {
            first.emplace(h, v);
            continue;
        }
        const int u = it->second;
        bool same = true;
        for(int pose = 0; pose < m.np && same; pose++)
        {
            const trivertx_t& a = tv[pose * m.nv + v];
            const trivertx_t& b = tv[pose * m.nv + u];
            same = a.v[0] == b.v[0] && a.v[1] == b.v[1] && a.v[2] == b.v[2];
        }
        if(same)
        {
            m.rep[static_cast<za::SizeT>(v)] = u;
        }
    }

    // The triangles' edges between representatives.
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    za::Vector<za::U64> edges;
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        int c[3];
        for(int k = 0; k < 3; k++)
        {
            c[k] = m.rep[desc[idx[i + k]].vertindex];
        }
        for(int k = 0; k < 3; k++)
        {
            const int a = c[k], b = c[(k + 1) % 3];
            if(a != b)
            {
                edges.pushBack(static_cast<za::U64>(a) << 32 | static_cast<za::U64>(b));
                edges.pushBack(static_cast<za::U64>(b) << 32 | static_cast<za::U64>(a));
            }
        }
    }
    za::Vector<int> count(static_cast<za::SizeT>(m.nv + 1), 0);
    for(const za::U64 e : edges)
    {
        count[static_cast<za::SizeT>(e >> 32) + 1]++;
    }
    m.adjStart.resize(static_cast<za::SizeT>(m.nv + 1), 0);
    for(int v = 0; v < m.nv; v++)
    {
        m.adjStart[static_cast<za::SizeT>(v + 1)] = m.adjStart[static_cast<za::SizeT>(v)] + count[static_cast<za::SizeT>(v + 1)];
    }
    m.adj.resize(edges.size());
    za::Vector<int> fill(m.adjStart.data(), m.adjStart.data() + m.adjStart.size());
    for(const za::U64 e : edges)
    {
        const int a = static_cast<int>(e >> 32);
        m.adj[static_cast<za::SizeT>(fill[static_cast<za::SizeT>(a)]++)] = static_cast<int>(e & 0xffffffffu);
    }
    return true;
}

// Each label's rigid transform per pose, fitted to its members (the representatives labelled so).
struct Transforms
{
    int labels{0}, np{0};
    za::Vector<glm::quat> rot;
    za::Vector<glm::vec3> pos;
    za::Vector<int> members;

    void fit(const Mesh& m, const za::Vector<int>& reps, const za::Vector<int>& label, int count)
    {
        labels = count;
        np = m.np;
        rot.resize(static_cast<za::SizeT>(count * np));
        pos.resize(static_cast<za::SizeT>(count * np));
        members.clear();
        members.resize(static_cast<za::SizeT>(count), 0);
        for(const int v : reps)
        {
            const int l = label[static_cast<za::SizeT>(v)];
            if(l >= 0 && l < count)
            {
                members[static_cast<za::SizeT>(l)]++;
            }
        }
        za::Vector<Fit> fits(static_cast<za::SizeT>(count));
        for(int pose = 0; pose < np; pose++)
        {
            for(Fit& f : fits)
            {
                f = Fit{};
            }
            for(const int v : reps)
            {
                const int l = label[static_cast<za::SizeT>(v)];
                if(l >= 0 && l < count)
                {
                    fits[static_cast<za::SizeT>(l)].add(m.at(0, v), m.at(pose, v));
                }
            }
            for(int l = 0; l < count; l++)
            {
                fits[static_cast<za::SizeT>(l)].solve(rot[static_cast<za::SizeT>(l * np + pose)], pos[static_cast<za::SizeT>(l * np + pose)]);
            }
        }
    }

    // How badly label l's transforms carry vertex v (the squared distances summed over the poses).
    [[nodiscard]] float error(const Mesh& m, int l, int v) const
    {
        if(members[static_cast<za::SizeT>(l)] < 3)
        {
            return 1e30f;
        }
        const glm::vec3 r = m.at(0, v);
        float e = 0.f;
        for(int pose = 0; pose < np; pose++)
        {
            const za::SizeT i = static_cast<za::SizeT>(l * np + pose);
            const glm::vec3 d = rot[i] * r + pos[i] - m.at(pose, v);
            e += glm::dot(d, d);
        }
        return e;
    }
};

// Reassigns each representative to the label (its own, or a neighbour's) whose transforms carry it best, until none
// moves. `fixed`: labels that never change (loose pieces). Returns the rms error (units).
float refine(const Mesh& m, const za::Vector<int>& reps, za::Vector<int>& label, int count, int maxIterations, Transforms& t)
{
    float total = 0.f;
    for(int it = 0; it < maxIterations; it++)
    {
        t.fit(m, reps, label, count);
        int changed = 0;
        total = 0.f;
        for(const int v : reps)
        {
            const int own = label[static_cast<za::SizeT>(v)];
            if(own < 0 || own >= count)
            {
                continue;
            }
            int best = own;
            float bestE = t.error(m, own, v);
            for(int k = m.adjStart[static_cast<za::SizeT>(v)]; k < m.adjStart[static_cast<za::SizeT>(v) + 1]; k++)
            {
                const int l = label[static_cast<za::SizeT>(m.adj[static_cast<za::SizeT>(k)])];
                if(l == best || l < 0 || l >= count)
                {
                    continue;
                }
                const float e = t.error(m, l, v);
                if(e < bestE)
                {
                    bestE = e;
                    best = l;
                }
            }
            total += bestE < 1e29f ? bestE : 0.f;
            if(best != own)
            {
                label[static_cast<za::SizeT>(v)] = best;
                changed++;
            }
        }
        if(changed == 0)
        {
            break;
        }
    }
    t.fit(m, reps, label, count);
    return za::sqrt(total / static_cast<float>(za::max<za::SizeT>(reps.size(), 1) * static_cast<za::SizeT>(m.np)));
}

constexpr int clusterCount = 18; // the motion clusters (more than the bones: the seeds gather them)
constexpr float looseGap = 4.f;  // units: a piece this far from the body in some pose is loose (the shotgun)

bool derive(qmodel_t* model, const SeedTable& table, Rig& rig)
{
    const double t0 = Sys_DoubleTime();
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || Mod_NextSurface(const_cast<aliashdr_t*>(hdr)) ||
        hdr->numverts != table.numVerts || hdr->numposes < 2 || table.count > maxBones)
    {
        Con_DPrintf("ragdoll: %s is not the model its seed table was made for\n", model->name);
        return false;
    }
    Mesh m;
    loadMesh(hdr, m);

    // The pieces (welded), the largest the body; a piece that leaves it in some pose is loose.
    za::Vector<int> piece(static_cast<za::SizeT>(m.nv), -1);
    za::Vector<int> pieceSize;
    za::Vector<int> stack;
    for(int v = 0; v < m.nv; v++)
    {
        if(m.rep[static_cast<za::SizeT>(v)] != v || piece[static_cast<za::SizeT>(v)] >= 0)
        {
            continue;
        }
        const int id = static_cast<int>(pieceSize.size());
        pieceSize.pushBack(0);
        piece[static_cast<za::SizeT>(v)] = id;
        stack.clear();
        stack.pushBack(v);
        while(!stack.empty())
        {
            const int u = stack.back();
            stack.popBack();
            pieceSize[static_cast<za::SizeT>(id)]++;
            for(int k = m.adjStart[static_cast<za::SizeT>(u)]; k < m.adjStart[static_cast<za::SizeT>(u) + 1]; k++)
            {
                const int w = m.adj[static_cast<za::SizeT>(k)];
                if(piece[static_cast<za::SizeT>(w)] < 0)
                {
                    piece[static_cast<za::SizeT>(w)] = id;
                    stack.pushBack(w);
                }
            }
        }
    }
    int body = 0;
    for(int i = 1; i < static_cast<int>(pieceSize.size()); i++)
    {
        body = pieceSize[static_cast<za::SizeT>(i)] > pieceSize[static_cast<za::SizeT>(body)] ? i : body;
    }
    za::Vector<int> reps, bodyReps;
    for(int v = 0; v < m.nv; v++)
    {
        if(m.rep[static_cast<za::SizeT>(v)] == v)
        {
            reps.pushBack(v);
            if(piece[static_cast<za::SizeT>(v)] == body)
            {
                bodyReps.pushBack(v);
            }
        }
    }
    za::Vector<int> loosePieces; // the loose pieces' ids, in order (their bones follow the seeds')
    za::Vector<uint8_t> isLoose(pieceSize.size(), 0);
    for(int id = 0; id < static_cast<int>(pieceSize.size()); id++)
    {
        if(id == body || pieceSize[static_cast<za::SizeT>(id)] < 4)
        {
            continue;
        }
        float gap = 0.f;
        for(int pose = 0; pose < m.np; pose += 2)
        {
            float nearest = 1e30f;
            for(const int v : reps)
            {
                if(piece[static_cast<za::SizeT>(v)] != id)
                {
                    continue;
                }
                for(const int u : bodyReps)
                {
                    const glm::vec3 d = m.at(pose, v) - m.at(pose, u);
                    nearest = za::min(nearest, glm::dot(d, d));
                }
            }
            gap = za::max(gap, za::sqrt(nearest));
        }
        if(gap > looseGap && table.count + static_cast<int>(loosePieces.size()) < maxBones)
        {
            isLoose[static_cast<za::SizeT>(id)] = 1;
            loosePieces.pushBack(id);
        }
    }

    // The motion clusters: the farthest trajectories as the first centres, then each representative to its nearest
    // centre, then refined by the clusters' rigid motion.
    za::Vector<int> moving; // the representatives not in a loose piece
    for(const int v : reps)
    {
        if(!isLoose[static_cast<za::SizeT>(piece[static_cast<za::SizeT>(v)])])
        {
            moving.pushBack(v);
        }
    }
    const auto trajDist = [&](int a, int b) {
        float d = 0.f;
        for(int pose = 0; pose < m.np; pose++)
        {
            const glm::vec3 e = m.at(pose, a) - m.at(pose, b);
            d += glm::dot(e, e);
        }
        return d;
    };
    za::Vector<int> centres;
    za::Vector<float> nearestD(static_cast<za::SizeT>(m.nv), 1e30f);
    centres.pushBack(moving[0]);
    while(static_cast<int>(centres.size()) < clusterCount && static_cast<int>(centres.size()) < static_cast<int>(moving.size()))
    {
        const int c = centres.back();
        int farthest = -1;
        float farD = -1.f;
        for(const int v : moving)
        {
            float& d = nearestD[static_cast<za::SizeT>(v)];
            d = za::min(d, trajDist(v, c));
            if(d > farD)
            {
                farD = d;
                farthest = v;
            }
        }
        centres.pushBack(farthest);
    }
    const int k = static_cast<int>(centres.size());
    za::Vector<int> label(static_cast<za::SizeT>(m.nv), -1);
    for(const int v : moving)
    {
        int best = 0;
        float bestD = 1e30f;
        for(int c = 0; c < k; c++)
        {
            const float d = trajDist(v, centres[static_cast<za::SizeT>(c)]);
            if(d < bestD)
            {
                bestD = d;
                best = c;
            }
        }
        label[static_cast<za::SizeT>(v)] = best;
    }
    Transforms t;
    rig.clusterRms = refine(m, moving, label, k, 30, t);

    // The clusters to the seeds' bones: each to the bone whose seed is nearest its middle in the rest pose.
    za::Vector<int> boneOfCluster(static_cast<za::SizeT>(k), 0);
    for(int c = 0; c < k; c++)
    {
        glm::vec3 sum{0.f};
        int n = 0;
        for(const int v : moving)
        {
            if(label[static_cast<za::SizeT>(v)] == c)
            {
                sum += m.at(0, v);
                n++;
            }
        }
        if(n == 0)
        {
            continue;
        }
        const glm::vec3 mid = sum / static_cast<float>(n);
        float bestD = 1e30f;
        for(int b = 0; b < table.count; b++)
        {
            const glm::vec3 d = mid - table.seeds[b].centre;
            if(glm::dot(d, d) < bestD)
            {
                bestD = glm::dot(d, d);
                boneOfCluster[static_cast<za::SizeT>(c)] = b;
            }
        }
    }
    for(const int v : moving)
    {
        label[static_cast<za::SizeT>(v)] = boneOfCluster[static_cast<za::SizeT>(label[static_cast<za::SizeT>(v)])];
    }
    for(int i = 0; i < static_cast<int>(loosePieces.size()); i++)
    {
        for(const int v : reps)
        {
            if(piece[static_cast<za::SizeT>(v)] == loosePieces[static_cast<za::SizeT>(i)])
            {
                label[static_cast<za::SizeT>(v)] = table.count + i;
            }
        }
    }
    rig.boneRms = refine(m, moving, label, table.count, 40, t);
    const int numBones = table.count + static_cast<int>(loosePieces.size());
    for(int b = 0; b < table.count; b++)
    {
        if(t.members[static_cast<za::SizeT>(b)] < 3)
        {
            Con_Printf("ragdoll: %s: bone %s got %d vertices: no ragdoll\n", model->name, table.seeds[b].name,
                t.members[static_cast<za::SizeT>(b)]);
            return false;
        }
    }
    t.fit(m, reps, label, numBones); // (the loose pieces' too)

    // The rig.
    rig.model = model;
    rig.numBones = numBones;
    rig.numVerts = m.nv;
    rig.numPoses = m.np;
    rig.vertBone.resize(static_cast<za::SizeT>(m.nv));
    for(int v = 0; v < m.nv; v++)
    {
        rig.vertBone[static_cast<za::SizeT>(v)] = static_cast<uint8_t>(label[static_cast<za::SizeT>(m.rep[static_cast<za::SizeT>(v)])]);
    }
    rig.poseRot.resize(static_cast<za::SizeT>(m.np * numBones));
    rig.posePos.resize(static_cast<za::SizeT>(m.np * numBones));
    rig.poseHidden.resize(static_cast<za::SizeT>(m.np * numBones), 0);
    for(int pose = 0; pose < m.np; pose++)
    {
        za::Array<glm::vec3, maxBones> lo, hi;
        for(int b = 0; b < numBones; b++)
        {
            lo[static_cast<za::SizeT>(b)] = glm::vec3{1e30f};
            hi[static_cast<za::SizeT>(b)] = glm::vec3{-1e30f};
        }
        for(const int v : reps)
        {
            const int b = label[static_cast<za::SizeT>(v)];
            lo[static_cast<za::SizeT>(b)] = glm::min(lo[static_cast<za::SizeT>(b)], m.at(pose, v));
            hi[static_cast<za::SizeT>(b)] = glm::max(hi[static_cast<za::SizeT>(b)], m.at(pose, v));
        }
        for(int b = 0; b < numBones; b++)
        {
            const glm::vec3 size = hi[static_cast<za::SizeT>(b)] - lo[static_cast<za::SizeT>(b)];
            rig.poseHidden[static_cast<za::SizeT>(pose * numBones + b)] = za::max(size.x, za::max(size.y, size.z)) < 0.5f ? 1 : 0;
        }
    }
    for(int pose = 0; pose < m.np; pose++)
    {
        for(int b = 0; b < numBones; b++)
        {
            rig.poseRot[static_cast<za::SizeT>(pose * numBones + b)] = t.rot[static_cast<za::SizeT>(b * m.np + pose)];
            rig.posePos[static_cast<za::SizeT>(pose * numBones + b)] = t.pos[static_cast<za::SizeT>(b * m.np + pose)];
        }
    }
    for(int b = 0; b < numBones; b++)
    {
        Bone& bone = rig.bones[b];
        bone.points.clear();
        for(const int v : reps)
        {
            if(label[static_cast<za::SizeT>(v)] == b)
            {
                bone.points.pushBack(m.at(0, v));
            }
        }
        if(b >= table.count)
        {
            glm::vec3 mid{0.f};
            for(const glm::vec3& p : bone.points)
            {
                mid += p;
            }
            mid /= static_cast<float>(za::max<za::SizeT>(bone.points.size(), 1));
            strcpy(bone.name, "loose");
            bone.parent = -1;
            bone.joint = Joint::Loose;
            bone.pivot = bone.end = mid;
            continue;
        }
        const Seed& s = table.seeds[b];
        strncpy(bone.name, s.name, sizeof(bone.name) - 1);
        bone.parent = s.parent;
        bone.joint = s.joint;
        bone.pivot = s.pivot;
        bone.end = s.end;
        bone.capsule = s.capsule;
        bone.cone = s.cone * deg;
        bone.twist = s.twist * deg;
        if(s.joint == Joint::Hinge)
        {
            // Its axis from how it bends at rest (the parent's direction crossed with its own: bending further turns
            // it that way); nearly straight, the table's.
            const Seed& p = table.seeds[s.parent];
            const glm::vec3 u = glm::normalize(p.end - p.pivot), l = glm::normalize(s.end - s.pivot);
            const glm::vec3 c = glm::cross(u, l);
            const float bend = za::acos(za::clamp(glm::dot(u, l), -1.f, 1.f));
            bone.hinge = glm::length(c) > 0.2f ? glm::normalize(c) : s.hinge;
            const float rest = glm::dot(glm::cross(u, l), bone.hinge) >= 0.f ? bend : -bend;
            bone.lower = -za::max(rest - 3.f * deg, 0.f);
            bone.upper = za::max(s.flex * deg - rest, 5.f * deg);
        }
    }
    const SeedTable& tb = table;
    rig.deaths = tb.deaths;
    for(int i = 0; i < tb.deaths; i++)
    {
        rig.deathFirst[i] = tb.deathFirst[i];
        rig.deathLast[i] = tb.deathLast[i];
    }
    rig.deriveMs = (Sys_DoubleTime() - t0) * 1000.0;
    Con_DPrintf("ragdoll: %s rigged: %d bones (%d loose), clusters %.2f, bones %.2f units rms, %.1f ms\n", model->name, numBones,
        static_cast<int>(loosePieces.size()), rig.clusterRms, rig.boneRms, rig.deriveMs);
    return true;
}

// ----------------------------------------------------------------------------
// The rigs (models' slots: a game dir change, a model reload), and the skinned models made from them.

struct RigCache
{
    za::Vector<za::UniquePtr<Rig>> rigs; // made (and kept) for these models (a rig stays where it is: ragdolls point to it)
    za::Vector<const qmodel_t*> failed;  // tried, not rigged
    auto members() { return mem::list(rigs, failed); }
};
mem::Cache<RigCache> rigCache{"ragdoll rigs", mem::GameDirChange}; // (not a model reload: ragdolls point to their rigs)

[[nodiscard]] Rig* findRig(const qmodel_t* model)
{
    for(za::UniquePtr<Rig>& r : rigCache.rigs)
    {
        if(r->model == model)
        {
            return r.get();
        }
    }
    return nullptr;
}

constexpr const char* skinnedSuffix = "#rag";

// ----------------------------------------------------------------------------
// What the server publishes (its ragdolls' bones in the world) and what the client swapped for a frame. The main thread.

struct Published
{
    const Rig* rig{nullptr};
    int bodies{0}; // the bones from this on are hidden
    float scale{1.f};
    za::Array<glm::quat, maxBones> rot{};
    za::Array<glm::vec3, maxBones> pos{};
};

struct Swapped
{
    int num{0};
    qmodel_t* original{nullptr};
    qmodel_t* skinned{nullptr};
    glm::vec3 ref{0.f};
    int bones{0};
    za::Array<float, maxBones * 12> skin{};
};

struct DrawState
{
    za::Vector<Published> byNum;  // by edict number (rig null: none)
    za::Vector<Swapped> swapped;  // this frame's (swapModels .. restoreModels)
    auto members() { return mem::list(byNum, swapped); }
};
mem::Cache<DrawState> draw{"ragdolls drawn", mem::MapChange};

[[nodiscard]] const Swapped* swappedOf(const entity_t* e)
{
    for(const Swapped& s : draw.swapped)
    {
        if(&cl_entities[s.num] == e)
        {
            return &s;
        }
    }
    return nullptr;
}

} // namespace

namespace qvr::ragdoll
{

bool eligible(const qmodel_t* model)
{
    return tableOf(model) != nullptr;
}

const Rig* rigFor(qmodel_t* model)
{
    const SeedTable* table = tableOf(model);
    if(!table)
    {
        return nullptr;
    }
    if(Rig* r = findRig(model))
    {
        return r;
    }
    for(const qmodel_t* f : rigCache.failed)
    {
        if(f == model)
        {
            return nullptr;
        }
    }
    za::UniquePtr<Rig> r = za::makeUnique<Rig>();
    if(!derive(model, *table, *r))
    {
        rigCache.failed.pushBack(model);
        return nullptr;
    }
    Rig* made = r.get();
    rigCache.rigs.pushBack(static_cast<za::UniquePtr<Rig>&&>(r));
    return made;
}

void bonePose(const Rig& rig, int pose, int b, glm::quat& rot, glm::vec3& pos)
{
    pose = za::clamp(pose, 0, rig.numPoses - 1);
    rot = rig.poseRot[static_cast<za::SizeT>(pose * rig.numBones + b)];
    pos = rig.posePos[static_cast<za::SizeT>(pose * rig.numBones + b)];
}

int poseOfFrame(const qmodel_t* model, int frame)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->numframes <= 0)
    {
        return 0;
    }
    frame = za::clamp(frame, 0, hdr->numframes - 1);
    return hdr->frames[frame].firstpose;
}

float deathProgress(const Rig& rig, int frame)
{
    for(int i = 0; i < rig.deaths; i++)
    {
        if(frame >= rig.deathFirst[i] && frame <= rig.deathLast[i])
        {
            return static_cast<float>(frame - rig.deathFirst[i]) / static_cast<float>(za::max(rig.deathLast[i] - rig.deathFirst[i], 1));
        }
    }
    return -1.f;
}

bool collapsed(const Rig& rig, int pose, int b)
{
    pose = za::clamp(pose, 0, rig.numPoses - 1);
    return rig.poseHidden[static_cast<za::SizeT>(pose * rig.numBones + b)] != 0;
}

void publish(int num, const Rig* rig, int bodies, const glm::quat* rot, const glm::vec3* pos, float scale)
{
    if(num < 0)
    {
        return;
    }
    if(num >= static_cast<int>(draw.byNum.size()))
    {
        draw.byNum.resize(static_cast<za::SizeT>(num) + 32);
    }
    Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    p.rig = rig;
    p.bodies = bodies;
    p.scale = scale;
    for(int b = 0; b < bodies; b++)
    {
        p.rot[static_cast<za::SizeT>(b)] = rot[b];
        p.pos[static_cast<za::SizeT>(b)] = pos[b];
    }
}

void unpublish(int num)
{
    if(num >= 0 && num < static_cast<int>(draw.byNum.size()))
    {
        draw.byNum[static_cast<za::SizeT>(num)].rig = nullptr;
    }
}

void unpublishAll()
{
    for(Published& p : draw.byNum)
    {
        p.rig = nullptr;
    }
}

void swapModels()
{
    draw.swapped.clear();
    if(!sv.active || cls.state != ca_connected)
    {
        return;
    }
    for(int num = 1; num < static_cast<int>(draw.byNum.size()) && num < cl.num_entities; num++)
    {
        const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
        entity_t* e = &cl_entities[num];
        if(!p.rig || e->model != p.rig->model)
        {
            continue; // (the client shows something else: a head, a gib, nothing yet)
        }
        char name[MAX_QPATH];
        q_snprintf(name, sizeof(name), "%s%s", p.rig->model->name, skinnedSuffix);
        qmodel_t* skinned = Mod_ForName(name, false);
        if(!skinned || skinned->type != mod_alias)
        {
            continue;
        }
        Swapped s;
        s.num = num;
        s.original = e->model;
        s.skinned = skinned;
        s.ref = p.pos[0];
        s.bones = p.rig->numBones;
        for(int b = 0; b < s.bones; b++)
        {
            // (A hidden bone: all its vertices at the pelvis, its triangles gone.)
            const bool shown = b < p.bodies;
            const glm::mat3 r = shown ? glm::mat3_cast(p.rot[static_cast<za::SizeT>(b)]) * p.scale : glm::mat3{0.f};
            const glm::vec3 t = shown ? p.pos[static_cast<za::SizeT>(b)] - s.ref : glm::vec3{0.f};
            float* out = &s.skin[static_cast<za::SizeT>(b * 12)];
            for(int row = 0; row < 3; row++)
            {
                out[row * 4 + 0] = r[0][row];
                out[row * 4 + 1] = r[1][row];
                out[row * 4 + 2] = r[2][row];
                out[row * 4 + 3] = t[row];
            }
        }
        e->model = skinned;
        draw.swapped.pushBack(s);
    }
}

void restoreModels()
{
    for(const Swapped& s : draw.swapped)
    {
        entity_t* e = &cl_entities[s.num];
        if(e->model == s.skinned)
        {
            e->model = s.original;
        }
    }
    draw.swapped.clear();
}

int bonePoses(const entity_t* e, const float** matrices)
{
    const Swapped* s = swappedOf(e);
    if(!s || e->model != s->skinned)
    {
        return 0;
    }
    if(matrices)
    {
        *matrices = s->skin.data();
    }
    return s->bones;
}

bool drawMatrix(const entity_t* e, float matrix[16])
{
    const Swapped* s = swappedOf(e);
    if(!s || e->model != s->skinned)
    {
        return false;
    }
    for(int i = 0; i < 16; i++)
    {
        matrix[i] = i % 5 == 0 ? 1.f : 0.f;
    }
    matrix[12] = s->ref.x;
    matrix[13] = s->ref.y;
    matrix[14] = s->ref.z;
    return true;
}

bool skinnedVertices(int num, za::Vector<glm::vec3>& out, za::Vector<glm::vec3>* normals)
{
    if(num < 0 || num >= static_cast<int>(draw.byNum.size()) || !draw.byNum[static_cast<za::SizeT>(num)].rig)
    {
        return false;
    }
    const Published& p = draw.byNum[static_cast<za::SizeT>(num)];
    const Rig& rig = *p.rig;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(rig.model)));
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    out.resize(static_cast<za::SizeT>(rig.numVerts));
    if(normals)
    {
        normals->resize(static_cast<za::SizeT>(rig.numVerts));
    }
    for(int v = 0; v < rig.numVerts; v++)
    {
        const trivertx_t& t = tv[v]; // the rest pose (0)
        const glm::vec3 r{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
        const int b = rig.vertBone[static_cast<za::SizeT>(v)];
        out[static_cast<za::SizeT>(v)] = b < p.bodies ? p.rot[static_cast<za::SizeT>(b)] * (r * p.scale) + p.pos[static_cast<za::SizeT>(b)] : p.pos[0];
        if(normals)
        {
            const glm::vec3 n{r_avertexnormals[t.lightnormalindex][0], r_avertexnormals[t.lightnormalindex][1],
                r_avertexnormals[t.lightnormalindex][2]};
            (*normals)[static_cast<za::SizeT>(v)] = b < p.bodies ? p.rot[static_cast<za::SizeT>(b)] * n : glm::vec3{0.f};
        }
    }
    return true;
}

void info_f()
{
    qmodel_t* model = Mod_ForName("progs/soldier.mdl", false);
    if(Cmd_Argc() > 1)
    {
        model = Mod_ForName(Cmd_Argv(1), false);
    }
    if(!model)
    {
        Con_Printf("vr_ragdoll_info: no such model\n");
        return;
    }
    const Rig* rig = rigFor(model);
    if(!rig)
    {
        Con_Printf("vr_ragdoll_info: %s has no ragdoll (vr_ragdoll 1: the grunt's)\n", model->name);
        return;
    }
    Con_Printf("vr_ragdoll_info: %s: %d bones, %d vertices, %d poses; motion clusters %.2f units rms, bones %.2f; derived in %.1f ms\n",
        model->name, rig->numBones, rig->numVerts, rig->numPoses, rig->clusterRms, rig->boneRms, rig->deriveMs);
    for(int b = 0; b < rig->numBones; b++)
    {
        const Bone& bone = rig->bones[b];
        int n = 0;
        for(const uint8_t vb : rig->vertBone)
        {
            n += vb == b ? 1 : 0;
        }
        static constexpr const char* kinds[] = {"root", "ball", "hinge", "loose"};
        Con_Printf("  %2d %-11s parent %2d %-5s %3d verts (%2d places) pivot %5.1f %5.1f %5.1f", b, bone.name, bone.parent,
            kinds[static_cast<int>(bone.joint)], n, static_cast<int>(bone.points.size()), bone.pivot.x, bone.pivot.y, bone.pivot.z);
        if(bone.joint == Joint::Ball)
        {
            Con_Printf(" cone %.0f twist %.0f", bone.cone / deg, bone.twist / deg);
        }
        else if(bone.joint == Joint::Hinge)
        {
            Con_Printf(" axis %.2f %.2f %.2f range %.0f..%.0f", bone.hinge.x, bone.hinge.y, bone.hinge.z, bone.lower / deg, bone.upper / deg);
        }
        Con_Printf("\n");
    }
}

} // namespace qvr::ragdoll

// ----------------------------------------------------------------------------
// The skinned model ("<model>#rag"), made as Mod_LoadModel loads it: the .mdl's rest pose as a skeletal (IQM-style)
// mesh, each vertex on its bone, the bones' bind pose the rest pose itself; every frame shows its one pose. The skins
// are the .mdl's (its textures, not copies).
extern "C" int VR_SyntheticModel(qmodel_t* mod)
{
    const size_t len = strlen(mod->name), suffix = strlen(skinnedSuffix);
    if(len <= suffix || strcmp(mod->name + len - suffix, skinnedSuffix) != 0)
    {
        return false;
    }
    char base[MAX_QPATH];
    q_strlcpy(base, mod->name, za::min(sizeof(base), len - suffix + 1));
    qmodel_t* src = Mod_ForName(base, false);
    const Rig* rig = src ? ragdoll::rigFor(src) : nullptr;
    if(!rig)
    {
        return false;
    }
    const auto* sh = static_cast<const aliashdr_t*>(Mod_Extradata(src));
    const int numVerts = sh->numverts_vbo, numIndexes = sh->numindexes, numBones = rig->numBones, numFrames = za::max(sh->numframes, 1);

    const int start = Hunk_LowMark();
    const size_t hdrSize = sizeof(aliashdr_t) + sizeof(maliasframedesc_t) * static_cast<size_t>(numFrames - 1);
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(hdrSize)));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numBones));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numBones));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));

    hdr->ident = sh->ident;
    hdr->version = sh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = sh->boundingradius;
    hdr->numskins = sh->numskins;
    hdr->skinwidth = sh->skinwidth;
    hdr->skinheight = sh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = sh->numtris;
    hdr->numframes = numFrames;
    hdr->synctype = sh->synctype;
    hdr->flags = sh->flags;
    hdr->size = sh->size;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numBones;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    for(int f = 0; f < numFrames; f++)
    {
        hdr->frames[f] = sh->frames[za::min(f, sh->numframes - 1)];
        hdr->frames[f].firstpose = 0;
        hdr->frames[f].numposes = 1;
    }
    memcpy(hdr->gltextures, sh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, sh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, sh->texels, sizeof(hdr->texels));
    for(int b = 0; b < numBones; b++)
    {
        q_strlcpy(bones[b].name, rig->bones[b].name, sizeof(bones[b].name));
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(sh) + sh->meshdesc);
    const auto* tv = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(sh) + sh->vertexes); // pose 0
    for(int v = 0; v < numVerts; v++)
    {
        const trivertx_t& t = tv[desc[v].vertindex];
        iqmvert_t& o = verts[v];
        for(int i = 0; i < 3; i++)
        {
            o.xyz[i] = t.v[i] * sh->scale[i] + sh->scale_origin[i];
            o.norm[i] = static_cast<int8_t>(127.f * r_avertexnormals[t.lightnormalindex][i]);
        }
        o.norm[3] = 0;
        o.st[0] = (static_cast<float>(desc[v].st[0]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinwidth));
        o.st[1] = (static_cast<float>(desc[v].st[1]) + 0.5f) / static_cast<float>(TexMgr_PadConditional(sh->skinheight));
        o.weight[0] = 255;
        o.weight[1] = o.weight[2] = o.weight[3] = 0;
        o.idx[0] = rig->vertBone[desc[v].vertindex];
        o.idx[1] = o.idx[2] = o.idx[3] = 0;
    }
    memcpy(indexes, reinterpret_cast<const byte*>(sh) + sh->indexes, sizeof(unsigned short) * static_cast<size_t>(numIndexes));
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);

    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod); // (made again: its cache was given back)
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = numFrames;
    mod->synctype = src->synctype;
    mod->flags = src->flags;
    mod->path_id = src->path_id;
    // Its bounds: everything it can reach lying about (the renderer doesn't cull a posed entity by them).
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -64.f;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = 64.f;
    }
    const int total = Hunk_LowMark() - start;
    Cache_Alloc(&mod->cache, total, mod->name);
    if(!mod->cache.data)
    {
        Hunk_FreeToLowMark(start);
        return false;
    }
    memcpy(mod->cache.data, hdr, static_cast<size_t>(total));
    Hunk_FreeToLowMark(start);
    Con_DPrintf("ragdoll: %s made (%d vertices, %d bones)\n", mod->name, numVerts, numBones);
    return true;
}

extern "C" void VR_RagdollSwap(void)
{
    ragdoll::swapModels();
}

extern "C" void VR_RagdollRestore(void)
{
    ragdoll::restoreModels();
}
