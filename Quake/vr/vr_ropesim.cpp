// vr_ropesim.cpp -- see vr_ropesim.hpp.
//
// The rope as the game has it: straight pieces from the gun to the hook, bent at the corners it wraps round (a pillar's
// edge, a doorway's jamb, a box's top). Each server frame:
//   - corners something moved into (a prop dropped on the rope, a door closed on it) go (the wrap puts them back);
//   - unwrap: a corner whose neighbours see each other is no longer needed (the rope came off it);
//   - wrap: a piece that no longer sees through (its ends moved: the rope swept into something) gets a corner where it
//     first met it: the piece's move over the frame (from last frame's ends to this frame's) is searched for the moment
//     it touched, and the corner put there, off the surface by the rope's thickness;
//   - tighten: each corner slides towards the straight line between its neighbours as far as both pieces stay clear,
//     coming to rest against the edge it goes round.
// Each piece is a line cast against Box3D's shapes (the world's mesh, doors and lifts, the props; not the rope's own
// ends), so no piece passes through anything: the rope wraps rather than cuts, and nothing lying on the floor snags it
// (a piece only bends where the straight line is blocked). The taut path is the gun's end (the game's: your body, or
// the gun), the corners, the hook; the QC's rope measures and pulls along it (vr_grapple.qc VR_Grapple_RopeShape).
//
// The slack between the corners is the client's to draw (vr_rope.cpp: a chain hanging between them); the server sends
// only the corners, when they change (and a refresh now and then: the datagram may be lost), the first as a coordinate
// and the others as 2-byte offsets from the one before, in eighths of a unit. The ends travel with the rope's beam.

#include "vr_ropesim.hpp"
#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace qvr;

namespace
{

constexpr int maxCorners = 16;       // corners of a rope at most
constexpr float sightSlack = 1.f;    // units short of the far end a line of sight may stop (the end is on a surface)
constexpr float sentMoved = 1.f;     // units a corner moves before it is sent again
constexpr double refreshTime = 0.25; // seconds between two sends of unchanged corners (a lost datagram)
constexpr int searchSteps = 10;      // halvings of the frame searching for when a piece met something

struct Rope
{
    std::vector<glm::vec3> corners; // from the gun's end to the hook's
    glm::vec3 lastA{0.f};           // last frame's ends: the gun, the hook
    glm::vec3 lastB{0.f};
    int frame{-1};                  // the server frame stepped last (sv.qcvm.time's)
    double time{0.0};
    int blocked{0};                 // pieces left blocked last step (no corner found: a last resort, drawn through)
    ropesim::Shape shape;
    std::vector<glm::vec3> sent;    // the corners as last sent, and when
    double sentAt{-1.0};
    int emptySends{0};              // no corners, sent this many times since (twice: a lost datagram)
};

std::unordered_map<int, Rope> ropes;
int countedFrame = -1;
int countRopes = 0;
int countPoints = 0;

// vr_grapple_rope_netstats: the rope messages' bytes (tests; "the rope's bandwidth").
long long netBytes = 0;
int netFrames = 0, netMax = 0, netFrame = -1, netThisFrame = 0;

[[nodiscard]] float radius()
{
    return std::clamp(vr_grapple_rope_radius.value, 0.1f, 8.f);
}

[[nodiscard]] float sightRadius()
{
    return 0.25f * radius();
}

// Whether `b` is seen from `a` (a thin line of sight; stopping just short of `b` counts: it lies on a surface).
[[nodiscard]] bool sees(const glm::vec3& a, const glm::vec3& b, int skipA, int skipB)
{
    box3d::RopeHit hit;
    if(!box3d::ropeCast(a, b, sightRadius(), skipA, skipB, hit))
    {
        return true;
    }
    const float d = glm::distance(a, b);
    return hit.fraction * d >= d - sightSlack;
}

// A corner for the blocked piece `s`..`e` (last frame `s0`..`e0`), off the surface by the rope's thickness: where the
// piece first met something as it moved; or, blocked already last frame (a door closed on it, the first frame), a point
// off what blocks it that sees both ends. False: none found.
bool findCorner(const glm::vec3& s0, const glm::vec3& e0, const glm::vec3& s, const glm::vec3& e, int skipA, int skipB,
    glm::vec3& out)
{
    const float margin = radius();
    box3d::RopeHit hit;
    if(sees(s0, e0, skipA, skipB))
    {
        float lo = 0.f, hi = 1.f;
        for(int i = 0; i < searchSteps; i++)
        {
            const float mid = 0.5f * (lo + hi);
            if(sees(glm::mix(s0, s, mid), glm::mix(e0, e, mid), skipA, skipB))
            {
                lo = mid;
            }
            else
            {
                hi = mid;
            }
        }
        const glm::vec3 a = glm::mix(s0, s, hi), b = glm::mix(e0, e, hi);
        if(box3d::ropeCast(a, b, sightRadius(), skipA, skipB, hit))
        {
            const glm::vec3 c = hit.centre + hit.normal * margin;
            if(!box3d::ropeOverlaps(c, sightRadius(), skipA, skipB))
            {
                out = c;
                return true;
            }
        }
    }

    // Blocked all along: round what is in the way, from either end's first hit, the nearest point off it seeing both.
    const glm::vec3 d = e - s;
    const float len = glm::length(d);
    if(len < 1e-3f)
    {
        return false;
    }
    const glm::vec3 along = d / len;
    for(int from = 0; from < 2; from++)
    {
        if(!box3d::ropeCast(from == 0 ? s : e, from == 0 ? e : s, sightRadius(), skipA, skipB, hit))
        {
            continue;
        }
        const glm::vec3 base = hit.centre + hit.normal * margin;
        glm::vec3 side = glm::cross(along, hit.normal);
        side = glm::dot(side, side) > 1e-6f ? glm::normalize(side) : glm::vec3{0.f, 1.f, 0.f};
        glm::vec3 lift = hit.normal - along * glm::dot(hit.normal, along);
        lift = glm::dot(lift, lift) > 1e-6f ? glm::normalize(lift) : glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 dirs[] = {lift, side, -side, glm::normalize(lift + side), glm::normalize(lift - side),
            glm::vec3{0.f, 0.f, 1.f}};
        for(const float reach : {2.f, 4.f, 8.f, 16.f, 32.f, 64.f})
        {
            for(const glm::vec3& dir : dirs)
            {
                const glm::vec3 c = base + dir * reach;
                if(!box3d::ropeOverlaps(c, sightRadius(), skipA, skipB) && sees(s, c, skipA, skipB) && sees(c, e, skipA, skipB))
                {
                    out = c;
                    return true;
                }
            }
        }
    }
    return false;
}

// Each corner towards the straight line between its neighbours as far as both pieces stay clear (it comes to rest
// against the edge the rope goes round); one the straight line doesn't need goes.
void tighten(Rope& r, const glm::vec3& a, const glm::vec3& b, int skipA, int skipB)
{
    std::vector<glm::vec3>& c = r.corners;
    for(int pass = 0; pass < 2 && !c.empty(); pass++)
    {
        for(std::size_t k = 0; k < c.size();)
        {
            const glm::vec3 prev = k == 0 ? a : c[k - 1];
            const glm::vec3 next = k + 1 == c.size() ? b : c[k + 1];
            if(sees(prev, next, skipA, skipB))
            {
                c.erase(c.begin() + static_cast<std::ptrdiff_t>(k));
                continue;
            }
            const glm::vec3 pn = next - prev;
            const float pn2 = glm::dot(pn, pn);
            const float t = pn2 > 1e-4f ? std::clamp(glm::dot(c[k] - prev, pn) / pn2, 0.f, 1.f) : 0.f;
            const glm::vec3 target = prev + pn * t;
            float lo = 0.f, hi = 1.f;
            for(int step = 0; step < 6; step++)
            {
                const float mid = 0.5f * (lo + hi);
                const glm::vec3 x = glm::mix(c[k], target, mid);
                if(sees(prev, x, skipA, skipB) && sees(x, next, skipA, skipB))
                {
                    lo = mid;
                }
                else
                {
                    hi = mid;
                }
            }
            c[k] = glm::mix(c[k], target, lo);
            k++;
        }
    }
}

// This frame's corners for the rope from `a` (the gun) to `b` (the hook).
void update(Rope& r, const glm::vec3& a, const glm::vec3& b, int skipA, int skipB)
{
    std::vector<glm::vec3>& c = r.corners;
    // Corners something moved into.
    c.erase(std::remove_if(c.begin(), c.end(),
                [&](const glm::vec3& q) { return box3d::ropeOverlaps(q, 0.5f * sightRadius(), skipA, skipB); }),
        c.end());

    // Unwrap: a corner the rope no longer needs.
    for(std::size_t k = 0; k < c.size();)
    {
        const glm::vec3 prev = k == 0 ? a : c[k - 1];
        const glm::vec3 next = k + 1 == c.size() ? b : c[k + 1];
        if(sees(prev, next, skipA, skipB))
        {
            c.erase(c.begin() + static_cast<std::ptrdiff_t>(k));
            k = k > 0 ? k - 1 : 0;
            continue;
        }
        k++;
    }

    // Wrap: each piece that doesn't see through gets a corner where it met what is in the way (the gun inside something,
    // a hand through a wall: nothing is wrapped this frame).
    r.blocked = 0;
    if(!box3d::ropeOverlaps(a, 0.5f * sightRadius(), skipA, skipB))
    {
        int budget = 3 * maxCorners;
        for(std::size_t i = 0; i <= c.size() && budget > 0; budget--)
        {
            const bool first = i == 0, last = i == c.size();
            const glm::vec3 s = first ? a : c[i - 1];
            const glm::vec3 e = last ? b : c[i];
            if(sees(s, e, skipA, skipB))
            {
                i++;
                continue;
            }
            glm::vec3 corner;
            const bool found = static_cast<int>(c.size()) < maxCorners &&
                               findCorner(first ? r.lastA : s, last ? r.lastB : e, s, e, skipA, skipB, corner) &&
                               glm::distance(corner, s) > 0.5f && glm::distance(corner, e) > 0.5f;
            if(!found)
            {
                r.blocked++;
                i++;
                continue;
            }
            c.insert(c.begin() + static_cast<std::ptrdiff_t>(i), corner); // (the piece up to it is looked at again)
        }
    }
    tighten(r, a, b, skipA, skipB);
}

// The taut path from `game` along the corners to `end`: its shape.
ropesim::Shape shapeOf(const Rope& r, const glm::vec3& game, const glm::vec3& end)
{
    ropesim::Shape s;
    glm::vec3 at = game;
    for(const glm::vec3& q : r.corners)
    {
        s.path += glm::distance(at, q);
        at = q;
    }
    s.path += glm::distance(at, end);
    s.pivotA = r.corners.empty() ? end : r.corners.front();
    s.beyondA = s.path - glm::distance(game, s.pivotA);
    s.pivotB = r.corners.empty() ? game : r.corners.back();
    s.beyondB = s.path - glm::distance(end, s.pivotB);
    return s;
}

void count(int points)
{
    if(countedFrame != host_framecount)
    {
        countedFrame = host_framecount;
        countRopes = 0;
        countPoints = 0;
    }
    countRopes++;
    countPoints += points;
}

void netCount(int bytes)
{
    const int frame = static_cast<int>(std::lround(sv.qcvm.time * 1000.0));
    if(frame != netFrame)
    {
        netFrame = frame;
        netFrames++;
        netThisFrame = 0;
    }
    netThisFrame += bytes;
    netBytes += bytes;
    netMax = std::max(netMax, netThisFrame);
}

// vr_grapple_rope_netstats [reset]: the rope messages' bytes so far, and the server frames they went in.
void netstats_f()
{
    Con_Printf("rope net: %lld bytes in %d server frames with rope messages (%.1f a frame, at most %d)\n", netBytes, netFrames,
        netFrames ? static_cast<double>(netBytes) / netFrames : 0.0, netMax);
    if(Cmd_Argc() > 1)
    {
        netBytes = 0;
        netFrames = netMax = 0;
    }
}

// vr_grapple_rope_dump: each rope's corners and taut path (tests).
void dump_f()
{
    for(const auto& [num, r] : ropes)
    {
        Con_Printf("rope %d: %d corners; taut path %.1f, first corners %.1f %.1f %.1f (%.1f beyond) and %.1f %.1f %.1f (%.1f "
                   "beyond)\n",
            num, static_cast<int>(r.corners.size()), static_cast<double>(r.shape.path), static_cast<double>(r.shape.pivotA.x),
            static_cast<double>(r.shape.pivotA.y), static_cast<double>(r.shape.pivotA.z), static_cast<double>(r.shape.beyondA),
            static_cast<double>(r.shape.pivotB.x), static_cast<double>(r.shape.pivotB.y), static_cast<double>(r.shape.pivotB.z),
            static_cast<double>(r.shape.beyondB));
        int inside = 0; // (corners in the world or a prop: none, the rope goes round)
        for(const glm::vec3& q : r.corners)
        {
            inside += box3d::ropeOverlaps(q, 0.5f * sightRadius(), 0, 0) ? 1 : 0;
        }
        Con_Printf("rope %d: %d corners inside the world or a prop, %d pieces blocked\n", num, inside, r.blocked);
        for(std::size_t i = 0; i < r.corners.size(); i++)
        {
            Con_Printf("rope %d corner %d: %.1f %.1f %.1f\n", num, static_cast<int>(i), static_cast<double>(r.corners[i].x),
                static_cast<double>(r.corners[i].y), static_cast<double>(r.corners[i].z));
        }
    }
}

// vr_grapple_rope_cast x y z x y z [radius]: the rope's sweep between two points (tests).
void cast_f()
{
    if(Cmd_Argc() < 7)
    {
        Con_Printf("vr_grapple_rope_cast x y z x y z [radius]\n");
        return;
    }
    const glm::vec3 a{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const glm::vec3 b{Q_atof(Cmd_Argv(4)), Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6))};
    const float rad = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) : radius();
    box3d::RopeHit hit;
    const bool any = box3d::ropeCast(a, b, rad, 0, 0, hit);
    Con_Printf("rope cast: %s, fraction %.3f, centre %.1f %.1f %.1f, normal %.2f %.2f %.2f; overlaps at the end: %d\n",
        any ? "hit" : "clear", static_cast<double>(hit.fraction), static_cast<double>(hit.centre.x),
        static_cast<double>(hit.centre.y), static_cast<double>(hit.centre.z), static_cast<double>(hit.normal.x),
        static_cast<double>(hit.normal.y), static_cast<double>(hit.normal.z), box3d::ropeOverlaps(b, rad, 0, 0) ? 1 : 0);
}

void registerCommands()
{
    static bool registered = false;
    if(!registered)
    {
        registered = true;
        Cmd_AddCommand("vr_grapple_rope_dump", dump_f);
        Cmd_AddCommand("vr_grapple_rope_cast", cast_f);
        Cmd_AddCommand("vr_grapple_rope_netstats", netstats_f);
    }
}

} // namespace

namespace qvr::ropesim
{

Shape step(edict_t* hook, const glm::vec3& gun, const glm::vec3& game, const glm::vec3& end, float /*length*/, int skipA,
    int skipB)
{
    Rope& r = ropes[NUM_FOR_EDICT(hook)];
    const int frame = static_cast<int>(std::lround(sv.qcvm.time * 1000.0));
    if(r.frame == frame)
    {
        return r.shape;
    }
    QVR_PROFILE("grapple rope sim");
    registerCommands();
    // A new rope (or one not stepped for a while, or whose ends jumped): straight, from here on.
    const bool fresh = r.frame < 0 || sv.qcvm.time - r.time > 0.5 || glm::distance(r.lastA, gun) > 256.f ||
                       glm::distance(r.lastB, end) > 256.f;
    if(fresh)
    {
        r.corners.clear();
        r.lastA = gun;
        r.lastB = end;
    }
    r.frame = frame;
    r.time = sv.qcvm.time;
    update(r, gun, end, skipA, skipB);
    r.lastA = gun;
    r.lastB = end;
    r.shape = shapeOf(r, game, end);
    count(static_cast<int>(r.corners.size()) + 2);
    if(vr_grapple_debug.value >= 2 && static_cast<int>(sv.qcvm.time * 2.0) != static_cast<int>((sv.qcvm.time - host_frametime) * 2.0))
    {
        Con_Printf("grapple: rope %d: %d corners (%d pieces blocked), taut path %.1f, first corner %.1f %.1f %.1f (%.1f beyond)\n",
            NUM_FOR_EDICT(hook), static_cast<int>(r.corners.size()), r.blocked, static_cast<double>(r.shape.path),
            static_cast<double>(r.shape.pivotA.x), static_cast<double>(r.shape.pivotA.y), static_cast<double>(r.shape.pivotA.z),
            static_cast<double>(r.shape.beyondA));
    }
    return r.shape;
}

const Shape& shape(edict_t* hook)
{
    static const Shape none;
    const auto it = ropes.find(NUM_FOR_EDICT(hook));
    return it == ropes.end() ? none : it->second.shape;
}

void send(edict_t* hook, edict_t* owner, int beamId)
{
    const auto it = ropes.find(NUM_FOR_EDICT(hook));
    if(it == ropes.end() || sv.state != ss_active)
    {
        return;
    }
    Rope& r = it->second;
    const std::vector<glm::vec3>& c = r.corners;
    bool changed = c.size() != r.sent.size();
    for(std::size_t i = 0; i < c.size() && !changed; i++)
    {
        changed = glm::distance(c[i], r.sent[i]) > sentMoved;
    }
    const bool due = sv.qcvm.time - r.sentAt >= refreshTime || sv.qcvm.time < r.sentAt;
    if(!changed && !(due && (!c.empty() || r.emptySends < 2)))
    {
        return;
    }
    const int n = static_cast<int>(c.size());
    if(sv.datagram.cursize + 16 + 6 * n > MAX_DATAGRAM - 64)
    {
        return;
    }
    const int before = sv.datagram.cursize;
    MSG_WriteByte(&sv.datagram, protocol::svc_quakevr);
    MSG_WriteByte(&sv.datagram, protocol::QVR_SVC_ROPE);
    MSG_WriteShort(&sv.datagram, NUM_FOR_EDICT(owner));
    MSG_WriteByte(&sv.datagram, beamId);
    MSG_WriteByte(&sv.datagram, n);
    r.sent.resize(c.size());
    glm::vec3 at{0.f};
    for(int i = 0; i < n; i++)
    {
        if(i == 0)
        {
            for(int k = 0; k < 3; k++)
            {
                MSG_WriteCoord(&sv.datagram, c[0][k], sv.protocolflags);
            }
            at = c[0];
        }
        else
        {
            // From the one before as the client has it (no error adds up): eighths of a unit.
            for(int k = 0; k < 3; k++)
            {
                const int q = std::clamp(static_cast<int>(std::lround((c[static_cast<std::size_t>(i)][k] - at[k]) * 8.f)), -32767, 32767);
                MSG_WriteShort(&sv.datagram, q);
                at[k] += static_cast<float>(q) / 8.f;
            }
        }
        r.sent[static_cast<std::size_t>(i)] = c[static_cast<std::size_t>(i)];
    }
    r.emptySends = n == 0 ? (changed ? 1 : r.emptySends + 1) : 0;
    r.sentAt = sv.qcvm.time;
    netCount(sv.datagram.cursize - before);
}

void forget(int num)
{
    ropes.erase(num);
}

void reset()
{
    ropes.clear();
    registerCommands();
}

void profileCounts(int& nRopes, int& nPoints)
{
    const bool now = countedFrame == host_framecount || countedFrame == host_framecount - 1;
    nRopes = now ? countRopes : 0;
    nPoints = now ? countPoints : 0;
}

} // namespace qvr::ropesim
