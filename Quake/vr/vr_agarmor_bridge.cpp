// vr_agarmor_bridge.cpp -- the elbows' and the knees' leather, made each frame the body is drawn (vr_agarmor_internal.hpp,
// Bridge; ROUND24.md, "Leather bridge at elbows and knees"). vr_agarmor.cpp sets each joint's bridge up as a shell is made
// (bridgesOf: its two end rings, skinned as the plates' feet there or the skin's, and the skin between) and draws it as an
// entity of its own (relink, bonePoses); here it is posed: a tube along a Hermite curve from the upper part's last row to the
// lower part's first, each ring turned with the curve, its corners lerped from the one row's to the other's, its bellows
// standing on the inside of the bend and stretched flat on its outside, pushed out over the skin between wherever the skin
// would show through. Its vertices are each a bone of the entity's own: their matrices carry each frame's places to the
// skinned path (lighting, shadows, culling and the rest as the plates').

#include "vr_agarmor.hpp"
#include "vr_agarmor_internal.hpp"

#include "vr_armourdata.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_ragdoll.hpp"
#include "vr_rig2.hpp"
#include "vr_rigpose.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Tan.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>

namespace qvr::agarmor::detail
{

namespace
{

using geom::pi;

constexpr float bridgeSkinGap = 0.2f;     // units: the leather at least this far off the skin it keeps under it
constexpr float bridgePushMost = 5.f;     // units: a corner pushed out at most this far for it (an enforcer's elbow: 4 at rest)
constexpr float bridgePushSpread = 0.6f;  // a push shared with the corners next to it round the ring and along the tube (a lump eased)
constexpr int bridgePushPasses = 3;       // the skin measured and the corners pushed this many times (each over the last's)
constexpr float bridgeBendFull = 1.5f;    // radians: a bend this far or further has every fold on its inside
constexpr float bridgeInsideDeep = 1.25f; // the folds on the inside of a full bend: this many times their depth
constexpr float bridgeEnvelope = 0.35f;   // armour.txt's taper and stand-off at full this far in from each end (of the length)
constexpr float bridgeArcMost = 2.9f;     // radians: the turn the curve's tangents are sized for at most
constexpr float bridgeCrease = 0.5f;      // the skin this far round to the inside of a bend (cosine) is the crease's
constexpr float bridgeCreaseFade = 0.3f;  // ... its push faded out over this much further in (cosine)...
constexpr float bridgeCreaseFrom = 0.3f;  // ... and in the bend from this (of bridgeBendFull: 26 degrees)...
constexpr float bridgeCreaseBend = 0.75f; // ... over this much more (to 90 degrees; to 52 before)
constexpr float bridgeSoftMax = 0.2f;    // the round tangent and the arc's eased into each other over this (of the chord)
constexpr float bridgeForwardLeast = 0.05f; // the curve's middle leg: at least this far forward along each row (of the chord)
constexpr float bridgeBackFrom = -0.5f;   // the curve's tangent against its rings' frames (cosine): the tangents a half chord's here...
constexpr float bridgeBackTo = 0.f;       // ... their own from here
constexpr float bridgeTangentTurn = 0.35f; // the middle rings turned this share of the way from their slerped frames onto the curve (1 before)...
constexpr float bridgeTurnBend = 0.7f;    // ... in full from this bend (radians, 40 degrees), none straight
constexpr float bridgeRowsCrossed = 1.0f; // units: the rows closer than this along the joint (a corner of one behind the other's): crossed
constexpr float bridgePushShare = 0.25f;  // a sample's corners' weights' squares summed: at least this (a cell's middle's)

constexpr int ringVerts = bridgeCols + 1; // (a ring's corners and its seam's copy)

// The cost (vr_agarmor_bridge_stats): since the last report.
struct Stats
{
    double seconds{0.0}, frameSeconds{0.0}, worstFrame{0.0};
    int joints{0}, frames{0}, frame{-1};
};
Stats stats;

[[nodiscard]] glm::mat3 turnOf(const float* bones, int b)
{
    const float* m = bones + b * 12;
    return glm::mat3{glm::vec3{m[0], m[4], m[8]}, glm::vec3{m[1], m[5], m[9]}, glm::vec3{m[2], m[6], m[10]}};
}

// The turn taking unit `a` to unit `b` (the shortest).
[[nodiscard]] glm::quat turnBetween(const glm::vec3& a, const glm::vec3& b)
{
    const float d = glm::dot(a, b);
    if(d < -0.9999f)
    {
        const glm::vec3 side = za::abs(a.x) < 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 1.f, 0.f};
        return glm::angleAxis(pi, glm::normalize(glm::cross(a, side)));
    }
    return glm::normalize(glm::quat{1.f + d, glm::cross(a, b)});
}

using geom::smooth01;

// The turn (degrees) taking a ring's frame (its x and its normal) to another's.
[[nodiscard]] float frameTurnDeg(const glm::vec3& xa, const glm::vec3& za, const glm::vec3& xb, const glm::vec3& zb)
{
    const glm::mat3 A{xa, glm::cross(za, xa), za}, B{xb, glm::cross(zb, xb), zb};
    const glm::mat3 R = B * glm::transpose(A);
    return za::acos(za::clamp((R[0][0] + R[1][1] + R[2][2] - 1.f) * 0.5f, -1.f, 1.f)) * (180.f / pi);
}

// Where a ray from the ring's middle along `dir` (its plane, local) leaves its polygon `corner` (6, local xy); 0 none.
[[nodiscard]] float polygonReach(const glm::vec2* corner, const glm::vec2& dir, int& edge, float& along)
{
    const auto cross2 = [](const glm::vec2& a, const glm::vec2& b) { return a.x * b.y - a.y * b.x; };
    float best = 0.f;
    for(int k = 0; k < bridgeCols; k++)
    {
        const glm::vec2 a = corner[k], e = corner[(k + 1) % bridgeCols] - a;
        const float den = cross2(dir, e);
        if(za::abs(den) < 1e-8f)
        {
            continue;
        }
        const float r = cross2(a, e) / den, s = cross2(a, dir) / den;
        if(r > best && s >= -1e-4f && s <= 1.0001f)
        {
            edge = k;
            along = za::clamp(s, 0.f, 1.f);
            best = r;
        }
    }
    return best;
}

// A joint's posed rings (its curve's), before their corners.
struct Rings
{
    int n{0};
    glm::vec3 centre[bridgeMostRings];
    glm::quat turn[bridgeMostRings];
    glm::vec3 along[bridgeMostRings];   // the curve's direction there
    glm::vec3 inside[bridgeMostRings];  // towards the inside of the bend, square to `along` (none: zero)
    glm::vec2 corner[bridgeMostRings][bridgeCols]; // local xy (the ring's frame), folds and pushes on
    float z[bridgeMostRings][bridgeCols];
};

} // namespace

Skinned skinnedOf(const glm::vec3& p, const W& w)
{
    // (as makeModel quantises a vertex's weights: the first four bones weighing on it in order, each in 255ths, the last the rest)
    Skinned s;
    s.p = p;
    int order[4] = {-1, -1, -1, -1};
    int k = 0;
    for(int b = 0; b < rig2::maxBones && k < 4; b++)
    {
        if(w.w[b] > 0.f)
        {
            order[k++] = b;
        }
    }
    int left = 255;
    for(int i = 0; i < k; i++)
    {
        const int bone = order[i];
        const int wb = i + 1 < k ? static_cast<int>(w.w[bone] * 255.f + 0.5f) : left;
        const int q = za::clamp(za::min(wb, left), 0, 255);
        s.bone[i] = bone;
        s.w[i] = static_cast<float>(q) / 255.f;
        left -= q;
    }
    s.n = k;
    return s;
}

bool posedOf(const Skinned& s, const float* bones, int have, glm::vec3& out)
{
    glm::vec3 o{0.f};
    for(int i = 0; i < s.n; i++)
    {
        const int b = s.bone[i];
        if(b < 0 || b >= have)
        {
            return false;
        }
        const float* m = bones + b * 12;
        const glm::vec3& p = s.p;
        o += s.w[i] * glm::vec3{m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
                          m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
    }
    out = o;
    return s.n > 0;
}

bool evalBridge(const Shell& s, int j, const float* bones, int have, bool upperWorn, bool lowerWorn, glm::vec3* outP,
    glm::vec3* outN, BridgeDiag* diag)
{
    const Bridge& br = s.bridges[j];
    if(!br.ok || br.rings < 2 || br.rings > bridgeMostRings)
    {
        return false;
    }
    // The two ends: the plates' rows worn, else the skin's rings there; each ring posed, its middle, its frame turned with its
    // part's bone, its corners in that frame.
    const BridgeEnd* ends[2] = {upperWorn ? &br.plates[0] : &br.skin[0], lowerWorn ? &br.plates[1] : &br.skin[1]};
    glm::vec3 mid[2], axis[2];
    glm::quat frameTurn[2];
    glm::vec3 local[2][bridgeCols];
    for(int e = 0; e < 2; e++)
    {
        const BridgeEnd& E = *ends[e];
        if(!E.ok || E.bone < 0 || E.bone >= have)
        {
            return false;
        }
        glm::vec3 q[bridgeCols];
        mid[e] = glm::vec3{0.f};
        for(int c = 0; c < bridgeCols; c++)
        {
            if(!posedOf(E.ring[c], bones, have, q[c]))
            {
                return false;
            }
            mid[e] += q[c] / static_cast<float>(bridgeCols);
        }
        const glm::mat3 R = turnOf(bones, E.bone);
        const glm::vec3 z = geom::safeNormalize(R * E.z, E.z, 1e-8f);
        const glm::vec3 rx = R * E.x;
        const glm::vec3 x = geom::safeNormalize(rx - z * glm::dot(rx, z), E.x, 1e-8f);
        const glm::mat3 F{x, glm::cross(z, x), z};
        frameTurn[e] = glm::normalize(glm::quat_cast(F));
        axis[e] = z;
        const glm::mat3 Ft = glm::transpose(F);
        for(int c = 0; c < bridgeCols; c++)
        {
            local[e][c] = Ft * (q[c] - mid[e]);
        }
    }

    // The curve: Hermite from the one row's middle to the other's, along each row's direction, round the joint: its tangents
    // reach the joint's place along each row's line (a Bezier's inner points there), so it turns close round the elbow's or
    // the knee's point rather than cutting the corner as a circle's arc would (the skin's point showed through); a joint
    // behind a row (a part moved by armour.txt): the circle's arc's.
    const float chord = glm::distance(mid[0], mid[1]);
    const float theta = za::acos(za::clamp(glm::dot(axis[0], axis[1]), -1.f, 1.f));
    const float th = za::min(theta, bridgeArcMost);
    const float arc = th < 1e-3f ? chord : chord * 2.f * za::tan(th * 0.25f) / za::sin(th * 0.5f);
    Skinned pivot;
    pivot.p = br.pivot;
    pivot.bone[0] = ends[1]->bone;
    pivot.w[0] = 1.f;
    pivot.n = 1;
    glm::vec3 joint;
    (void)posedOf(pivot, bones, have, joint);
    const float a0 = glm::dot(joint - mid[0], axis[0]), a1 = glm::dot(mid[1] - joint, axis[1]);
    // (each end's tangent the larger of the round one and the arc's, eased into each other where they meet: a switch from the
    // one to the other at a tenth of the chord, where they differ by most of a chord, changed a tangent by a chord in a few
    // degrees of the joint, the middle rings turning 20 degrees in a step: the grunt's left elbow, the knight's right knee,
    // the enforcer's left elbow)
    const float soft = bridgeSoftMax * chord;
    const auto softMax = [soft](float a, float b) { return 0.5f * (a + b + za::sqrt((a - b) * (a - b) + soft * soft)); };
    float m0 = softMax(arc, 3.f * a0), m1 = softMax(arc, 3.f * a1);
    // (both scaled down together as far as needed for the curve's middle leg, its Bezier's two inner points apart, to run
    // forwards along the middle ring's frame (the rows' directions halved): the round tangent four chords long on the
    // knight's sword arm, the joint off the upper row's line and the forearm's row before the corner, took the curve past
    // the forearm's row and back, its tangent against its rings' frames, the rings spinning; eased there by how far back it
    // turned, its tangents went from a half chord to four chords in a few degrees, the rings turning 36 degrees in a step.
    // The fit is smooth in the pose: the chord's reach along the middle direction over the inner points' (never nothing
    // over a bend short of straight back), the leg kept a little forward.)
    const glm::vec3 midDir = geom::safeNormalize(axis[0] + axis[1], axis[1], 1e-8f);
    const float least = bridgeForwardLeast * chord, halfCos = glm::dot(axis[0], midDir);
    const float inner = (m0 + m1) / 3.f * halfCos, reach = za::max(glm::dot(mid[1] - mid[0], midDir) - least, least);
    const float fit = inner > 1e-6f ? za::min(1.f, reach / inner) : 1.f;
    m0 *= fit;
    m1 *= fit;
    const float bend = za::clamp(theta / bridgeBendFull, 0.f, 1.f);
    const glm::vec3 turnDir = axis[1] - axis[0];

    Rings rg;
    rg.n = br.rings;
    const int last = rg.n - 1;
    float tOf[bridgeMostRings]{}, foldOf[bridgeMostRings]{}, scaleOf[bridgeMostRings]{}, offOf[bridgeMostRings]{};
    glm::quat frameOf[bridgeMostRings];
    for(int i = 0; i < rg.n; i++)
    {
        tOf[i] = static_cast<float>(i) / static_cast<float>(last);
        frameOf[i] = glm::slerp(frameTurn[0], frameTurn[1], tOf[i]);
    }
    const auto speedOf = [&](float t, float k0, float k1) {
        const float t2 = t * t;
        return (6.f * t2 - 6.f * t) * mid[0] + (3.f * t2 - 4.f * t + 1.f) * k0 * axis[0] + (-6.f * t2 + 6.f * t) * mid[1] + (3.f * t2 - 2.f * t) * k1 * axis[1];
    };
    // A curve whose tangent turns back against its rings' frames (a ring's frame turned onto it by the shortest turn: none
    // is defined for a tangent pointing back, and the rings spun from step to step): its tangents shortened, eased by how far
    // back it turns, towards a half chord's, which hug the chord. The knight's right elbow: its sword arm's forearm row sits
    // before the corner of the two rows' lines, and the joint, off the upper row's line, sized the round tangents at four
    // times the chord: the curve looped round (281 degrees of turn for a 79 degree bend), its tangent at -0.998 to the frame.
    // (A net now: the fit above keeps the middle forward; no checked model's curve turns back in its joints' sweeps.)
    float back = 1.f;
    for(int i = 1; i < last; i++)
    {
        const glm::vec3 zq = frameOf[i] * glm::vec3{0.f, 0.f, 1.f};
        back = za::min(back, glm::dot(zq, geom::safeNormalize(speedOf(tOf[i], m0, m1), zq, 1e-8f)));
    }
    const float keep = smooth01((back - bridgeBackFrom) / (bridgeBackTo - bridgeBackFrom)), hug = 0.5f * chord;
    m0 = hug + (m0 - hug) * keep;
    m1 = hug + (m1 - hug) * keep;
    const glm::vec3 t0 = axis[0] * m0, t1 = axis[1] * m1;
    const auto curveAt = [&](float t) {
        const float t2 = t * t, t3 = t2 * t;
        return (2.f * t3 - 3.f * t2 + 1.f) * mid[0] + (t3 - 2.f * t2 + t) * t0 + (-2.f * t3 + 3.f * t2) * mid[1] + (t3 - t2) * t1;
    };
    // (the rings even along the curve: the curve's parameter for each ring's share of its length. At the curve's own parameter
    // the rings bunched where its tangent is short (the enforcer's forearm row: 1.75 against 3.9 at the upper row) while the
    // rows' shapes are lerped evenly by ring: the forearm row's tilt, straightened, then drew a column back faster than the
    // curve carried it forward, and the cells there turned in (his holes, the enforcer's right elbow straight))
    float curveT[bridgeMostRings]{};
    {
        constexpr int samples = 48;
        float len[samples + 1]{};
        glm::vec3 was = mid[0];
        for(int k = 1; k <= samples; k++)
        {
            const glm::vec3 p = curveAt(static_cast<float>(k) / static_cast<float>(samples));
            len[k] = len[k - 1] + glm::distance(p, was);
            was = p;
        }
        int k = 0;
        for(int i = 0; i < rg.n; i++)
        {
            const float want = tOf[i] * len[samples];
            while(k < samples - 1 && len[k + 1] < want)
            {
                k++;
            }
            const float span = len[k + 1] - len[k];
            const float f = span > 1e-6f ? za::clamp((want - len[k]) / span, 0.f, 1.f) : 0.f;
            curveT[i] = i == 0 ? 0.f : i == last ? 1.f : (static_cast<float>(k) + f) / static_cast<float>(samples);
        }
    }
    // (each middle ring turned only part way from its slerped frame onto the curve's tangent: turned fully, a short sheared tube
    // (the rows offset across the limb, the curve an S) tilted its rings by more than their spacing allows, they cut through
    // each other and the cells between faced in: holes in every elbow and knee with the limb straight, worst the enforcer's
    // (his notes, 2026-11-09; ROUND24.md, "Elbow leather at rest"). Not at all: the enforcer's and the knight's elbows bent
    // folded in more than before; bridgeTangentTurn is the share that does neither. And none with the limb straight: the axes
    // parallel, the rings evenly spaced, the rows' shapes lerped, each column then runs forward wherever its rows do not
    // cross; turned, the S's tilt drew the grunt's and the knees' columns back, 2026-11-10)
    const float ringTurn = bridgeTangentTurn * smooth01(theta / bridgeTurnBend);
    for(int i = 0; i < rg.n; i++)
    {
        const float t = tOf[i];
        rg.centre[i] = curveAt(curveT[i]);
        const glm::vec3 d = speedOf(curveT[i], m0, m1);
        const glm::quat q = frameOf[i];
        const glm::vec3 zq = q * glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 zt = geom::safeNormalize(d, zq, 1e-8f);
        rg.turn[i] = i == 0 ? frameTurn[0] : i == last ? frameTurn[1] : glm::normalize(glm::slerp(q, turnBetween(zq, zt) * q, ringTurn));
        rg.along[i] = rg.turn[i] * glm::vec3{0.f, 0.f, 1.f};
        const glm::vec3 in = turnDir - rg.along[i] * glm::dot(turnDir, rg.along[i]);
        rg.inside[i] = glm::length(in) > 1e-4f ? glm::normalize(in) : glm::vec3{0.f};
        // (the bellows: a fold's top on every other ring, none at the ends; armour.txt's taper and stand-off, none at the ends)
        const float wave = za::sin(pi * static_cast<float>(br.folds) * t);
        foldOf[i] = i == 0 || i == last ? 0.f : br.depth * wave * wave;
        const float env = smooth01(t / bridgeEnvelope) * smooth01((1.f - t) / bridgeEnvelope);
        const float k = t < 0.5f ? br.taper[0] + (br.taper[1] - br.taper[0]) * (2.f * t) : br.taper[2] + (br.taper[3] - br.taper[2]) * (2.f * t - 1.f);
        scaleOf[i] = 1.f + (k * br.scale - 1.f) * env;
        offOf[i] = br.standoff * env;
    }
    float push[bridgeMostRings][bridgeCols]{}, pushedBefore[bridgeMostRings][bridgeCols]{};
    const auto lay = [&]
    {
        for(int i = 0; i < rg.n; i++)
        {
            const float t = tOf[i];
            for(int c = 0; c < bridgeCols; c++)
            {
                const glm::vec3 L = local[0][c] + (local[1][c] - local[0][c]) * t;
                const glm::vec2 xy = glm::vec2{L.x, L.y} * scaleOf[i];
                const float r = glm::length(xy);
                const glm::vec2 dir = r > 1e-5f ? xy / r : glm::vec2{1.f, 0.f};
                // (the folds deep on the inside of the bend, flat on its outside; all round on a straight limb)
                const glm::vec3 radial = rg.turn[i] * glm::vec3{dir.x, dir.y, 0.f};
                const float cosIn = glm::dot(radial, rg.inside[i]);
                const float fold = foldOf[i] * ((1.f - bend) + bend * bridgeInsideDeep * smooth01((cosIn + 0.3f) / 1.1f));
                rg.corner[i][c] = dir * (r + offOf[i] + fold + push[i][c]);
                rg.z[i][c] = L.z;
            }
        }
    };
    lay();

    // The skin between kept under it: each of its vertices posed, at its rest place between two rings, its reach out from
    // the curve there against the leather's polygon in that direction; short, the corners round it pushed out (not the end
    // rings: the rows'). Twice, the second over the first's pushes. Not on the inside of a bend: the limbs overlap there.
    float firstNeed = 0.f, firstCrease = 0.f; // (the first pass's most asked, and its crease share: diag)
    int passNow = -1;
    const auto measure = [&](bool pushing, BridgeDiag* d)
    {
        for(int k = 0; k < br.numUnder; k++)
        {
            glm::vec3 u;
            if(!posedOf(br.under[k], bones, have, u))
            {
                continue;
            }
            // (measured in the leather's cross-section at its own place along it, from where it lies at rest: found between
            // the posed rings' planes, it went from one stretch to another far along as a bend's crossed planes swept by, or
            // came into one from none, and its push jumped with it, the corners flung out and back)
            const float where = za::clamp(br.underAlong[k], 0.f, 1.f) * static_cast<float>(last);
            const int seg = za::min(static_cast<int>(where), last - 1);
            const float f = where - static_cast<float>(seg);
            const glm::vec3 c = rg.centre[seg] + (rg.centre[seg + 1] - rg.centre[seg]) * f;
            const glm::quat q = glm::slerp(rg.turn[seg], rg.turn[seg + 1], f);
            const glm::vec3 l = glm::inverse(q) * (u - c);
            const glm::vec2 xy{l.x, l.y};
            const float ru = glm::length(xy);
            if(ru < 1e-4f)
            {
                continue;
            }
            glm::vec2 poly[bridgeCols];
            for(int col = 0; col < bridgeCols; col++)
            {
                poly[col] = rg.corner[seg][col] + (rg.corner[seg + 1][col] - rg.corner[seg][col]) * f;
            }
            int edge = 0;
            float s = 0.f;
            const float reach = polygonReach(poly, xy / ru, edge, s);
            if(reach <= 0.f)
            {
                continue;
            }
            const glm::vec3 inside = rg.inside[seg] + (rg.inside[seg + 1] - rg.inside[seg]) * f;
            // (the crease's skin not pushed: faded in past the crease's edge, in the bend and round the ring, so a sample
            // crossing it as the joint moves changes the push smoothly; a hard edge snapped the leather by the push. In the
            // bend from 26 to 90 degrees: faded in by 52, the enforcer's inner elbow's lump of skin let its push go 2 units in
            // 8 degrees, the leather moving 8 times as far as its rows, pumping in and out as his arms swung through it.)
            const float cosIn = glm::dot(q * glm::vec3{xy.x / ru, xy.y / ru, 0.f}, inside);
            const float creaseOf = smooth01((bend - bridgeCreaseFrom) / bridgeCreaseBend) * smooth01((cosIn - bridgeCrease) / bridgeCreaseFade);
            const bool crease = creaseOf > 0.f;
            if(d)
            {
                d->under++;
                const float where = static_cast<float>(seg) + f;
                if(ru > reach + 0.01f && (where < 0.5f || where > static_cast<float>(last) - 0.5f))
                {
                    d->atRows++; // (at a row: under its plates' own fit, the upper's lapping edge)
                }
                else if(ru > reach + 0.01f)
                {
                    d->outside += crease ? 0 : 1;
                    d->outsideInner += crease ? 1 : 0;
                    d->worstAt = !crease && ru - reach > d->worst ? static_cast<float>(seg) + f : d->worstAt;
                    d->worst = crease ? d->worst : za::max(d->worst, ru - reach);
                }
            }
            const float need = (ru + bridgeSkinGap - reach) * (1.f - creaseOf);
            if(pushing && passNow == 0 && ru + bridgeSkinGap - reach > firstNeed)
            {
                firstNeed = ru + bridgeSkinGap - reach;
                firstCrease = creaseOf;
            }
            if(d)
            {
                d->creaseCut = za::max(d->creaseCut, (ru + bridgeSkinGap - reach) * creaseOf);
                d->needMost = za::max(d->needMost, ru + bridgeSkinGap - reach);
            }
            if(!pushing || need <= 0.f)
            {
                continue;
            }
            const int e1 = (edge + 1) % bridgeCols;
            const int ringOf[4] = {seg, seg, seg + 1, seg + 1}, colOf[4] = {edge, e1, edge, e1};
            float w[4] = {(1.f - f) * (1.f - s), (1.f - f) * s, f * (1.f - s), f * s};
            float sum2 = 0.f;
            for(int i = 0; i < 4; i++)
            {
                w[i] = ringOf[i] == 0 || ringOf[i] == last ? 0.f : w[i];
                sum2 += w[i] * w[i];
            }
            // (on top of the pushes the polygon was measured with: each sample's own, the corners keeping the most asked;
            // the least push of the corners that moves the leather there by `need`, its share no less than a cell's middle:
            // near an end ring, whose corners stay on the plates' row, the free corners' weights go to nothing, and
            // `need / sum2` grew without bound there: a sample sliding towards the row flung its corners out to the cap and
            // back, up to 5 units in a frame on every model, vr_agarmor_bridge_check's continuity)
            const float kk = za::min(need, bridgePushMost) / za::max(sum2, bridgePushShare);
            for(int i = 0; i < 4; i++)
            {
                float& p = push[ringOf[i]][colOf[i]];
                p = za::min(za::max(p, pushedBefore[ringOf[i]][colOf[i]] + w[i] * kk), bridgePushMost);
            }
        }
    };
    for(int pass = 0; pass < bridgePushPasses && br.numUnder > 0; pass++)
    {
        memcpy(pushedBefore, push, sizeof(push));
        passNow = pass;
        measure(true, nullptr);
        // (eased: each corner at least a share of its neighbours' pushes, so a pushed corner is a swell, not a spike)
        float eased[bridgeMostRings][bridgeCols]{};
        for(int i = 1; i < last; i++)
        {
            for(int c = 0; c < bridgeCols; c++)
            {
                const float near = za::max(za::max(push[i][(c + 1) % bridgeCols], push[i][(c + bridgeCols - 1) % bridgeCols]),
                    za::max(push[i - 1][c], push[i + 1][c]));
                eased[i][c] = za::max(push[i][c], near * bridgePushSpread);
            }
        }
        memcpy(push, eased, sizeof(push));
        lay();
    }
    if(diag)
    {
        *diag = BridgeDiag{};
        measure(false, diag);
        diag->theta = theta;
        diag->a0 = a0;
        diag->a1 = a1;
        diag->chord = chord;
        diag->fit = fit;
        diag->firstNeed = firstNeed;
        diag->firstCrease = firstCrease;
        diag->keep = keep;
        diag->back = back;
        diag->m0 = m0;
        diag->m1 = m1;
        diag->rings = rg.n;
        for(int i = 0; i < rg.n; i++)
        {
            diag->ringX[i] = rg.turn[i] * glm::vec3{1.f, 0.f, 0.f};
            diag->ringZ[i] = rg.turn[i] * glm::vec3{0.f, 0.f, 1.f};
            for(int c = 0; c < bridgeCols; c++)
            {
                diag->pushMost = za::max(diag->pushMost, push[i][c]);
            }
        }
    }

    // The places, a ring at a time (its seam's copy last), and their normals across the grid.
    for(int i = 0; i < rg.n; i++)
    {
        for(int c = 0; c <= bridgeCols; c++)
        {
            const int cc = c % bridgeCols;
            outP[i * ringVerts + c] = rg.centre[i] + rg.turn[i] * glm::vec3{rg.corner[i][cc].x, rg.corner[i][cc].y, rg.z[i][cc]};
        }
    }
    for(int i = 0; i < rg.n; i++)
    {
        for(int c = 0; c <= bridgeCols; c++)
        {
            const int cc = c % bridgeCols;
            const glm::vec3& p = outP[i * ringVerts + cc];
            const glm::vec3 round = outP[i * ringVerts + (cc + 1) % bridgeCols] - outP[i * ringVerts + (cc + bridgeCols - 1) % bridgeCols];
            const glm::vec3 down = outP[za::min(i + 1, last) * ringVerts + cc] - outP[za::max(i - 1, 0) * ringVerts + cc];
            glm::vec3 n = geom::safeNormalize(glm::cross(down, round), p - rg.centre[i], 1e-10f);
            n = glm::dot(n, p - rg.centre[i]) < 0.f ? -n : n;
            outN[i * ringVerts + c] = geom::safeNormalize(n, glm::vec3{0.f, 0.f, 1.f}, 1e-10f);
        }
    }
    return true;
}

void bridgeRest(Shell& s)
{
    s.bridgeRestP.clear();
    s.bridgeRestN.clear();
    s.bridgeTris.clear();
    if(s.bridgeVerts <= 0 || s.bridgeVerts > bridgeMostVerts)
    {
        s.bridgeVerts = 0;
        return;
    }
    s.bridgeRestP.resize(static_cast<za::SizeT>(s.bridgeVerts), glm::vec3{0.f});
    s.bridgeRestN.resize(static_cast<za::SizeT>(s.bridgeVerts), glm::vec3{0.f, 0.f, 1.f});
    float identity[rig2::maxBones * 12]{};
    for(int b = 0; b < rig2::maxBones; b++)
    {
        identity[b * 12 + 0] = identity[b * 12 + 5] = identity[b * 12 + 10] = 1.f;
    }
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        if(!br.ok)
        {
            continue;
        }
        glm::vec3 P[bridgeMostRings * ringVerts], N[bridgeMostRings * ringVerts];
        if(!evalBridge(s, j, identity, rig2::maxBones, true, true, P, N, nullptr))
        {
            continue;
        }
        const int n = br.rings * ringVerts;
        for(int v = 0; v < n; v++)
        {
            at(s.bridgeRestP, br.first + v) = P[v];
            at(s.bridgeRestN, br.first + v) = N[v];
        }
        // (wound outward: the way most of its quads face out)
        float facing = 0.f;
        for(int i = 0; i + 1 < br.rings; i++)
        {
            for(int c = 0; c < bridgeCols; c++)
            {
                const int a = i * ringVerts + c;
                facing += glm::dot(glm::cross(P[a + 1] - P[a], P[a + ringVerts + 1] - P[a]), N[a] + N[a + ringVerts + 1]);
            }
        }
        const bool flip = facing < 0.f;
        for(int i = 0; i + 1 < br.rings; i++)
        {
            for(int c = 0; c < bridgeCols; c++)
            {
                const int a = br.first + i * ringVerts + c, b = a + 1, cc = a + ringVerts + 1, d = a + ringVerts;
                for(const int x : {a, flip ? cc : b, flip ? b : cc, a, flip ? d : cc, flip ? cc : d})
                {
                    s.bridgeTris.pushBack(x);
                }
            }
        }
    }
}

int bridgeBones(const Shell& s, int pieces, const float* bones, int have, float* out)
{
    const double t0 = Sys_DoubleTime();
    int joints = 0;
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        if(!br.ok)
        {
            continue;
        }
        const int n = br.rings * ringVerts;
        float* o = out + br.first * 12;
        const bool upperWorn = ((pieces >> br.upper) & 1) != 0, lowerWorn = ((pieces >> br.lower) & 1) != 0;
        glm::vec3 P[bridgeMostRings * ringVerts], N[bridgeMostRings * ringVerts];
        if(!bones || (!upperWorn && !lowerWorn) || !evalBridge(s, j, bones, have, upperWorn, lowerWorn, P, N, nullptr))
        {
            memset(o, 0, sizeof(float) * 12u * static_cast<size_t>(n)); // (not drawn: collapsed)
            continue;
        }
        joints++;
        // Each vertex's bone: its rest normal turned onto its normal now, its rest place carried onto its place now.
        for(int v = 0; v < n; v++)
        {
            const glm::vec3& rp = at(s.bridgeRestP, br.first + v);
            const glm::mat3 R = glm::mat3_cast(turnBetween(at(s.bridgeRestN, br.first + v), N[v]));
            const glm::vec3 t = P[v] - R * rp;
            float* m = o + v * 12;
            for(int row = 0; row < 3; row++)
            {
                m[row * 4 + 0] = R[0][row];
                m[row * 4 + 1] = R[1][row];
                m[row * 4 + 2] = R[2][row];
                m[row * 4 + 3] = t[row];
            }
        }
    }
    bridgeStatsAdd(Sys_DoubleTime() - t0, joints);
    return s.bridgeVerts;
}

bool makeBridgeModel(qmodel_t* mod, const Shell& s, const aliashdr_t* kh)
{
    const int numVerts = s.bridgeVerts, numIndexes = count(s.bridgeTris);
    if(numVerts <= 0 || numVerts > 256 || numIndexes <= 0 || !kh)
    {
        return false;
    }
    aliashdr_t skinHdr;
    memcpy(&skinHdr, kh, sizeof(aliashdr_t)); // (as makeModel: the hunk growing below may evict the skins' model)
    kh = &skinHdr;
    const int start = Hunk_LowMark();
    auto* hdr = static_cast<aliashdr_t*>(Hunk_Alloc(static_cast<int>(sizeof(aliashdr_t))));
    auto* bones = static_cast<boneinfo_t*>(Hunk_Alloc(static_cast<int>(sizeof(boneinfo_t)) * numVerts));
    auto* bind = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numVerts));
    auto* poses = static_cast<bonepose_t*>(Hunk_Alloc(static_cast<int>(sizeof(bonepose_t)) * numVerts));
    auto* verts = static_cast<iqmvert_t*>(Hunk_Alloc(static_cast<int>(sizeof(iqmvert_t)) * numVerts));
    auto* indexes = static_cast<unsigned short*>(Hunk_Alloc(static_cast<int>(sizeof(unsigned short)) * numIndexes));
    hdr->ident = kh->ident;
    hdr->version = kh->version;
    hdr->scale[0] = hdr->scale[1] = hdr->scale[2] = 1.f;
    hdr->boundingradius = 64.f;
    hdr->numskins = kh->numskins;
    hdr->skinwidth = kh->skinwidth;
    hdr->skinheight = kh->skinheight;
    hdr->numverts = numVerts;
    hdr->numverts_vbo = numVerts;
    hdr->numtris = numIndexes / 3;
    hdr->numframes = 1;
    hdr->synctype = ST_SYNC;
    hdr->numindexes = numIndexes;
    hdr->numposes = 1;
    hdr->numbones = numVerts;
    hdr->poseverttype = aliashdr_t::PV_IQM;
    hdr->nextsurface = 0;
    hdr->frames[0].firstpose = 0;
    hdr->frames[0].numposes = 1;
    hdr->frames[0].interval = 0.1f;
    q_strlcpy(hdr->frames[0].name, "bridge", sizeof(hdr->frames[0].name));
    memcpy(hdr->gltextures, kh->gltextures, sizeof(hdr->gltextures));
    memcpy(hdr->fbtextures, kh->fbtextures, sizeof(hdr->fbtextures));
    memcpy(hdr->texels, kh->texels, sizeof(hdr->texels));
    for(int b = 0; b < numVerts; b++)
    {
        q_snprintf(bones[b].name, sizeof(bones[b].name), "bridge:%d", b);
        bones[b].parent = -1;
        for(int i = 0; i < 12; i++)
        {
            const float id = i % 5 == 0 ? 1.f : 0.f;
            bones[b].inverse.mat[i] = id;
            bind[b].mat[i] = id;
            poses[b].mat[i] = id;
        }
    }
    // (the leather's place in the atlas, as the sleeves': round it u; along it v, a fold of geometry a fold of paint (make_agarmor.py
    // paints four down the block); no folds: the paint's eight, mirrored about the joint)
    constexpr float jx0 = 224.f, jy0 = 64.f, jx1 = 256.f, jy1 = 96.f; // (the atlas's joint block, make_agarmor.py)
    const float padW = static_cast<float>(TexMgr_PadConditional(kh->skinwidth)), padH = static_cast<float>(TexMgr_PadConditional(kh->skinheight));
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        for(int i = 0; br.ok && i < br.rings; i++)
        {
            const float t = static_cast<float>(i) / static_cast<float>(br.rings - 1);
            const float v = br.folds > 0 ? t * static_cast<float>(br.folds) * 0.25f : 1.f - za::abs(2.f * t - 1.f);
            for(int c = 0; c <= bridgeCols; c++)
            {
                const int k = br.first + i * ringVerts + c;
                const float u = static_cast<float>(c) / static_cast<float>(bridgeCols);
                iqmvert_t& o = verts[k];
                const glm::vec3& p = at(s.bridgeRestP, k);
                const glm::vec3& n = at(s.bridgeRestN, k);
                for(int a = 0; a < 3; a++)
                {
                    o.xyz[a] = p[a];
                    o.norm[a] = static_cast<int8_t>(za::clamp(static_cast<int>(n[a] * 127.f + (n[a] < 0.f ? -0.5f : 0.5f)), -127, 127));
                }
                o.norm[3] = 0;
                o.st[0] = (jx0 + 0.5f + za::clamp(u, 0.f, 1.f) * (jx1 - jx0 - 1.f)) / padW;
                o.st[1] = (jy0 + 0.5f + za::clamp(v, 0.f, 1.f) * (jy1 - jy0 - 1.f)) / padH;
                for(int q = 0; q < 4; q++)
                {
                    o.weight[q] = 0;
                    o.idx[q] = 0;
                }
                o.weight[0] = 255;
                o.idx[0] = static_cast<uint8_t>(k);
            }
        }
    }
    for(int i = 0; i + 2 < numIndexes; i += 3)
    {
        indexes[i] = static_cast<unsigned short>(at(s.bridgeTris, i));
        indexes[i + 1] = static_cast<unsigned short>(at(s.bridgeTris, s.body.nativeFlip ? i + 2 : i + 1));
        indexes[i + 2] = static_cast<unsigned short>(at(s.bridgeTris, s.body.nativeFlip ? i + 1 : i + 2));
    }
    hdr->boneinfo = reinterpret_cast<byte*>(bones) - reinterpret_cast<byte*>(hdr);
    hdr->bindpose = reinterpret_cast<byte*>(bind) - reinterpret_cast<byte*>(hdr);
    hdr->boneposedata = reinterpret_cast<byte*>(poses) - reinterpret_cast<byte*>(hdr);
    hdr->vertexes = reinterpret_cast<byte*>(verts) - reinterpret_cast<byte*>(hdr);
    hdr->indexes = reinterpret_cast<byte*>(indexes) - reinterpret_cast<byte*>(hdr);
    if(mod->meshvbo)
    {
        GLMesh_DeleteVertexBuffer(mod);
    }
    GLMesh_LoadVertexBuffer(mod, hdr);
    mod->type = mod_alias;
    mod->needload = false;
    mod->numframes = 1;
    mod->synctype = ST_SYNC;
    mod->flags = 0;
    for(int i = 0; i < 3; i++)
    {
        mod->mins[i] = mod->ymins[i] = mod->rmins[i] = -128.f;
        mod->maxs[i] = mod->ymaxs[i] = mod->rmaxs[i] = 128.f;
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
    Con_DPrintf("agarmor: %s made (%d vertices, %d triangles)\n", mod->name, numVerts, numIndexes / 3);
    return true;
}

void bridgeStatsAdd(double seconds, int joints)
{
    Stats& st = stats;
    if(st.frame != host_framecount)
    {
        st.worstFrame = za::max(st.worstFrame, st.frameSeconds);
        st.frameSeconds = 0.0;
        st.frame = host_framecount;
        st.frames++;
    }
    st.seconds += seconds;
    st.frameSeconds += seconds;
    st.joints += joints;
}

void bridgeStats_f()
{
    Stats& st = stats;
    st.worstFrame = za::max(st.worstFrame, st.frameSeconds);
    const double frames = static_cast<double>(za::max(st.frames, 1));
    Con_Printf("agarmor bridge stats: %d frames, %d joints made (%.1f a frame), %.1f us a frame (worst %.1f), %.2f us a joint\n", st.frames, st.joints,
        static_cast<double>(st.joints) / frames, st.seconds * 1e6 / frames, st.worstFrame * 1e6,
        st.joints > 0 ? st.seconds * 1e6 / static_cast<double>(st.joints) : 0.0);
    st = Stats{};
}

namespace
{
// vr_agarmor_bridge_trace's last frame, a few bodies' worth.
constexpr int traceBodies = 8;
struct TraceBody
{
    int ent{-1}, frame{-1};
    bool had[NumJoints]{};
    glm::vec3 P[NumJoints][bridgeMostRings * ringVerts];
    glm::vec3 X[NumJoints][bridgeMostRings], Z[NumJoints][bridgeMostRings];
};
TraceBody traced[traceBodies];
} // namespace

void bridgeTrace(const Shell& s, int ent, const char* model, int pieces, const float* bones, int have)
{
    const int mask = static_cast<int>(vr_agarmor_bridge_trace.value);
    if(mask <= 0 || !bones)
    {
        return;
    }
    TraceBody* tb = nullptr;
    for(TraceBody& t : traced)
    {
        tb = !tb && t.ent == ent ? &t : tb;
    }
    for(TraceBody& t : traced)
    {
        tb = !tb && (t.ent < 0 || t.frame < host_framecount - 2) ? &t : tb;
    }
    if(!tb)
    {
        return;
    }
    const bool fresh = tb->ent != ent || tb->frame != host_framecount - 1;
    tb->ent = ent;
    tb->frame = host_framecount;
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        glm::vec3 P[bridgeMostRings * ringVerts], N[bridgeMostRings * ringVerts];
        BridgeDiag d;
        const bool upperWorn = ((pieces >> br.upper) & 1) != 0, lowerWorn = ((pieces >> br.lower) & 1) != 0;
        if(!((mask >> j) & 1) || !br.ok || (!upperWorn && !lowerWorn) || !evalBridge(s, j, bones, have, upperWorn, lowerWorn, P, N, &d))
        {
            tb->had[j] = false;
            continue;
        }
        const int n = br.rings * ringVerts, lastRing = (br.rings - 1) * ringVerts;
        if(tb->had[j] && !fresh)
        {
            float ref = 0.f, jump = 0.f, spin = 0.f, endSpin = 0.f;
            int at = 0, spinAt = 0;
            for(int v = 0; v < n; v++)
            {
                const float m = glm::distance(P[v], tb->P[j][v]);
                ref = v < ringVerts || v >= lastRing ? za::max(ref, m) : ref;
                at = m > jump ? v / ringVerts : at;
                jump = za::max(jump, m);
            }
            for(int i = 0; i < d.rings; i++)
            {
                const float a = frameTurnDeg(tb->X[j][i], tb->Z[j][i], d.ringX[i], d.ringZ[i]);
                endSpin = i == 0 || i == d.rings - 1 ? za::max(endSpin, a) : endSpin;
                spinAt = i > 0 && i < d.rings - 1 && a > spin ? i : spinAt;
                spin = i > 0 && i < d.rings - 1 ? za::max(spin, a) : spin;
            }
            Con_Printf("bridge trace: %d %s %s f%d move %.2f (ring %d) rows %.2f; turn %.1f (ring %d) rows %.1f; bend %.1f a %.2f %.2f "
                       "chord %.2f back %.3f keep %.2f m %.2f %.2f push %.2f\n",
                ent, model, pieceNames[br.lower], host_framecount, static_cast<double>(jump), at, static_cast<double>(ref),
                static_cast<double>(spin), spinAt, static_cast<double>(endSpin), static_cast<double>(d.theta * 180.f / pi),
                static_cast<double>(d.a0), static_cast<double>(d.a1), static_cast<double>(d.chord), static_cast<double>(d.back),
                static_cast<double>(d.keep), static_cast<double>(d.m0), static_cast<double>(d.m1), static_cast<double>(d.pushMost));
        }
        tb->had[j] = true;
        memcpy(tb->P[j], P, sizeof(glm::vec3) * static_cast<size_t>(n));
        for(int i = 0; i < d.rings; i++)
        {
            tb->X[j][i] = d.ringX[i];
            tb->Z[j][i] = d.ringZ[i];
        }
    }
}

namespace
{
void bonesOfPose(const rigpose::Rigid* F, int numBones, float* bones)
{
    for(int b = 0; b < numBones; b++)
    {
        const glm::mat3 r = glm::mat3_cast(F[b].rot);
        for(int row = 0; row < 3; row++)
        {
            bones[b * 12 + row * 4 + 0] = r[0][row];
            bones[b * 12 + row * 4 + 1] = r[1][row];
            bones[b * 12 + row * 4 + 2] = r[2][row];
            bones[b * 12 + row * 4 + 3] = F[b].pos[row];
        }
    }
}

// The leather through the model's animation frames as the game draws them (each frame's bones lerped to the next's in
// small steps): no vertex of it may jump much further in a step than its two rows do (the leather is a function of the
// bones: smooth bones, smooth leather; a jump is a choice in it flipping, the knight's right elbow's spazzing).
constexpr int continuitySteps = 8;       // steps between one frame and the next
constexpr float continuityShare = 2.f;   // a vertex may move this many times its rows' furthest in a step (3 before)...
constexpr float continuitySlack = 0.25f; // ... and this much more (units; 0.5 before)
constexpr float turnShare = 2.f;         // a middle ring may turn this many times its rows' furthest in a step, plus the bend's change...
constexpr float turnSlack = 2.f;         // ... and this much more (degrees)
constexpr float sweepStep = 1.f;         // the joint swept through its range in steps of this (degrees)
constexpr float sweepShare = 2.5f;       // a vertex may move this many times its rows' furthest in a sweep's step...
constexpr float sweepSlack = 0.1f;       // ... and this much more (units)
constexpr float bridgeRestSlack = 0.05f; // the skin at rest may be this far outside the leather (units)
constexpr float bridgeInFacing = 0.25f;  // a cell turned in: its face this far (a cosine) against the way out (a fold's side, edge on: no)
constexpr const char* bridgeCheckModels[] = {"progs/vrgrunt.mdl", "progs/vrenforcer.mdl", "progs/vrknight.mdl", "progs/vrhknight.mdl", "progs/vrbody.mdl"};

// A step of the leather: its furthest vertex move against its end rings' (the rows', carried by the bones), its middle
// rings' furthest turn against its end rings'.
struct Step
{
    float jump{0.f}, ref{0.f}, turn{0.f}, endTurn{0.f}, bendTurn{0.f}; // (bendTurn: the rows' bend's change, degrees)
    int at{0};
};

[[nodiscard]] Step stepOf(const glm::vec3* P, const glm::vec3* was, const BridgeDiag& d, const BridgeDiag& dWas)
{
    Step st;
    const int n = d.rings * ringVerts, lastRing = (d.rings - 1) * ringVerts;
    for(int v = 0; v < n; v++)
    {
        const float m = glm::distance(P[v], was[v]);
        st.ref = v < ringVerts || v >= lastRing ? za::max(st.ref, m) : st.ref;
        st.at = m > st.jump ? v : st.at;
        st.jump = za::max(st.jump, m);
    }
    for(int i = 0; i < d.rings; i++)
    {
        const float a = frameTurnDeg(dWas.ringX[i], dWas.ringZ[i], d.ringX[i], d.ringZ[i]);
        st.endTurn = i == 0 || i == d.rings - 1 ? za::max(st.endTurn, a) : st.endTurn;
        st.turn = i > 0 && i < d.rings - 1 ? za::max(st.turn, a) : st.turn;
    }
    st.bendTurn = za::abs(d.theta - dWas.theta) * (180.f / pi);
    return st;
}

void bonesOfRig(const rig2::Rig& rig, const rig2::Pose& pose, float* bones)
{
    rigpose::Rigid F[rig2::maxBones];
    rig2::forward(rig, pose, F);
    bonesOfPose(F, rig.numBones, bones);
}

// The columns whose rows cross: a corner of the lower row less than bridgeRowsCrossed ahead of the upper row's along the
// joint's mean axis. The plates' own rows, skinned across the joint, tilt against each other as it moves (the enforcer's and
// the knight's elbows straightened from their 65-76 degree bind pose: the forearm's row 1.3-1.6 units behind the upper arm's
// on the inside, his bracer_r start -1.13 further), so the leather's column there runs back whatever is laid between; its
// cells lie in the plates' lap and are drawn from both sides (VR_AliasTwoSided). Returns the closest the rows come.
float crossedColumns(const glm::vec3* P, const BridgeDiag& d, int rings, bool* crossed)
{
    const int last = rings - 1;
    const glm::vec3 axis = geom::safeNormalize(d.ringZ[0] + d.ringZ[last], d.ringZ[0], 1e-8f);
    float closest = 1e9f;
    for(int c = 0; c < bridgeCols; c++)
    {
        const float g = glm::dot(P[last * ringVerts + c] - P[c], axis);
        crossed[c] = g < bridgeRowsCrossed;
        closest = za::min(closest, g);
    }
    return closest;
}

// (a cell in the lap: either of its columns crossed)
bool inLap(const bool* crossed, int q)
{
    return crossed[q % bridgeCols] || crossed[(q % bridgeCols + 1) % bridgeCols];
}

// The leather at rest (the joints straight), each joint: its cells facing in (against the way out from its rings' middle by
// more than bridgeInFacing: drawn from behind, a hole) and the skin of its two limb bones between its end rings outside it
// (the arm cutting through), each ring's corners' reach and their turn round it, for the left and the right side alike.
template <typename Check>
void restCheck(const char* model, const Shell& s, const rig2::Rig& rig, const Check& check)
{
    constexpr int most = bridgeMostRings * ringVerts;
    glm::vec3 P[most], N[most];
    float bones[rig2::maxBones * 12]{};
    char what[512];
    // (twice: the bind pose, then the joint straight (its hinge at 0, or the nearest its range allows): his report, the enforcer's
    // right elbow and the grunt's left, holed with the arm straight; the enforcer's gun arm is bent at the bind pose)
    for(int jj = 0; jj < NumJoints * 2; jj++)
    {
        const int j = jj % NumJoints;
        const bool straight = jj >= NumJoints;
        const Bridge& br = s.bridges[j];
        const int hinge = br.ok ? br.plates[1].bone : -1;
        if(hinge < 0 || hinge >= rig.numBones)
        {
            continue;
        }
        rig2::Pose pose = rig2::restPose(rig);
        const float bindDeg = pose.a[hinge].x;
        pose.a[hinge].x = straight ? za::clamp(0.f, rig.bones[hinge].range.x, rig.bones[hinge].range.y) : bindDeg;
        bonesOfRig(rig, pose, bones);
        BridgeDiag dg;
        if(!evalBridge(s, j, bones, rig.numBones, true, true, P, N, &dg))
        {
            continue;
        }
        const int last = br.rings - 1;
        if(developer.value >= 1.f)
        {
            // (the end rings' planes (Newell) against each other and against their frames' axes, the chord)
            glm::vec3 nrm[2]{glm::vec3{0.f}, glm::vec3{0.f}}, mids[2]{glm::vec3{0.f}, glm::vec3{0.f}};
            for(int e = 0; e < 2; e++)
            {
                const int r = e == 0 ? 0 : last;
                for(int c = 0; c < bridgeCols; c++)
                {
                    const glm::vec3& a = P[r * ringVerts + c];
                    const glm::vec3& b = P[r * ringVerts + (c + 1) % bridgeCols];
                    nrm[e] += glm::vec3{(a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)};
                    mids[e] += a / static_cast<float>(bridgeCols);
                }
                nrm[e] = geom::safeNormalize(nrm[e], dg.ringZ[r], 1e-8f);
                nrm[e] = glm::dot(nrm[e], dg.ringZ[r]) < 0.f ? -nrm[e] : nrm[e];
            }
            const glm::vec3 ch = geom::safeNormalize(mids[1] - mids[0], dg.ringZ[0], 1e-8f);
            const auto deg = [](const glm::vec3& a, const glm::vec3& b) { return static_cast<double>(za::acos(za::clamp(glm::dot(a, b), -1.f, 1.f)) * (180.f / pi)); };
            Con_Printf("agarmor bridge rest: %s %s ends: axes %.0f deg apart, planes %.0f; upper plane %.0f off its axis, lower %.0f; chord %.2f, %.0f off the upper axis, %.0f off the lower\n",
                model, pieceNames[br.lower], deg(dg.ringZ[0], dg.ringZ[last]), deg(nrm[0], nrm[1]), deg(nrm[0], dg.ringZ[0]), deg(nrm[1], dg.ringZ[last]),
                static_cast<double>(glm::distance(mids[0], mids[1])), deg(ch, dg.ringZ[0]), deg(ch, dg.ringZ[last]));
            Con_Printf("agarmor bridge rest: %s %s tangents: along %.2f, a0 %.2f a1 %.2f, m0 %.2f m1 %.2f, fit %.2f keep %.2f, push %.2f\n", model,
                pieceNames[br.lower], static_cast<double>(glm::dot(mids[1] - mids[0], dg.ringZ[0])), static_cast<double>(dg.a0), static_cast<double>(dg.a1),
                static_cast<double>(dg.m0), static_cast<double>(dg.m1), static_cast<double>(dg.fit), static_cast<double>(dg.keep), static_cast<double>(dg.pushMost));
        }
        glm::vec3 C[bridgeMostRings];
        for(int i = 0; i < br.rings; i++)
        {
            C[i] = glm::vec3{0.f};
            for(int c = 0; c < bridgeCols; c++)
            {
                C[i] += P[i * ringVerts + c] / static_cast<float>(bridgeCols);
            }
        }
        // (the drawn winding: bridgeRest's, the way most cells face at the bind pose)
        float facing = 0.f, signs[bridgeMostRings * bridgeCols]{};
        for(int i = 0; i < last; i++)
        {
            for(int c = 0; c < bridgeCols; c++)
            {
                const int a = i * ringVerts + c;
                const glm::vec3 mid = (P[a] + P[a + 1] + P[a + ringVerts] + P[a + ringVerts + 1]) * 0.25f;
                const glm::vec3 n = glm::cross(P[a + 1] - P[a], P[a + ringVerts + 1] - P[a]) + glm::cross(P[a + ringVerts + 1] - P[a], P[a + ringVerts] - P[a]);
                signs[i * bridgeCols + c] = glm::dot(geom::safeNormalize(n, glm::vec3{0.f}, 1e-12f), geom::safeNormalize(mid - (C[i] + C[i + 1]) * 0.5f, glm::vec3{0.f}, 1e-12f));
                facing += signs[i * bridgeCols + c];
            }
        }
        // (developer 1: the two parts' grids' radii at the rows the ends sit on, then each ring's corners round its middle,
        // angle and reach in the upper row's frame)
        for(int e = 0; e < 2 && developer.value >= 1.f; e++)
        {
            const PlateGrid& g = s.tubes[e == 0 ? br.upper : br.lower].grid;
            const int ring = e == 0 ? g.rows : 0;
            Con_Printf("agarmor bridge rest: %s %s grid %d ring %d of %d: %.2f %.2f %.2f %.2f %.2f %.2f axis %.2f %.2f, h %.2f\n", model,
                pieceNames[e == 0 ? br.upper : br.lower], g.cols, ring, g.rows, static_cast<double>(g.at2(ring, 0)), static_cast<double>(g.at2(ring, 1)),
                static_cast<double>(g.at2(ring, 2)), static_cast<double>(g.at2(ring, 3)), static_cast<double>(g.at2(ring, 4)),
                static_cast<double>(g.at2(ring, 5)), static_cast<double>(at(g.axis, ring).x), static_cast<double>(at(g.axis, ring).y),
                static_cast<double>(g.hAt(ring)));
            for(int r = 0; r <= g.rows && e == 1; r++)
            {
                Con_Printf("agarmor bridge rest: %s %s ring %d axis %.2f %.2f radii %.2f %.2f %.2f %.2f %.2f %.2f\n", model, pieceNames[br.lower], r,
                    static_cast<double>(at(g.axis, r).x), static_cast<double>(at(g.axis, r).y), static_cast<double>(g.at2(r, 0)),
                    static_cast<double>(g.at2(r, 1)), static_cast<double>(g.at2(r, 2)), static_cast<double>(g.at2(r, 3)), static_cast<double>(g.at2(r, 4)),
                    static_cast<double>(g.at2(r, 5)));
            }
        }
        for(int i = 0; i < br.rings && developer.value >= 1.f; i++)
        {
            const glm::vec3 x = geom::safeNormalize(P[0] - C[0], glm::vec3{1.f, 0.f, 0.f}, 1e-8f);
            const glm::vec3 z = geom::safeNormalize(C[last] - C[0], glm::vec3{0.f, 0.f, 1.f}, 1e-8f);
            const glm::vec3 y = glm::cross(z, x);
            char line[256];
            int at0 = q_snprintf(line, sizeof(line), "ring %d:", i);
            for(int c = 0; c < bridgeCols && at0 < static_cast<int>(sizeof(line)) - 24; c++)
            {
                const glm::vec3 q = P[i * ringVerts + c] - C[i];
                at0 += q_snprintf(line + at0, sizeof(line) - static_cast<size_t>(at0), " %4.0f/%.2f/%.2f", static_cast<double>(za::atan2(glm::dot(q, y), glm::dot(q, x)) * (180.f / pi)),
                    static_cast<double>(glm::length(q - z * glm::dot(q, z))), static_cast<double>(glm::dot(P[i * ringVerts + c] - C[0], z)));
            }
            Con_Printf("agarmor bridge rest: %s %s %s\n", model, pieceNames[br.lower], line);
        }
        // (the rows crossed: those cells ('c') are in the plates' lap, counted apart)
        bool crossed[bridgeCols]{};
        const float rowGap = crossedColumns(P, dg, br.rings, crossed);
        int in = 0, inCrossed = 0;
        char map[96]{};
        int w = 0;
        for(int q = 0; q < last * bridgeCols; q++)
        {
            const bool turned = facing > 0.f ? signs[q] < -bridgeInFacing : signs[q] > bridgeInFacing;
            const bool lap = inLap(crossed, q);
            in += turned && !lap ? 1 : 0;
            inCrossed += turned && lap ? 1 : 0;
            map[w++] = turned ? (lap ? 'c' : 'x') : '.';
            if(q % bridgeCols == bridgeCols - 1 && w + 2 < static_cast<int>(sizeof(map)))
            {
                map[w++] = '/';
            }
        }
        map[w] = 0;
        // The skin between the end rings outside the leather: each vertex of the two limb bones (mostly), found on the rings'
        // centre line, against the polygon there in its direction (the two rings' polygons in the segment's plane, lerped).
        const int boneA = s.tubes[br.upper].bone, boneB = s.tubes[br.lower].bone;
        int outside = 0, tested = 0, underRows = 0;
        float worst = 0.f, worstAt = 0.f;
        for(int v = 0; v < count(s.body.p); v++)
        {
            const W& wv = at(s.body.w, v);
            const int b = heaviest(wv);
            if(b != boneA && b != boneB)
            {
                continue;
            }
            glm::vec3 p;
            if(!posedOf(skinnedOf(at(s.body.p, v), wv), bones, rig.numBones, p))
            {
                continue;
            }
            for(int i = 0; i < last; i++)
            {
                const glm::vec3 d = C[i + 1] - C[i];
                const float len2 = glm::dot(d, d);
                const float t = len2 > 1e-8f ? glm::dot(p - C[i], d) / len2 : -1.f;
                if(t < 0.f || t > 1.f)
                {
                    continue;
                }
                const glm::vec3 axis = d / sqrtf(len2);
                const glm::vec3 o = C[i] + d * t;
                glm::vec3 rel = p - o;
                rel -= axis * glm::dot(rel, axis);
                const float ru = glm::length(rel);
                // (the polygons' reach that way: each ring's corners in the plane, the edge it crosses)
                float reach[2] = {0.f, 0.f};
                for(int e = 0; e < 2; e++)
                {
                    glm::vec2 poly[bridgeCols];
                    const glm::vec3 u = geom::safeNormalize(rel, geom::safeNormalize(glm::cross(axis, glm::vec3{0.f, 0.f, 1.f}), glm::vec3{1.f, 0.f, 0.f}, 1e-6f), 1e-6f);
                    const glm::vec3 vv = glm::cross(axis, u);
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        const glm::vec3 q = P[(i + e) * ringVerts + c] - C[i + e];
                        poly[c] = glm::vec2{glm::dot(q, u), glm::dot(q, vv)};
                    }
                    int edge = 0;
                    float along = 0.f;
                    reach[e] = polygonReach(poly, glm::vec2{1.f, 0.f}, edge, along);
                }
                const float r = reach[0] + (reach[1] - reach[0]) * t;
                if(r <= 0.f || ru > 1.6f * r)
                {
                    break; // (not under this leather: the chest, the other limb)
                }
                tested++;
                // (within half a ring of either row: under the plates' own lapping rows, their fit's, not the leather's: apart)
                const float along = static_cast<float>(i) + t;
                if(ru > r + bridgeRestSlack && (along < 0.5f || along > static_cast<float>(last) - 0.5f))
                {
                    underRows++;
                }
                else if(ru > r + bridgeRestSlack)
                {
                    outside++;
                    worstAt = ru - r > worst ? static_cast<float>(i) + t : worstAt;
                    worst = za::max(worst, ru - r);
                }
                break;
            }
        }
        // Each ring's corners' least reach over its mean, and the most a ring's corner turned round its middle against the
        // upper row's (the lerp crossing itself: a twist).
        float thin = 1e9f, twist = 0.f;
        for(int i = 0; i < br.rings; i++)
        {
            const glm::vec3 axis = geom::safeNormalize(i < last ? C[i + 1] - C[i] : C[i] - C[i - 1], glm::vec3{0.f, 0.f, 1.f}, 1e-8f);
            float lo = 1e9f, mean = 0.f;
            for(int c = 0; c < bridgeCols; c++)
            {
                const float r = glm::distance(P[i * ringVerts + c], C[i]);
                lo = za::min(lo, r);
                mean += r / static_cast<float>(bridgeCols);
                glm::vec3 a = P[c] - C[0], bq = P[i * ringVerts + c] - C[i];
                a -= axis * glm::dot(a, axis);
                bq -= axis * glm::dot(bq, axis);
                const float cosT = glm::dot(geom::safeNormalize(a, glm::vec3{1.f, 0.f, 0.f}, 1e-8f), geom::safeNormalize(bq, glm::vec3{1.f, 0.f, 0.f}, 1e-8f));
                twist = za::max(twist, za::acos(za::clamp(cosT, -1.f, 1.f)) * (180.f / pi));
            }
            thin = za::min(thin, mean > 1e-6f ? lo / mean : 0.f);
        }
        q_snprintf(what, sizeof(what), "%s %s at rest (%s, %.0f deg): no cell turned in (%d of %d: %s; %d more where the rows cross, %.2f apart at the closest), the arm not through it (%d of %d skin vertices out, %d more under the rows, "
                                       "furthest %.2f at ring %.1f of %d); thinnest ring %.2f, most twist %.0f deg",
            model, pieceNames[br.lower], straight ? "straight" : "bind", static_cast<double>(pose.a[hinge].x), in, last * bridgeCols, map, inCrossed, static_cast<double>(rowGap), outside,
            tested, underRows, static_cast<double>(worst), static_cast<double>(worstAt), last, static_cast<double>(thin), static_cast<double>(twist));
        // (straight, his holes: a verdict on the cells (none turned in outside the rows' lap; the leather is drawn from both
        // sides, VR_AliasTwoSided, but a cell turned in shows its back); the skin and the bind pose a measure: ROUND24.md,
        // "Elbow leather, part 3")
        if(straight)
        {
            check(in == 0, what);
        }
        else
        {
            Con_Printf("agarmor bridge check: (info) %s\n", what);
        }
    }
}

// The leather as each joint is swept through its range a degree at a time, the rest of the body at rest (vr_agarmor_bend's
// pose): no vertex may move much further in a step than its rows do (a lump pushed out and let go over a few degrees: the
// enforcer's elbows, the crease's push).
template <typename Check>
void sweepCheck(const char* model, const Shell& s, const rig2::Rig& rig, const Check& check)
{
    constexpr int most = bridgeMostRings * ringVerts;
    glm::vec3 P[most], N[most], was[most];
    float bones[rig2::maxBones * 12]{};
    char what[320];
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        const int hinge = br.ok ? br.plates[1].bone : -1;
        if(hinge < 0 || hinge >= rig.numBones)
        {
            continue;
        }
        const glm::vec2 range = rig.bones[hinge].range;
        const int steps = za::max(static_cast<int>((range.y - range.x) / sweepStep + 0.5f), 1);
        BridgeDiag d, dWas, worstD, worstWas;
        Step worst;
        float worstExcess = -1e9f, worstDeg = 0.f;
        int worstIn = -1, worstInOf = 0, worstLap = 0;
        char worstMap[96]{};
        float worstInDeg = 0.f, worstThin = 1e9f;
        bool had = false;
        for(int k = 0; k <= steps; k++)
        {
            rig2::Pose pose = rig2::restPose(rig);
            pose.a[hinge].x = range.x + (range.y - range.x) * static_cast<float>(k) / static_cast<float>(steps);
            bonesOfRig(rig, pose, bones);
            if(!evalBridge(s, j, bones, rig.numBones, true, true, P, N, &d))
            {
                had = false;
                continue;
            }
            // (its cells turned in: a cell's face against the way out from its rings' middle, against the way most face
            // (the model's winding, bridgeRest): drawn from behind, a hole; and its thinnest ring, its corners' least reach
            // from its middle over its mean)
            {
                int in = 0, total = 0;
                float facing = 0.f;
                float signs[bridgeMostRings * bridgeCols]{};
                for(int i = 0; i + 1 < br.rings; i++)
                {
                    glm::vec3 c0{0.f}, c1{0.f};
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        c0 += P[i * ringVerts + c] / static_cast<float>(bridgeCols);
                        c1 += P[(i + 1) * ringVerts + c] / static_cast<float>(bridgeCols);
                    }
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        const int a = i * ringVerts + c;
                        const glm::vec3 mid = (P[a] + P[a + 1] + P[a + ringVerts] + P[a + ringVerts + 1]) * 0.25f;
                        const glm::vec3 n = glm::cross(P[a + 1] - P[a], P[a + ringVerts + 1] - P[a]) +
                                            glm::cross(P[a + ringVerts + 1] - P[a], P[a + ringVerts] - P[a]);
                        signs[i * bridgeCols + c] = glm::dot(geom::safeNormalize(n, glm::vec3{0.f}, 1e-12f), geom::safeNormalize(mid - (c0 + c1) * 0.5f, glm::vec3{0.f}, 1e-12f));
                        facing += signs[i * bridgeCols + c];
                    }
                }
                bool crossed[bridgeCols]{};
                (void)crossedColumns(P, d, br.rings, crossed);
                int lapIn = 0;
                for(int q = 0; q < (br.rings - 1) * bridgeCols; q++)
                {
                    const bool turned = facing > 0.f ? signs[q] < -bridgeInFacing : signs[q] > bridgeInFacing;
                    in += turned && !inLap(crossed, q) ? 1 : 0;
                    lapIn += turned && inLap(crossed, q) ? 1 : 0;
                    total++;
                }
                worstLap = za::max(worstLap, lapIn);
                float thin = 1e9f;
                for(int i = 0; i < br.rings; i++)
                {
                    glm::vec3 c0{0.f};
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        c0 += P[i * ringVerts + c] / static_cast<float>(bridgeCols);
                    }
                    float lo = 1e9f, mean = 0.f;
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        const float r = glm::distance(P[i * ringVerts + c], c0);
                        lo = za::min(lo, r);
                        mean += r / static_cast<float>(bridgeCols);
                    }
                    thin = za::min(thin, mean > 1e-6f ? lo / mean : 0.f);
                }
                if(in > worstIn || (k == 0 && worstIn < 0))
                {
                    worstIn = in;
                    worstInDeg = pose.a[hinge].x;
                    worstInOf = total;
                    // (which: a ring's cells a character each, x turned in, / between rings)
                    int w = 0;
                    for(int q = 0; q < (br.rings - 1) * bridgeCols && w + 2 < static_cast<int>(sizeof(worstMap)); q++)
                    {
                        const bool turned = facing > 0.f ? signs[q] < -bridgeInFacing : signs[q] > bridgeInFacing;
                        worstMap[w++] = turned ? (inLap(crossed, q) ? 'c' : 'x') : '.';
                        worstMap[w] = q % bridgeCols == bridgeCols - 1 ? '/' : worstMap[w];
                        w += q % bridgeCols == bridgeCols - 1 ? 1 : 0;
                    }
                    worstMap[w] = 0;
                }
                worstThin = za::min(worstThin, thin);
            }
            if(had)
            {
                const Step st = stepOf(P, was, d, dWas);
                const float excess = st.jump - sweepShare * st.ref;
                if(developer.value >= 2.f)
                {
                    Con_Printf("agarmor bridge sweep: %s %s %.0f deg: move %.3f (ring %d) rows %.3f, turn %.2f rows %.2f; push %.2f, crease gave up %.2f, "
                               "most asked %.2f; first asked %.2f crease %.2f; bend %.1f; back %.3f keep %.2f m %.2f %.2f\n",
                        model, pieceNames[br.lower], static_cast<double>(pose.a[hinge].x), static_cast<double>(st.jump), st.at / ringVerts,
                        static_cast<double>(st.ref), static_cast<double>(st.turn), static_cast<double>(st.endTurn), static_cast<double>(d.pushMost),
                        static_cast<double>(d.creaseCut), static_cast<double>(d.needMost), static_cast<double>(d.firstNeed),
                        static_cast<double>(d.firstCrease), static_cast<double>(d.theta * 180.f / pi),
                        static_cast<double>(d.back), static_cast<double>(d.keep), static_cast<double>(d.m0), static_cast<double>(d.m1));
                }
                if(excess > worstExcess)
                {
                    worstExcess = excess;
                    worst = st;
                    worstDeg = pose.a[hinge].x;
                    worstD = d;
                    worstWas = dWas;
                }
            }
            had = true;
            memcpy(was, P, sizeof(glm::vec3) * static_cast<size_t>(br.rings * ringVerts));
            dWas = d;
        }
        Con_DPrintf("agarmor bridge check:   %s %s sweep's worst step at %.0f deg, ring %d: most pushed %.2f -> %.2f, crease gave up %.2f -> %.2f\n", model,
            pieceNames[br.lower], static_cast<double>(worstDeg), worst.at / ringVerts, static_cast<double>(worstWas.pushMost),
            static_cast<double>(worstD.pushMost), static_cast<double>(worstWas.creaseCut), static_cast<double>(worstD.creaseCut));
        q_snprintf(what, sizeof(what), "%s %s: swept %.0f..%.0f deg a degree at a time, no vertex jumps: worst %.3f moved against its rows' %.3f (at most %.1fx + %.2f), %.0f deg",
            model, pieceNames[br.lower], static_cast<double>(range.x), static_cast<double>(range.y), static_cast<double>(worst.jump),
            static_cast<double>(worst.ref), static_cast<double>(sweepShare), static_cast<double>(sweepSlack), static_cast<double>(worstDeg));
        check(worstExcess <= sweepSlack, what);
        // (a measure, not a verdict yet: every joint of every kind has some, ROUND24.md "Elbow leather patches")
        Con_Printf("agarmor bridge check: (info) %s %s: swept, cells of the leather turned in: worst %d of %d at %.0f deg (rings / cells, x in, "
                   "c in where the rows cross: %s); at most %d where the rows cross; its thinnest ring's least corner %.2f of its mean\n",
            model, pieceNames[br.lower], za::max(worstIn, 0), worstInOf, static_cast<double>(worstInDeg), worstMap, worstLap, static_cast<double>(worstThin));
    }
}

template <typename Check>
void continuityCheck(qmodel_t* src, const Shell& s, const rig2::Rig& rig, const Check& check)
{
    const ragdoll::Rig* rr = ragdoll::rigFor(src);
    if(!rr || !rr->authored || rr->numBones != rig.numBones || rr->numPoses < 2)
    {
        return;
    }
    constexpr int most = bridgeMostRings * ringVerts;
    glm::vec3 P[most], N[most], was[NumJoints][most];
    BridgeDiag dWas[NumJoints], worstBefore[NumJoints], worstAfter[NumJoints];
    float worstExcess[NumJoints], worstJump[NumJoints]{}, worstRef[NumJoints]{};
    int worstPose[NumJoints]{}, worstStep[NumJoints]{}, worstVert[NumJoints]{}, steps = 0;
    float worstTurnExcess[NumJoints], worstTurn[NumJoints]{}, worstEndTurn[NumJoints]{}, worstBendTurn[NumJoints]{};
    int worstTurnPose[NumJoints]{};
    for(int j = 0; j < NumJoints; j++)
    {
        worstExcess[j] = -1e9f;
        worstTurnExcess[j] = -1e9f;
    }
    rigpose::Rigid F[rig2::maxBones];
    float bones[rig2::maxBones * 12]{};
    for(int p = 0; p + 1 < rr->numPoses; p++)
    {
        for(int k = 0; k <= continuitySteps; k++)
        {
            rig2::posedBetween(*rr, p, p + 1, static_cast<float>(k) / static_cast<float>(continuitySteps), F);
            bonesOfPose(F, rig.numBones, bones);
            steps += k > 0 ? 1 : 0;
            for(int j = 0; j < NumJoints; j++)
            {
                const Bridge& br = s.bridges[j];
                BridgeDiag d;
                if(!br.ok || !evalBridge(s, j, bones, rig.numBones, true, true, P, N, &d))
                {
                    continue;
                }
                const int n = br.rings * ringVerts;
                if(k > 0)
                {
                    // (the furthest any vertex moved against the furthest the end rings did: the rows, carried by the bones)
                    const Step st = stepOf(P, was[j], d, dWas[j]);
                    const float excess = st.jump - continuityShare * st.ref;
                    if(excess > worstExcess[j])
                    {
                        worstExcess[j] = excess;
                        worstJump[j] = st.jump;
                        worstRef[j] = st.ref;
                        worstPose[j] = p;
                        worstStep[j] = k;
                        worstVert[j] = st.at;
                        worstBefore[j] = dWas[j];
                        worstAfter[j] = d;
                    }
                    // (and the middle rings' turn against the rows': a ring spinning on its curve)
                    const float turnExcess = st.turn - turnShare * st.endTurn - st.bendTurn;
                    if(developer.value >= 2.f && (turnExcess > turnSlack || excess > continuitySlack))
                    {
                        const BridgeDiag& a = dWas[j];
                        Con_Printf("agarmor bridge step: %s %s frame %d step %d: move %.2f rows %.2f turn %.1f rows %.1f; bend %.1f->%.1f a %.2f %.2f->%.2f %.2f "
                                   "chord %.2f back %.3f->%.3f keep %.2f->%.2f m %.2f %.2f->%.2f %.2f round %.2f->%.2f push %.2f->%.2f\n",
                            src->name, pieceNames[br.lower], p, k, static_cast<double>(st.jump), static_cast<double>(st.ref), static_cast<double>(st.turn),
                            static_cast<double>(st.endTurn), static_cast<double>(a.theta * 180.f / pi), static_cast<double>(d.theta * 180.f / pi),
                            static_cast<double>(a.a0), static_cast<double>(a.a1), static_cast<double>(d.a0), static_cast<double>(d.a1),
                            static_cast<double>(d.chord), static_cast<double>(a.back), static_cast<double>(d.back), static_cast<double>(a.keep),
                            static_cast<double>(d.keep), static_cast<double>(a.m0), static_cast<double>(a.m1), static_cast<double>(d.m0),
                            static_cast<double>(d.m1), static_cast<double>(a.fit), static_cast<double>(d.fit), static_cast<double>(a.pushMost),
                            static_cast<double>(d.pushMost));
                    }
                    if(turnExcess > worstTurnExcess[j])
                    {
                        worstTurnExcess[j] = turnExcess;
                        worstTurn[j] = st.turn;
                        worstEndTurn[j] = st.endTurn;
                        worstBendTurn[j] = st.bendTurn;
                        worstTurnPose[j] = p;
                    }
                }
                memcpy(was[j], P, sizeof(glm::vec3) * static_cast<size_t>(n));
                dWas[j] = d;
            }
        }
    }
    char what[320];
    for(int j = 0; j < NumJoints; j++)
    {
        const Bridge& br = s.bridges[j];
        if(!br.ok)
        {
            continue;
        }
        const BridgeDiag& a = worstBefore[j];
        const BridgeDiag& b = worstAfter[j];
        Con_DPrintf("agarmor bridge check:   %s %s worst step: frame %d step %d, ring %d; bend %.1f->%.1f deg, joint along the rows %.2f, %.2f "
                    "-> %.2f, %.2f (chord %.2f), curve against its frames %.3f -> %.3f (tangents kept %.2f -> %.2f), most pushed %.2f -> %.2f\n",
            src->name, pieceNames[br.lower], worstPose[j], worstStep[j], worstVert[j] / ringVerts, static_cast<double>(a.theta * 180.f / pi),
            static_cast<double>(b.theta * 180.f / pi), static_cast<double>(a.a0), static_cast<double>(a.a1), static_cast<double>(b.a0),
            static_cast<double>(b.a1), static_cast<double>(b.chord), static_cast<double>(a.back), static_cast<double>(b.back),
            static_cast<double>(a.keep), static_cast<double>(b.keep), static_cast<double>(a.pushMost), static_cast<double>(b.pushMost));
        q_snprintf(what, sizeof(what), "%s %s: through %d frames (%d steps) no vertex jumps: worst %.2f moved against its rows' %.2f, %.2f over %.0fx theirs (at most %.2f), frame %d",
            src->name, pieceNames[br.lower], rr->numPoses, steps, static_cast<double>(worstJump[j]), static_cast<double>(worstRef[j]),
            static_cast<double>(worstExcess[j]), static_cast<double>(continuityShare), static_cast<double>(continuitySlack), worstPose[j]);
        check(worstExcess[j] <= continuitySlack, what);
        q_snprintf(what, sizeof(what), "%s %s: through %d frames no ring spins: worst %.1f deg turned against its rows' %.1f and the bend's %.1f, %.1f over %.0fx theirs + the bend (at most %.0f), frame %d",
            src->name, pieceNames[br.lower], rr->numPoses, static_cast<double>(worstTurn[j]), static_cast<double>(worstEndTurn[j]),
            static_cast<double>(worstBendTurn[j]), static_cast<double>(worstTurnExcess[j]), static_cast<double>(turnShare), static_cast<double>(turnSlack),
            worstTurnPose[j]);
        check(worstTurnExcess[j] <= turnSlack, what);
    }
}
} // namespace

void bridgeCheck_f()
{
    const char* only = Cmd_Argc() > 1 ? Cmd_Argv(1) : nullptr;
    constexpr const char* testName = "agarmor bridge check";
    testkit::Tally tally;
    const auto check = [&](bool ok, const char* what) {
        if(tally.check(ok, testName, what))
        {
            Con_Printf("%s: PASS %s\n", testName, what);
        }
    };
    char what[320];
    for(const char* model : bridgeCheckModels)
    {
        if(only && strcmp(only, model))
        {
            continue;
        }
        qmodel_t* src = Mod_ForName(model, false);
        const Shell* s = src ? shellFor(src) : nullptr;
        const rig2::Rig* rig = src ? rigOfShell(src) : nullptr;
        if(!s || !rig)
        {
            q_snprintf(what, sizeof(what), "%s: no shell", model);
            check(false, what);
            continue;
        }
        int made = 0;
        for(const Bridge& br : s->bridges)
        {
            made += br.ok ? 1 : 0;
        }
        q_snprintf(what, sizeof(what), "%s: %d joints bridged (4 wanted), %d vertices (at most %d)", model, made, s->bridgeVerts, bridgeMostVerts);
        check(made == NumJoints && s->bridgeVerts <= bridgeMostVerts && count(s->bridgeRestP) == s->bridgeVerts, what);
        for(int j = 0; j < NumJoints; j++)
        {
            const Bridge& br = s->bridges[j];
            const int hinge = br.ok ? br.plates[1].bone : -1;
            if(hinge < 0 || hinge >= rig->numBones)
            {
                continue;
            }
            // The plates' own vertices nearest each end ring's corners (their feet), carried as they're drawn.
            Skinned feet[2][bridgeCols];
            float restGap = 0.f;
            for(int e = 0; e < 2; e++)
            {
                const int piece = e == 0 ? br.upper : br.lower;
                for(int c = 0; c < bridgeCols; c++)
                {
                    float best = 1e9f;
                    for(const Vert& v : s->verts)
                    {
                        const float d = v.piece == piece ? glm::distance(v.p, br.plates[e].ring[c].p) : 1e9f;
                        if(d < best)
                        {
                            best = d;
                            feet[e][c] = skinnedOf(v.p, v.w);
                        }
                    }
                    restGap = za::max(restGap, best);
                }
            }
            // The joint swept through its range (its bend from straight), the rest of the body at rest.
            const glm::vec2 range = rig->bones[hinge].range;
            float worstGap = 0.f, worstOut = 0.f, worstAlone = 0.f, worstAt = 0.f, worstDeg = 0.f;
            int outside = 0, inner = 0, tested = 0, bad = 0, atRows = 0;
            constexpr int steps = 9;
            for(int k = 0; k < steps; k++)
            {
                rig2::Pose pose = rig2::restPose(*rig);
                pose.a[hinge].x = range.x + (range.y - range.x) * static_cast<float>(k) / static_cast<float>(steps - 1);
                rigpose::Rigid F[rig2::maxBones];
                rig2::forward(*rig, pose, F);
                float bones[rig2::maxBones * 12]{};
                for(int b = 0; b < rig->numBones; b++)
                {
                    const glm::mat3 r = glm::mat3_cast(F[b].rot);
                    for(int row = 0; row < 3; row++)
                    {
                        bones[b * 12 + row * 4 + 0] = r[0][row];
                        bones[b * 12 + row * 4 + 1] = r[1][row];
                        bones[b * 12 + row * 4 + 2] = r[2][row];
                        bones[b * 12 + row * 4 + 3] = F[b].pos[row];
                    }
                }
                glm::vec3 P[bridgeMostRings * ringVerts], N[bridgeMostRings * ringVerts];
                BridgeDiag d;
                if(!evalBridge(*s, j, bones, rig->numBones, true, true, P, N, &d))
                {
                    bad++;
                    continue;
                }
                for(int e = 0; e < 2; e++)
                {
                    const int ring = e == 0 ? 0 : br.rings - 1;
                    for(int c = 0; c <= bridgeCols; c++)
                    {
                        glm::vec3 foot;
                        if(posedOf(feet[e][c % bridgeCols], bones, rig->numBones, foot))
                        {
                            worstGap = za::max(worstGap, glm::distance(foot, P[ring * ringVerts + c]));
                        }
                    }
                }
                for(int v = 0; v < br.rings * ringVerts; v++)
                {
                    bad += P[v] == P[v] && N[v] == N[v] ? 0 : 1; // (NaN)
                }
                Con_DPrintf("agarmor bridge check:   %s %s %.0f deg: %d of %d out (worst %.2f at %.2f), crease %d, rows %d\n", model, pieceNames[br.lower],
                    static_cast<double>(pose.a[hinge].x), d.outside, d.under, static_cast<double>(d.worst), static_cast<double>(d.worstAt),
                    d.outsideInner, d.atRows);
                tested += d.under;
                outside += d.outside;
                atRows += d.atRows;
                inner += d.outsideInner;
                if(d.worst > worstOut)
                {
                    worstOut = d.worst;
                    worstAt = d.worstAt;
                    worstDeg = pose.a[hinge].x;
                }
                // (a part worn alone: the other end on the skin; the worn row's ring still on its row)
                for(int alone = 0; alone < 2; alone++)
                {
                    if(!evalBridge(*s, j, bones, rig->numBones, alone == 0, alone == 1, P, N, nullptr))
                    {
                        bad++;
                        continue;
                    }
                    const int ring = alone == 0 ? 0 : br.rings - 1;
                    for(int c = 0; c < bridgeCols; c++)
                    {
                        glm::vec3 foot;
                        if(posedOf(feet[alone][c], bones, rig->numBones, foot))
                        {
                            worstAlone = za::max(worstAlone, glm::distance(foot, P[ring * ringVerts + c]));
                        }
                    }
                }
            }
            q_snprintf(what, sizeof(what), "%s %s: end rings on the plates' rows over %.0f..%.0f deg: furthest %.3f (rest %.3f; alone %.3f), %d bad",
                model, pieceNames[br.lower], static_cast<double>(range.x), static_cast<double>(range.y), static_cast<double>(worstGap),
                static_cast<double>(restGap), static_cast<double>(worstAlone), bad);
            check(bad == 0 && worstGap <= restGap + 0.02f && worstAlone <= restGap + 0.02f, what);
            q_snprintf(what, sizeof(what), "%s %s: skin under the leather: %d of %d samples out (furthest %.2f at ring %.2f of %d, %.0f deg), %d in the crease, %d at the rows (%d kept)",
                model, pieceNames[br.lower], outside, tested, static_cast<double>(worstOut), static_cast<double>(worstAt), br.rings - 1,
                static_cast<double>(worstDeg), inner, atRows, br.numUnder);
            check(tested > 0 && outside == 0, what);
        }
        continuityCheck(src, *s, *rig, check);
        sweepCheck(model, *s, *rig, check);
        restCheck(model, *s, *rig, check);
    }
    // armour.txt's settings on the leather: the grunt's left knee's sleeve parts (the table's entries put back after).
    qmodel_t* grunt = !only || !strcmp(only, checked[0]) ? Mod_ForName(checked[0], false) : nullptr;
    const Shell* s = grunt ? shellFor(grunt) : nullptr;
    if(s && s->bridges[KneeL].ok && s->bridges[KneeR].ok)
    {
        using namespace armourdata;
        (void)entry("grunt", PartThighSleeve, SideLeft, true);
        (void)entry("grunt", PartShinSleeve, SideLeft, true);
        Override* tu = entry("grunt", PartThighSleeve, SideLeft, false);
        Override* tl = entry("grunt", PartShinSleeve, SideLeft, false);
        const Override wasU = *tu, wasL = *tl;
        const uint32_t hashWas = s->hash;
        const auto ringMean = [](const Shell& sh, int j, int ring) {
            const Bridge& br = sh.bridges[j];
            glm::vec3 m{0.f};
            for(int c = 0; c < bridgeCols; c++)
            {
                m += at(sh.bridgeRestP, br.first + ring * ringVerts + c) / static_cast<float>(bridgeCols);
            }
            float r = 0.f;
            for(int c = 0; c < bridgeCols; c++)
            {
                r += glm::distance(at(sh.bridgeRestP, br.first + ring * ringVerts + c), m) / static_cast<float>(bridgeCols);
            }
            return r;
        };
        const int midRing = s->bridges[KneeL].rings / 2, lastRing = s->bridges[KneeL].rings - 1;
        const float midWas[2] = {ringMean(*s, KneeL, midRing), ringMean(*s, KneeR, midRing)};
        const float endWas = ringMean(*s, KneeL, 0) + ringMean(*s, KneeL, lastRing);
        // (leather "0" on the thigh's sleeve: that knee's leather gone, the other's kept)
        tu->v[Leather] = 0.f;
        tu->has |= 1u << Leather;
        changed();
        s = shellFor(grunt);
        q_snprintf(what, sizeof(what), "leather \"0\" on the left thigh's sleeve: the left knee's leather %s, the right's %s",
            s && s->bridges[KneeL].ok ? "made" : "gone", s && s->bridges[KneeR].ok ? "made" : "gone");
        check(s && !s->bridges[KneeL].ok && s->bridges[KneeR].ok, what);
        // (the taper: the thigh's sleeve 1 -> 1.4, the shin's 1.4 -> 0.7: the middle ring wider, the ends on their rows)
        *tu = wasU;
        tu->v[TaperStart] = 1.f;
        tu->v[TaperEnd] = 1.4f;
        tl->v[TaperStart] = 1.4f;
        tl->v[TaperEnd] = 0.7f;
        tu->has |= (1u << TaperStart) | (1u << TaperEnd);
        tl->has |= (1u << TaperStart) | (1u << TaperEnd);
        changed();
        s = shellFor(grunt);
        const float midL = s ? ringMean(*s, KneeL, midRing) / za::max(midWas[0], 1e-3f) : 0.f;
        const float midR = s ? ringMean(*s, KneeR, midRing) / za::max(midWas[1], 1e-3f) : 0.f;
        const float ends = s ? ringMean(*s, KneeL, 0) + ringMean(*s, KneeL, lastRing) - endWas : 1.f;
        q_snprintf(what, sizeof(what), "the left knee's leather taper: its middle ring %.3fx (about 1.4 wanted), the right's %.3fx (1), its ends moved %.4f (0)",
            static_cast<double>(midL), static_cast<double>(midR), static_cast<double>(ends));
        check(s && midL > 1.25f && za::abs(midR - 1.f) < 0.01f && za::abs(ends) < 1e-3f, what);
        // (folds 1 on the shin's sleeve: the left knee's one fold, seven rings; the right's the global setting)
        *tu = wasU;
        *tl = wasL;
        tl->v[Folds] = 1.f;
        tl->has |= 1u << Folds;
        changed();
        s = shellFor(grunt);
        const int global = za::clamp(static_cast<int>(vr_agarmor_folds.value + 0.5f), 0, bridgeMostFolds);
        q_snprintf(what, sizeof(what), "folds \"1\" on the left shin's sleeve: the left knee's folds %d in %d rings (1 in 7), the right's %d (%d)",
            s ? s->bridges[KneeL].folds : -1, s ? s->bridges[KneeL].rings : -1, s ? s->bridges[KneeR].folds : -1, global);
        check(s && s->bridges[KneeL].folds == 1 && s->bridges[KneeL].rings == 7 && s->bridges[KneeR].folds == global, what);
        *tu = wasU;
        *tl = wasL;
        changed();
        s = shellFor(grunt);
        check(s && s->hash == hashWas, "the leather's settings cleared: the same shell as before");
    }
    tally.verdict(testName);
}

} // namespace qvr::agarmor::detail
