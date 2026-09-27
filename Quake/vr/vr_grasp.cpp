// vr_grasp.cpp -- see vr_grasp.hpp.

#include "vr_grasp.hpp"
#include "vr_render.hpp"
#include "vr_api_render.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace qvr::grasp
{
namespace
{

using handrig::data::firstVertex;
using handrig::data::fingerTriangles;
using handrig::data::numFingerTriangles;
using handrig::data::vertices;

constexpr float maxCurl = 4.f; // the tightest fist
constexpr int halvings = 10;   // a contact found to 1/1000 of a step

// Closing a finger: the curl per step (the fingertips then move under a third of a finger's width), coarse when
// comparing placements of the hand.
constexpr float fineStep = 0.05f;
constexpr float coarseStep = 0.2f;

struct Box
{
    glm::vec3 lo{1e30f}, hi{-1e30f};

    void add(const glm::vec3& p)
    {
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    [[nodiscard]] bool overlaps(const Box& o) const
    {
        return lo.x <= o.hi.x && o.lo.x <= hi.x && lo.y <= o.hi.y && o.lo.y <= hi.y && lo.z <= o.hi.z && o.lo.z <= hi.z;
    }
};

[[nodiscard]] Box boxOf(const Triangle& t)
{
    Box b;
    for(const glm::vec3& p : t.p)
    {
        b.add(p);
    }
    return b;
}

// Whether segment a-b passes through triangle t (its inside, ends included).
[[nodiscard]] bool segmentHits(const glm::vec3& a, const glm::vec3& b, const Triangle& t)
{
    const glm::vec3 e1 = t.p[1] - t.p[0], e2 = t.p[2] - t.p[0], d = b - a;
    const glm::vec3 pv = glm::cross(d, e2);
    const float det = glm::dot(e1, pv);
    if(std::fabs(det) < 1e-12f)
    {
        return false;
    }
    const float inv = 1.f / det;
    const glm::vec3 tv = a - t.p[0];
    const float u = glm::dot(tv, pv) * inv;
    if(u < 0.f || u > 1.f)
    {
        return false;
    }
    const glm::vec3 qv = glm::cross(tv, e1);
    const float v = glm::dot(d, qv) * inv;
    if(v < 0.f || u + v > 1.f)
    {
        return false;
    }
    const float s = glm::dot(e2, qv) * inv;
    return s >= 0.f && s <= 1.f;
}

// Two triangles cross when an edge of one passes through the other.
[[nodiscard]] bool trianglesCross(const Triangle& a, const Triangle& b)
{
    for(int i = 0; i < 3; i++)
    {
        if(segmentHits(a.p[i], a.p[(i + 1) % 3], b) || segmentHits(b.p[i], b.p[(i + 1) % 3], a))
        {
            return true;
        }
    }
    return false;
}

// The held thing's triangles near the hand, in a grid of cells for the tests.
class Scene
{
public:
    Scene(const std::vector<Triangle>& all, const Box& region)
    {
        for(const Triangle& t : all)
        {
            const Box b = boxOf(t);
            if(b.overlaps(region))
            {
                tris.push_back(t);
                boxes.push_back(b);
                bounds.add(b.lo);
                bounds.add(b.hi);
            }
        }
        if(tris.empty())
        {
            return;
        }
        bounds.lo = glm::max(bounds.lo, region.lo);
        bounds.hi = glm::min(bounds.hi, region.hi);
        for(int k = 0; k < 3; k++)
        {
            size[k] = std::clamp(static_cast<int>((bounds.hi[k] - bounds.lo[k]) / cell) + 1, 1, 64);
        }
        cells.assign(static_cast<size_t>(size[0] * size[1] * size[2]), {});
        for(size_t i = 0; i < tris.size(); i++)
        {
            forCells(boxes[i], [&](std::vector<int>& c) { c.push_back(static_cast<int>(i)); });
        }
        stamps.assign(tris.size(), 0u);
    }

    [[nodiscard]] bool empty() const { return tris.empty(); }
    [[nodiscard]] int count() const { return static_cast<int>(tris.size()); }

    // Whether `t` crosses any of the held thing's triangles.
    [[nodiscard]] bool crosses(const Triangle& t) const
    {
        const Box b = boxOf(t);
        if(!b.overlaps(bounds))
        {
            return false;
        }
        stamp++;
        bool hit = false;
        forCells(b, [&](const std::vector<int>& c) {
            for(const int i : c)
            {
                if(hit || stamps[i] == stamp)
                {
                    continue;
                }
                stamps[i] = stamp;
                hit = b.overlaps(boxes[i]) && trianglesCross(t, tris[i]);
            }
        });
        return hit;
    }

private:
    static constexpr float cell = 1.f; // hand units

    template <typename F>
    void forCells(const Box& b, F&& f) const
    {
        int lo[3], hi[3];
        for(int k = 0; k < 3; k++)
        {
            lo[k] = std::clamp(static_cast<int>(std::floor((b.lo[k] - bounds.lo[k]) / cell)), 0, size[k] - 1);
            hi[k] = std::clamp(static_cast<int>(std::floor((b.hi[k] - bounds.lo[k]) / cell)), 0, size[k] - 1);
        }
        for(int z = lo[2]; z <= hi[2]; z++)
        {
            for(int y = lo[1]; y <= hi[1]; y++)
            {
                for(int x = lo[0]; x <= hi[0]; x++)
                {
                    f(const_cast<std::vector<int>&>(cells[static_cast<size_t>((z * size[1] + y) * size[0] + x)]));
                }
            }
        }
    }

    std::vector<Triangle> tris;
    std::vector<Box> boxes;
    Box bounds;
    int size[3]{1, 1, 1};
    std::vector<std::vector<int>> cells;
    mutable std::vector<std::uint32_t> stamps;
    mutable std::uint32_t stamp{0};
};

// A finger's triangles, by segment (the most distal bone of their vertices: 0 the root, glued to the palm).
struct FingerMesh
{
    std::vector<std::array<int, 3>> tris;
    std::vector<int> link;
};

const FingerMesh& fingerMesh(int finger)
{
    static const std::array<FingerMesh, handrig::FingerCount> meshes = [] {
        std::array<FingerMesh, handrig::FingerCount> m;
        for(int t = 0; t < numFingerTriangles; t++)
        {
            const auto* tri = fingerTriangles[t];
            const int f = vertices[tri[0]].finger;
            int link = 0;
            for(int k = 0; k < 3; k++)
            {
                link = std::max(link, static_cast<int>(vertices[tri[k]].bone));
            }
            m[f].tris.push_back({tri[0], tri[1], tri[2]});
            m[f].link.push_back(link);
        }
        return m;
    }();
    return meshes[finger];
}

// The segments (from `fromLink` on) of `finger` posed at `c`, the hand moved by `d`, that cross the held thing, as
// bits (1 << segment).
[[nodiscard]] unsigned crossing(const handrig::Pose& pose, int finger, const float c[3], int fromLink, const glm::vec3& d,
    const Scene& scene, handrig::Posed& posed)
{
    handrig::poseFinger(pose, finger, c, posed);
    const FingerMesh& mesh = fingerMesh(finger);
    unsigned hits = 0;
    for(size_t i = 0; i < mesh.tris.size(); i++)
    {
        const int link = mesh.link[i];
        if(link < fromLink || (hits & (1u << link)))
        {
            continue;
        }
        const Triangle t{{posed.vertex[mesh.tris[i][0]] + d, posed.vertex[mesh.tris[i][1]] + d, posed.vertex[mesh.tris[i][2]] + d}};
        if(scene.crosses(t))
        {
            hits |= 1u << link;
        }
    }
    return hits;
}

// Closes one finger (see vr_grasp.hpp), the hand moved by `d`, `step` curl a step.
void solveFinger(const handrig::Pose& pose, int finger, const glm::vec3& d, const Scene& scene, float step, bool settle,
    FingerStop& out)
{
    out = FingerStop{};
    handrig::Posed posed;
    float c[3]{0.f, 0.f, 0.f};
    int firstActive = 0; // joints from this one on still close
    float at = 0.f;
    if(crossing(pose, finger, c, 1, d, scene, posed))
    {
        // Open, the finger is already in it (a gun's frame, where the straight fingers point). Then it closes from
        // the other end: the tightest curl it is clear at, opening from the full fist (a thick grip through the
        // fist: the fingers open round it), and from there joint by joint as below.
        float free = -1.f;
        for(float t = maxCurl; t > 0.f; t -= step)
        {
            const float cs[3]{t, t, t};
            if(!crossing(pose, finger, cs, 1, d, scene, posed))
            {
                free = t;
                break;
            }
        }
        if(free < 0.f)
        {
            out.startsInside = true;
            return;
        }
        if(free < maxCurl)
        {
            float lo = free, hi = std::min(free + step, maxCurl);
            for(int h = 0; h < halvings; h++)
            {
                const float mid = 0.5f * (lo + hi);
                const float cs[3]{mid, mid, mid};
                (crossing(pose, finger, cs, 1, d, scene, posed) ? hi : lo) = mid;
            }
            free = lo;
        }
        out.fromClosed = true;
        at = free;
        c[0] = c[1] = c[2] = free;
    }
    while(firstActive < handrig::jointsPerFinger && at < maxCurl)
    {
        const float next = std::min(at + step, maxCurl);
        float trial[3];
        for(int j = 0; j < 3; j++)
        {
            trial[j] = j >= firstActive ? next : c[j];
        }
        unsigned hits = crossing(pose, finger, trial, firstActive + 1, d, scene, posed);
        if(!hits)
        {
            at = next;
            for(int j = firstActive; j < 3; j++)
            {
                c[j] = next;
            }
            continue;
        }
        // Halve the step to the contact: `lo` free, `hi` crossing.
        float lo = at, hi = next;
        for(int h = 0; h < halvings; h++)
        {
            const float mid = 0.5f * (lo + hi);
            for(int j = firstActive; j < 3; j++)
            {
                trial[j] = mid;
            }
            const unsigned m = crossing(pose, finger, trial, firstActive + 1, d, scene, posed);
            if(m)
            {
                hi = mid;
                hits = m;
            }
            else
            {
                lo = mid;
            }
        }
        at = lo;
        for(int j = firstActive; j < 3; j++)
        {
            c[j] = lo;
        }
        // The most distal segment met stops the joints that move it; those past it go on.
        int link = 3;
        while(link > 0 && !(hits & (1u << link)))
        {
            link--;
        }
        for(int j = firstActive; j < link; j++)
        {
            out.stop[j] = lo;
        }
        out.met = true;
        firstActive = std::max(firstActive, link);
    }

    // Then it settles round what it met: a joint closes further while a joint past it opens as much as that needs
    // (a fingertip slides along the surface as the knuckles close), as long as that closes the finger more, knuckles
    // first; the joints are never past the tightest fist.
    if(out.met && settle)
    {
        float s[3]{out.stop[0], out.stop[1], out.stop[2]};
        constexpr float weight[3] = {1.2f, 1.f, 0.8f};
        const auto value = [&](const float* c) { return weight[0] * c[0] + weight[1] * c[1] + weight[2] * c[2]; };
        for(float delta = 4.f * step; delta >= step * 0.99f; delta *= 0.5f)
        {
            for(bool better = true; better;)
            {
                better = false;
                for(int j = 0; j < 3 && !better; j++)
                {
                    if(s[j] + delta > maxCurl)
                    {
                        continue;
                    }
                    // Alone, or with one joint past it opening by as much.
                    for(int k = j; k < 3 && !better; k++)
                    {
                        float t[3]{s[0], s[1], s[2]};
                        t[j] += delta;
                        if(k > j)
                        {
                            t[k] -= delta;
                            if(t[k] < 0.f)
                            {
                                continue;
                            }
                        }
                        if(value(t) > value(s) + 1e-4f && !crossing(pose, finger, t, 1, d, scene, posed))
                        {
                            s[0] = t[0];
                            s[1] = t[1];
                            s[2] = t[2];
                            better = true;
                        }
                    }
                }
            }
        }
        out.stop[0] = s[0];
        out.stop[1] = s[1];
        out.stop[2] = s[2];
    }
}

// How well a finger holds: -1 in it at every curl, 0 touching nothing, else its closure where it stopped (0..1).
// A wrap (its joints stopped one after another: the segments round what it holds) beats a finger stopped by its tip.
[[nodiscard]] float score(const FingerStop& s)
{
    if(s.startsInside)
    {
        return -1.f;
    }
    if(!s.met)
    {
        return 0.f;
    }
    const float closure = (s.stop[0] + s.stop[1] + s.stop[2]) / (3.f * maxCurl);
    const float wrap = (s.stop[1] - s.stop[0] > 0.1f ? 0.5f : 0.f) + (s.stop[2] - s.stop[1] > 0.1f ? 0.5f : 0.f);
    return 0.6f * closure + 0.4f * wrap;
}

// The thumb's turn across the palm at its base (opposition): about the hand's long axis, from beside the index finger
// towards the palm, `degrees`.
[[nodiscard]] glm::quat opposition(float degrees)
{
    return glm::angleAxis(glm::radians(degrees), glm::vec3{-1.f, 0.f, 0.f});
}

// The thumb's swing in the palm's plane (about its normal): positive down towards the little finger's side,
// negative up and away (over the top of what the hand holds).
[[nodiscard]] glm::quat swing(float degrees)
{
    return glm::angleAxis(glm::radians(degrees), glm::vec3{0.f, 1.f, 0.f});
}

// The thumb: closed at each turn of its metacarpal (across the palm, and swung in its plane), the one holding best
// (the least turn when as good).
void solveThumb(handrig::Pose& pose, const glm::vec3& d, const Scene& scene, float step, FingerStop& out, glm::quat& turn)
{
    float best = -1e9f;
    const glm::quat keep = pose.metacarpal;
    // A turn costs a little (a thumb held naturally beats a contorted one that holds a little better).
    for(const float o : {0.f, 15.f, 30.f, 45.f})
    {
        for(const float s : {0.f, -15.f, 15.f, -30.f})
        {
            const glm::quat q = swing(s) * opposition(o);
            pose.metacarpal = q;
            FingerStop st;
            solveFinger(pose, handrig::Thumb, d, scene, step, true, st);
            const float value = score(st) - 0.004f * (std::fabs(o) + std::fabs(s));
            if(!st.startsInside && value > best)
            {
                best = value;
                out = st;
                turn = q;
            }
        }
    }
    if(best > -1e9f)
    {
        pose.metacarpal = keep;
        return;
    }

    // In it at every turn and curl (its base in a grip): as little in it as it can be, among the natural turns.
    handrig::Posed posed;
    const FingerMesh& mesh = fingerMesh(handrig::Thumb);
    int fewest = 1 << 30;
    for(const float o : {0.f, 15.f, 30.f, 45.f})
    {
        for(const float s : {0.f, -15.f, 15.f})
        {
            pose.metacarpal = swing(s) * opposition(o);
            for(float c = 0.f; c <= maxCurl; c += 1.f)
            {
                const float cs[3]{c, c, c};
                handrig::poseFinger(pose, handrig::Thumb, cs, posed);
                int crossings = 0;
                for(size_t i = 0; i < mesh.tris.size(); i++)
                {
                    const Triangle t{{posed.vertex[mesh.tris[i][0]] + d, posed.vertex[mesh.tris[i][1]] + d,
                        posed.vertex[mesh.tris[i][2]] + d}};
                    crossings += scene.crosses(t) ? 1 : 0;
                }
                if(crossings < fewest)
                {
                    fewest = crossings;
                    turn = pose.metacarpal;
                    out = FingerStop{};
                    out.met = true;
                    out.leastInside = true;
                    out.stop[0] = out.stop[1] = out.stop[2] = c;
                }
            }
        }
    }
    pose.metacarpal = keep;
}

// The palm and the fingers' roots (glued to it), as triangles; not the thumb's socket and the thenar round it (the
// metacarpal's: the ball of the thumb gives round what the hand holds, and the thumb closes on its own).
void palmTriangles(const handrig::Pose& pose, std::vector<Triangle>& out)
{
    out.clear();
    handrig::Posed posed;
    handrig::pose(pose, posed);
    for(int t = 0; t < handrig::data::numPalmTriangles; t++)
    {
        const auto* tri = handrig::data::palmTriangles[t];
        if(handrig::data::palmVertices[tri[0]].thumb > 0.f || handrig::data::palmVertices[tri[1]].thumb > 0.f ||
            handrig::data::palmVertices[tri[2]].thumb > 0.f)
        {
            continue;
        }
        out.push_back({{handrig::palmVertex(posed, tri[0]), handrig::palmVertex(posed, tri[1]), handrig::palmVertex(posed, tri[2])}});
    }
    for(int f = handrig::Index; f < handrig::FingerCount; f++)
    {
        const FingerMesh& mesh = fingerMesh(f);
        for(size_t i = 0; i < mesh.tris.size(); i++)
        {
            if(mesh.link[i] == 0)
            {
                out.push_back({{posed.vertex[mesh.tris[i][0]], posed.vertex[mesh.tris[i][1]], posed.vertex[mesh.tris[i][2]]}});
            }
        }
    }
}

[[nodiscard]] bool palmCrosses(const std::vector<Triangle>& palm, const glm::vec3& d, const Scene& scene)
{
    for(const Triangle& p : palm)
    {
        if(scene.crosses(Triangle{{p.p[0] + d, p.p[1] + d, p.p[2] + d}}))
        {
            return true;
        }
    }
    return false;
}

// Along the palm's normal (+y) from `from`: the place where the palm just touches the held thing, coming from the
// side away from it (from `from.y - reach` up to `from.y + reach`: the last clear place before it is met; if clear
// all the way, none). False if there is none.
[[nodiscard]] bool flush(const std::vector<Triangle>& palm, const Scene& scene, const glm::vec3& from, float reach, glm::vec3& out)
{
    constexpr float move = 0.25f;
    bool wasClear = false;
    float clearAt = 0.f;
    for(float t = -reach; t <= reach + 1e-4f; t += move)
    {
        const bool clear = !palmCrosses(palm, from + glm::vec3{0.f, t, 0.f}, scene);
        if(clear)
        {
            wasClear = true;
            clearAt = t;
            continue;
        }
        if(!wasClear)
        {
            continue; // still in it on the far side
        }
        float lo = clearAt, hi = t;
        for(int h = 0; h < halvings; h++)
        {
            const float mid = 0.5f * (lo + hi);
            (palmCrosses(palm, from + glm::vec3{0.f, mid, 0.f}, scene) ? hi : lo) = mid;
        }
        out = from + glm::vec3{0.f, lo, 0.f};
        return true;
    }
    return false;
}

struct Placement
{
    glm::vec3 d{0.f};
    FingerStop fingers[handrig::FingerCount];
    glm::quat thumbTurn{1.f, 0.f, 0.f, 0.f};
    float total{-1e30f};
};

// The hand at `d`: its fingers closed, scored: how well its fingers hold, less a little for how far it moved from
// the controller (a move along the fingers, x, three times). A quick look (`full` false) closes the middle, ring and
// little fingers coarsely; the full solve all five finely, settled, the thumb at each opposition.
void place(handrig::Pose& pose, const Scene& scene, const glm::vec3& d, bool full, Placement& p)
{
    p.d = d;
    p.total = -0.1f * glm::length(glm::vec3{3.f * d.x, d.y, d.z});
    const float step = full ? fineStep : coarseStep;
    if(full)
    {
        solveThumb(pose, d, scene, step, p.fingers[handrig::Thumb], p.thumbTurn);
        p.total += score(p.fingers[handrig::Thumb]);
    }
    for(int f = full ? handrig::Index : handrig::Middle; f < handrig::FingerCount; f++)
    {
        solveFinger(pose, f, d, scene, step, full, p.fingers[f]);
        p.total += score(p.fingers[f]);
    }
}

} // namespace

bool worldTriangles(const entity_t& e, bool mirrored, int frame, std::vector<Triangle>& out)
{
    out.clear();
    const qmodel_t* model = e.model;
    if(!model)
    {
        return false;
    }
    float m[16];
    const auto apply = [&](const glm::vec3& v) {
        return glm::vec3{m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12], m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13],
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]};
    };

    if(model->type == mod_brush)
    {
        vec3_t origin, angles{-e.angles[0], e.angles[1], e.angles[2]};
        VectorCopy(e.origin, origin);
        R_EntityMatrix(m, origin, angles, e.scale);
        VR_BrushTransform(&e, m);
        for(int i = 0; i < model->nummodelsurfaces; i++)
        {
            const msurface_t& surf = model->surfaces[model->firstmodelsurface + i];
            const auto vertex = [&](int k) {
                const int ed = model->surfedges[surf.firstedge + k];
                const mvertex_t& v = model->vertexes[ed >= 0 ? model->edges[ed].v[0] : model->edges[-ed].v[1]];
                return apply(glm::vec3{v.position[0], v.position[1], v.position[2]});
            };
            for(int k = 2; k < surf.numedges; k++)
            {
                out.push_back({{vertex(0), vertex(k - 1), vertex(k)}});
            }
        }
        return !out.empty();
    }

    if(model->type != mod_alias)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc || hdr->numframes <= 0)
    {
        return false;
    }
    render::entityMatrix(e, mirrored, e.scale, glm::vec3{0.f}, m);
    const int f = frame >= 0 ? frame : e.frame;
    const int pose = hdr->frames[f >= 0 && f < hdr->numframes ? f : 0].firstpose;
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + pose * hdr->numverts;
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        Triangle t;
        for(int k = 0; k < 3; k++)
        {
            const trivertx_t& v = verts[mesh[indexes[i + k]].vertindex];
            t.p[k] = apply(glm::vec3{v.v[0], v.v[1], v.v[2]});
        }
        out.push_back(t);
    }
    return !out.empty();
}

void solve(const handrig::Pose& start, const std::vector<Triangle>& tris, Solution& out, float palmLimit)
{
    const auto t0 = std::chrono::steady_clock::now();
    out = Solution{};
    handrig::Pose pose = start;
    pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};

    // What the hand can reach: its bounds, fully open or closed, and as far as it may move.
    Box region;
    {
        handrig::Posed posed;
        for(const float c : {0.f, 1.5f, 3.f, 4.f})
        {
            handrig::Pose p = pose;
            for(auto& finger : p.curl)
            {
                finger[0] = finger[1] = finger[2] = c;
            }
            for(const float o : {0.f, 60.f})
            {
                p.metacarpal = opposition(o);
                handrig::pose(p, posed);
                for(const glm::vec3& v : posed.vertex)
                {
                    region.add(v);
                }
                for(int i = 0; i < handrig::data::numPalmVertices; i++)
                {
                    region.add(handrig::palmVertex(posed, i));
                }
            }
        }
        region.lo -= glm::vec3{palmLimit + 1.f};
        region.hi += glm::vec3{palmLimit + 1.f};
    }
    const Scene scene(tris, region);
    out.triangles = scene.count();
    if(scene.empty())
    {
        out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        return;
    }

    // Where the palm may sit: as the controller has it if clear, and flush along its normal (in to touch what it
    // holds, or out of it); if it is in the held thing (a gun's grip through the hand), also a little lower or
    // higher along the grip (z) and back (x), flush there. The fingers are closed at each (coarsely) and the one
    // holding best wins, solved again finely.
    std::vector<Triangle> palm;
    palmTriangles(pose, palm);
    const bool inside = palmCrosses(palm, glm::vec3{0.f}, scene);
    int tried = 0;
    Placement best;
    const auto consider = [&](float dx, float dz) {
        const float reach = std::sqrt(std::fmax(palmLimit * palmLimit - dx * dx - dz * dz, 0.f));
        glm::vec3 at;
        if(reach > 0.f && flush(palm, scene, glm::vec3{dx, 0.f, dz}, reach, at))
        {
            Placement p;
            place(pose, scene, at, false, p);
            tried++;
            if(p.total > best.total)
            {
                best = p;
            }
        }
    };
    if(!inside)
    {
        place(pose, scene, glm::vec3{0.f}, false, best);
        tried++;
    }
    if(palmLimit > 0.f)
    {
        if(!inside)
        {
            consider(0.f, 0.f); // flush along the palm's normal
        }
        else
        {
            // A grip through the hand: a coarse grid of places along the fingers (x) and the grip (z), then finer
            // round the best.
            for(const float dx : {-1.f, 0.f, 1.f, 2.f})
            {
                for(const float dz : {-3.f, -2.f, -1.f, 0.f, 1.f})
                {
                    consider(dx, dz);
                }
            }
            if(tried > 0)
            {
                const glm::vec3 centre = best.d;
                for(const float ox : {-0.5f, 0.f, 0.5f})
                {
                    for(const float oz : {-0.5f, 0.f, 0.5f})
                    {
                        if(ox != 0.f || oz != 0.f)
                        {
                            consider(centre.x + ox, centre.z + oz);
                        }
                    }
                }
            }
        }
    }
    if(tried == 0)
    {
        best.d = glm::vec3{0.f}; // in it wherever it may go: the fingers do what they can
    }
    Placement final;
    place(pose, scene, best.d, true, final);
    out.palm = final.d;
    out.thumbTurn = final.thumbTurn;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        out.finger[f] = final.fingers[f];
    }
    out.places = tried;
    out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

float pathCurl(float curl)
{
    const float c = std::fmin(std::fmax(curl, 0.f), 5.f);
    return c <= maxCurl ? c : 2.f * maxCurl - c;
}

void curls(const FingerStop& stop, float curl, float engage, float out[handrig::jointsPerFinger])
{
    const float path = pathCurl(curl);
    if(!stop.met)
    {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            out[j] = path;
        }
        return;
    }
    const float reach = path + std::fmin(std::fmax(engage, 0.f), 1.f) * (maxCurl - path);
    for(int j = 0; j < handrig::jointsPerFinger; j++)
    {
        out[j] = std::fmin(stop.stop[j], reach);
    }
}

} // namespace qvr::grasp
