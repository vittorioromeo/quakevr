// vr_handrig.cpp -- see vr_handrig.hpp.
//
// The rig from the file (round 21, "Hand editable in Blender"; the author's guide: docs/vr-port/HANDS_IN_BLENDER.md):
//
// - Read from progs/hand_rig.md5mesh: the mesh (each vertex's rest place: its weights' joint + offset, their weighted
//   mean in double; the files are written so that this is exactly the float asked for, see md5hand.py), its
//   triangles, its weights, and the joints' pivots (a finger segment's joint sits at the pivot it turns about). The
//   joints must be the 33 this code knows, by name and in order; progs/hand_rig.md5anim must list them too (the
//   engine's MD5 loader needs it).
// - Derived as it is read:
//   - Each hinge's axis turns with its segment's direction: if the pivots put a segment another way than the compiled
//     rig's, that joint's turns (per curl frame) are turned by the least rotation between the two. The curl frames'
//     angles themselves are the compiled ones: they are the hand's motion, not its shape.
//   - The grasp solver's spheres are the compiled ones (tuned with the solver) moved and resized by how the mesh
//     around each of them changed from the compiled mesh: a finger segment's by its section there (where its palm's
//     side is, how wide it is, and where along the segment it sits, the segment's length measured from the pivots or,
//     for a fingertip, from the mesh); the palm's and the thenar's by the skin over each (measured along the line to
//     its nearest point on the compiled mesh). The grip channel (vr_grasp.cpp) comes from the spheres.
//   With the shipped files every measurement is the same number as on the compiled mesh, so every sphere comes out
//   the same, bit for bit (vr_hand_rig_info says so).
// - Kept whatever the file says: the palm's frame (the palm joint doesn't move: it is rig space), the placement
//   constants and palmCentre (weapon placements and cups are measured from them: they stay where they were on the
//   controller, and an edited hand changes shape around them), the wrist (where the arm meets the hand).

#include "vr_handrig.hpp"
#include "vr_mem.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Count.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/constants.hpp>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::handrig
{
namespace
{

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

// ----------------------------------------------------------------------------
// The compiled rig (vr_handrig_data.inc)

Rig compiledRig()
{
    Rig r;
    r.source = "compiled";
    for(int f = 0; f < FingerCount; f++)
    {
        for(int k = 0; k < jointsPerFinger; k++)
        {
            r.pivot[f][k] = vec(data::pivots[f][k]);
            for(int fr = 0; fr < data::numFrames; fr++)
            {
                const float* q = data::turns[f][fr][k];
                r.turns[f][fr][k] = glm::quat{q[3], q[0], q[1], q[2]};
            }
        }
    }
    // The palm's vertices, then the fingers' (the fingers' first rings and the webs are in both: duplicates).
    r.vertices.reserve(data::numPalmVertices + data::numVertices);
    for(const data::PalmVertex& pv : data::palmVertices)
    {
        Vertex v;
        v.pos = vec(pv.pos);
        v.count = pv.count;
        za::copy(pv.joint, pv.joint + 4, v.joint);
        za::copy(pv.weight, pv.weight + 4, v.weight);
        r.vertices.pushBack(v);
    }
    for(const data::Vertex& dv : data::vertices)
    {
        Vertex v;
        v.pos = vec(dv.pos);
        v.count = dv.count;
        za::copy(dv.joint, dv.joint + 4, v.joint);
        za::copy(dv.weight, dv.weight + 4, v.weight);
        r.vertices.pushBack(v);
    }
    for(const auto& t : data::palmTriangles)
    {
        r.triangles.pushBack({t[0], t[1], t[2]});
    }
    for(const auto& t : data::fingerTriangles)
    {
        r.triangles.pushBack({data::numPalmVertices + t[0], data::numPalmVertices + t[1], data::numPalmVertices + t[2]});
    }
    for(const data::SegmentSphere& s : data::segmentSpheres)
    {
        r.segmentSpheres.pushBack({s.finger, s.bone, vec(s.c), s.r});
    }
    for(const auto& s : data::palmSpheres)
    {
        r.palmSpheres.pushBack({vec(s), s[3]});
    }
    for(const auto& s : data::thenarSpheres)
    {
        r.thenarSpheres.pushBack({vec(s), s[3]});
    }
    return r;
}

// The shipped hand: the reference edits are measured against.
const Rig& reference()
{
    static const Rig r = compiledRig();
    return r;
}

Rig& current()
{
    static Rig r = compiledRig();
    return r;
}

unsigned rigGeneration = 1;
bool reloading = false; // vr_hand_reload is loading the model: its report is printed, not only in developer mode

[[nodiscard]] glm::quat turnAt(int finger, int joint, int frame)
{
    return current().turns[finger][frame][joint];
}

[[nodiscard]] glm::quat turn(int finger, int joint, float curl)
{
    const float c = za::fmin(za::fmax(curl, 0.f), static_cast<float>(data::numFrames - 1));
    const int a = za::min(static_cast<int>(c), data::numFrames - 2);
    return glm::slerp(turnAt(finger, joint, a), turnAt(finger, joint, a + 1), c - static_cast<float>(a));
}

// A turn about a joint's pivot.
[[nodiscard]] Rigid about(int finger, int joint, const glm::quat& q)
{
    const glm::mat3 r = glm::mat3_cast(q);
    const glm::vec3 pivot = current().pivot[finger][joint];
    return {r, pivot - r * pivot};
}

void segments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1],
    glm::quat turns[jointsPerFinger])
{
    out[0] = {glm::mat3{1.f}, p.shift[finger]};
    for(int k = 0; k < jointsPerFinger; k++)
    {
        glm::quat q = turn(finger, k, curls[k]);
        if(finger == Thumb && k == 0)
        {
            q = p.metacarpal * q;
        }
        q = glm::normalize(q);
        turns[k] = q;
        out[k + 1] = out[k] * about(finger, k, q);
    }
}

// A vertex where its joints put it (the GPU's blend).
[[nodiscard]] glm::vec3 blend(const Posed& posed, const Vertex& v)
{
    if(v.count == 1)
    {
        return posed.joint[v.joint[0]](v.pos);
    }
    glm::vec3 out{0.f};
    for(int i = 0; i < v.count; i++)
    {
        out += v.weight[i] * posed.joint[v.joint[i]](v.pos);
    }
    return out;
}

struct ModelCheck
{
    qmodel_t* model{nullptr};
    za::String name; // the model's (its slot is reused by another after a game change)
    bool usable{false};
};
ModelCheck checked;

// ----------------------------------------------------------------------------
// Reading the MD5 files

za::String format(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    q_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

// Tokens as md5hand.py reads them: quoted names, ( ) { } alone, // comments skipped. The first error sticks.
class Reader
{
public:
    Reader(const char* text, const char* what) : p_(text), what_(what) {}

    [[nodiscard]] bool failed() const { return !error_.empty(); }
    [[nodiscard]] const za::String& error() const { return error_; }

    void fail(const za::String& message)
    {
        if(error_.empty())
        {
            error_ = format("%s, line %d: %s", what_, line_, message.cStr());
        }
    }

    [[nodiscard]] za::String next()
    {
        if(failed())
        {
            return {};
        }
        for(;;)
        {
            while(*p_ && (*p_ == ' ' || *p_ == '\t' || *p_ == '\r' || *p_ == '\n'))
            {
                line_ += *p_ == '\n';
                p_++;
            }
            if(p_[0] == '/' && p_[1] == '/')
            {
                while(*p_ && *p_ != '\n')
                {
                    p_++;
                }
                continue;
            }
            break;
        }
        if(!*p_)
        {
            fail("the file ends early");
            return {};
        }
        if(*p_ == '"')
        {
            const char* end = strchr(p_ + 1, '"');
            if(!end)
            {
                fail("a name's closing quote is missing");
                return {};
            }
            za::String s{p_ + 1, static_cast<za::SizeT>(end - (p_ + 1))};
            p_ = end + 1;
            return s;
        }
        if(strchr("(){}", *p_))
        {
            const char* c = p_++;
            return za::String{c, 1};
        }
        const char* start = p_;
        while(*p_ && !strchr(" \t\r\n(){}\"", *p_))
        {
            p_++;
        }
        return za::String{start, static_cast<za::SizeT>(p_ - start)};
    }

    [[nodiscard]] za::String peek()
    {
        const char* p = p_;
        const int line = line_;
        za::String s = next();
        p_ = p;
        line_ = line;
        return s;
    }

    void expect(const char* s)
    {
        const za::String t = next();
        if(!failed() && t != s)
        {
            fail(format("expected \"%s\", found \"%s\"", s, t.cStr()));
        }
    }

    [[nodiscard]] long integer()
    {
        const za::String t = next();
        if(failed())
        {
            return 0;
        }
        char* end = nullptr;
        const long v = strtol(t.cStr(), &end, 10);
        if(t.empty() || *end)
        {
            fail(format("expected a whole number, found \"%s\"", t.cStr()));
        }
        return v;
    }

    [[nodiscard]] double number()
    {
        const za::String t = next();
        if(failed())
        {
            return 0.0;
        }
        char* end = nullptr;
        const double v = strtod(t.cStr(), &end);
        if(t.empty() || *end)
        {
            fail(format("expected a number, found \"%s\"", t.cStr()));
        }
        else if(!ZA_ISFINITE(v))
        {
            fail(format("%s is not a finite number", t.cStr()));
        }
        return v;
    }

    template <typename T>
    void numbers(T* out, int n)
    {
        expect("(");
        for(int i = 0; i < n; i++)
        {
            out[i] = static_cast<T>(number());
        }
        expect(")");
    }

private:
    const char* p_;
    const char* what_;
    int line_{1};
    za::String error_;
};

struct Md5Joint
{
    za::String name;
    long parent{0};
    float pos[3]{};
    float quat[3]{};
};

struct Md5Weight
{
    long joint{0};
    float bias{0.f};
    double offset[3]{}; // read in double: see the top
};

struct Md5Vert
{
    long first{0}, count{0};
};

struct Md5Mesh
{
    za::Vector<Md5Joint> joints;
    za::Vector<Md5Vert> verts;
    za::Vector<za::Array<int, 3>> tris;
    za::Vector<Md5Weight> weights;
};

bool parseMesh(const char* text, Md5Mesh& m, za::String& error)
{
    Reader r(text, meshFile);
    r.expect("MD5Version");
    if(r.integer() != 10 && !r.failed())
    {
        r.fail("not MD5Version 10");
    }
    if(r.peek() == "commandline")
    {
        (void)r.next();
        (void)r.next();
    }
    r.expect("numJoints");
    const long numJoints = r.integer();
    r.expect("numMeshes");
    const long numMeshes = r.integer();
    if(!r.failed() && numMeshes != 1)
    {
        r.fail(format("%ld meshes: the hand is one mesh (in Blender, one object)", numMeshes));
    }
    if(!r.failed() && (numJoints < 1 || numJoints > 256))
    {
        r.fail(format("%ld joints", numJoints));
    }
    r.expect("joints");
    r.expect("{");
    for(long j = 0; j < numJoints && !r.failed(); j++)
    {
        Md5Joint jt;
        jt.name = r.next();
        jt.parent = r.integer();
        r.numbers(jt.pos, 3);
        r.numbers(jt.quat, 3);
        m.joints.pushBack(jt);
    }
    r.expect("}");
    r.expect("mesh");
    r.expect("{");
    if(r.peek() == "shader")
    {
        (void)r.next();
        (void)r.next();
    }
    r.expect("numverts");
    const long numVerts = r.integer();
    if(!r.failed() && (numVerts < 3 || numVerts > 65535))
    {
        r.fail(format("%ld vertices", numVerts));
    }
    for(long v = 0; v < numVerts && !r.failed(); v++)
    {
        r.expect("vert");
        (void)r.integer();
        float st[2];
        r.numbers(st, 2);
        Md5Vert mv;
        mv.first = r.integer();
        mv.count = r.integer();
        m.verts.pushBack(mv);
    }
    r.expect("numtris");
    const long numTris = r.integer();
    if(!r.failed() && (numTris < 1 || numTris > 65535))
    {
        r.fail(format("%ld triangles", numTris));
    }
    for(long t = 0; t < numTris && !r.failed(); t++)
    {
        r.expect("tri");
        (void)r.integer();
        za::Array<int, 3> tri;
        for(int& i : tri)
        {
            i = static_cast<int>(r.integer());
        }
        m.tris.pushBack(tri);
    }
    r.expect("numweights");
    const long numWeights = r.integer();
    if(!r.failed() && (numWeights < 1 || numWeights > 1 << 20))
    {
        r.fail(format("%ld weights", numWeights));
    }
    for(long w = 0; w < numWeights && !r.failed(); w++)
    {
        r.expect("weight");
        (void)r.integer();
        Md5Weight mw;
        mw.joint = r.integer();
        mw.bias = static_cast<float>(r.number());
        r.numbers(mw.offset, 3);
        m.weights.pushBack(mw);
    }
    r.expect("}");
    error = r.error();
    return !r.failed();
}

// The md5anim's joints (the engine's MD5 loader refuses a model whose anim doesn't list the mesh's joints).
bool parseAnim(const char* text, za::Vector<qza::Pair<za::String, long>>& hierarchy, za::String& error)
{
    Reader r(text, animFile);
    r.expect("MD5Version");
    (void)r.integer();
    if(r.peek() == "commandline")
    {
        (void)r.next();
        (void)r.next();
    }
    r.expect("numFrames");
    const long frames = r.integer();
    r.expect("numJoints");
    const long joints = r.integer();
    r.expect("frameRate");
    (void)r.number();
    r.expect("numAnimatedComponents");
    (void)r.integer();
    if(!r.failed() && frames < 1)
    {
        r.fail("no frames");
    }
    r.expect("hierarchy");
    r.expect("{");
    for(long j = 0; j < joints && j < 256 && !r.failed(); j++)
    {
        za::String name = r.next();
        const long parent = r.integer();
        (void)r.integer();
        (void)r.integer();
        hierarchy.emplaceBack(name, parent);
    }
    r.expect("}");
    error = r.error();
    return !r.failed();
}

// A vertex's rest place from its weights (as md5hand.py's rest_position): in double, the offsets turned by their
// joint's orientation (MD5: w the negative root), their weighted mean rounded to float.
[[nodiscard]] glm::vec3 restPlace(const Md5Mesh& m, const Md5Vert& v)
{
    double acc[3]{}, total = 0.0;
    for(long i = 0; i < v.count; i++)
    {
        const Md5Weight& w = m.weights[v.first + i];
        const Md5Joint& j = m.joints[w.joint];
        const double x = j.quat[0], y = j.quat[1], z = j.quat[2];
        double p[3] = {w.offset[0], w.offset[1], w.offset[2]};
        if(x != 0.0 || y != 0.0 || z != 0.0)
        {
            const double t = 1.0 - (x * x + y * y + z * z);
            const double qw = t > 0.0 ? -za::sqrt(t) : 0.0;
            const double* o = w.offset;
            const double tx = 2.0 * (y * o[2] - z * o[1]), ty = 2.0 * (z * o[0] - x * o[2]), tz = 2.0 * (x * o[1] - y * o[0]);
            p[0] = o[0] + qw * tx + (y * tz - z * ty);
            p[1] = o[1] + qw * ty + (z * tx - x * tz);
            p[2] = o[2] + qw * tz + (x * ty - y * tx);
        }
        const double b = w.bias;
        for(int k = 0; k < 3; k++)
        {
            acc[k] += b * (static_cast<double>(j.pos[k]) + p[k]);
        }
        total += b;
    }
    return {static_cast<float>(acc[0] / total), static_cast<float>(acc[1] / total), static_cast<float>(acc[2] / total)};
}

// ----------------------------------------------------------------------------
// Measuring a mesh (the derivation's)

// Each vertex's finger (-1 the palm) and segment (0..3), from its heaviest joint: a helper counts for the segment
// after its joint when it turns half as far or more (the ring at a joint), else for the one before (the thenar's).
struct Classes
{
    za::Vector<int> finger, segment;
};

Classes classify(const Rig& r)
{
    Classes c;
    for(const Vertex& v : r.vertices)
    {
        int best = 0;
        for(int i = 1; i < v.count; i++)
        {
            if(v.weight[i] > v.weight[best])
            {
                best = i;
            }
        }
        const data::Joint& j = data::joints[v.joint[best]];
        switch(j.kind)
        {
        case data::PalmJoint:
            c.finger.pushBack(-1);
            c.segment.pushBack(0);
            break;
        case data::SegmentJoint:
            c.finger.pushBack(j.finger);
            c.segment.pushBack(j.index);
            break;
        case data::PartJoint:
            c.finger.pushBack(j.finger);
            c.segment.pushBack(j.share >= 0.5f ? j.index + 1 : j.index);
            break;
        }
    }
    return c;
}

// Positions compared as numbers: the order that makes a measurement the same whichever way a triangle lists them.
[[nodiscard]] bool before(const glm::vec3& a, const glm::vec3& b)
{
    return a.x != b.x ? a.x < b.x : a.y != b.y ? a.y < b.y : a.z < b.z;
}

// A finger's section by the plane through `a` across `x`: how far its skin reaches along D (the palm's side), and
// from where to where along E (across). Only that finger's triangles past the palm, within `reach` of `a`.
struct Section
{
    bool ok{false};
    float reach{-1e30f}, back{1e30f}, lo{1e30f}, hi{-1e30f};
};

Section section(const Rig& r, const Classes& c, int finger, const glm::vec3& a, const glm::vec3& x, const glm::vec3& D,
    const glm::vec3& E)
{
    constexpr float reach = 3.f;
    Section s;
    for(const auto& t : r.triangles)
    {
        bool mine = true;
        for(const int i : t)
        {
            mine = mine && c.finger[i] == finger && c.segment[i] >= 1;
        }
        if(!mine)
        {
            continue;
        }
        for(int e = 0; e < 3; e++)
        {
            glm::vec3 p = r.vertices[t[e]].pos, q = r.vertices[t[(e + 1) % 3]].pos;
            if(before(q, p))
            {
                za::genericSwap(p, q);
            }
            const float dp = glm::dot(p - a, x), dq = glm::dot(q - a, x);
            if((dp > 0.f) == (dq > 0.f))
            {
                continue;
            }
            const glm::vec3 rel = p + (q - p) * (dp / (dp - dq)) - a;
            if(glm::dot(rel, rel) > reach * reach)
            {
                continue;
            }
            const float u = glm::dot(rel, D), w = glm::dot(rel, E);
            s.ok = true;
            s.reach = za::fmax(s.reach, u);
            s.back = za::fmin(s.back, u);
            s.lo = za::fmin(s.lo, w);
            s.hi = za::fmax(s.hi, w);
        }
    }
    s.ok = s.ok && s.lo < 0.f && s.hi > 0.f && s.back < 0.f && s.reach > 0.f; // all round the axis: a whole section
    return s;
}

// How far a finger's last segment reaches past its last joint, along x.
[[nodiscard]] float tipLength(const Rig& r, const Classes& c, int finger, const glm::vec3& pivot, const glm::vec3& x)
{
    float most = 0.f;
    for(size_t i = 0; i < r.vertices.size(); i++)
    {
        if(c.finger[i] == finger && c.segment[i] == 3)
        {
            most = za::fmax(most, glm::dot(r.vertices[i].pos - pivot, x));
        }
    }
    return most;
}

// The point of triangle abc nearest p (Ericson, Real-Time Collision Detection, 5.1.5).
[[nodiscard]] glm::vec3 closestOnTriangle(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return a + ab * (d1 / (d1 - d3));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denom = 1.f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

[[nodiscard]] glm::vec3 nearestOnMesh(const Rig& r, const glm::vec3& p)
{
    glm::vec3 best = p;
    float bestD = 1e30f;
    for(const auto& t : r.triangles)
    {
        const glm::vec3 q = closestOnTriangle(p, r.vertices[t[0]].pos, r.vertices[t[1]].pos, r.vertices[t[2]].pos);
        const float d = glm::dot(q - p, q - p);
        if(d < bestD)
        {
            bestD = d;
            best = q;
        }
    }
    return best;
}

// Where the line o + dir t meets the mesh nearest t = want (within `window`).
[[nodiscard]] bool meetNear(const Rig& r, const glm::vec3& o, const glm::vec3& dir, float want, float window, float& out)
{
    bool found = false;
    float bestGap = window;
    for(const auto& t : r.triangles)
    {
        glm::vec3 v[3] = {r.vertices[t[0]].pos, r.vertices[t[1]].pos, r.vertices[t[2]].pos};
        za::quickSort(v, v + 3, before);
        const glm::vec3 e1 = v[1] - v[0], e2 = v[2] - v[0];
        const glm::vec3 pv = glm::cross(dir, e2);
        const float det = glm::dot(e1, pv);
        if(za::fabs(det) < 1e-12f)
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 tv = o - v[0];
        const float u = glm::dot(tv, pv) * inv;
        constexpr float edge = 1e-4f; // a line through an edge or a corner (the nearest point often is one) meets it
        if(u < -edge || u > 1.f + edge)
        {
            continue;
        }
        const glm::vec3 qv = glm::cross(tv, e1);
        const float w = glm::dot(dir, qv) * inv;
        if(w < -edge || u + w > 1.f + edge)
        {
            continue;
        }
        const float d = glm::dot(e2, qv) * inv;
        if(za::fabs(d - want) < bestGap)
        {
            bestGap = za::fabs(d - want);
            out = d;
            found = true;
        }
    }
    return found;
}

// A segment's direction at rest (the last segment's is the middle one's: no pivot past it).
[[nodiscard]] glm::vec3 segmentDir(const Rig& r, int finger, int bone)
{
    const int b = za::min(bone, 2);
    return glm::normalize(r.pivot[finger][b] - r.pivot[finger][b - 1]);
}

// A segment's frame: x along it, y towards the palm's side (+y made square to x), z across.
[[nodiscard]] glm::mat3 segmentFrame(const glm::vec3& x)
{
    const glm::vec3 y = glm::normalize(glm::vec3{0.f, 1.f, 0.f} - x * x.y);
    return glm::mat3{x, y, glm::cross(x, y)};
}

// The least turn from a to b (unit vectors); false when there is none to speak of (the compiled numbers are kept).
[[nodiscard]] bool turnBetween(const glm::vec3& a, const glm::vec3& b, glm::quat& out)
{
    if(a == b)
    {
        return false;
    }
    const glm::vec3 axis = glm::cross(a, b);
    const float d = glm::dot(a, b);
    if(glm::length(axis) < 1e-6f && d > 0.f)
    {
        return false;
    }
    if(d < -0.999f)
    {
        return false; // turned back on itself: not a hand
    }
    out = glm::normalize(glm::quat{1.f + d, axis.x, axis.y, axis.z});
    return true;
}

// What reading a file found, for the console.
struct Report
{
    za::Vector<za::String> notes;
    double ms{0.0}; // reading and deriving it took
    float pivotMove{0.f};
    float sphereMove{0.f};
    float radiusLo{1.f}, radiusHi{1.f};
    int turnedJoints{0};
    int unmeasured{0};
};

// The rig's derived parts from its mesh and pivots (see the top).
void derive(Rig& out, Report& report)
{
    const Rig& ref = reference();
    const Classes refClasses = classify(ref), newClasses = classify(out);

    // The hinges: turned with their segment.
    glm::quat turnOf[FingerCount][jointsPerFinger];
    bool turned[FingerCount][jointsPerFinger]{};
    for(int f = 0; f < FingerCount; f++)
    {
        for(int k = 0; k < jointsPerFinger; k++)
        {
            const int bone = k == 0 ? 1 : 2;
            turned[f][k] = turnBetween(segmentDir(ref, f, bone), segmentDir(out, f, bone), turnOf[f][k]);
            report.turnedJoints += turned[f][k];
            for(int fr = 0; fr < data::numFrames; fr++)
            {
                out.turns[f][fr][k] = turned[f][k] ? glm::normalize(turnOf[f][k] * ref.turns[f][fr][k] * glm::conjugate(turnOf[f][k]))
                                                   : ref.turns[f][fr][k];
            }
        }
    }

    // The segments' spheres.
    out.segmentSpheres.clear();
    for(const SegmentSphere& s : ref.segmentSpheres)
    {
        const int f = s.finger, b = s.bone, k = b == 1 ? 0 : 1;
        const glm::vec3 p0 = ref.pivot[f][b - 1], p0n = out.pivot[f][b - 1];
        const glm::vec3 xr = segmentDir(ref, f, b), xn = segmentDir(out, f, b);
        const glm::mat3 rr = segmentFrame(xr);
        const glm::mat3 rn = turned[f][k] ? glm::mat3_cast(turnOf[f][k]) * rr : rr;
        const glm::vec3 l = glm::transpose(rr) * (s.c - p0);
        const float t = l.x;
        const glm::vec3 d{0.f, l.y, l.z};
        const float dl = glm::length(d);
        const glm::vec3 dh = dl > 1e-4f ? d / dl : glm::vec3{0.f, 1.f, 0.f};
        const glm::vec3 eh = glm::cross(glm::vec3{1.f, 0.f, 0.f}, dh);
        const float lr = b < 3 ? glm::length(ref.pivot[f][b] - p0) : tipLength(ref, refClasses, f, ref.pivot[f][2], xr);
        const float ln = b < 3 ? glm::length(out.pivot[f][b] - p0n) : tipLength(out, newClasses, f, out.pivot[f][2], xn);
        const float tn = lr > 0.f && ln > 0.f ? t * (ln / lr) : t;
        // Measured at the sphere's place along the segment or, where the finger's own skin doesn't reach across (at
        // a knuckle, inside the palm's front), a little further into the segment (on both meshes alike).
        float scale = 1.f, du = 0.f, de = 0.f;
        bool measured = false;
        const float inward = t < 0.5f * lr ? 1.f : -1.f;
        for(const float step : {0.f, 0.25f, 0.5f, 0.75f})
        {
            const float at = t + inward * step, atn = tn + inward * step * (lr > 0.f && ln > 0.f ? ln / lr : 1.f);
            const Section sr = section(ref, refClasses, f, p0 + xr * at, xr, rr * dh, rr * eh);
            const Section sn = section(out, newClasses, f, p0n + xn * atn, xn, rn * dh, rn * eh);
            if(sr.ok && sn.ok)
            {
                scale = za::fmin(za::fmax((sn.hi - sn.lo) / (sr.hi - sr.lo), 0.5f), 2.f);
                du = (sn.reach - sr.reach) - (sr.reach - dl) * (scale - 1.f);
                de = (sn.hi + sn.lo) * 0.5f - (sr.hi + sr.lo) * 0.5f;
                measured = true;
                break;
            }
        }
        report.unmeasured += !measured;
        const glm::vec3 moved = l + glm::vec3{tn - t, 0.f, 0.f} + dh * du + eh * de;
        SegmentSphere o = s;
        o.c = s.c + (p0n - p0) + (rn * moved - rr * l);
        o.r = s.r * scale;
        out.segmentSpheres.pushBack(o);
        report.sphereMove = za::fmax(report.sphereMove, glm::distance(o.c, s.c));
        report.radiusLo = za::fmin(report.radiusLo, scale);
        report.radiusHi = za::fmax(report.radiusHi, scale);
    }

    // The palm's and the thenar's: with the skin over them.
    const auto skinSpheres = [&](const za::Vector<Sphere>& from, za::Vector<Sphere>& to) {
        to.clear();
        for(const Sphere& s : from)
        {
            const glm::vec3 q = nearestOnMesh(ref, s.c);
            const float h = glm::distance(q, s.c);
            const glm::vec3 dir = h > 1e-5f ? (q - s.c) / h : glm::vec3{0.f, 1.f, 0.f};
            float tr = 0.f, tn = 0.f;
            Sphere o = s;
            if(meetNear(ref, s.c, dir, h, 1.5f, tr) && meetNear(out, s.c, dir, h, 1.5f, tn))
            {
                o.c = s.c + dir * (tn - tr);
            }
            else
            {
                report.unmeasured++;
            }
            to.pushBack(o);
            report.sphereMove = za::fmax(report.sphereMove, glm::distance(o.c, s.c));
        }
    };
    skinSpheres(ref.palmSpheres, out.palmSpheres);
    skinSpheres(ref.thenarSpheres, out.thenarSpheres);
}

// progs/hand_rig.md5mesh (and .md5anim) as a rig, or why not.
bool readRig(const char* meshText, Rig& out, za::String& error, Report& report)
{
    Md5Mesh m;
    if(!parseMesh(meshText, m, error))
    {
        return false;
    }

    // The joints: the 33 this code knows, by name and in order.
    za::String missing, unknown;
    for(const data::Joint& j : data::joints)
    {
        if(!za::anyOf(m.joints.begin(), m.joints.end(), [&](const Md5Joint& mj) { return mj.name == j.name; }))
        {
            missing += format("%s\"%s\"", missing.empty() ? "" : ", ", j.name);
        }
    }
    for(const Md5Joint& mj : m.joints)
    {
        if(!za::anyOf(data::joints, data::joints + za::getArraySize(data::joints), [&](const data::Joint& j) { return mj.name == j.name; }))
        {
            unknown += format("%s\"%s\"", unknown.empty() ? "" : ", ", mj.name.cStr());
        }
    }
    if(!missing.empty() || !unknown.empty())
    {
        error = format("%s: the bones must be the hand's %d, unrenamed.%s%s%s%s%s%s Bones can be moved, not renamed or deleted.",
            meshFile, data::numJoints, missing.empty() ? "" : " Missing: ", missing.cStr(), missing.empty() ? "" : ".",
            unknown.empty() ? "" : " Not the hand's: ", unknown.cStr(), unknown.empty() ? "" : ".");
        return false;
    }
    if(static_cast<int>(m.joints.size()) != data::numJoints)
    {
        error = format("%s: %d joints, the hand has %d", meshFile, static_cast<int>(m.joints.size()), data::numJoints);
        return false;
    }
    for(int j = 0; j < data::numJoints; j++)
    {
        if(m.joints[j].name != data::joints[j].name)
        {
            error = format("%s: joint %d is \"%s\", the hand's is \"%s\" there (export the hand with the Quake VR add-on: it "
                           "writes them in order)",
                meshFile, j, m.joints[j].name.cStr(), data::joints[j].name);
            return false;
        }
        if(m.joints[j].parent < -1 || m.joints[j].parent >= data::numJoints)
        {
            error = format("%s: joint \"%s\"'s parent %ld is not a joint", meshFile, m.joints[j].name.cStr(), m.joints[j].parent);
            return false;
        }
    }

    // The md5anim: the engine's loader needs the same joints there.
    if(byte* anim = COM_LoadMallocFile(animFile, nullptr))
    {
        za::Vector<qza::Pair<za::String, long>> hierarchy;
        za::String animError;
        const bool ok = parseAnim(reinterpret_cast<const char*>(anim), hierarchy, animError);
        free(anim);
        if(!ok)
        {
            error = animError;
            return false;
        }
        if(static_cast<int>(hierarchy.size()) != data::numJoints)
        {
            error = format("%s lists %d joints, the mesh %d", animFile, static_cast<int>(hierarchy.size()), data::numJoints);
            return false;
        }
        for(int j = 0; j < data::numJoints; j++)
        {
            if(hierarchy[j].first != m.joints[j].name || hierarchy[j].second != m.joints[j].parent)
            {
                error = format("%s's joint %d (\"%s\", parent %ld) is not the mesh's (\"%s\", parent %ld): export both files together",
                    animFile, j, hierarchy[j].first.cStr(), hierarchy[j].second, m.joints[j].name.cStr(), m.joints[j].parent);
                return false;
            }
        }
    }
    else
    {
        error = format("%s is missing (the engine needs it with the mesh)", animFile);
        return false;
    }

    // The weights.
    const long numWeights = static_cast<long>(m.weights.size());
    for(long w = 0; w < numWeights; w++)
    {
        const Md5Weight& mw = m.weights[w];
        if(mw.joint < 0 || mw.joint >= data::numJoints)
        {
            error = format("%s: weight %ld is on joint %ld: there is no such joint", meshFile, w, mw.joint);
            return false;
        }
        if(!(mw.bias > 0.f) || mw.bias > 1.0001f)
        {
            error = format("%s: weight %ld (on \"%s\") is %g: weights are above 0 and at most 1", meshFile, w,
                data::joints[mw.joint].name, mw.bias);
            return false;
        }
    }
    const long numVerts = static_cast<long>(m.verts.size());
    out.vertices.resize(numVerts);
    for(long v = 0; v < numVerts; v++)
    {
        const Md5Vert& mv = m.verts[v];
        if(mv.count <= 0)
        {
            error = format("%s: vertex %ld has no weights: it is weighted to no bone (in Blender: select it, give it a weight "
                           "on the bone it should move with)",
                meshFile, v);
            return false;
        }
        if(mv.first < 0 || mv.first + mv.count > numWeights)
        {
            error = format("%s: vertex %ld's weights (%ld from %ld) run past the file's %ld: weights are missing", meshFile, v,
                mv.count, mv.first, numWeights);
            return false;
        }
        if(mv.count > 4)
        {
            error = format("%s: vertex %ld has %ld weights; the hand takes 4 at most (in Blender: Weights > Limit Total, 4)",
                meshFile, v, mv.count);
            return false;
        }
        double total = 0.0;
        Vertex& out_v = out.vertices[v];
        out_v.count = static_cast<int>(mv.count);
        for(long i = 0; i < mv.count; i++)
        {
            const Md5Weight& mw = m.weights[mv.first + i];
            out_v.joint[i] = static_cast<short>(mw.joint);
            out_v.weight[i] = mw.bias;
            total += mw.bias;
        }
        if(za::fabs(total - 1.0) > 0.01)
        {
            error = format("%s: vertex %ld's weights add up to %.3f, not 1 (in Blender: Weights > Normalize All)", meshFile, v, total);
            return false;
        }
        out_v.pos = restPlace(m, mv);
        if(!ZA_ISFINITE(out_v.pos.x) || !ZA_ISFINITE(out_v.pos.y) || !ZA_ISFINITE(out_v.pos.z) || glm::length(out_v.pos) > 100.f)
        {
            error = format("%s: vertex %ld is at (%g %g %g), nowhere near the hand", meshFile, v, out_v.pos.x, out_v.pos.y, out_v.pos.z);
            return false;
        }
    }

    // The triangles.
    int degenerate = 0;
    for(size_t t = 0; t < m.tris.size(); t++)
    {
        const auto& tri = m.tris[t];
        for(const int i : tri)
        {
            if(i < 0 || i >= numVerts)
            {
                error = format("%s: triangle %d uses vertex %d; there are %ld", meshFile, static_cast<int>(t), i, numVerts);
                return false;
            }
        }
        degenerate += tri[0] == tri[1] || tri[1] == tri[2] || tri[0] == tri[2];
    }
    if(degenerate * 2 > static_cast<int>(m.tris.size()))
    {
        error = format("%s: %d of its %d triangles have no area", meshFile, degenerate, static_cast<int>(m.tris.size()));
        return false;
    }
    if(degenerate)
    {
        report.notes.pushBack(format("%d triangles use a vertex twice", degenerate));
    }
    out.triangles = m.tris;

    // Each finger has some of the mesh, and the mesh some size.
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const Vertex& v : out.vertices)
    {
        lo = glm::min(lo, v.pos);
        hi = glm::max(hi, v.pos);
    }
    if(glm::length(hi - lo) < 1.f)
    {
        error = format("%s: the whole mesh is %.3g units across (a hand is about 20)", meshFile, glm::length(hi - lo));
        return false;
    }
    const Classes classes = classify(out);
    constexpr const char* names[FingerCount] = {"thumb", "index", "middle", "ring", "pinky"};
    for(int f = 0; f < FingerCount; f++)
    {
        if(za::count(classes.finger.begin(), classes.finger.end(), f) == 0)
        {
            report.notes.pushBack(format("no vertex rides the %s (its spheres stay the shipped hand's)", names[f]));
        }
    }

    // The pivots: the segments' joints; the helpers sit at the pivot of the joint they share.
    for(int f = 0; f < FingerCount; f++)
    {
        for(int k = 0; k < jointsPerFinger; k++)
        {
            out.pivot[f][k] = vec(m.joints[1 + f * jointsPerFinger + k].pos);
            report.pivotMove = za::fmax(report.pivotMove, glm::distance(out.pivot[f][k], reference().pivot[f][k]));
        }
        for(int k = 0; k + 1 < jointsPerFinger; k++)
        {
            if(glm::distance(out.pivot[f][k], out.pivot[f][k + 1]) < 0.05f)
            {
                error = format("%s: %s's joints %d and %d are at the same place", meshFile, names[f], k + 1, k + 2);
                return false;
            }
        }
    }
    for(int j = 0; j < data::numJoints; j++)
    {
        const data::Joint& dj = data::joints[j];
        if(dj.kind == data::PartJoint && glm::distance(vec(m.joints[j].pos), out.pivot[dj.finger][dj.index]) > 0.01f)
        {
            report.notes.pushBack(format("\"%s\" isn't at its joint's pivot: it turns there all the same", dj.name));
        }
        if(dj.kind == data::PalmJoint && glm::length(vec(m.joints[j].pos)) > 0.01f)
        {
            report.notes.pushBack("the palm bone was moved: the palm is the hand's frame and stays (move its vertices instead)");
        }
    }

    derive(out, report);
    return true;
}

void install(Rig&& r)
{
    current() = ZA_MOVE(r);
    rigGeneration++;
    checked = ModelCheck{};
}

void printReport(const char* who, const Rig& r, const Report& report, bool loud)
{
    auto print = loud ? Con_Printf : Con_DPrintf;
    print("%s: %s: %d vertices, %d triangles; the pivots moved %.2f units at most, %d hinges turned; the grasp's spheres "
          "moved %.2f at most, sized x%.2f .. x%.2f (read in %.1f ms)\n",
        who, r.source.cStr(), static_cast<int>(r.vertices.size()), static_cast<int>(r.triangles.size()), report.pivotMove,
        report.turnedJoints, report.sphereMove, report.radiusLo, report.radiusHi, report.ms);
    if(report.unmeasured)
    {
        print("%s: %d spheres found no skin where the shipped hand's is: they stay where they were\n", who, report.unmeasured);
    }
    for(const za::String& n : report.notes)
    {
        print("%s: note: %s\n", who, n.cStr());
    }
}

} // namespace

const Rig& rig()
{
    return current();
}

unsigned generation()
{
    return rigGeneration;
}

void pose(const Pose& p, Posed& out)
{
    for(int f = 0; f < FingerCount; f++)
    {
        segments(p, f, p.curl[f], out.segment[f], out.turn[f]);
    }
    const glm::quat none{1.f, 0.f, 0.f, 0.f};
    for(int j = 0; j < data::numJoints; j++)
    {
        const data::Joint& d = data::joints[j];
        switch(d.kind)
        {
        case data::PalmJoint:
            out.joint[j] = Rigid{};
            break;
        case data::SegmentJoint:
            out.joint[j] = out.segment[d.finger][d.index];
            break;
        case data::PartJoint:
            // After the segment before the joint, the joint's turn by its share (slerped: the ring there keeps its
            // size however far the joint turns).
            out.joint[j] = out.segment[d.finger][d.index] *
                           about(d.finger, d.index, glm::normalize(glm::slerp(none, out.turn[d.finger][d.index], d.share)));
            break;
        }
    }
}

void vertices(const Posed& posed, za::Vector<glm::vec3>& out)
{
    const Rig& r = current();
    out.resize(r.vertices.size());
    for(size_t v = 0; v < r.vertices.size(); v++)
    {
        out[v] = blend(posed, r.vertices[v]);
    }
}

void fingerSegments(const Pose& p, int finger, const float curls[jointsPerFinger], Rigid out[jointsPerFinger + 1])
{
    glm::quat turns[jointsPerFinger];
    segments(p, finger, curls, out, turns);
}

float jointRate(int finger, int joint)
{
    float rate = 0.f;
    for(int f = 0; f + 1 < data::numFrames - 1; f++) // the closing path, frames 0..4
    {
        glm::quat d = glm::normalize(glm::inverse(turnAt(finger, joint, f)) * turnAt(finger, joint, f + 1));
        if(d.w < 0.f)
        {
            d = -d;
        }
        rate = za::fmax(rate, glm::angle(d));
    }
    return rate;
}

void skin(const Posed& posed, float out[data::numJoints * 12])
{
    for(int j = 0; j < data::numJoints; j++)
    {
        const Rigid& m = posed.joint[j];
        float* o = out + j * 12;
        for(int row = 0; row < 3; row++)
        {
            o[row * 4 + 0] = m.r[0][row];
            o[row * 4 + 1] = m.r[1][row];
            o[row * 4 + 2] = m.r[2][row];
            o[row * 4 + 3] = m.t[row];
        }
    }
}

void reset()
{
    checked = ModelCheck{};
}

bool usable(qmodel_t* model)
{
    if(model == checked.model && (!model || checked.name == model->name))
    {
        return checked.usable;
    }
    checked = ModelCheck{model, model ? model->name : "", false};
    if(!model || model->type != mod_alias || model->needload)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(model));
    if(hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones != data::numJoints)
    {
        Con_DPrintf("%s: not the jointed hand this engine expects (%d joints), the six hand models are drawn\n", model->name,
            data::numJoints);
        return false;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    for(int j = 0; j < data::numJoints; j++)
    {
        if(strcmp(bones[j].name, data::joints[j].name) != 0)
        {
            Con_DPrintf("%s: joint %d is \"%s\", not \"%s\"\n", model->name, j, bones[j].name, data::joints[j].name);
            return false;
        }
    }
    checked.usable = true;
    return true;
}

void reload_f()
{
    byte* text = COM_LoadMallocFile(meshFile, nullptr);
    if(!text)
    {
        Con_Warning("vr_hand_reload: %s not found; kept the hand you had\n", meshFile);
        return;
    }
    Rig r;
    za::String error;
    Report report;
    const bool ok = readRig(reinterpret_cast<const char*>(text), r, error, report);
    free(text);
    if(!ok)
    {
        Con_Warning("vr_hand_reload: %s\n", error.cStr());
        Con_Printf("vr_hand_reload: kept the hand you had\n");
        return;
    }
    qmodel_t* model = Mod_ForName(modelName, false);
    if(!model)
    {
        Con_Warning("vr_hand_reload: %s not found\n", modelName);
        return;
    }
    reloading = true;
    Mod_ReloadAliasModel(model); // the engine reads the files again; VR_ModelReplacementOk puts the rig in use
    reloading = false;
    reset();
    mem::on(mem::ModelReload); // the registered caches of models' data (vr_mem.hpp)
    if(!usable(model))
    {
        Con_Warning("vr_hand_reload: the engine didn't load %s as the jointed hand (Models: Enhanced off?); the six hand "
                    "models are drawn\n",
            meshFile);
    }
}

void info_f()
{
    const Rig& r = current();
    const Rig& ref = reference();
    Con_Printf("vr_hand_rig_info: the rig in use is %s (%d vertices, %d triangles, %d + %d + %d spheres)\n", r.source.cStr(),
        static_cast<int>(r.vertices.size()), static_cast<int>(r.triangles.size()), static_cast<int>(r.segmentSpheres.size()),
        static_cast<int>(r.palmSpheres.size()), static_cast<int>(r.thenarSpheres.size()));
    // Against the compiled rig, number for number (== on floats: the same bits, but for signed zeros).
    bool pivots = true, turns = true;
    for(int f = 0; f < FingerCount; f++)
    {
        for(int k = 0; k < jointsPerFinger; k++)
        {
            pivots = pivots && r.pivot[f][k] == ref.pivot[f][k];
            for(int fr = 0; fr < data::numFrames; fr++)
            {
                turns = turns && r.turns[f][fr][k] == ref.turns[f][fr][k];
            }
        }
    }
    bool spheres = r.segmentSpheres.size() == ref.segmentSpheres.size() && r.palmSpheres.size() == ref.palmSpheres.size() &&
                   r.thenarSpheres.size() == ref.thenarSpheres.size();
    for(size_t i = 0; spheres && i < r.segmentSpheres.size(); i++)
    {
        const SegmentSphere &a = r.segmentSpheres[i], &b = ref.segmentSpheres[i];
        spheres = a.finger == b.finger && a.bone == b.bone && a.c == b.c && a.r == b.r;
    }
    for(size_t i = 0; spheres && i < r.palmSpheres.size(); i++)
    {
        spheres = r.palmSpheres[i].c == ref.palmSpheres[i].c && r.palmSpheres[i].r == ref.palmSpheres[i].r;
    }
    for(size_t i = 0; spheres && i < r.thenarSpheres.size(); i++)
    {
        spheres = r.thenarSpheres[i].c == ref.thenarSpheres[i].c && r.thenarSpheres[i].r == ref.thenarSpheres[i].r;
    }
    // The mesh as the triangles' corners (the file's vertices are split at the skin's seams, the tables' at the
    // palm's and the fingers' parts: the same corners either way).
    bool mesh = r.triangles.size() == ref.triangles.size();
    for(size_t t = 0; mesh && t < r.triangles.size(); t++)
    {
        for(int c = 0; c < 3 && mesh; c++)
        {
            const Vertex &a = r.vertices[r.triangles[t][c]], &b = ref.vertices[ref.triangles[t][c]];
            mesh = a.pos == b.pos && a.count == b.count;
            for(int i = 0; i < a.count && mesh; i++)
            {
                mesh = a.joint[i] == b.joint[i] && a.weight[i] == b.weight[i];
            }
        }
    }
    const auto same = [](bool s) { return s ? "identical" : "different"; };
    Con_Printf("vr_hand_rig_info: against the compiled rig: pivots %s, curl turns %s, solver spheres %s, mesh (every "
               "triangle's corners: places, joints, weights) %s\n",
        same(pivots), same(turns), same(spheres), same(mesh));
    if(pivots && turns && spheres && mesh)
    {
        Con_Printf("vr_hand_rig_info: the rig in use is the compiled one, bit for bit\n");
    }
}

} // namespace qvr::handrig

// Mod_LoadModel's MD5 replacement, before it is loaded (vr_api.h): the jointed hand's file is checked and read as the
// rig; one the rig can't use is refused (the placeholder .mdl loads, the six hand models are drawn), with the reason.
extern "C" int VR_ModelReplacementOk(const char* name, const char* md5mesh)
{
    using namespace qvr::handrig;
    if(q_strcasecmp(name, modelName) != 0)
    {
        return 1;
    }
    Rig r;
    za::String error;
    Report report;
    const double start = Sys_DoubleTime();
    const bool ok = readRig(md5mesh, r, error, report);
    report.ms = (Sys_DoubleTime() - start) * 1000.0;
    if(!ok)
    {
        Con_Warning("%s: %s\n", reloading ? "vr_hand_reload" : "the jointed hand", error.cStr());
        if(!reloading)
        {
            Con_Printf("the jointed hand: the six hand models are drawn until the file is fixed (then vr_hand_reload)\n");
        }
        return 0;
    }
    r.source = meshFile;
    printReport(reloading ? "vr_hand_reload" : "the jointed hand", r, report, reloading);
    install(ZA_MOVE(r));
    return 1;
}
